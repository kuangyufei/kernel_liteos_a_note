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

#include "It_los_task.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static TSK_INFO_S g_taskInfo;
static UINT16 g_testPrio;
static void TaskF01(void)
{
    UINT32 ret;

    ret = LOS_TaskInfoGet(-1, &g_taskInfo);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_TSK_ID_INVALID, ret, EXIT1);

    ret = LOS_TaskInfoGet(LOSCFG_BASE_CORE_TSK_CONFIG - 1, &g_taskInfo);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_TSK_NOT_CREATED, ret, EXIT1);

    ret = LOS_TaskInfoGet(g_testTaskID01, NULL);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_TSK_PTR_NULL, ret, EXIT1);

    ret = LOS_TaskInfoGet(g_testTaskID01, &g_taskInfo);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(g_taskInfo.acName, "Tsk049A", g_taskInfo.acName, EXIT1);
    ICUNIT_GOTO_EQUAL(g_taskInfo.uwTaskID, g_testTaskID01, g_taskInfo.uwTaskID, EXIT1);
    // 2, assert usTaskPrio change.
    ICUNIT_GOTO_EQUAL(g_taskInfo.usTaskPrio, g_testPrio - 2, g_taskInfo.usTaskPrio, EXIT1);
    ICUNIT_GOTO_EQUAL(OS_TASK_STATUS_RUNNING & g_taskInfo.usTaskStatus, OS_TASK_STATUS_RUNNING,
        OS_TASK_STATUS_RUNNING & g_taskInfo.usTaskStatus, EXIT1);
    ICUNIT_GOTO_EQUAL(g_taskInfo.uwBottomOfStack, g_taskInfo.uwSP + g_taskInfo.uwCurrUsed, g_taskInfo.uwBottomOfStack,
        EXIT1);

    g_testCount++;
    return;

EXIT1:
    LOS_TaskDelete(g_testTaskID01);

    return;
}


static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };

    g_testPrio = TASK_PRIO_TEST_TASK;
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "Tsk049A";
    // 2, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
    task1.usTaskPrio = g_testPrio - 2;
    task1.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    g_testCount = 0;
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
    return LOS_OK;

EXIT:
    LOS_TaskDelete(g_testTaskID01);

    return LOS_NOK;
}

void ItLosTask049(void) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosTask049", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
