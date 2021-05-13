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

#include "los_atomic.h"
#include "It_los_task.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static UINT32 g_testid, g_cpuid;

static void TaskF01(void)
{
    UINT32 ret;

    LOS_AtomicInc(&g_testCount);

    /* loop for 15 ticks */
    LOS_TaskDelay(15); // 15, set delay time

    ret = OS_TCB_FROM_TID(g_testid)->currCpu;
    ICUNIT_ASSERT_EQUAL_VOID(ret, g_cpuid, ret);
    LOS_AtomicInc(&g_testCount);
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, currCpuid;

    g_testCount = 0;

    currCpuid = ArchCurrCpuid();
    /* make sure that created test task is definitely on another core */
    g_cpuid = (currCpuid + 1) % LOSCFG_KERNEL_CORE_NUM;

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_smp_task_027_f01", TaskF01, TASK_PRIO_TEST_TASK - 1,
        CPUID_TO_AFFI_MASK(currCpuid));
    ret = LOS_TaskCreate(&g_testid, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    /* loop for 2 ticks */
    TestBusyTaskDelay(2); // 2, set delay time
    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);

    /* set the affinity of test task */
    LOS_TaskCpuAffiSet(g_testid, CPUID_TO_AFFI_MASK(g_cpuid));

    /* loop for 20 ticks */
    TestBusyTaskDelay(20); // 20, set delay time
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, assert that g_testCount is equal to this.

EXIT:
    LOS_TaskDelete(g_testid);

    return LOS_OK;
}

void ItSmpLosTask027(void)
{
    TEST_ADD_CASE("ItSmpLosTask027", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
