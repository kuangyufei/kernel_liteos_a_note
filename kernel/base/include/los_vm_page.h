/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//注意: vmPage 中并没有虚拟地址，只有物理地址
typedef struct VmPage {	//虚拟内存描述符
    LOS_DL_LIST         node;        /**< vm object dl list */ //虚拟内存节点,通过它挂到全局虚拟页上
    UINT32              index;       /**< vm page index to vm object */	//索引位置
    PADDR_T             physAddr;    /**< vm page physical addr */		//物理地址
    Atomic              refCounts;   /**< vm page ref count */			//被引用次数,共享内存会被多次引用
    UINT32              flags;       /**< vm page flags */				//页标签，（共享/引用/活动/被锁==）
    UINT8               order;       /**< vm page in which order list */	//被安置在伙伴算法的几号序列(              2^0,2^1,2^2,...,2^order)
    UINT8               segID;       /**< the segment id of vm page */	//所在段ID
    UINT16              nPages;      /**< the vm page is used for kernel heap */	//vm页面用于内核堆
} LosVmPage;

extern LosVmPage *g_vmPageArray;//物理内存所有页框(page frame)记录数组
extern size_t g_vmPageArraySize;//物理内存总页框(page frame)数

LosVmPage *LOS_VmPageGet(PADDR_T paddr);
VOID OsVmPageStartup(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_PAGE_H__ */
