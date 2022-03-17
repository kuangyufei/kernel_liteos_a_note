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

#include "osTest.h"
#include "los_config.h"
#include "los_bitmap.h"
#include "It_los_mux.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

static VOID TaskFe4Func(VOID)
{
    UINT32 ret;
    g_testCount++;
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 3, g_testCount); // 3, here assert the result.

    ret = LOS_MuxLock(&g_mutexTest1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    g_testCount++;
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 4, g_testCount); // 4, here assert the result.

    ret = LOS_MuxUnlock(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
}

static VOID TaskService5Func(VOID)
{
    UINT32 ret;
    g_testCount++;
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 2, g_testCount); // 2, here assert the result.

    ret = LOS_MuxLock(&g_mutexTest1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    g_testCount++;
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 5, g_testCount); // 5, here assert the result.

    ret = LOS_MuxUnlock(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
}

static VOID TaskMisc10Func(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = {0};
    g_testCount++;
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 1, g_testCount);

    ret = LOS_MuxLock(&g_mutexTest1, 0);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskService5Func;
    task1.usTaskPrio = 5; // 5, set reasonable priority.
    task1.pcName = "TaskService_5";
    task1.uwStackSize = LOS_TASK_MIN_STACK_SIZE;
    task1.uwResved = LOS_TASK_ATTR_JOINABLE;
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID02, &task1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 2, g_testCount); // 2, here assert the result.

    LOS_TaskDelay(5); // 5, delay for Timing control.

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 3, g_testCount); // 3, here assert the result.
    ret = LOS_MuxUnlock(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
}


static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task = {0};
    g_testCount = 0;

    ret = LosMuxCreate(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskMisc10Func;
    task.usTaskPrio = 10; // 10, set reasonable priority.
    task.pcName = "TaskMisc_10";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_ATTR_JOINABLE;
#ifdef LOSCFG_KERNEL_SMP
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID01, &task);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, here assert the result.

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskFe4Func;
    task.usTaskPrio = 4; // 4, set reasonable priority.
    task.pcName = "TaskFe_4";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_ATTR_JOINABLE;
#ifdef LOSCFG_KERNEL_SMP
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID03, &task);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(200); // 200, set delay time.

    ICUNIT_ASSERT_EQUAL(g_testCount, 5, g_testCount); // 5, here assert the result.

    ret = LOS_MuxDestroy(&g_mutexTest1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    // 10, here assert the result.
    ICUNIT_GOTO_EQUAL(LOS_TaskPriGet(g_testTaskID01), 10, LOS_TaskPriGet(g_testTaskID01), EXIT);
    // 5, here assert the result.
    ICUNIT_GOTO_EQUAL(LOS_TaskPriGet(g_testTaskID02), 5, LOS_TaskPriGet(g_testTaskID02), EXIT);
    // 4, here assert the result.
    ICUNIT_GOTO_EQUAL(LOS_TaskPriGet(g_testTaskID03), 4, LOS_TaskPriGet(g_testTaskID03), EXIT);

EXIT:
    ret = LOS_TaskJoin(g_testTaskID01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    ret = LOS_TaskJoin(g_testTaskID02, NULL);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    ret = LOS_TaskJoin(g_testTaskID03, NULL);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;
}

VOID ItLosMux036(void)
{
    TEST_ADD_CASE("ItLosMux036", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
