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

#include "los_vm_page.h"
#include "los_vm_common.h"
#include "los_vm_phys.h"
#include "los_vm_boot.h"
#include "los_vm_filemap.h"


#ifdef LOSCFG_KERNEL_VM

LosVmPage *g_vmPageArray = NULL;//物理页框数组
size_t g_vmPageArraySize;//物理页框大小
//页框初始化
STATIC VOID OsVmPageInit(LosVmPage *page, paddr_t pa, UINT8 segID)
{
    LOS_ListInit(&page->node);			//页节点初始化	
    page->flags = FILE_PAGE_FREE;		//页标签,初始为空闲页
    LOS_AtomicSet(&page->refCounts, 0);	//引用次数0
    page->physAddr = pa;				//物理地址
    page->segID = segID;				//物理地址使用段管理，段ID
    page->order = VM_LIST_ORDER_MAX;	//初始化值,不属于任何块组
    page->nPages = 0;
}
///伙伴算法初始化
STATIC INLINE VOID OsVmPageOrderListInit(LosVmPage *page, size_t nPages)
{//@note_why 此时所有页面 page->order = VM_LIST_ORDER_MAX,能挂入伙伴算法的链表吗?
    OsVmPhysPagesFreeContiguous(page, nPages);//释放连续的物理页框
}
#define VMPAGEINIT(page, pa, segID) do {    \
    OsVmPageInit(page, pa, segID);          \
    (page)++;                               \
    (pa) += PAGE_SIZE;                      \
} while (0)

/*!
 完成对物理内存整体初始化,本函数一定运行在实模式下
 1.申请大块内存g_vmPageArray存放LosVmPage,按4K一页划分物理内存存放在数组中.
*/
VOID OsVmPageStartup(VOID)
{
    struct VmPhysSeg *seg = NULL;
    LosVmPage *page = NULL;
    paddr_t pa;
    UINT32 nPage;
    INT32 segID;

    OsVmPhysAreaSizeAdjust(ROUNDUP((g_vmBootMemBase - KERNEL_ASPACE_BASE), PAGE_SIZE));//校正 g_physArea size

    /*
     * Pages getting from OsVmPhysPageNumGet() interface here contain the memory
     * struct LosVmPage occupied, which satisfies the equation:
     * nPage * sizeof(LosVmPage) + nPage * PAGE_SIZE = OsVmPhysPageNumGet() * PAGE_SIZE.
     */
    nPage = OsVmPhysPageNumGet() * PAGE_SIZE / (sizeof(LosVmPage) + PAGE_SIZE);
    g_vmPageArraySize = nPage * sizeof(LosVmPage);//页表总大小
    g_vmPageArray = (LosVmPage *)OsVmBootMemAlloc(g_vmPageArraySize);//实模式下申请内存,此时还没有初始化MMU

    OsVmPhysAreaSizeAdjust(ROUNDUP(g_vmPageArraySize, PAGE_SIZE));//

    OsVmPhysSegAdd();// 完成对段的初始化
    OsVmPhysInit();// 加入空闲链表和设置置换算法,LRU(最近最久未使用)算法

    for (segID = 0; segID < g_vmPhysSegNum; segID++) {//遍历物理段,将段切成一页一页
        seg = &g_vmPhysSeg[segID];
        nPage = seg->size >> PAGE_SHIFT;//本段总页数
        UINT32 count = nPage >> 3; /* 3: 2 ^ 3, nPage / 8, cycle count */
        UINT32 left = nPage & 0x7; /* 0x7: nPage % 8, left page */

        for (page = seg->pageBase, pa = seg->start; count > 0; count--) {
            /* note: process large amount of data, optimize performance */
            VMPAGEINIT(page, pa, segID);
            VMPAGEINIT(page, pa, segID);
            VMPAGEINIT(page, pa, segID);
            VMPAGEINIT(page, pa, segID);
            VMPAGEINIT(page, pa, segID);
            VMPAGEINIT(page, pa, segID);
            VMPAGEINIT(page, pa, segID);
            VMPAGEINIT(page, pa, segID);
        }
        for (; left > 0; left--) {
            VMPAGEINIT(page, pa, segID);
        }
        OsVmPageOrderListInit(seg->pageBase, nPage);//伙伴算法初始化,将所有页加入空闲链表供分配
    }
}
///通过物理地址获取页框
LosVmPage *LOS_VmPageGet(PADDR_T paddr)
{
    INT32 segID;
    LosVmPage *page = NULL;

    for (segID = 0; segID < g_vmPhysSegNum; segID++) {//物理内存采用段页管理
        page = OsVmPhysToPage(paddr, segID);//通过物理地址和段ID找出物理页框
        if (page != NULL) {
            break;
        }
    }

    return page;
}
#endif

