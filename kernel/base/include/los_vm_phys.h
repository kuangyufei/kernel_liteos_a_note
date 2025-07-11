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

#ifndef __LOS_VM_PHYS_H__
#define __LOS_VM_PHYS_H__

#include "los_typedef.h"
#include "los_list.h"
#include "los_spinlock.h"
#include "los_vm_page.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @defgroup vm_phys_macro 物理内存管理宏定义
 * @ingroup kernel_vm
 * @{ 
 */
/**
 * @brief 伙伴系统支持的最大阶数
 * @note 阶数表示连续页块的数量为2^order，最大值9表示最大支持512个连续页块(2^9=512)
 */
#define VM_LIST_ORDER_MAX    9
/**
 * @brief 物理内存段的最大数量
 * @note 系统支持的物理内存区域分段上限，用于管理不连续的物理内存块
 */
#define VM_PHYS_SEG_MAX    32

/**
 * @brief 最小值计算宏
 * @note 若系统未定义min宏，则在此定义，用于比较两个数的大小并返回较小值
 */
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

/**
 * @brief 将页结构体指针转换为物理地址
 * @param page 页结构体指针
 * @return 对应的物理地址值
 */
#define VM_PAGE_TO_PHYS(page)    ((page)->physAddr)
/**
 * @brief 将阶数转换为页数
 * @param order 阶数(0~VM_LIST_ORDER_MAX)
 * @return 页数，计算公式：1 << order (2^order)
 */
#define VM_ORDER_TO_PAGES(order) (1 << (order))
/**
 * @brief 将阶数转换为物理内存大小
 * @param order 阶数(0~VM_LIST_ORDER_MAX)
 * @return 物理内存大小(字节)，计算公式：1 << (PAGE_SHIFT + order)
 * @note PAGE_SHIFT为页大小的位移值，例如4KB页对应PAGE_SHIFT=12
 */
#define VM_ORDER_TO_PHYS(order)  (1 << (PAGE_SHIFT + (order)))
/**
 * @brief 将物理内存大小转换为阶数
 * @param phys 物理内存大小(字节)
 * @return 阶数，取phys/PAGE_SIZE的低比特位和最大阶数的较小值
 * @note 使用LOS_LowBitGet获取最低置位比特位置，确保不超过VM_LIST_ORDER_MAX-1
 */
#define VM_PHYS_TO_ORDER(phys)   (min(LOS_LowBitGet((phys) >> PAGE_SHIFT), VM_LIST_ORDER_MAX - 1))
/** @} */

/**
 * @brief 伙伴系统空闲页链表结构
 * @note 用于管理特定阶数的连续空闲页块
 */
struct VmFreeList {
    LOS_DL_LIST node;          /* 双向链表节点，用于连接相同阶数的空闲页块 */
    UINT32 listCnt;            /* 该阶数空闲页块的数量计数 */
};

/**
 * @brief LRU(最近最少使用)页面链表类型枚举
 * @note 用于页面回收算法中对不同类型页面进行分类管理
 */
enum OsLruList {
    VM_LRU_INACTIVE_ANON = 0,  /* 非活跃匿名页链表(未关联文件的内存页) */
    VM_LRU_ACTIVE_ANON,        /* 活跃匿名页链表 */
    VM_LRU_INACTIVE_FILE,      /* 非活跃文件页链表(关联文件的内存页) */
    VM_LRU_ACTIVE_FILE,        /* 活跃文件页链表 */
    VM_LRU_UNEVICTABLE,        /* 不可回收页链表(如内核关键数据页) */
    VM_NR_LRU_LISTS            /* LRU链表类型总数 */
};

/**
 * @brief 物理内存段结构
 * @note 描述系统中的一段连续物理内存区域及其管理结构
 */
typedef struct VmPhysSeg {
    PADDR_T start;            /* 物理内存区域起始地址 */
    size_t size;              /* 物理内存区域大小(字节) */
    LosVmPage *pageBase;      /* 该区域首个页结构体的地址 */

    SPIN_LOCK_S freeListLock; /* 伙伴链表自旋锁，保护空闲页操作的原子性 */
    struct VmFreeList freeList[VM_LIST_ORDER_MAX];  /* 伙伴系统空闲页链表数组，按阶数索引 */

    SPIN_LOCK_S lruLock;      /* LRU链表自旋锁，保护LRU页面操作的原子性 */
    size_t lruSize[VM_NR_LRU_LISTS]; /* 各类型LRU链表的页面数量 */
    LOS_DL_LIST lruList[VM_NR_LRU_LISTS]; /* LRU页面链表数组，按OsLruList类型索引 */
} LosVmPhysSeg;

/**
 * @brief 物理内存区域描述结构
 * @note 用于定义系统中的一段物理内存范围
 */
struct VmPhysArea {
    PADDR_T start;            /* 物理内存区域起始地址 */
    size_t size;              /* 物理内存区域大小(字节) */
};
extern struct VmPhysSeg g_vmPhysSeg[VM_PHYS_SEG_MAX];
extern INT32 g_vmPhysSegNum;

UINT32 OsVmPagesToOrder(size_t nPages);
struct VmPhysSeg *OsVmPhysSegGet(LosVmPage *page);
LosVmPhysSeg *OsGVmPhysSegGet(VOID);
VOID *OsVmPageToVaddr(LosVmPage *page);
VOID OsVmPhysSegAdd(VOID);
VOID OsVmPhysInit(VOID);
VOID OsVmPhysAreaSizeAdjust(size_t size);
UINT32 OsVmPhysPageNumGet(VOID);
LosVmPage *OsVmVaddrToPage(VOID *ptr);
VOID OsPhysSharePageCopy(PADDR_T oldPaddr, PADDR_T *newPaddr, LosVmPage *newPage);
VOID OsVmPhysPagesFreeContiguous(LosVmPage *page, size_t nPages);
LosVmPage *OsVmPhysToPage(paddr_t pa, UINT8 segID);
LosVmPage *OsVmPaddrToPage(paddr_t paddr);

LosVmPage *LOS_PhysPageAlloc(VOID);
VOID LOS_PhysPageFree(LosVmPage *page);
size_t LOS_PhysPagesAlloc(size_t nPages, LOS_DL_LIST *list);
size_t LOS_PhysPagesFree(LOS_DL_LIST *list);
VOID *LOS_PhysPagesAllocContiguous(size_t nPages);
VOID LOS_PhysPagesFreeContiguous(VOID *ptr, size_t nPages);
VADDR_T *LOS_PaddrToKVaddr(PADDR_T paddr);
PADDR_T OsKVaddrToPaddr(VADDR_T kvaddr);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_PHYS_H__ */
