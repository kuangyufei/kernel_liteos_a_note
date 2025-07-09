/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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

/**
 * @defgroup los_vm_dump virtual memory dump operation
 * @ingroup kernel
 */

#include "los_vm_dump.h"
#include "los_mmu_descriptor_v6.h"
#ifdef LOSCFG_FS_VFS
#include "fs/file.h"
#include "vnode.h"
#endif
#include "los_printf.h"
#include "los_vm_page.h"
#include "los_vm_phys.h"
#include "los_process_pri.h"
#include "los_atomic.h"
#include "los_vm_lock.h"
#include "los_memory_pri.h"


#ifdef LOSCFG_KERNEL_VM

#define     FLAG_SIZE               4
#define     FLAG_START              2
//获取线性区的名称或文件路径
const CHAR *OsGetRegionNameOrFilePath(LosVmMapRegion *region)
{
    struct Vnode *vnode = NULL;
    if (region == NULL) {
        return "";
#ifdef LOSCFG_FS_VFS
    } else if (LOS_IsRegionFileValid(region)) {
        vnode = region->unTypeData.rf.vnode;
        return vnode->filePath;
#endif
    } else if (region->regionFlags & VM_MAP_REGION_FLAG_HEAP) {//堆区
        return "HEAP";
    } else if (region->regionFlags & VM_MAP_REGION_FLAG_STACK) {//栈区
        return "STACK";
    } else if (region->regionFlags & VM_MAP_REGION_FLAG_TEXT) {//文本区
        return "Text";
    } else if (region->regionFlags & VM_MAP_REGION_FLAG_VDSO) {//虚拟动态链接对象区（Virtual Dynamically Shared Object、VDSO）
        return "VDSO";
    } else if (region->regionFlags & VM_MAP_REGION_FLAG_MMAP) {//映射区
        return "MMAP";
    } else if (region->regionFlags & VM_MAP_REGION_FLAG_SHM) {//共享区
        return "SHM";
    } else if (region->regionFlags & VM_MAP_REGION_FLAG_LITEIPC) {
        return "LITEIPC";
    } else {
        return "";
    }
    return "";
}

INT32 OsRegionOverlapCheckUnlock(LosVmSpace *space, LosVmMapRegion *region)
{
    LosVmMapRegion *regionTemp = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeNext = NULL;

    /* search the region list */
    RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)
        regionTemp = (LosVmMapRegion *)pstRbNode;
        if (region->range.base == regionTemp->range.base && region->range.size == regionTemp->range.size) {
            continue;
        }
        if (((region->range.base + region->range.size) > regionTemp->range.base) &&
            (region->range.base < (regionTemp->range.base + regionTemp->range.size))) {
            VM_ERR("overlap between regions:\n"
                   "flags:%#x base:%p size:%08x space:%p\n"
                   "flags:%#x base:%p size:%08x space:%p",
                   region->regionFlags, region->range.base, region->range.size, region->space,
                   regionTemp->regionFlags, regionTemp->range.base, regionTemp->range.size, regionTemp->space);
            return -1;
        }
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)

    return 0;
}

UINT32 OsShellCmdProcessVmUsage(LosVmSpace *space)
{
    LosVmMapRegion *region = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeNext = NULL;
    UINT32 used = 0;

    if (space == NULL) {
        return 0;
    }

    if (space == LOS_GetKVmSpace()) {//内核空间
        OsShellCmdProcessPmUsage(space, NULL, &used);
        return used;
    }
    UINT32 ret = LOS_MuxAcquire(&space->regionMux);
    if (ret != 0) {
        return 0;
    }
        RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)//开始扫描红黑树
            region = (LosVmMapRegion *)pstRbNode;//拿到线性区,注意LosVmMapRegion结构体的第一个变量就是pstRbNode,所以可直接(LosVmMapRegion *)转
            used += region->range.size;//size叠加,算出总使用
        RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)//结束扫描红黑树
    (VOID)LOS_MuxRelease(&space->regionMux);
    return used;
}

