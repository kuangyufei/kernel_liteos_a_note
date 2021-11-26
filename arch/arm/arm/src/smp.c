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

#include "smp.h"
#include "arch_config.h"
#include "los_base.h"
#include "los_hw.h"
#include "los_atomic.h"
#include "los_arch_mmu.h"
#include "gic_common.h"
#include "los_task_pri.h"

#ifdef LOSCFG_KERNEL_SMP

extern VOID reset_vector(VOID);

struct OsCpuInit {
    ArchCpuStartFunc cpuStart;
    VOID *arg;
    Atomic initFlag;
};

STATIC struct OsCpuInit g_cpuInit[CORE_NUM - 1] = {0};

VOID HalArchCpuOn(UINT32 cpuNum, ArchCpuStartFunc func, struct SmpOps *ops, VOID *arg)
{
    struct OsCpuInit *cpuInit = &g_cpuInit[cpuNum - 1];
    UINTPTR startEntry = (UINTPTR)&reset_vector - KERNEL_VMM_BASE + SYS_MEM_BASE;
    INT32 ret;

    cpuInit->cpuStart = func;
    cpuInit->arg = arg;
    cpuInit->initFlag = 0;

    DCacheFlushRange((UINTPTR)cpuInit, (UINTPTR)cpuInit + sizeof(struct OsCpuInit));

    LOS_ASSERT(ops != NULL);


    ret = ops->SmpCpuOn(cpuNum, startEntry);
    if (ret < 0) {
        PRINT_ERR("cpu start failed, cpu num: %u, ret: %d\n", cpuNum, ret);
        return;
    }

    while (!LOS_AtomicRead(&cpuInit->initFlag)) {
        WFE;
    }
}

VOID HalSecondaryCpuStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();
    struct OsCpuInit *cpuInit = &g_cpuInit[cpuid - 1];

    OsCurrTaskSet(OsGetMainTask());

    LOS_AtomicSet(&cpuInit->initFlag, 1);
    SEV;

#ifdef LOSCFG_KERNEL_MMU
    OsArchMmuInitPerCPU();
#endif

    /* store each core's hwid */
    CPU_MAP_SET(cpuid, OsHwIDGet());
    HalIrqInitPercpu();

    cpuInit->cpuStart(cpuInit->arg);

    while (1) {
        WFI;
    }
}
#endif

