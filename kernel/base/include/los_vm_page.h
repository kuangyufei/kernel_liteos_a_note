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
/**
 * @brief 物理页面管理结构体
 * @details 描述系统中单个物理页面的元数据信息，包括物理地址、引用计数、状态标志等
 * @ingroup kernel_vm
 */
typedef struct VmPage {
    LOS_DL_LIST         node;        /**< 双向链表节点，用于将页面链接到不同状态的链表(如空闲链表、LRU链表) */
    PADDR_T             physAddr;    /**< 物理页面地址，存储该页面在物理内存中的起始地址 */
    Atomic              refCounts;   /**< 页面引用计数，原子变量确保并发安全，记录页面被引用的次数 */
    UINT32              flags;       /**< 页面状态标志位，用于表示页面的各种属性(如脏页、锁定、活跃等) */
    UINT8               order;       /**< 伙伴系统阶数，表示该页面属于2^order大小的连续页块 */
    UINT8               segID;       /**< 内存段ID，标识该页面所属的物理内存段(LosVmPhysSeg)索引 */
    UINT16              nPages;      /**< 页面数量，用于内核堆分配时标识连续分配的页面数 */
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK
    SPIN_LOCK_S         lock;        /**< 页表项精细锁，用于多处理器环境下保护单个页表项的并发访问 */
#endif
} LosVmPage;

extern LosVmPage *g_vmPageArray;
extern size_t g_vmPageArraySize;

LosVmPage *LOS_VmPageGet(PADDR_T paddr);
VOID OsVmPageStartup(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_PAGE_H__ */
