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

#include "path_cache.h"
#include "los_config.h"
#include "los_hash.h"
#include "stdlib.h"
#include "limits.h"
#include "vnode.h"
// 路径缓存哈希掩码，用于计算哈希表索引（哈希表大小-1）
#define PATH_CACHE_HASH_MASK (LOSCFG_MAX_PATH_CACHE_SIZE - 1)
// 路径缓存哈希表，数组元素为链表头
LIST_HEAD g_pathCacheHashEntrys[LOSCFG_MAX_PATH_CACHE_SIZE];

#ifdef LOSCFG_DEBUG_VERSION                          // 如果启用调试版本
// 路径缓存命中总数
static int g_totalPathCacheHit = 0;
// 路径缓存尝试查找总数
static int g_totalPathCacheTry = 0;
// 跟踪缓存尝试查找次数的宏
#define TRACE_TRY_CACHE() do { g_totalPathCacheTry++; } while (0)
// 跟踪缓存命中次数的宏（更新缓存项和全局命中计数）
#define TRACE_HIT_CACHE(pc) do { pc->hit++; g_totalPathCacheHit++; } while (0)

/**
 * @brief 重置路径缓存命中统计信息
 * @param hit [输出] 命中次数
 * @param try [输出] 尝试次数
 */
void ResetPathCacheHitInfo(int *hit, int *try)
{
    *hit = g_totalPathCacheHit;                     // 保存当前命中次数
    *try = g_totalPathCacheTry;                     // 保存当前尝试次数
    g_totalPathCacheHit = 0;                        // 重置命中计数
    g_totalPathCacheTry = 0;                        // 重置尝试计数
}
#else                                                // 否则(非调试版本)
// 空宏定义，不进行缓存跟踪
#define TRACE_TRY_CACHE()
// 空宏定义，不进行缓存跟踪
#define TRACE_HIT_CACHE(pc)
#endif

/**
 * @brief 初始化路径缓存哈希表
 * @return 成功返回LOS_OK，否则返回错误码
 */
int PathCacheInit(void)
{
    for (int i = 0; i < LOSCFG_MAX_PATH_CACHE_SIZE; i++) {  // 遍历哈希表所有桶
        LOS_ListInit(&g_pathCacheHashEntrys[i]);            // 初始化每个桶的链表头
    }
    return LOS_OK;                                          // 返回初始化成功
}

/**
 * @brief 打印路径缓存内容（调试用）
 */
void PathCacheDump(void)
{
    PRINTK("-------->pathCache dump in\n");              // 打印 dump 开始标记
    for (int i = 0; i < LOSCFG_MAX_PATH_CACHE_SIZE; i++) {  // 遍历哈希表所有桶
        struct PathCache *pc = NULL;                        // 路径缓存项指针
        LIST_HEAD *nhead = &g_pathCacheHashEntrys[i];       // 当前桶的链表头

        // 遍历当前桶中的所有缓存项
        LOS_DL_LIST_FOR_EACH_ENTRY(pc, nhead, struct PathCache, hashEntry) {
            // 打印缓存项信息：哈希桶索引、路径名、父/子虚拟节点指针、路径长度
            PRINTK("    pathCache dump hash %d item %s %p %p %d\n", i,
                pc->name, pc->parentVnode, pc->childVnode, pc->nameLen);
        }
    }
    PRINTK("-------->pathCache dump out\n");             // 打印 dump 结束标记
}

/**
 * @brief 打印路径缓存内存使用情况（调试用）
 */
