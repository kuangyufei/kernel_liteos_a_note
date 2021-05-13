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

#define OS_MUX_NUM 1000
static LosMux g_testMux1;
static LosMux g_testMux2;
static LosMux g_testMux3;
static LosMux g_testMux4;
static LosMux g_testMux5;

static VOID TaskFe7Func(VOID)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 15, g_testCount); // 15, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux4, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 17, g_testCount); // 17, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 19, g_testCount); // 19, here assert the result.
    g_testCount++;
    return;
}


static VOID TaskFe8Func(VOID)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 13, g_testCount); // 13, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux3, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 20, g_testCount); // 20, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 21, g_testCount); // 21, here assert the result.
    g_testCount++;
    return;
}

static VOID TaskFe9Func(VOID)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 11, g_testCount); // 11, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux2, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 22, g_testCount); // 22, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 23, g_testCount); // 23, here assert the result.
    g_testCount++;
    return;
}

static VOID TaskMisc10Func(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1, task2, task3;

    g_testCount++;

    ret = LOS_MuxLock(&g_testMux2, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 2, g_testCount); // 2, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux3, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 3, g_testCount); // 3, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux4, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 4, g_testCount); // 4, here assert the result.
    g_testCount++;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 5, g_testCount); // 5, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux2, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 6, g_testCount); // 6, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux3, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 7, g_testCount); // 7, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux3, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 8, g_testCount); // 8, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux3, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 9, g_testCount); // 9, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    g_testCount++;
    return;
}


static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task;
    g_testCount = 0;

    ret = LosMuxCreate(&g_testMux1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LosMuxCreate(&g_testMux2);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LosMuxCreate(&g_testMux3);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LosMuxCreate(&g_testMux4);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    g_testCount++;

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskMisc10Func;
    task.usTaskPrio = 10; // 10, set reasonable priority.
    task.pcName = "TaskMisc_10";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID01, &task);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 10, g_testCount, EXIT); // 10, here assert the result.
    g_testCount++;

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskFe9Func;
    task.usTaskPrio = 9; // 9, set reasonable priority.
    task.pcName = "TaskFe_9";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID02, &task);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 12, g_testCount, EXIT); // 12, here assert the result.
    g_testCount++;

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskFe8Func;
    task.usTaskPrio = 8; // 8, set reasonable priority.
    task.pcName = "TaskFe_8";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID03, &task);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 14, g_testCount, EXIT); // 14, here assert the result.
    g_testCount++;

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskFe7Func;
    task.usTaskPrio = 7; // 7, set reasonable priority.
    task.pcName = "TaskFe_7";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID04, &task);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 16, g_testCount, EXIT); // 16, here assert the result.
    g_testCount++;

    ret = LOS_TaskDelete(g_testTaskID01);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 18, g_testCount, EXIT); // 18, here assert the result.
    g_testCount++;

    ret = LOS_MuxUnlock(&g_testMux1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_ASSERT_EQUAL(g_testCount, 24, g_testCount); // 24, here assert the result.

    ret = LOS_MuxLock(&g_testMux2, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_MuxLock(&g_testMux3, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_MuxLock(&g_testMux4, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 24, g_testCount, EXIT); // 24, here assert the result.

EXIT:
    ret = LOS_MuxUnlock(&g_testMux2);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxUnlock(&g_testMux3);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxUnlock(&g_testMux4);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL(OsCurrTaskGet()->priority, TASK_PRIO_TEST, OsCurrTaskGet()->priority);

    ret = LOS_MuxDestroy(&g_testMux1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxDestroy(&g_testMux2);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxDestroy(&g_testMux3);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxDestroy(&g_testMux4);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    return LOS_OK;
}

VOID ItLosMux040(void)
{
    TEST_ADD_CASE("ItLosMux040", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