/**
 * @brief 计算内核进程的物理内存使用量
 * @param kSpace 内核虚拟空间指针
 * @param actualPm 输出参数，用于返回实际计算得到的内核物理内存使用量（字节）
 * @return 成功返回内核物理内存使用量，失败返回0
 * @note 该函数会统计内核常驻内存、动态内存，并排除用户进程使用的内存
 */
UINT32 OsKProcessPmUsage(LosVmSpace *kSpace, UINT32 *actualPm)
{
    UINT32 memUsed;          /* 内存使用量 */
    UINT32 totalMem;         /* 总内存大小 */
    UINT32 freeMem;          /* 空闲内存大小 */
    UINT32 usedCount = 0;    /* 已使用物理页数量 */
    UINT32 totalCount = 0;   /* 总物理页数量 */
    LosVmSpace *space = NULL;/* 虚拟空间指针 */
    LOS_DL_LIST *spaceList = NULL;/* 虚拟空间链表 */
    UINT32 UProcessUsed = 0; /* 用户进程使用的物理内存总量 */

    if (actualPm == NULL) {  /* 检查输出参数有效性 */
        return 0;
    }

    memUsed = LOS_MemTotalUsedGet(m_aucSysMem1);       /* 获取内存池已使用大小 */
    totalMem = LOS_MemPoolSizeGet(m_aucSysMem1);       /* 获取内存池总大小 */
    freeMem = totalMem - memUsed;                      /* 计算空闲内存大小 */

    OsVmPhysUsedInfoGet(&usedCount, &totalCount);      /* 获取物理内存使用页数信息 */
    /* Kernel resident memory, include default heap memory */
    memUsed = SYS_MEM_SIZE_DEFAULT - (totalCount << PAGE_SHIFT);  /* 计算内核常驻内存 */

    spaceList = LOS_GetVmSpaceList();                  // 获取虚拟空间链表,上面挂了所有虚拟空间
    LosMux *vmSpaceListMux = OsGVmSpaceMuxGet();
    (VOID)LOS_MuxAcquire(vmSpaceListMux);              /* 加锁保护虚拟空间链表操作 */
    /* 遍历所有虚拟空间，累加用户进程内存使用量 */
    LOS_DL_LIST_FOR_EACH_ENTRY(space, spaceList, LosVmSpace, node) {//遍历链表
        if (space == LOS_GetKVmSpace()) {              // 内核空间不统计
            continue;
        }
        UProcessUsed += OsUProcessPmUsage(space, NULL, NULL);  /* 累加用户进程物理内存 */
    }
    (VOID)LOS_MuxRelease(vmSpaceListMux);              /* 释放虚拟空间链表锁 */

    /* Kernel dynamic memory, include extended heap memory */  // 内核动态内存，包括扩展堆内存
    memUsed += ((usedCount << PAGE_SHIFT) - UProcessUsed);     /* 加上内核动态内存 */
    /* Remaining heap memory */  // 剩余堆内存
    memUsed -= freeMem;                               /* 减去空闲内存得到实际使用量 */

    *actualPm = memUsed;                               /* 通过输出参数返回结果 */
    return memUsed;
}

/**
 * @brief 处理进程物理内存使用量统计命令
 * @param space 虚拟空间指针（可为内核或用户空间）
 * @param sharePm 输出参数，用于返回共享内存大小（字节）
 * @param actualPm 输出参数，用于返回实际物理内存使用量（字节）
 * @return 成功返回物理内存使用量，失败返回0
 * @note 根据空间类型自动分发到内核或用户进程内存统计函数
 */
UINT32 OsShellCmdProcessPmUsage(LosVmSpace *space, UINT32 *sharePm, UINT32 *actualPm)
{
    if (space == NULL) {                          /* 检查虚拟空间有效性 */
        return 0;
    }

    if ((sharePm == NULL) && (actualPm == NULL)) {/* 至少需要一个输出参数 */
        return 0;
    }

    if (space == LOS_GetKVmSpace()) {             /* 判断是否为内核空间 */
        return OsKProcessPmUsage(space, actualPm);/* 调用内核进程内存统计 */
    }
    return OsUProcessPmUsage(space, sharePm, actualPm);/* 调用用户进程内存统计 */
}

