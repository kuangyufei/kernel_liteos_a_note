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

static volatile UINT32 g_itTestResult = LOS_NOK;
static volatile int g_flag = 0;

static void TaskF02(void const * argument)
{
    LOS_AtomicInc(&g_testCount);
    while (1) {
    }
}

static void TaskF01(void const * argument)
{
    UINT64 timesliceCount;

    LOS_AtomicInc(&g_testCount);
    // 5, 1000, Used to calculate timesliceCount.
    timesliceCount = TestTickCountGet() + (LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT / 1000) * 5;

    while (1) {
        if (timesliceCount <= TestTickCountGet()) {
            break;
        }
    }
    if (g_testCount == LOSCFG_KERNEL_CORE_NUM) {
        g_itTestResult = LOS_OK;
        g_flag = 1;
    }
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    UINT32 testTaskIDSmp[LOSCFG_KERNEL_CORE_NUM];
    const CHAR *taskAll = "-a";

    g_testCount = 0;
    TEST_TASK_PARAM_INIT(task1, "it_smp_task_037", (TSK_ENTRY_FUNC)TaskF02, TASK_PRIO_TEST_TASK);
    int i;

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        /* take control of every cores */
        task1.usCpuAffiMask = CPUID_TO_AFFI_MASK((ArchCurrCpuid() + i + 1) % (LOSCFG_KERNEL_CORE_NUM));
        ret = LOS_TaskCreate(&testTaskIDSmp[i], &task1);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    TestBusyTaskDelay(2); // 2, set delay time
    /* check if every core's task is running */
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        OsShellCmdDumpTask(1, &taskAll);
        do {
            ret = OS_TCB_FROM_TID(testTaskIDSmp[i])->taskStatus;
        } while (ret & OS_TASK_STATUS_READY);

        ICUNIT_GOTO_EQUAL(ret, OS_TASK_STATUS_RUNNING | OS_TASK_STATUS_DETACHED, ret, EXIT);
    }

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.usCpuAffiMask = 0;
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    /* wait for task01 is finished */
    while (g_flag != 1) {
    }

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        ret = LOS_TaskDelete(testTaskIDSmp[i]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    }

    return g_itTestResult;

EXIT:
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        LOS_TaskDelete(testTaskIDSmp[i]);
    }

    return LOS_NOK;
}

void ItSmpLosTask037(void)
{
    TEST_ADD_CASE("ItSmpLosTask037", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
