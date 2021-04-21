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

#include "los_vm_iomap.h"
#include "los_printf.h"
#include "los_vm_zone.h"
#include "los_vm_common.h"
#include "los_vm_map.h"
#include "los_vm_lock.h"
#include "los_memory.h"


VOID *LOS_DmaMemAlloc(DMA_ADDR_T *dmaAddr, size_t size, size_t align, enum DmaMemType type)
{
    VOID *kVaddr = NULL;

    if (size == 0) {
        return NULL;
    }

    if ((type != DMA_CACHE) && (type != DMA_NOCACHE)) {
        VM_ERR("The dma type = %d is not support!", type);
        return NULL;
    }

#ifdef LOSCFG_KERNEL_VM
    kVaddr = LOS_KernelMallocAlign(size, align);
#else
    kVaddr = LOS_MemAllocAlign(OS_SYS_MEM_ADDR, size, align);
#endif
    if (kVaddr == NULL) {
        VM_ERR("failed, size = %u, align = %u", size, align);
        return NULL;
    }

    if (dmaAddr != NULL) {
        *dmaAddr = (DMA_ADDR_T)LOS_PaddrQuery(kVaddr);
    }

    if (type == DMA_NOCACHE) {
        kVaddr = (VOID *)VMM_TO_UNCACHED_ADDR((UINTPTR)kVaddr);
    }

    return kVaddr;
}

VOID LOS_DmaMemFree(VOID *vaddr)
{
    UINTPTR addr;

    if (vaddr == NULL) {
        return;
    }
    addr = (UINTPTR)vaddr;

    if ((addr >= UNCACHED_VMM_BASE) && (addr < UNCACHED_VMM_BASE + UNCACHED_VMM_SIZE)) {
        addr = UNCACHED_TO_VMM_ADDR(addr);
#ifdef LOSCFG_KERNEL_VM
        LOS_KernelFree((VOID *)addr);
#else
        LOS_MemFree(OS_SYS_MEM_ADDR, (VOID *)addr);
#endif
    } else if ((addr >= KERNEL_VMM_BASE) && (addr < KERNEL_VMM_BASE + KERNEL_VMM_SIZE)) {
#ifdef LOSCFG_KERNEL_VM
        LOS_KernelFree((VOID *)addr);
#else
        LOS_MemFree(OS_SYS_MEM_ADDR, (VOID *)addr);
#endif
    } else {
        VM_ERR("addr %#x not in dma area!!!", vaddr);
    }
    return;
}

DMA_ADDR_T LOS_DmaVaddrToPaddr(VOID *vaddr)
{
    return (DMA_ADDR_T)LOS_PaddrQuery(vaddr);
}