/**
 * @brief 计算用户进程的物理内存使用量
 * @param space 用户进程虚拟空间指针
 * @param sharePm 输出参数，返回共享内存总大小（字节）
 * @param actualPm 输出参数，返回实际物理内存使用量（字节，考虑共享页）
 * @return 成功返回实际物理内存使用量，失败返回0
 * @note 采用比例分配法计算共享内存（1/refCounts），精确反映进程实际占用
 */
UINT32 OsUProcessPmUsage(LosVmSpace *space, UINT32 *sharePm, UINT32 *actualPm)
{
    LosVmMapRegion *region = NULL;                /* 虚拟内存区域指针 */
    LosRbNode *pstRbNode = NULL;                  /* 红黑树节点（遍历用） */
    LosRbNode *pstRbNodeNext = NULL;              /* 下一个红黑树节点（安全遍历） */
    LosVmPage *page = NULL;                       /* 物理页框指针 */
    VADDR_T vaddr;                                /* 虚拟地址 */
    size_t size;                                  /* 区域大小 */
    PADDR_T paddr;                                /* 物理地址 */
    STATUS_T ret;                                 /* 函数返回状态 */
    INT32 shareRef;                               /* 共享引用计数 */
    UINT32 pmSize = 0;                            /* 物理内存使用总量 */

    if (sharePm != NULL) {
        *sharePm = 0;                             /* 初始化共享内存统计值 */
    }

    ret = LOS_MuxAcquire(&space->regionMux);      /* 获取区域链表互斥锁 */
    if (ret != 0) {
        return 0;
    }
    /* 遍历所有虚拟内存区域（红黑树安全遍历） */
    RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)
        region = (LosVmMapRegion *)pstRbNode;     /* 转换为区域结构体 */
        vaddr = region->range.base;               /* 区域起始虚拟地址 */
        size = region->range.size;                /* 区域大小 */
        /* 按页遍历区域 */
        for (; size > 0; vaddr += PAGE_SIZE, size -= PAGE_SIZE) {
            /* 查询虚拟地址对应的物理地址 */
            ret = LOS_ArchMmuQuery(&space->archMmu, vaddr, &paddr, NULL);
            if (ret < 0) {                        /* 跳过未映射的地址 */
                continue;
            }
            page = LOS_VmPageGet(paddr);          /* 获取物理页框结构 */
            if (page == NULL) {                   /* 跳过无效页框 */
                continue;
            }

            /* 获取页框引用计数（共享内存核心判断） */
            shareRef = LOS_AtomicRead(&page->refCounts);
            if (shareRef > 1) {                   // ref > 1 表示该页被多个空间共享
                if (sharePm != NULL) {
                    *sharePm += PAGE_SIZE;        // 累加共享内存大小（4KB/页）
                }
                pmSize += PAGE_SIZE / shareRef;   /* 按引用比例计算实际占用 */
            } else {
                pmSize += PAGE_SIZE;               /* 独占页直接累加 */
            }
        }
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)  /* 修正原代码中oldVmSpace变量错误 @doubao太神奇了 */

    (VOID)LOS_MuxRelease(&space->regionMux);      /* 释放区域链表互斥锁 */

    if (actualPm != NULL) {
        *actualPm = pmSize;                       /* 通过输出参数返回结果 */
    }

    return pmSize;
}

/**
 * @brief 根据虚拟地址空间查找对应的进程控制块
 * @param[in]  space  虚拟地址空间指针
 * @return 成功返回进程控制块指针，失败返回 NULL
 */
LosProcessCB *OsGetPIDByAspace(const LosVmSpace *space)
{
    UINT32 pid;  // 进程ID循环变量
    UINT32 intSave;  // 中断状态保存变量
    LosProcessCB *processCB = NULL;  // 进程控制块指针

    SCHEDULER_LOCK(intSave);  // 锁定调度器防止进程切换
    // 遍历系统进程池（数组实现）查找匹配的虚拟地址空间
    for (pid = 0; pid < g_processMaxNum; ++pid) {
        processCB = g_processCBArray + pid;  // 获取当前索引的进程控制块
        if (OsProcessIsUnused(processCB)) {  // 跳过未使用的进程控制块
            continue;  // 继续下一个进程查找
        }

        if (processCB->vmSpace == space) {  // 找到虚拟地址空间所属进程
            SCHEDULER_UNLOCK(intSave);  // 解锁调度器
            return processCB;  // 返回找到的进程控制块
        }
    }
    SCHEDULER_UNLOCK(intSave);  // 遍历结束解锁调度器
    return NULL;  // 未找到对应进程
}

