/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "internal.h"
#include "proc_fs.h"
#include "vnode.h"
#include "path_cache.h"
#include "los_vm_filemap.h"

#ifdef LOSCFG_DEBUG_VERSION

// 缓存清理命令宏定义
#define CLEAR_ALL_CACHE  "clear all"        // 清除所有缓存（路径缓存+页缓存）
#define CLEAR_PATH_CACHE "clear pathcache"  // 仅清除路径缓存
#define CLEAR_PAGE_CACHE "clear pagecache"  // 仅清除页缓存

/**
 * @brief 将Vnode类型枚举转换为字符串标识
 * @param type Vnode类型枚举值
 * @return 对应类型的3字符缩写字符串（UKN表示未知类型，BAD表示无效类型）
 */
static char* VnodeTypeToStr(enum VnodeType type)
{
    switch (type) {
        case VNODE_TYPE_UNKNOWN: return "UKN";  // 未知类型
        case VNODE_TYPE_REG:     return "REG";  // 普通文件
        case VNODE_TYPE_DIR:     return "DIR";  // 目录文件
        case VNODE_TYPE_BLK:     return "BLK";  // 块设备文件
        case VNODE_TYPE_CHR:     return "CHR";  // 字符设备文件
        case VNODE_TYPE_BCHR:    return "BCH";  // 缓冲字符设备
        case VNODE_TYPE_FIFO:    return "FIF";  // 管道文件
        case VNODE_TYPE_LNK:     return "LNK";  // 符号链接
        default:                 return "BAD";  // 无效类型
    }
}

/**
 * @brief 处理Vnode列表并格式化输出到序列缓冲区
 * @param buf 序列缓冲区指针，用于存储Vnode信息
 * @param list 要处理的Vnode链表头指针
 * @return 处理的Vnode数量
 */
static int VnodeListProcess(struct SeqBuf *buf, LIST_HEAD* list)
{
    int count = 0;                          // Vnode计数
    struct Vnode *item = NULL;              // 当前Vnode节点
    struct Vnode *nextItem = NULL;          // 下一个Vnode节点（用于安全遍历）

    // 安全遍历Vnode链表（防止遍历中节点被释放导致崩溃）
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, nextItem, list, struct Vnode, actFreeEntry) {
        // 格式化输出Vnode详细信息：地址、父节点、数据指针、操作集、哈希值、引用计数、类型、GID、UID、权限和路径
        LosBufPrintf(buf, "%-10p    %-10p     %-10p    %10p    0x%08x    %-3d    %-4s    %-3d    %-3d    %-8o\t%s\n",
            item, item->parent, item->data, item->vop, item->hash, item->useCount,
            VnodeTypeToStr(item->type), item->gid, item->uid, item->mode, item->filePath);
        count++;  // 计数递增
    }

    return count;  // 返回处理的Vnode总数
}

/**
 * @brief 处理路径缓存列表并格式化输出到序列缓冲区
 * @param buf 序列缓冲区指针，用于存储路径缓存信息
 * @return 处理的路径缓存条目数量
 */
static int PathCacheListProcess(struct SeqBuf *buf)
{
    int count = 0;                          // 路径缓存计数
    LIST_HEAD* bucketList = GetPathCacheList();  // 获取路径缓存哈希桶列表

    // 遍历所有路径缓存哈希桶（LOSCFG_MAX_PATH_CACHE_SIZE为哈希桶数量）
    for (int i = 0; i < LOSCFG_MAX_PATH_CACHE_SIZE; i++) {
        struct PathCache *pc = NULL;        // 当前路径缓存条目
        LIST_HEAD *list = &bucketList[i];   // 当前哈希桶链表头

        // 遍历当前哈希桶中的所有路径缓存条目
        LOS_DL_LIST_FOR_EACH_ENTRY(pc, list, struct PathCache, hashEntry) {
            // 格式化输出路径缓存信息：桶编号、缓存地址、父Vnode、子Vnode、命中次数和路径名
            LosBufPrintf(buf, "%-3d    %-10p    %-11p    %-10p    %-9d    %s\n", i, pc,
                pc->parentVnode, pc->childVnode, pc->hit, pc->name);
            count++;  // 计数递增
        }
    }

    return count;  // 返回处理的路径缓存总数
}

