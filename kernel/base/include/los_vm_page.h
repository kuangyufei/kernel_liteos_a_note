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

#ifndef __LOS_VM_PAGE_H__
#define __LOS_VM_PAGE_H__

#include "los_typedef.h"
#include "los_bitmap.h"
#include "los_list.h"
#include "los_atomic.h"
#include "los_spinlock.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/*!
 * @brief 物理页框描述符 虚拟内存体现的是程序对内存资源的需求，而物理内存是对该请求的供应。
 伙伴算法的思想是：把内存中连续的空闲页框空间看成是空闲页框块，并按照它们的大小（连续页框的数目）分组
 * @attention vmPage 中并没有虚拟地址，只有物理地址
 \n 关于nPages和order的关系说明,当请求分配为5页时,order是等于3的,因为只有2^3才能满足5页的请求
 */
typedef struct VmPage {
    LOS_DL_LIST         node;        /**< vm object dl list | 物理内框节点,通过它挂/摘到全局g_vmPhysSeg[segID]->freeList[order]物理页框链表 或被使用的链表
    									上, 例如 共享内存的shmIDSource.node*/
    PADDR_T             physAddr;    /**< vm page physical addr | 物理页框起始物理地址,只能用于计算,不会用于操作(读/写数据==)*/
    Atomic              refCounts;   /**< vm page ref count | 被引用次数,共享内存会有多次引用*/
    UINT32              flags;       /**< vm page flags | 页标签，同时可以有多个标签（共享/引用/活动/被锁==）*/
    UINT8               order;       /**< vm page in which order list | 被安置在伙伴算法的几号序列(              2^0,2^1,2^2,...,2^order)*/
    UINT8               segID;       /**< the segment id of vm page | 所属物理内存段编号ID*/
    UINT16              nPages;      /**< the vm page is used for kernel heap | 分配页数,标识从本页开始连续的几页将一块被分配*/
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK
    SPIN_LOCK_S         lock;        /**< lock for page table entry */
#endif
} LosVmPage;

extern LosVmPage *g_vmPageArray;    ///< 物理页框(page frame)池,在g_vmPageArray中:不可能存在两个物理地址一样的物理页框,
extern size_t g_vmPageArraySize;    ///< 物理总页框(page frame)数

LosVmPage *LOS_VmPageGet(PADDR_T paddr);
VOID OsVmPageStartup(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_PAGE_H__ */
