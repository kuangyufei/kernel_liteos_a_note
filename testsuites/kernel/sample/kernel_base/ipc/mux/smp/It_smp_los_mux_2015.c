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

static VOID TaskF01(VOID)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 2, g_testCount); // 2, here assert the result.
    LOS_AtomicInc(&g_testCount);

    ret = LOS_MuxLock(&g_mutexkernelTest, 5); // 5, init mutex.
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_ERRNO_MUX_TIMEOUT, ret);
    PRINT_DEBUG("TaskF01 pend timeout\n");

    LOS_AtomicInc(&g_testCount);
    TestDumpCpuid();
    return;
}

static VOID TaskF02(VOID)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 0, g_testCount);
    LOS_AtomicInc(&g_testCount);

    ret = LOS_MuxLock(&g_mutexkernelTest, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    LOS_AtomicInc(&g_testCount);
    TestDumpCpuid();
    PRINT_DEBUG("delay in task f02\n");
    LOS_TaskDelay(10); // 10, delay for Timing control.
    ret = LOS_MuxUnlock(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    PRINT_DEBUG("post mux in task f02\n");
    LOS_AtomicInc(&g_testCount);
    return;
}
static VOID HwiF01(VOID)
{
    UINT32 ret;
    ret = LOS_MuxUnlock(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_ERRNO_MUX_PEND_INTERR, ret);
    PRINT_DEBUG("after post in hwi \n");

    LOS_AtomicInc(&g_testCount);
    TestDumpCpuid();
    return;
}
static VOID HwiF02(VOID)
{
    UINT32 ret;
    ret = LOS_MuxDestroy(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    LOS_AtomicInc(&g_testCount);
    TestDumpCpuid();
    return;
}
static UINT32 Testcase(VOID)
{
    UINT32 ret, currCpuid;
    TSK_INIT_PARAM_S testTask;

    g_testCount = 0;

    ret = LosMuxCreate(&g_mutexkernelTest);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    ret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, (HWI_PROC_FUNC)HwiF01, 0);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_HwiCreate(HWI_NUM_TEST1, 1, 0, (HWI_PROC_FUNC)HwiF02, 0);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK(currCpuid));  // other cpu
    HalIrqSetAffinity(HWI_NUM_TEST1, CPUID_TO_AFFI_MASK(currCpuid)); // other cpu

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_MUX_2015_task2", TaskF02, TASK_PRIO_TEST - 1,
        CPUID_TO_AFFI_MASK(currCpuid)); // other cpu
    ret = LOS_TaskCreate(&g_testTaskID02, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    do {
        __asm__ volatile("nop");
    } while (g_testCount != 2); // 2, wait until g_testCount == 2.

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_MUX_2015_task1", TaskF01, TASK_PRIO_TEST + 1,
        CPUID_TO_AFFI_MASK(ArchCurrCpuid())); // current cpu
    ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(1);                                     // task f01 run
    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, here assert the result.

    TestHwiTrigger(HWI_NUM_TEST);

    LOS_TaskDelay(1);

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT); // 4, here assert the result.

    PRINT_DEBUG("delay in testtask begin\n");
    LOS_TaskDelay(20); // 20, delay for Timing control.
    PRINT_DEBUG("delay in testtask end\n");
    TestHwiTrigger(HWI_NUM_TEST1);

    LOS_TaskDelay(1);
    ICUNIT_GOTO_EQUAL(g_testCount, 7, g_testCount, EXIT); // 7, here assert the result.

EXIT:
    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID02);
    ret = LOS_HwiDelete(HWI_NUM_TEST, NULL);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    ret = LOS_HwiDelete(HWI_NUM_TEST1, NULL);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    LOS_MuxDestroy(&g_mutexkernelTest);
    return LOS_OK;
}


VOID ItSmpLosMux2015(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItSmpLosMux2015", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
