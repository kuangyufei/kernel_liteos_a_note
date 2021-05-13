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

static LosMux g_testMux1;
static UINT32 g_mainTaskID;
static VOID TaskFe4Func(VOID)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 4, g_testCount); // 4, here assert the result.
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 8, g_testCount); // 8, here assert the result.
    g_testCount++;

    return;
}

static VOID TaskMisc10Func(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1, task2, task3;
    LosTaskCB *taskCB = OS_TCB_FROM_TID(g_mainTaskID);
    g_testCount++;

    ret = LOS_MuxLock(&g_testMux1, 10); // 10, init mutex.
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_ETIMEDOUT, ret);

    ICUNIT_ASSERT_EQUAL_VOID(taskCB->priority, 4, taskCB->priority); // 4, here assert the result.
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 6, g_testCount);           // 6, here assert the result.
    g_testCount++;

    return;
}


static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskParam;
    g_testCount = 0;
    LosTaskCB *task = NULL;
    UINT16 prio = OsCurrTaskGet()->priority;
    g_mainTaskID = OsCurrTaskGet()->taskID;

    ret = LosMuxCreate(&g_testMux1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    g_testCount++;
    g_testCount++;

    taskParam.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskMisc10Func;
    taskParam.usTaskPrio = 10; // 10, set reasonable priority.
    taskParam.pcName = "TaskMisc_10";
    taskParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskParam.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    taskParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID01, &taskParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(OsCurrTaskGet()->priority, 10, OsCurrTaskGet()->priority, EXIT); // 10, here assert the result.
    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT);                              // 3, here assert the result.
    g_testCount++;

    taskParam.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskFe4Func;
    taskParam.usTaskPrio = 4; // 4, set reasonable priority.
    taskParam.pcName = "TaskFe_4";
    taskParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskParam.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    taskParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID03, &taskParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(OsCurrTaskGet()->priority, 4, OsCurrTaskGet()->priority, EXIT); // 4, here assert the result.
    ICUNIT_GOTO_EQUAL(g_testCount, 5, g_testCount, EXIT);                             // 5, here assert the result.
    g_testCount++;

    LOS_TaskDelay(11); // 11, set delay time.

    ICUNIT_GOTO_EQUAL(g_testCount, 7, g_testCount, EXIT); // 7, here assert the result.
    g_testCount++;

    ret = LOS_MuxUnlock(&g_testMux1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(OsCurrTaskGet()->priority, prio, OsCurrTaskGet()->priority, EXIT);

    ret = LOS_MuxLock(&g_testMux1, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 9, g_testCount, EXIT); // 9, here assert the result.

EXIT:
    ret = LOS_MuxUnlock(&g_testMux1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_MuxDestroy(&g_testMux1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    return LOS_OK;
}

VOID ItLosMux042(void)
{
    TEST_ADD_CASE("ItLosMux042", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
