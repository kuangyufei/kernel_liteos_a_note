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

/**
 * @defgroup los_arch_mmu architecture mmu
 * @ingroup kernel
 */

#ifndef __LOS_ARCH_MMU_H__
#define __LOS_ARCH_MMU_H__

#include "los_typedef.h"
#include "los_mux.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef struct ArchMmu {
    LosMux              mtx;            /**< arch mmu page table entry modification mutex lock *///对页表操作的互斥量
    VADDR_T             *virtTtb;       /**< translation table base virtual addr */ //注意:这里是个指针,内核操作都用这个地址
    PADDR_T             physTtb;        /**< translation table base phys addr */	//注意:这里是个值,这个值是记录给MMU使用的,MMU只认它,内核是无法使用的
    UINT32              asid;           /**< TLB asid */			//标识进程用的，由mmu分配，有了它在mmu层面才知道是哪个进程的虚拟地址
    LOS_DL_LIST         ptList;         /**< page table vm page list *///L1 为表头，后面挂的是n多L2
} LosArchMmu;

BOOL OsArchMmuInit(LosArchMmu *archMmu, VADDR_T *virtTtb);
STATUS_T LOS_ArchMmuQuery(const LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T *paddr, UINT32 *flags);
STATUS_T LOS_ArchMmuUnmap(LosArchMmu *archMmu, VADDR_T vaddr, size_t count);
STATUS_T LOS_ArchMmuMap(LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T paddr, size_t count, UINT32 flags);
STATUS_T LOS_ArchMmuChangeProt(LosArchMmu *archMmu, VADDR_T vaddr, size_t count, UINT32 flags);
STATUS_T LOS_ArchMmuMove(LosArchMmu *archMmu, VADDR_T oldVaddr, VADDR_T newVaddr, size_t count, UINT32 flags);
VOID LOS_ArchMmuContextSwitch(LosArchMmu *archMmu);
STATUS_T LOS_ArchMmuDestroy(LosArchMmu *archMmu);
VOID OsArchMmuInitPerCPU(VOID);
VADDR_T *OsGFirstTableGet(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_PAGE_H__ */

