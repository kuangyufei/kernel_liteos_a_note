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

static volatile UINT32 g_itTestResult = LOS_OK;

static void TaskF01(void const * argument)
{
    UINT32 ret;
    UINT64 timesliceCount;
    UINT64 timesliceCount2;
    LOS_AtomicInc(&g_testCount);

    // 50, It is used to calculate timesliceCount
    timesliceCount = TestTickCountGet() + 50;

    ret = LOS_TaskDelay(50); // 50, set delay time.
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    timesliceCount2 = TestTickCountGet();

    dprintf("Time1 = 0x%llX ; Time2 = 0x%llX \n", timesliceCount, timesliceCount2);
    if ((timesliceCount > (timesliceCount2 + 1)) || ((timesliceCount < timesliceCount2 - 1))) {
        g_itTestResult = LOS_NOK;
    }

    while (1) {
    }
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    UINT32 testTaskIDSmp[LOSCFG_KERNEL_CORE_NUM];

    g_testCount = 0;
    TEST_TASK_PARAM_INIT(task1, "it_smp_task_046", (TSK_ENTRY_FUNC)TaskF01, TASK_PRIO_TEST_TASK);
    int i;

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        /* take control of every cores */
        task1.usCpuAffiMask = CPUID_TO_AFFI_MASK((ArchCurrCpuid() + i + 1) % (LOSCFG_KERNEL_CORE_NUM));
        ret = LOS_TaskCreate(&testTaskIDSmp[i], &task1);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    LOS_TaskDelay(30); // 30, set delay time.

    /* check every cores is running test task */
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        ret = OS_TCB_FROM_TID(testTaskIDSmp[i])->taskStatus;
        ICUNIT_GOTO_NOT_EQUAL(ret & OS_TASK_STATUS_DELAY, 0, ret, EXIT);
    }

    ICUNIT_GOTO_EQUAL(g_testCount, LOSCFG_KERNEL_CORE_NUM - 1, g_testCount, EXIT);

    TestBusyTaskDelay(60); // 60, set delay time

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        ret = LOS_TaskDelete(testTaskIDSmp[i]);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    ICUNIT_ASSERT_EQUAL(g_itTestResult, LOS_OK, g_itTestResult);

    return LOS_OK;

EXIT:
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        ret = LOS_TaskDelete(testTaskIDSmp[i]);
    }

    return LOS_NOK;
}

void ItSmpLosTask046(void)
{
    TEST_ADD_CASE("ItSmpLosTask046", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
