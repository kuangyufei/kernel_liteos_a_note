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

#include "los_smp.h"
#include "arch_config.h"
#include "los_atomic.h"
#include "los_task_pri.h"
#include "los_init_pri.h"
#include "los_process_pri.h"
#include "los_sched_pri.h"
#include "los_swtmr_pri.h"

#ifdef LOSCFG_KERNEL_SMP
STATIC struct SmpOps *g_smpOps = NULL;

STATIC VOID OsSmpSecondaryInit(VOID *arg)
{
    UNUSED(arg);

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    OsSwtmrInit();
#endif

    OsIdleTaskCreate();
    OsInitCall(LOS_INIT_LEVEL_KMOD_TASK);

    OsSchedStart();
}

VOID LOS_SmpOpsSet(struct SmpOps *ops)
{
    g_smpOps = ops;
}

VOID OsSmpInit(VOID)
{
    UINT32 cpuNum = 1;  /* Start the secondary cpus. */

    if (g_smpOps == NULL) {
        PRINT_ERR("Must call the interface(LOS_SmpOpsSet) to register smp operations firstly!\n");
        return;
    }

    for (; cpuNum < CORE_NUM; cpuNum++) {
        HalArchCpuOn(cpuNum, OsSmpSecondaryInit, g_smpOps, 0);
    }

    return;
}
#endif
