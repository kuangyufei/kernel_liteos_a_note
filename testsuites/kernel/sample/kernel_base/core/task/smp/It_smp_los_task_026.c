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

static void TaskF01(void)
{
    LOS_AtomicInc(&g_testCount);
    while (1) {
        WFI;
    }
}

static void TaskF02(void)
{
    LOS_AtomicInc(&g_testCount);
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, currCpuid;
    UINT32 testid[2];

    g_testCount = 0;

    currCpuid = (ArchCurrCpuid() + 1) % LOSCFG_KERNEL_CORE_NUM;

    // 3, It is used to calculate a priority relative to OS_TASK_PRIORITY_LOWEST.
    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_smp_task_026_f01", TaskF01, OS_TASK_PRIORITY_LOWEST - 3,
        CPUID_TO_AFFI_MASK(currCpuid));
    ret = LOS_TaskCreate(&testid[0], &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    // 2, It is used to calculate a priority relative to OS_TASK_PRIORITY_LOWEST.
    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_smp_task_026_f02", TaskF02, OS_TASK_PRIORITY_LOWEST - 2,
        CPUID_TO_AFFI_MASK(currCpuid));
    ret = LOS_TaskCreate(&testid[1], &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    /* Wait TaskF1 to start */
    TestAssertBusyTaskDelay(100, 1); // 100, Set the timeout of runtime; 1, test runing count
    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);

    /* lower down the priority of TaskF01 to trigger preeption */
    LOS_TaskPriSet(testid[0], OS_TASK_PRIORITY_LOWEST - 1);

    LOS_TaskDelay(5); // 5, set delay time.
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, assert that g_testCount is equal to this.

EXIT:
    LOS_TaskDelete(testid[0]);
    LOS_TaskDelete(testid[1]);

    return LOS_OK;
}

void ItSmpLosTask026(void)
{
    TEST_ADD_CASE("ItSmpLosTask026", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