/**
 * @brief 统计虚拟内存区域的页面数量及PSS(比例共享大小)
 * @param[in]  space     虚拟地址空间指针
 * @param[in]  region    虚拟内存区域指针
 * @param[out] pssPages  PSS页面数量（可为NULL，表示不计算）
 * @return 区域中已映射的物理页面总数
 */
UINT32 OsCountRegionPages(LosVmSpace *space, LosVmMapRegion *region, UINT32 *pssPages)
{
    UINT32 regionPages = 0;  // 区域页面计数器
    PADDR_T paddr;  // 物理地址变量
    VADDR_T vaddr;  // 虚拟地址变量
    UINT32 ref;  // 页面引用计数
    STATUS_T status;  // 函数调用状态
    float pss = 0;  // PSS值累加器
    LosVmPage *page = NULL;  // 物理页面结构指针

    // 按页面大小遍历虚拟内存区域
    for (vaddr = region->range.base; vaddr < region->range.base + region->range.size; vaddr += PAGE_SIZE) {
        status = LOS_ArchMmuQuery(&space->archMmu, vaddr, &paddr, NULL);  // 查询虚拟地址对应的物理地址
        if (status == LOS_OK) {  // 物理地址映射有效
            regionPages++;  // 增加区域页面计数
            if (pssPages == NULL) {
                continue;  // 无需计算PSS，跳过后续逻辑
            }
            page = LOS_VmPageGet(paddr);  // 获取物理页面结构
            if (page != NULL) {
                ref = LOS_AtomicRead(&page->refCounts);  // 原子读取页面引用计数
                pss += (ref > 0) ? (1.0f / ref) : 1.0f;  // 按引用计数计算PSS贡献值
            } else {
                pss += 1.0f;  // 物理页面结构不存在，PSS直接加1
            }
        }
    }

    if (pssPages != NULL) {
        *pssPages = (UINT32)(pss + 0.5f);  // 四舍五入转换为整数PSS值
    }

    return regionPages;  // 返回区域映射页面总数
}

/**
 * @brief 统计整个虚拟地址空间使用的物理页面总数
 * @param[in] space  虚拟地址空间指针
 * @return 地址空间中已映射的物理页面总数
 */
UINT32 OsCountAspacePages(LosVmSpace *space)
{
    UINT32 spacePages = 0;  // 地址空间页面计数器
    LosVmMapRegion *region = NULL;  // 虚拟内存区域指针
    LosRbNode *pstRbNode = NULL;  // 红黑树节点指针
    LosRbNode *pstRbNodeNext = NULL;  // 红黑树下一节点指针

    // 安全遍历虚拟地址空间的红黑树区域（防止遍历中区域被修改）
    RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)
        region = (LosVmMapRegion *)pstRbNode;  // 将红黑树节点转换为区域结构
        spacePages += OsCountRegionPages(space, region, NULL);  // 累加当前区域的页面数
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)
    return spacePages;  // 返回地址空间总页面数
}

/**
 * @brief 将架构相关的内存标志转换为可读字符串
 * @param[in] archFlags  架构相关的内存标志（包含权限、缓存等信息）
 * @return 标志字符串（系统内存池分配，使用后需注意释放），失败返回 NULL
 */