void PathCacheMemoryDump(void)
{
    int pathCacheNum = 0;                                 // 缓存项总数
    int nameSum = 0;                                      // 路径名总长度
    for (int i = 0; i < LOSCFG_MAX_PATH_CACHE_SIZE; i++) { // 遍历哈希表所有桶
        LIST_HEAD *dhead = &g_pathCacheHashEntrys[i];      // 当前桶的链表头
        struct PathCache *dent = NULL;                     // 路径缓存项指针

        // 遍历当前桶中的所有缓存项
        LOS_DL_LIST_FOR_EACH_ENTRY(dent, dhead, struct PathCache, hashEntry) {
            pathCacheNum++;                                // 增加缓存项计数
            nameSum += dent->nameLen;                      // 累加路径名长度
        }
    }
    PRINTK("pathCache number = %d\n", pathCacheNum);       // 打印缓存项总数
    // 打印总内存使用量：缓存项结构体大小 + 路径名字符串大小
    PRINTK("pathCache memory size = %d(B)\n", pathCacheNum * sizeof(struct PathCache) + nameSum);
}

/**
 * @brief 计算路径名的哈希值
 * @param name 路径名字符串
 * @param len 路径名长度
 * @param dvp 父目录虚拟节点
 * @return 计算得到的哈希值
 */
static uint32_t NameHash(const char *name, int len, struct Vnode *dvp)
{
    uint32_t hash;
    // 使用FNV-1a算法计算路径名字符串的哈希
    hash = LOS_HashFNV32aBuf(name, len, FNV1_32A_INIT);
    // 结合父目录虚拟节点指针计算最终哈希值
    hash = LOS_HashFNV32aBuf(&dvp, sizeof(struct Vnode *), hash);
    return hash;
}

/**
 * @brief 将路径缓存项插入哈希表
 * @param parent 父目录虚拟节点
 * @param cache 要插入的路径缓存项
 * @param name 路径名字符串
 * @param len 路径名长度
 */
static void PathCacheInsert(struct Vnode *parent, struct PathCache *cache, const char* name, int len)
{
    // 计算哈希索引并插入哈希表对应桶中
    int hash = NameHash(name, len, parent) & PATH_CACHE_HASH_MASK;
    LOS_ListAdd(&g_pathCacheHashEntrys[hash], &cache->hashEntry);
}

/**
 * @brief 分配并初始化路径缓存项
 * @param parent 父目录虚拟节点
 * @param vnode 对应的子虚拟节点
 * @param name 路径名字符串
 * @param len 路径名长度
 * @return 成功返回路径缓存项指针，失败返回NULL
 */
struct PathCache *PathCacheAlloc(struct Vnode *parent, struct Vnode *vnode, const char *name, uint8_t len)
{
    struct PathCache *pc = NULL;                          // 路径缓存项指针
    size_t pathCacheSize;                                 // 缓存项总大小
    int ret;                                              // 函数返回值

    // 参数合法性检查：路径名、长度、父节点、子节点均需有效
    if (name == NULL || len > NAME_MAX || parent == NULL || vnode == NULL) {
        return NULL;
    }
    pathCacheSize = sizeof(struct PathCache) + len + 1;   // 计算缓存项总大小（结构体+路径名+结束符）

    pc = (struct PathCache*)zalloc(pathCacheSize);        // 分配缓存项内存
    if (pc == NULL) {                                     // 检查内存分配是否失败
        PRINT_ERR("pathCache alloc failed, no memory!\n");  // 输出内存分配失败错误信息
        return NULL;
    }

    // 安全拷贝路径名字符串
    ret = strncpy_s(pc->name, len + 1, name, len);
    if (ret != LOS_OK) {                                  // 检查字符串拷贝是否失败
        free(pc);                                         // 释放已分配的内存
        return NULL;
    }

    pc->parentVnode = parent;                             // 设置父目录虚拟节点
    pc->nameLen = len;                                    // 设置路径名长度
    pc->childVnode = vnode;                               // 设置子虚拟节点

    // 将缓存项添加到父节点的子路径缓存链表
    LOS_ListAdd((&(parent->childPathCaches)), (&(pc->childEntry)));
    // 将缓存项添加到子节点的父路径缓存链表
    LOS_ListAdd((&(vnode->parentPathCaches)), (&(pc->parentEntry)));

    PathCacheInsert(parent, pc, name, len);               // 将缓存项插入哈希表

    return pc;                                            // 返回初始化后的缓存项
}

