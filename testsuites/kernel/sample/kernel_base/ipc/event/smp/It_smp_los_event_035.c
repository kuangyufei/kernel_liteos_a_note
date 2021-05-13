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

#include "It_los_event.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static volatile UINT32 g_runFlag = 1;

static VOID TaskF01(VOID)
{
    UINT32 ret;

    LOS_AtomicInc(&g_testCount);
    ret = LOS_EventRead(&g_event, 0x11, LOS_WAITMODE_AND, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, 0x11, ret);
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 3, g_testCount); // 3, Here, assert that g_testCount is equal to 3.
    LOS_AtomicInc(&g_testCount);

    TestDumpCpuid();
    return;
}

static VOID TaskF02(VOID)
{
    UINT32 ret, uvIntSave;

    LOS_AtomicInc(&g_testCount);

    LOS_TaskLock(); // task lock on other cpu
    do {
        __asm__ volatile("nop");
    } while (g_runFlag == 1);

    LOS_TaskUnlock(); // schedule occur ---switch to task f01
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 4, g_testCount); // 4, Here, assert that g_testCount is equal to 4.
    TestDumpCpuid();
    return;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret, currCpuid, i;
    TSK_INIT_PARAM_S testTask;

    g_testCount = 0;
    g_runFlag = 1;
    g_event.uwEventID = 0;

    ret = LOS_EventInit(&g_event);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_sem_034_task1", TaskF01, TASK_PRIO_TEST - 1,
        CPUID_TO_AFFI_MASK(currCpuid)); // other cpu
    ret = LOS_TaskCreate(&g_testTaskID02, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_sem_034_task2", TaskF02, TASK_PRIO_TEST,
        CPUID_TO_AFFI_MASK(currCpuid)); // other cpu
    ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    while (g_testCount < 2) { // g_testCount < 2,do nothing
    }

    ret = LOS_EventWrite(&g_event, 0x11); // try to wake task f01
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    // 100, Set the timeout of runtime; 2, test runing count
    TestAssertBusyTaskDelay(100, 2); // cause tasklock on other cpu ,so task f01 wake failed

    LOS_AtomicInc(&g_testCount);

    g_runFlag = 0;                     // unlock the other cpu
    // 100, Set the timeout of runtime; 4, test runing count
    TestAssertBusyTaskDelay(100, 4); // not schedule in current cpu cause tasklock
EXIT:

    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID02);
    ret = LOS_EventDestroy(&g_event);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;
}

VOID ItSmpLosEvent035(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItSmpLosEvent035", Testcase, TEST_LOS, TEST_EVENT, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