CHAR *OsArchFlagsToStr(const UINT32 archFlags)
{
    UINT32 index;  // 循环索引变量
    UINT32 cacheFlags = archFlags & VM_MAP_REGION_FLAG_CACHE_MASK;  // 提取缓存类型标志
    UINT32 flagSize = FLAG_SIZE * BITMAP_BITS_PER_WORD * sizeof(CHAR);  // 计算字符串缓冲区大小
    CHAR *archMmuFlagsStr = (CHAR *)LOS_MemAlloc(m_aucSysMem0, flagSize);  // 分配字符串缓冲区
    if (archMmuFlagsStr == NULL) {
        return NULL;  // 内存分配失败
    }
    (VOID)memset_s(archMmuFlagsStr, flagSize, 0, flagSize);  // 初始化字符串缓冲区

    // 解析缓存类型标志并转换为字符串
    switch (cacheFlags) {
        case 0UL:
            (VOID)strcat_s(archMmuFlagsStr, flagSize, " CH\0");
            break;
        case 1UL:
            (VOID)strcat_s(archMmuFlagsStr, flagSize, " UC\0");
            break;
        case 2UL:
            (VOID)strcat_s(archMmuFlagsStr, flagSize, " UD\0");
            break;
        case 3UL:
            (VOID)strcat_s(archMmuFlagsStr, flagSize, " WC\0");
            break;
        default:
            break;
    }

    static const CHAR FLAGS[BITMAP_BITS_PER_WORD][FLAG_SIZE] = {
        [0 ... (__builtin_ffsl(VM_MAP_REGION_FLAG_PERM_USER) - 2)] = "???\0",
        [__builtin_ffsl(VM_MAP_REGION_FLAG_PERM_USER) - 1] = " US\0",
        [__builtin_ffsl(VM_MAP_REGION_FLAG_PERM_READ) - 1] = " RD\0",
        [__builtin_ffsl(VM_MAP_REGION_FLAG_PERM_WRITE) - 1] = " WR\0",
        [__builtin_ffsl(VM_MAP_REGION_FLAG_PERM_EXECUTE) - 1] = " EX\0",
        [__builtin_ffsl(VM_MAP_REGION_FLAG_NS) - 1] = " NS\0",
        [__builtin_ffsl(VM_MAP_REGION_FLAG_INVALID) - 1] = " IN\0",
        [__builtin_ffsl(VM_MAP_REGION_FLAG_INVALID) ... (BITMAP_BITS_PER_WORD - 1)] = "???\0",
    };

    // 遍历权限标志位并拼接对应字符串
    for (index = FLAG_START; index < BITMAP_BITS_PER_WORD; index++) {
        if (FLAGS[index][0] == '?') {
            continue;                            // 跳过未定义的标志位
        }

        if (archFlags & (1UL << index)) {         // 检查当前标志位是否置位
            if (strcat_s(archMmuFlagsStr, flagSize, FLAGS[index]) != EOK) {  // 拼接标志字符串
                PRINTK("flag string concat failed\n");  // 拼接失败打印错误
            }
        }
    }

    return archMmuFlagsStr;                       // 返回构建的标志字符串
}
/**
 * @brief 打印单个虚拟内存区域的详细信息
 * @param[in] space   虚拟地址空间指针
 * @param[in] region  虚拟内存区域指针
 * @details 输出区域地址、名称、地址范围、映射标志、页面数量及 PSS 信息
 */
VOID OsDumpRegion2(LosVmSpace *space, LosVmMapRegion *region)
{
    UINT32 pssPages = 0;  // PSS（比例共享大小）页面计数器
    UINT32 regionPages;   // 区域总页面计数器

    regionPages = OsCountRegionPages(space, region, &pssPages);  // 统计区域页面数和PSS
    CHAR *flagsStr = OsArchFlagsToStr(region->regionFlags);      // 转换内存标志为字符串
    if (flagsStr == NULL) {                                      // 内存标志转换失败
        return;                                                  // 直接返回，避免空指针访问
    }
    // 打印区域详细信息：地址、名称、基地址、大小、标志、页面数、PSS值
    PRINTK("\t %#010x  %-32.32s %#010x %#010x %-15.15s %4d    %4d\n",
        region, OsGetRegionNameOrFilePath(region), region->range.base,
        region->range.size, flagsStr, regionPages, pssPages);
    (VOID)LOS_MemFree(m_aucSysMem0, flagsStr);  // 释放标志字符串内存
}

/**
 * @brief 打印指定虚拟地址空间的内存使用概况
 * @param[in] space  虚拟地址空间指针
 * @details 输出进程ID、地址空间信息、总页面数及所有区域的详细分布
 */
