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

#include "los_vm_boot.h"
#include "los_config.h"
#include "los_base.h"
#include "los_vm_zone.h"
#include "los_vm_map.h"
#include "los_memory_pri.h"
#include "los_vm_page.h"
#include "los_arch_mmu.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

UINTPTR g_vmBootMemBase = (UINTPTR)&__bss_end;//内核空间可用于分配的区域
BOOL g_kHeapInited = FALSE;//内核堆区初始化变量
//虚拟内存区间检查, 需理解 los_vm_zone.h 中画出的鸿蒙虚拟内存全景图
UINT32 OsVmAddrCheck(size_t tempAddr, size_t length)
{
    if ((tempAddr >= KERNEL_VMM_BASE) && ((tempAddr + length) <= (PERIPH_UNCACHED_BASE + PERIPH_UNCACHED_SIZE))) {
        return LOS_OK;
    }

    return LOS_NOK;
}
//开机引导分配器分配内存,只有开机时采用的分配方式
VOID *OsVmBootMemAlloc(size_t len)
{
    UINTPTR ptr;

    if (g_kHeapInited) {//@note_why 在什么时候会变成true,没找到代码
        VM_ERR("kernel heap has been inited, should not to use boot mem alloc!");
        return NULL;
    }

    ptr = LOS_Align(g_vmBootMemBase, sizeof(UINTPTR));//对齐
    g_vmBootMemBase = ptr + LOS_Align(len, sizeof(UINTPTR));//通过改变 g_vmBootMemBase来获取内存
    //这样也行,g_vmBootMemBase 真是野蛮粗暴
    return (VOID *)ptr;
}

UINT32 OsSysMemInit(VOID)
{
    STATUS_T ret;

    OsKSpaceInit();//内核空间初始化

    ret = OsKHeapInit(OS_KHEAP_BLOCK_SIZE);// 内核动态内存初始化 512K   
    if (ret != LOS_OK) {
        VM_ERR("OsKHeapInit fail");
        return LOS_NOK;
    }

    OsVmPageStartup();// 物理内存初始化
    OsInitMappingStartUp();// 映射初始化

    ret = ShmInit();// 共享内存初始化
    if (ret < 0) {
        VM_ERR("ShmInit fail");  
        return LOS_NOK;
    }

    return LOS_OK;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