/**
 * @brief 释放路径缓存项
 * @param pc 要释放的路径缓存项
 * @return 成功返回LOS_OK，失败返回错误码
 */
int PathCacheFree(struct PathCache *pc)
{
    if (pc == NULL) {                                     // 检查缓存项指针是否有效
        PRINT_ERR("pathCache free: invalid pathCache\n"); // 输出无效缓存项错误信息
        return -ENOENT;
    }

    LOS_ListDelete(&pc->hashEntry);                       // 从哈希表中删除缓存项
    LOS_ListDelete(&pc->parentEntry);                     // 从父节点链表中删除缓存项
    LOS_ListDelete(&pc->childEntry);                      // 从子节点链表中删除缓存项
    free(pc);                                             // 释放缓存项内存

    return LOS_OK;                                        // 返回释放成功
}

/**
 * @brief 查找路径缓存项
 * @param parent 父目录虚拟节点
 * @param name 路径名字符串
 * @param len 路径名长度
 * @param vnode [输出] 找到的子虚拟节点
 * @return 成功返回LOS_OK，未找到返回-ENOENT
 */
int PathCacheLookup(struct Vnode *parent, const char *name, int len, struct Vnode **vnode)
{
    struct PathCache *pc = NULL;                          // 路径缓存项指针
    // 计算哈希索引并获取对应哈希桶
    int hash = NameHash(name, len, parent) & PATH_CACHE_HASH_MASK;
    LIST_HEAD *dhead = &g_pathCacheHashEntrys[hash];      // 哈希桶链表头

    TRACE_TRY_CACHE();                                    // 增加缓存尝试查找计数
    // 遍历哈希桶中的所有缓存项
    LOS_DL_LIST_FOR_EACH_ENTRY(pc, dhead, struct PathCache, hashEntry) {
        // 匹配条件：父节点相同、路径长度相同、路径名相同
        if (pc->parentVnode == parent && pc->nameLen == len && !strncmp(pc->name, name, len)) {
            *vnode = pc->childVnode;                      // 设置输出的子虚拟节点
            TRACE_HIT_CACHE(pc);                          // 增加缓存命中计数
            return LOS_OK;                                // 返回查找成功
        }
    }
    return -ENOENT;                                       // 未找到缓存项
}

/**
 * @brief 释放虚拟节点的子路径缓存项
 * @param vnode 虚拟节点
 */
static void FreeChildPathCache(struct Vnode *vnode)
{
    struct PathCache *item = NULL;                        // 当前缓存项
    struct PathCache *nextItem = NULL;                    // 下一个缓存项（防止删除当前项后链表断裂）

    // 安全遍历并释放子路径缓存链表中的所有项
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, nextItem, &(vnode->childPathCaches), struct PathCache, childEntry) {
        PathCacheFree(item);                              // 释放当前缓存项
    }
}

/**
 * @brief 释放虚拟节点的父路径缓存项
 * @param vnode 虚拟节点
 */
static void FreeParentPathCache(struct Vnode *vnode)
{
    struct PathCache *item = NULL;                        // 当前缓存项
    struct PathCache *nextItem = NULL;                    // 下一个缓存项（防止删除当前项后链表断裂）

    // 安全遍历并释放父路径缓存链表中的所有项
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, nextItem, &(vnode->parentPathCaches), struct PathCache, parentEntry) {
        PathCacheFree(item);                              // 释放当前缓存项
    }
}

/**
 * @brief 释放虚拟节点关联的所有路径缓存项
 * @param vnode 虚拟节点
 */
void VnodePathCacheFree(struct Vnode *vnode)
{
    if (vnode == NULL) {                                  // 检查虚拟节点指针是否有效
        return;
    }
    FreeParentPathCache(vnode);                           // 释放父路径缓存项
    FreeChildPathCache(vnode);                            // 释放子路径缓存项
}

/**
 * @brief 获取路径缓存哈希表
 * @return 哈希表数组首地址
 */
LIST_HEAD* GetPathCacheList()
{
    return g_pathCacheHashEntrys;                         // 返回全局哈希表数组
}