VOID OsDumpAspace(LosVmSpace *space)
{
    LosVmMapRegion *region = NULL;               // 虚拟内存区域指针
    LosRbNode *pstRbNode = NULL;                 // 红黑树节点指针
    LosRbNode *pstRbNodeNext = NULL;             // 红黑树下一节点指针
    UINT32 spacePages;                           // 地址空间总页面数
    LosProcessCB *pcb = OsGetPIDByAspace(space);  // 通过虚拟空间找到进程实体

    if (pcb == NULL) {                           // 进程控制块不存在
        return;                                  // 无效地址空间，直接返回
    }
    spacePages = OsCountAspacePages(space);      // 获取地址空间总页数
    // 打印地址空间概览表头
    PRINTK("\r\n PID    aspace     name       base       size     pages \n");
    PRINTK(" ----   ------     ----       ----       -----     ----\n");
    // 打印进程ID、地址空间地址、名称、基地址、大小和总页数
    PRINTK(" %-4d %#010x %-10.10s %#010x %#010x     %d\n", pcb->processID, space, pcb->processName,
        space->base, space->size, spacePages);

    // 打印区域详情表头
    PRINTK("\r\n\t region      name                base       size       mmu_flags      pages   pg/ref\n");
    PRINTK("\t ------      ----                ----       ----       ---------      -----   -----\n");
    // 安全遍历地址空间的红黑树区域（防止遍历中区域被修改）
    RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)
        region = (LosVmMapRegion *)pstRbNode;  // 将红黑树节点转换为区域结构
        if (region != NULL) {
            OsDumpRegion2(space, region);       // 打印单个区域详情
            (VOID)OsRegionOverlapCheck(space, region);  // 检查区域重叠
        } else {
            PRINTK("region is NULL\n");         // 区域为空时打印提示
        }
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)
    return;
}

/**
 * @brief 打印系统中所有虚拟地址空间的内存使用情况
 * @details 遍历所有地址空间，依次打印每个空间的详细信息
 */
VOID OsDumpAllAspace(VOID)
{
    LosVmSpace *space = NULL;                    // 虚拟地址空间指针
    LOS_DL_LIST *aspaceList = LOS_GetVmSpaceList();  // 获取所有地址空间链表
    // 循环遍历所有地址空间节点
    LOS_DL_LIST_FOR_EACH_ENTRY(space, aspaceList, LosVmSpace, node) {
        (VOID)LOS_MuxAcquire(&space->regionMux);  // 获取区域互斥锁
        OsDumpAspace(space);                     // 打印当前地址空间信息
        (VOID)LOS_MuxRelease(&space->regionMux);  // 释放区域互斥锁
    }
    return;
}

/**
 * @brief 检查虚拟内存区域是否存在重叠（带锁版本）
 * @param[in] space   虚拟地址空间指针
 * @param[in] region  待检查的虚拟内存区域
 * @return 成功返回0，失败返回-1
 * @details 内部会获取区域锁并调用无锁版本的重叠检查函数
 */
STATUS_T OsRegionOverlapCheck(LosVmSpace *space, LosVmMapRegion *region)
{
    int ret;                                     // 函数返回值

    if (space == NULL || region == NULL) {       // 参数合法性检查
        return -1;                               // 参数无效，返回错误
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);     // 获取区域操作互斥锁
    ret = OsRegionOverlapCheckUnlock(space, region);  // 调用无锁版本重叠检查
    (VOID)LOS_MuxRelease(&space->regionMux);     // 释放区域操作互斥锁
    return ret;                                  // 返回检查结果
}

/**
 * @brief 转储指定虚拟地址对应的页表项信息
 * @param[in] vaddr  待查询的虚拟地址
 * @details 输出L1/L2页表索引、页表项内容、物理页框号及引用计数
 */
