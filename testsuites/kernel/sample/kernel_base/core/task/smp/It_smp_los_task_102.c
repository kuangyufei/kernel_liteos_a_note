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

static volatile int g_flag = 0;
static void TaskF01(void)
{
    UINT32 ret;

    LOS_TaskLock();

    LOS_AtomicInc(&g_testCount);

    while (g_flag == 0) {
    }

    LOS_TaskUnlock();

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 3, g_testCount); // 3, assert that g_testCount is equal to this.

    LOS_AtomicInc(&g_testCount);
}

static void TaskF02(void)
{
    UINT32 ret;

    LOS_AtomicInc(&g_testCount);

    LOS_TaskSuspend(g_testTaskID02);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 2, g_testCount); // 2, assert that g_testCount is equal to this.

    LOS_AtomicInc(&g_testCount);
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    UINT32 gTestTaskLock;
    g_testCount = 0;

    TEST_TASK_PARAM_INIT(task1, "it_smp_task_102", (TSK_ENTRY_FUNC)TaskF02, TASK_PRIO_TEST_TASK - 1);
    int i;

    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK((ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM));
    ret = LOS_TaskCreate(&g_testTaskID02, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    /* wait for other core's task to suspend */
    TestAssertBusyTaskDelay(100, 1); // 100, Set the timeout of runtime; 1, test runing count

    TestBusyTaskDelay(2); // 2, set delay time

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);

    ret = OS_TCB_FROM_TID(g_testTaskID02)->taskStatus;
    ICUNIT_GOTO_NOT_EQUAL((ret & OS_TASK_STATUS_SUSPEND), 0, ret, EXIT);

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.usTaskPrio = TASK_PRIO_TEST_TASK + 1;
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    /* wait for other core's task being running */
    TestAssertBusyTaskDelay(100, 2); // 100, Set the timeout of runtime; 2, test runing count
    TestBusyTaskDelay(10); // 10, set delay time

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, assert that g_testCount is equal to this.

    ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;
    ICUNIT_GOTO_NOT_EQUAL((ret & OS_TASK_STATUS_RUNNING), 0, ret, EXIT);

    /* wait for other core is Task_locked */
    TestBusyTaskDelay(2); // 2, set delay time

    ret = LOS_TaskResume(g_testTaskID02);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    TestBusyTaskDelay(2); // 2, set delay time

    ret = OS_TCB_FROM_TID(g_testTaskID02)->taskStatus;
    ICUNIT_GOTO_NOT_EQUAL((ret & OS_TASK_STATUS_READY), 0, ret, EXIT);

    g_flag = 1;

    // 4, assert that g_testCount is equal to this.
    while (g_testCount != 4) {
    }
    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT); // 4, assert that g_testCount is equal to this.

    LOS_TaskDelete(g_testTaskID01);

    LOS_TaskDelete(g_testTaskID02);

    return LOS_OK;

EXIT:
    LOS_TaskDelete(g_testTaskID01);

    LOS_TaskDelete(g_testTaskID02);

    return LOS_NOK;
}

void ItSmpLosTask102(void)
{
    TEST_ADD_CASE("ItSmpLosTask102", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