/**
 * @brief 处理单个页缓存映射并输出页偏移列表
 * @param buf 序列缓冲区指针，用于存储页缓存信息
 * @param mapping 页映射结构体指针
 * @return 处理的页缓存数量
 */
static int PageCacheEntryProcess(struct SeqBuf *buf, struct page_mapping *mapping)
{
    int total = 0;                          // 页缓存计数
    LosFilePage *fpage = NULL;              // 文件页指针

    // 如果没有缓存页，输出"null]"并返回
    if (mapping->nrpages == 0) {
        LosBufPrintf(buf, "null]\n");
        return total;
    }

    // 遍历该映射下的所有文件页
    LOS_DL_LIST_FOR_EACH_ENTRY(fpage, &mapping->page_list, LosFilePage, node) {
        LosBufPrintf(buf, "%d,", fpage->pgoff);  // 输出页偏移量，以逗号分隔
        total++;  // 计数递增
    }
    LosBufPrintf(buf, "]\n");  // 输出列表结束符
    return total;  // 返回处理的页缓存总数
}

/**
 * @brief 处理所有活动Vnode的页缓存映射并输出汇总信息
 * @param buf 序列缓冲区指针，用于存储页缓存汇总信息
 * @return 处理的总页缓存数量
 */
static int PageCacheMapProcess(struct SeqBuf *buf)
{
    LIST_HEAD *vnodeList = GetVnodeActiveList();  // 获取活动Vnode列表
    struct page_mapping *mapping = NULL;         // 页映射结构体指针
    struct Vnode *vnode = NULL;                  // Vnode指针
    int total = 0;                               // 总页缓存计数

    VnodeHold();  // 持有Vnode锁，防止遍历过程中Vnode被修改
    // 遍历所有活动Vnode
    LOS_DL_LIST_FOR_EACH_ENTRY(vnode, vnodeList, struct Vnode, actFreeEntry) {
        mapping = &vnode->mapping;  // 获取当前Vnode的页映射
        // 输出Vnode地址、路径和页偏移列表起始标记
        LosBufPrintf(buf, "%p, %s:[", vnode, vnode->filePath);
        total += PageCacheEntryProcess(buf, mapping);  // 处理并累加页缓存数量
    }
    VnodeDrop();  // 释放Vnode锁
    return total;  // 返回总页缓存数量
}

/**
 * @brief 填充文件系统缓存信息到序列缓冲区
 * @details 汇总输出Vnode、路径缓存和页缓存的详细信息及统计数据
 * @param buf 序列缓冲区指针，用于存储缓存信息
 * @param arg 未使用的参数
 * @return 0表示成功，非0表示错误码
 */
static int FsCacheInfoFill(struct SeqBuf *buf, void *arg)
{
    int vnodeFree;          // 空闲Vnode数量
    int vnodeActive;        // 活动Vnode数量
    int vnodeVirtual;       // 虚拟Vnode数量
    int vnodeTotal;         // Vnode总数

    int pathCacheTotal;     // 路径缓存总数
    int pathCacheTotalTry = 0;  // 路径缓存尝试访问次数
    int pathCacheTotalHit = 0;  // 路径缓存命中次数

    int pageCacheTotal;     // 页缓存总数
    int pageCacheTotalTry = 0;  // 页缓存尝试访问次数
    int pageCacheTotalHit = 0;  // 页缓存命中次数

    // 重置路径缓存和页缓存的命中统计信息
    ResetPathCacheHitInfo(&pathCacheTotalHit, &pathCacheTotalTry);
    ResetPageCacheHitInfo(&pageCacheTotalTry, &pageCacheTotalHit);

    VnodeHold();  // 持有Vnode锁，确保数据一致性
    // 输出Vnode信息表头
    LosBufPrintf(buf, "\n=================================================================\n");
    LosBufPrintf(buf,
        "VnodeAddr     ParentAddr     DataAddr      VnodeOps      Hash           Ref    Type    Gid    Uid    Mode\n");
    // 处理各类Vnode列表并统计数量
    vnodeVirtual = VnodeListProcess(buf, GetVnodeVirtualList());
    vnodeFree = VnodeListProcess(buf, GetVnodeFreeList());
    vnodeActive = VnodeListProcess(buf, GetVnodeActiveList());
    vnodeTotal = vnodeVirtual + vnodeFree + vnodeActive;  // 计算Vnode总数

    // 输出路径缓存信息表头
    LosBufPrintf(buf, "\n=================================================================\n");
    LosBufPrintf(buf, "No.    CacheAddr     ParentAddr     ChildAddr     HitCount     Name\n");
    pathCacheTotal = PathCacheListProcess(buf);  // 处理路径缓存并统计数量

    // 输出页缓存信息表头
    LosBufPrintf(buf, "\n=================================================================\n");
    pageCacheTotal = PageCacheMapProcess(buf);  // 处理页缓存并统计数量

    // 输出缓存统计汇总信息
    LosBufPrintf(buf, "\n=================================================================\n");
    LosBufPrintf(buf, "PathCache Total:%d Try:%d Hit:%d\n",
        pathCacheTotal, pathCacheTotalTry, pathCacheTotalHit);
    LosBufPrintf(buf, "Vnode Total:%d Free:%d Virtual:%d Active:%d\n",
        vnodeTotal, vnodeFree, vnodeVirtual, vnodeActive);
    LosBufPrintf(buf, "PageCache total:%d Try:%d Hit:%d\n", pageCacheTotal, pageCacheTotalTry, pageCacheTotalHit);
    VnodeDrop();  // 释放Vnode锁
    return 0;      // 成功完成信息填充
}

