/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

#include "It_los_task.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static UINT32 g_itTestTaskID01;
static UINT32 g_itTestTaskID02;
static volatile int g_itTimesliceTestCount = 0;
static volatile UINT32 g_itTestResult = LOS_OK;

static UINT32 ItTimeslice001F02(void const * argument)
{
    g_itTimesliceTestCount++;
    return LOS_OK;
}

static UINT32 ItTimeslice001F01(void const * argument)
{
    UINT64 timesliceCount;

    // 2, 1000, Used to calculate timesliceCount.
    timesliceCount = TestTickCountGet() + (LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT / 1000) * 2;

    while (1) {
        if (timesliceCount <= TestTickCountGet()) { // modify
            break;
        }
    }
    if (1 != g_itTimesliceTestCount) {
        g_itTestResult = LOS_NOK;
    }
    return LOS_OK;
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    UINT32 currCpuid;
    TSK_INIT_PARAM_S task;

    g_itTimesliceTestCount = 0;
    g_itTestResult = LOS_OK;

    LOS_TaskLock();
#if (LOSCFG_KERNEL_SMP == YES)
    currCpuid = (ArchCurrCpuid() + 1) % LOSCFG_KERNEL_CORE_NUM;
#endif

    memset(&task, 0, sizeof(TSK_INIT_PARAM_S));
    task.pfnTaskEntry = (TSK_ENTRY_FUNC)ItTimeslice001F01;
    task.usTaskPrio = TASK_PRIO_TEST_TASK + 1;
    task.pcName = "it_timeslice_001_f01";
    task.uwStackSize = LOS_TASK_MIN_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
    task.processID = LOS_GetCurrProcessID();

    ret = LOS_TaskCreateOnly(&g_itTestTaskID01, &task);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    LOS_TaskCpuAffiSet(g_itTestTaskID01, CPUID_TO_AFFI_MASK(currCpuid));

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)ItTimeslice001F02;
    task.usTaskPrio = TASK_PRIO_TEST_TASK + 1;
    task.pcName = "it_timeslice_001_f02";
    task.uwStackSize = LOS_TASK_MIN_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
    task.processID = LOS_GetCurrProcessID();

    ret = LOS_TaskCreateOnly(&g_itTestTaskID02, &task);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    LOS_TaskCpuAffiSet(g_itTestTaskID02, CPUID_TO_AFFI_MASK(currCpuid));

    ret = LOS_SetTaskScheduler(g_itTestTaskID01, LOS_SCHED_RR, task.usTaskPrio);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    ret = LOS_SetTaskScheduler(g_itTestTaskID02, LOS_SCHED_RR, task.usTaskPrio);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    LOS_TaskUnlock();
    // 10, Used to calculate delay time.
    LOS_TaskDelay(LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT * 10);

    ICUNIT_ASSERT_EQUAL(g_itTestResult, LOS_OK, g_itTestResult);
    return LOS_OK;
EXIT:
    LOS_TaskUnlock();
    return LOS_NOK;
}

void ItSmpLosTask024(void)
{
    TEST_ADD_CASE("ItSmpLosTask024", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