VOID OsDumpPte(VADDR_T vaddr)
{
    // 计算一级页表索引（L1）
    UINT32 l1Index = vaddr >> MMU_DESCRIPTOR_L1_SMALL_SHIFT;
    // 通过虚拟地址获取所属地址空间（内核分三个空间：内核进程空间、内核堆空间、用户进程空间）
    LosVmSpace *space = LOS_SpaceGet(vaddr);
    UINT32 ttEntry;                              // L1页表项内容
    LosVmPage *page = NULL;                      // 物理页面结构指针
    PTE_T *l2Table = NULL;                       // 二级页表（L2）基地址
    UINT32 l2Index;                              // 二级页表索引（L2）

    if (space == NULL) {                         // 地址空间无效
        return;                                  // 直接返回
    }

    ttEntry = space->archMmu.virtTtb[l1Index];   // 获取L1页表项内容
    if (ttEntry) {                               // L1页表项存在
        // 计算L2页表基地址（物理地址转内核虚拟地址）
        l2Table = LOS_PaddrToKVaddr(MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(ttEntry));
        // 计算L2页表索引
        l2Index = (vaddr % MMU_DESCRIPTOR_L1_SMALL_SIZE) >> PAGE_SHIFT;
        if (l2Table == NULL) {                   // L2页表不存在
            goto ERR;                            // 跳转到错误处理
        }
        // 获取物理页框（屏蔽页内偏移）
        page = LOS_VmPageGet(l2Table[l2Index] & ~(PAGE_SIZE - 1));
        if (page == NULL) {                      // 物理页框无效
            goto ERR;                            // 跳转到错误处理
        }
        // 打印页表详细信息：虚拟地址、L1/L2索引、页表项、物理页框号及引用计数
        PRINTK("vaddr %p, l1Index %d, ttEntry %p, l2Table %p, l2Index %d, pfn %p count %d\n",
               vaddr, l1Index, ttEntry, l2Table, l2Index, l2Table[l2Index], LOS_AtomicRead(&page->refCounts));
    } else {                                     // L1页表项不存在
        PRINTK("vaddr %p, l1Index %d, ttEntry %p\n", vaddr, l1Index, ttEntry);  // 打印L1页表项为空信息
    }
    return;
ERR:
    // 打印错误信息：函数名、虚拟地址、L2页表地址及索引
    PRINTK("%s, error vaddr: %#x, l2Table: %#x, l2Index: %#x\n", __FUNCTION__, vaddr, l2Table, l2Index);
}

UINT32 OsVmPhySegPagesGet(LosVmPhysSeg *seg)
{
    UINT32 intSave;
    UINT32 flindex;
    UINT32 segFreePages = 0;

    LOS_SpinLockSave(&seg->freeListLock, &intSave);
    for (flindex = 0; flindex < VM_LIST_ORDER_MAX; flindex++) {//遍历块组
        segFreePages += ((1 << flindex) * seg->freeList[flindex].listCnt);//1 << flindex等于页数, * 节点数 得到组块的总页数.
    }
    LOS_SpinUnlockRestore(&seg->freeListLock, intSave);

    return segFreePages;//返回剩余未分配的总物理页框
}
///dump 物理内存
/***********************************************************
*	phys_seg:物理页控制块地址信息
*	base:第一个物理页地址，即物理页内存起始地址
*	size:物理页内存大小
*	free_pages:空闲物理页数量
*	active anon: pagecache中，活跃的匿名页数量
*	inactive anon: pagecache中，不活跃的匿名页数量
*	active file: pagecache中，活跃的文件页数量
*	inactive file: pagecache中，不活跃的文件页数量
*	pmm pages total：总的物理页数，used：已使用的物理页数，free：空闲的物理页数
************************************************************/
/**
 * @brief 转储物理内存段的详细信息及使用统计
 * @details 输出每个物理内存段的地址、大小、空闲页面数、伙伴系统空闲块分布及LRU页面状态
 */
