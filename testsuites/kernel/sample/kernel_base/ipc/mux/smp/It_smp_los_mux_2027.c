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

#include "It_los_mux.h"

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
    ret = LOS_MuxLock(&g_mutexkernelTest, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 3, g_testCount); // 3, here assert the result.
    LOS_AtomicInc(&g_testCount);

    ret = LOS_MuxUnlock(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    TestDumpCpuid();
    return;
}

static VOID TaskF02(VOID)
{
    UINT32 ret, uvIntSave;

    LOS_TaskLock(); // task lock on other cpu
    LOS_AtomicInc(&g_testCount);

    do {
        __asm__ volatile("nop");
    } while (g_runFlag == 1);

    LOS_TaskUnlock();                                 // schedule occur ---switch to task f01
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 4, g_testCount); // 4, here assert the result.

    TestDumpCpuid();
    return;
}


static UINT32 Testcase(VOID)
{
    UINT32 ret, currCpuid, i;
    TSK_INIT_PARAM_S testTask = {0};

    g_testCount = 0;
    g_runFlag = 1;

    ret = LosMuxCreate(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxLock(&g_mutexkernelTest, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_sem_034_task1", TaskF01, TASK_PRIO_TEST_TASK - 2, // 2, set reasonable priority.
        CPUID_TO_AFFI_MASK(currCpuid));                                                  // other cpu
    ret = LOS_TaskCreate(&g_testTaskID02, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_sem_034_task2", TaskF02, TASK_PRIO_TEST_TASK - 1,
        CPUID_TO_AFFI_MASK(currCpuid)); // other cpu
    ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    TestAssertBusyTaskDelay(100, 2);                      // 100, 2, delay for Timing control.
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, here assert the result.

    /* 2, wait for task01 to pend mutex */
    TestBusyTaskDelay(2);

    ret = LOS_MuxUnlock(&g_mutexkernelTest); // try to wake task f01
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(10); // 10, delay enough time for other cpu

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, cause tasklock on other cpu ,so task f01 wake failed

    LOS_AtomicInc(&g_testCount);

    g_runFlag = 0;       // unlock the other cpu
    LOS_TaskDelay(10); // 10, delay for Timing control.

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT); // 4, not schedule in current cpu cause tasklock

EXIT:

    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID02);
    ret = LOS_MuxDestroy(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;
}

VOID ItSmpLosMux2027(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItSmpLosMux2027", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