/**
 * @brief 处理文件系统缓存清理命令
 * @param pf Proc文件结构体指针
 * @param buffer 输入缓冲区，包含清理命令
 * @param buflen 输入缓冲区长度
 * @param ppos 文件位置指针（未使用）
 * @return 成功返回输入长度，失败返回错误码
 */
static int FsCacheClear(struct ProcFile *pf, const char *buffer, size_t buflen, loff_t *ppos)
{
    // 检查输入缓冲区有效性及长度是否足够
    if (buffer == NULL || buflen < sizeof(CLEAR_ALL_CACHE)) {
        return -EINVAL;  // 参数无效，返回错误码
    }
    int vnodeCount = 0;  // 清理的Vnode数量
    int pageCount = 0;   // 清理的页数量

    // 根据命令字符串执行相应的缓存清理操作
    if (!strcmp(buffer, CLEAR_ALL_CACHE)) {
        // 清除所有缓存：Vnode+路径缓存+页缓存
        vnodeCount = VnodeClearCache();
        pageCount = OsTryShrinkMemory(VM_FILEMAP_MAX_SCAN);
    } else if (!strcmp(buffer, CLEAR_PAGE_CACHE)) {
        // 仅清除页缓存
        pageCount = OsTryShrinkMemory(VM_FILEMAP_MAX_SCAN);
    } else if (!strcmp(buffer, CLEAR_PATH_CACHE)) {
        // 仅清除路径缓存（通过清除Vnode关联缓存实现）
        vnodeCount = VnodeClearCache();
    } else {
        return -EINVAL;  // 未知命令，返回错误码
    }

    // 输出清理结果日志
    PRINTK("%d vnodes and related pathcaches cleared\n%d pages cleared\n", vnodeCount, pageCount);
    return buflen;  // 返回输入长度表示成功
}

// 文件系统缓存proc文件操作集
static const struct ProcFileOperations FS_CACHE_PROC_FOPS = {
    .read = FsCacheInfoFill,   // 读操作：填充缓存信息
    .write = FsCacheClear,     // 写操作：处理缓存清理命令
};

/**
 * @brief 初始化/proc/fs_cache节点
 * @details 创建proc文件系统中的fs_cache节点，并关联操作函数集
 */
void ProcFsCacheInit(void)
{
    // 创建/proc/fs_cache节点，权限为0400（仅所有者可读）
    struct ProcDirEntry *pde = CreateProcEntry("fs_cache", 0400, NULL);
    if (pde == NULL) {
        PRINT_ERR("create fs_cache error!\n");  // 创建失败，输出错误日志
        return;
    }

    pde->procFileOps = &FS_CACHE_PROC_FOPS;  // 关联fs_cache节点的操作函数集
}
#else
void ProcFsCacheInit(void)
{
    /* do nothing in release version */
}
#endif
