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

/*!
 * @brief 
 * @verbatim
    LRU是Least Recently Used的缩写，即最近最少使用页面置换算法，是为虚拟页式存储管理服务的，
    是根据页面调入内存后的使用情况进行决策了。由于无法预测各页面将来的使用情况，只能利用
    “最近的过去”作为“最近的将来”的近似，因此，LRU算法就是将最近最久未使用的页面予以淘汰。
 * @endverbatim
 */
#define VM_LIST_ORDER_MAX    9	///< 伙伴算法分组数量,从 2^0,2^1,...,2^8 (256*4K)=1M 
#define VM_PHYS_SEG_MAX    32	///< 最大支持32个段

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y)) 
#endif

#define VM_PAGE_TO_PHYS(page)    (page->physAddr) ///< 获取物理页框的物理基地址
#define VM_ORDER_TO_PAGES(order) (1 << (order)) ///< 伙伴算法由order 定位到该块组的页面单位,例如:order=2时，page[4]
#define VM_ORDER_TO_PHYS(order)  (1 << (PAGE_SHIFT + (order))) ///< 通过order块组跳到物理地址
#define VM_PHYS_TO_ORDER(phys)   (min(LOS_LowBitGet((phys) >> PAGE_SHIFT), VM_LIST_ORDER_MAX - 1)) ///< 通过物理地址定位到order

struct VmFreeList {
    LOS_DL_LIST node;	///< 双循环链表用于挂空闲物理内框节点,通过 VmPage->node 挂上来
    UINT32 listCnt;		///< 空闲物理页总数
};

/*!
 * @brief Lru全称是Least Recently Used，即最近最久未使用的意思 针对匿名页和文件页各拆成对应链表。
 */
enum OsLruList {// 页属性
    VM_LRU_INACTIVE_ANON = 0,	///< 非活动匿名页（swap）
    VM_LRU_ACTIVE_ANON,			///< 活动匿名页（swap）
    VM_LRU_INACTIVE_FILE,		///< 非活动文件页（磁盘）
    VM_LRU_ACTIVE_FILE,			///< 活动文件页（磁盘）
    VM_LRU_UNEVICTABLE,			///< 禁止换出的页
    VM_NR_LRU_LISTS
};
/*!
 * @brief 物理段描述符
 */
typedef struct VmPhysSeg {
    PADDR_T start;            /* The start of physical memory area | 物理内存段的开始地址*/
    size_t size;              /* The size of physical memory area | 物理内存段的大小*/
    LosVmPage *pageBase;      /* The first page address of this area | 本段首个物理页框地址*/
    SPIN_LOCK_S freeListLock; /* The buddy list spinlock | 伙伴算法自旋锁,用于操作freeList链表*/
    struct VmFreeList freeList[VM_LIST_ORDER_MAX];  /* The free pages in the buddy list | 伙伴算法的分组,默认分成10组 2^0,2^1,...,2^VM_LIST_ORDER_MAX*/
    SPIN_LOCK_S lruLock;		///< 用于置换的自旋锁,用于操作lruList
    size_t lruSize[VM_NR_LRU_LISTS];		///< 5个双循环链表大小，如此方便得到size
    LOS_DL_LIST lruList[VM_NR_LRU_LISTS];	///< 页面置换算法,5个双循环链表头，它们分别描述五中不同类型的链表
} LosVmPhysSeg;
/*!
 * @brief 物理区描述,仅用于方案商配置范围使用
 */
struct VmPhysArea {
    PADDR_T start; ///< 物理内存区基地址
    size_t size; ///< 物理内存总大小
};

extern struct VmPhysSeg g_vmPhysSeg[VM_PHYS_SEG_MAX]; ///< 物理内存采用段页式管理,先切段后伙伴算法页
extern INT32 g_vmPhysSegNum; ///< 段总数 

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
