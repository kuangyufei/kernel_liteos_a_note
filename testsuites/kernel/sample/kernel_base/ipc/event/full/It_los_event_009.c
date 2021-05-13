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

    g_testCount++;
    ret = LOS_EventRead(NULL, 0x11, LOS_WAITMODE_OR, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_EVENT_PTR_NULL, ret, EXIT);

    ret = LOS_EventRead(&g_event, 0, LOS_WAITMODE_OR, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_EVENT_EVENTMASK_INVALID, ret, EXIT);

    ret = LOS_EventRead(&g_event, 0xffffffff, LOS_WAITMODE_OR, 0);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_EVENT_SETBIT_INVALID, ret, EXIT);
    g_testCount++;

    ret = LOS_EventRead(&g_event, 0x11, 0xffffffff, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_EVENT_FLAGS_INVALID, ret, EXIT);

    ret = LOS_EventRead(&g_event, 0x11, 0, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_EVENT_FLAGS_INVALID, ret, EXIT);

    ret = LOS_EventRead(&g_event, 0x11, (LOS_WAITMODE_AND | LOS_WAITMODE_OR | LOS_WAITMODE_CLR), LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_EVENT_FLAGS_INVALID, ret, EXIT);

    g_testCount++;

EXIT:
    LOS_TaskDelete(g_testTaskID01);
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1;
    memset(&task1, 0, sizeof(TSK_INIT_PARAM_S));
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.pcName = "EventTsk9";
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    // 24, Set the priority according to the task purpose,a smaller number means a higher priority.
    task1.usTaskPrio = 24;

    g_testCount = 0;
    g_event.uwEventID = 0;

    LOS_EventInit(&g_event);

    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    TestExtraTaskDelay(2); // 2, delay enouge time

    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // 3, Here, assert that g_testCount is equal to 3.

    return LOS_OK;
}

VOID ItLosEvent009(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosEvent009", Testcase, TEST_LOS, TEST_EVENT, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
