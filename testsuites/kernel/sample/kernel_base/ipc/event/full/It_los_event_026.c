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

static VOID TaskF01(VOID)
{
    UINT32 ret;

    LOS_TaskLock();

    g_testCount++;

    ret = LOS_EventRead(&g_event, 0x11, LOS_WAITMODE_AND, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, g_event.uwEventID, ret, EXIT);
    ICUNIT_GOTO_EQUAL(ret, 0x11, ret, EXIT);

    g_testCount++;

    ret = LOS_EventRead(&g_event, 0x11, LOS_WAITMODE_AND, 0);
    ICUNIT_GOTO_EQUAL(ret, g_event.uwEventID, ret, EXIT);
    ICUNIT_GOTO_EQUAL(ret, 0x11, ret, EXIT);

    g_testCount++;

    LOS_TaskUnlock();

EXIT:
    LOS_TaskDelete(g_testTaskID01);
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = {0};

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.pcName = "EventTsk26";
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.usTaskPrio = TASK_PRIO_TEST - 2; // 2, set new task priority, it is higher than the current task.
    task1.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    g_testCount = 0;
    LOS_EventInit(&g_event);
    LOS_TaskLock();

    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_testCount, 0, g_testCount, EXIT);

    LOS_EventWrite(&g_event, 0x11);
    ICUNIT_GOTO_EQUAL(g_testCount, 0, g_testCount, EXIT1);

    LOS_TaskUnlock();

    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT1); // 3, Here, assert that g_testCount is equal to 3.

EXIT1:
    ret = LOS_EventClear(&g_event, 0);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

EXIT:
    LOS_TaskDelete(g_testTaskID01);

    ret = LOS_EventDestroy(&g_event);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    return LOS_OK;
}

VOID ItLosEvent026(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosEvent026", Testcase, TEST_LOS, TEST_EVENT, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
