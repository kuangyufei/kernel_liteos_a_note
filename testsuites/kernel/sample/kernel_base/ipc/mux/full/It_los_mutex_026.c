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
#endif /* __cplusplus */
#endif /* __cplusplus */

static VOID TaskDFunc(VOID)
{
    UINT32 ret;
    g_testCount++;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 6, g_testCount); // 6, here assert the result.

    g_testCount++;
    ret = LOS_MuxLock(&g_mutexTest3, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LOS_MuxUnlock(&g_mutexTest3);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    g_testCount++;
    ICUNIT_ASSERT_EQUAL_VOID(OsCurrTaskGet()->priority, 2, OsCurrTaskGet()->priority); // 2, here assert the result.
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 9, g_testCount);                             // 9, here assert the result.

    g_testCount++;
}

static VOID TaskCFunc(VOID)
{
    UINT32 ret;
    g_testCount++;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 4, g_testCount); // 4, here assert the result.

    g_testCount++;
    ret = LOS_MuxLock(&g_mutexTest2, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LOS_MuxUnlock(&g_mutexTest2);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    g_testCount++;

    ICUNIT_ASSERT_EQUAL_VOID(OsCurrTaskGet()->priority, 3, OsCurrTaskGet()->priority); // 3, here assert the result.
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 11, g_testCount);                            // 11, here assert the result.

    g_testCount++;
}

static VOID TaskBFunc(VOID)
{
    UINT32 ret;

    g_testCount++;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 2, g_testCount); // 2, here assert the result.

    g_testCount++;
    ret = LOS_MuxLock(&g_mutexTest1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LOS_MuxUnlock(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    g_testCount++;

    ICUNIT_ASSERT_EQUAL_VOID(OsCurrTaskGet()->priority, 5, OsCurrTaskGet()->priority); // 5, here assert the result.
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 13, g_testCount);                            // 13, here assert the result.
    g_testCount++;
}

static VOID TaskAFunc(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1, task2, task3;

    g_testCount++;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 1, g_testCount);
    ret = LOS_MuxLock(&g_mutexTest1, 0);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskBFunc;
    task1.usTaskPrio = 5; // 5, set reasonable priority.
    task1.pcName = "TaskB";
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testTaskID02, &task1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 3, g_testCount); // 3, here assert the result.

    ret = LosMuxCreate(&g_mutexTest2);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    ret = LOS_MuxLock(&g_mutexTest2, 0);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    task2.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskCFunc;
    task2.usTaskPrio = 3; // 3, set reasonable priority.
    task2.pcName = "TaskC";
    task2.uwStackSize = TASK_STACK_SIZE_TEST;
    task2.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task2.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testTaskID03, &task2);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LosMuxCreate(&g_mutexTest3);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    ret = LOS_MuxLock(&g_mutexTest3, 0);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 5, g_testCount); // 5, here assert the result.
    task3.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskDFunc;
    task3.usTaskPrio = 2; // 2, set reasonable priority.
    task3.pcName = "TaskD";
    task3.uwStackSize = TASK_STACK_SIZE_TEST;
    task3.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task3.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testTaskID04, &task3);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    g_testCount++;
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 8, g_testCount); // 8, here assert the result.

#if 1 // post mutex 3 -> mutex2 -> mutex1
    ret = LOS_MuxUnlock(&g_mutexTest3);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 10, g_testCount); // 10, here assert the result.

    ret = LOS_MuxUnlock(&g_mutexTest2);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 12, g_testCount); // 12, here assert the result.

    ret = LOS_MuxUnlock(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 14, g_testCount); // 14, here assert the result.
#endif

    ret = LOS_MuxDestroy(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LOS_MuxDestroy(&g_mutexTest2);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LOS_MuxDestroy(&g_mutexTest3);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 14, g_testCount);                             // 14, here assert the result.
    ICUNIT_ASSERT_EQUAL_VOID(OsCurrTaskGet()->priority, 10, OsCurrTaskGet()->priority); // 10, here assert the result.
    ICUNIT_ASSERT_EQUAL_VOID(LOS_HighBitGet(OsCurrTaskGet()->priBitMap), LOS_INVALID_BIT_INDEX,
        LOS_HighBitGet(OsCurrTaskGet()->priBitMap));
}


static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task;

    g_testCount = 0;

    ICUNIT_ASSERT_EQUAL(OsCurrTaskGet()->priority, 25, OsCurrTaskGet()->priority); // 25, here assert the result.

    ret = LosMuxCreate(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskAFunc;
    task.usTaskPrio = 10; // 10, set reasonable priority.
    task.pcName = "TaskA";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testTaskID01, &task);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL(g_testCount, 14, g_testCount);                             // 14, here assert the result.
    ICUNIT_ASSERT_EQUAL(OsCurrTaskGet()->priority, 25, OsCurrTaskGet()->priority); // 25, here assert the result.
    return LOS_OK;
}

VOID ItLosMux026(void)
{
    TEST_ADD_CASE("ItLosMux026", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
