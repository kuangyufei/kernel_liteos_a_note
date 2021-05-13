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

static volatile UINT32 g_flag1 = 1;
static volatile UINT32 g_flag2 = 1;
static VOID TaskF01(VOID)
{
    UINT32 ret, i;

    LOS_AtomicInc(&g_testCount);
    ret = LOS_MuxLock(&g_mutexkernelTest, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    dprintf("task f01 get mux\n");
    LOS_AtomicInc(&g_testCount);

    do {
        __asm__ volatile("nop");
    } while (g_flag1);

    dprintf("task f01 post mux\n");
    ret = LOS_MuxUnlock(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    TestDumpCpuid();
    return;
}
static VOID TaskF02(VOID)
{
    UINT32 ret;

    LOS_AtomicInc(&g_testCount);

    ret = LOS_MuxLock(&g_mutexkernelTest, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    dprintf("task f02 get mux\n");
    LOS_AtomicInc(&g_testCount);

    do {
        __asm__ volatile("nop");
    } while (g_flag2);
    dprintf("task f02 post mux\n");
    ret = LOS_MuxUnlock(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    TestDumpCpuid();
    return;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret, currCpuid;
    TSK_INIT_PARAM_S testTask;

    g_testCount = 0;
    g_flag1 = 1;
    g_flag2 = 1;

    ret = LosMuxCreate(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxLock(&g_mutexkernelTest, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_MUX_2018_task1", TaskF01, TASK_PRIO_TEST + 1,
        CPUID_TO_AFFI_MASK(currCpuid)); // other cpu
    ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_MUX_2018_task2", TaskF02, TASK_PRIO_TEST + 1,
        CPUID_TO_AFFI_MASK(ArchCurrCpuid())); // current  cpu, low prio
    ret = LOS_TaskCreate(&g_testTaskID02, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(2); // let task f02 run

    TestAssertBusyTaskDelay(100, 2); // 100, 2, wait for task f01 run

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, here assert the result.

    /* wait for task01 to pend sem */
    TestBusyTaskDelay(2); // 2, delay for Timing control.

    ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;
    ICUNIT_GOTO_NOT_EQUAL(ret & OS_TASK_STATUS_PEND, 0, ret, EXIT);

    ret = OS_TCB_FROM_TID(g_testTaskID02)->taskStatus;
    ICUNIT_GOTO_NOT_EQUAL(ret & OS_TASK_STATUS_PEND, 0, ret, EXIT);

    ret = LOS_MuxUnlock(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(10);                                    // 10, delay for Timing control.
    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, here assert the result.

    do {
        ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;

        __asm__ volatile("nop");
    } while (!(ret & (OS_TASK_STATUS_RUNNING | OS_TASK_STATUS_PEND)));

    if (ret & OS_TASK_STATUS_RUNNING) { // task f01 get the mux
        dprintf("running task f01\n");
        ret = LOS_TaskDelete(g_testTaskID02); // so delete the task f02
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
        g_flag1 = 0; // mux post in task f01
        g_flag2 = 1;
    } else if (ret & OS_TASK_STATUS_PEND) { // task f02 get the mux
        dprintf("pend task f01\n");
        ret = LOS_TaskDelete(g_testTaskID01); // so delete the task f01
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
        LOS_TaskDelay(2); // 2, cross core to delete task

        ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;
        ICUNIT_GOTO_NOT_EQUAL(ret & OS_TASK_STATUS_UNUSED, 0, ret, EXIT);
        g_flag2 = 0; // mux post in task f02
        g_flag1 = 1;
    } else {
        dprintf("ret = 0x%x\n", ret);
    }
    LOS_TaskDelay(10);                                    // 10, delay for Timing control.
    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, here assert the result.

EXIT:
    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID02);
    ret = LOS_MuxDestroy(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;
}


VOID ItSmpLosMux2018(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItSmpLosMux2018", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