VOID OsVmPhysDump(VOID)
{
    LosVmPhysSeg *seg = NULL;                // 物理内存段指针
    UINT32 segFreePages;                     // 当前段空闲页面数
    UINT32 totalFreePages = 0;               // 系统总空闲页面数
    UINT32 totalPages = 0;                   // 系统总物理页面数
    UINT32 segIndex;                         // 段索引计数器
    UINT32 intSave;                          // 中断状态保存变量
    UINT32 flindex;                          // 空闲链表索引计数器
    UINT32 listCount[VM_LIST_ORDER_MAX] = {0};  // 各阶空闲块数量数组

    // 遍历所有物理内存段（数组实现）
    for (segIndex = 0; segIndex < g_vmPhysSegNum; segIndex++) {  // 循环取段
        seg = &g_vmPhysSeg[segIndex];        // 获取当前索引的物理段
        if (seg->size > 0) {                 // 仅处理有效物理段
            segFreePages = OsVmPhySegPagesGet(seg);  // 获取当前段空闲页面数
#ifdef LOSCFG_SHELL_CMD_DEBUG
            // 打印段信息表头（调试模式）
            PRINTK("\r\n phys_seg      base         size        free_pages    \n");
            PRINTK(" --------      -------      ----------  ---------  \n");
#endif
            // 打印物理段基本信息：段地址、起始地址、大小、空闲页面数
            PRINTK(" 0x%08x    0x%08x   0x%08x   %8u  \n", seg, seg->start, seg->size, segFreePages);
            totalFreePages += segFreePages;  // 累加系统空闲页面数
            totalPages += (seg->size >> PAGE_SHIFT);  // 累加系统总页面数（右移计算页数）

            // 获取伙伴系统各阶空闲块数量（需加锁保护）
            LOS_SpinLockSave(&seg->freeListLock, &intSave);  // 加自旋锁保护空闲链表
            for (flindex = 0; flindex < VM_LIST_ORDER_MAX; flindex++) {
                listCount[flindex] = seg->freeList[flindex].listCnt;  // 记录各阶空闲块数量
            }
            LOS_SpinUnlockRestore(&seg->freeListLock, intSave);  // 释放自旋锁

            // 打印伙伴系统各阶空闲块分布
            for (flindex = 0; flindex < VM_LIST_ORDER_MAX; flindex++) {
                PRINTK("order = %d, free_count = %d\n", flindex, listCount[flindex]);
            }

            // 打印LRU（最近最少使用）页面状态统计
            PRINTK("active   anon   %d\n", seg->lruSize[VM_LRU_ACTIVE_ANON]);    // 活跃匿名页面数
            PRINTK("inactive anon   %d\n", seg->lruSize[VM_LRU_INACTIVE_ANON]);  // 非活跃匿名页面数
            PRINTK("active   file   %d\n", seg->lruSize[VM_LRU_ACTIVE_FILE]);    // 活跃文件页面数
            PRINTK("inactive file   %d\n", seg->lruSize[VM_LRU_INACTIVE_FILE]);  // 非活跃文件页面数
        }
    }
    // 打印系统物理内存总览：总页数、已使用页数、空闲页数
    PRINTK("\n\rpmm pages: total = %u, used = %u, free = %u\n",
           totalPages, (totalPages - totalFreePages), totalFreePages);
}

/**
 * @brief 获取物理内存使用统计信息
 * @param[out] usedCount  输出参数，物理内存已使用页数
 * @param[out] totalCount 输出参数，物理内存总页数
 * @details 遍历所有物理段计算总页数和已使用页数，忽略无效物理段
 */
VOID OsVmPhysUsedInfoGet(UINT32 *usedCount, UINT32 *totalCount)
{
    UINT32 index;                            // 段索引计数器
    UINT32 segFreePages;                     // 当前段空闲页面数
    LosVmPhysSeg *physSeg = NULL;            // 物理内存段指针

    if (usedCount == NULL || totalCount == NULL) {  // 检查输出参数有效性
        return;                              // 参数无效，直接返回
    }
    *usedCount = 0;                          // 初始化已使用页数
    *totalCount = 0;                         // 初始化总页数

    // 遍历所有物理内存段
    for (index = 0; index < g_vmPhysSegNum; index++) {  // 循环取段
        physSeg = &g_vmPhysSeg[index];       // 获取当前索引的物理段
        if (physSeg->size > 0) {             // 仅处理有效物理段
            // 累加当前段总页数（大小/页面大小）
            *totalCount += physSeg->size >> PAGE_SHIFT;  // 叠加段的总页数
            segFreePages = OsVmPhySegPagesGet(physSeg);  // 获取段的剩余页数
            // 累加当前段已使用页数（总页数-空闲页数）
            *usedCount += (*totalCount - segFreePages);  // 叠加段的使用页数
        }
    }
}
#endif

