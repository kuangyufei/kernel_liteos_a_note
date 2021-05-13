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

static void TaskF01(void)
{
    LOS_AtomicInc(&g_testCount);
    while (1) {
    }
}

static void TaskF02Preempt(void)
{
    LOS_AtomicInc(&g_testCount);
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    UINT32 testTaskIDSmp[LOSCFG_KERNEL_CORE_NUM];

    g_testCount = 0;
    // 2, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
    TEST_TASK_PARAM_INIT(task1, "it_smp_task_034", (TSK_ENTRY_FUNC)TaskF01, TASK_PRIO_TEST_TASK + 2);
    int i;

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        /* take control of every cores */
        // 2, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
        task1.usTaskPrio = TASK_PRIO_TEST_TASK + 2 + i;
        task1.usCpuAffiMask = 0;
        ret = LOS_TaskCreate(&testTaskIDSmp[i], &task1);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    }

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF02Preempt;
    task1.usTaskPrio = TASK_PRIO_TEST_TASK + 1;
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_GOTO_EQUAL(ret + 1, LOS_OK + 1, ret, EXIT);

    task1.usTaskPrio = TASK_PRIO_TEST_TASK - 1;
    ret = LOS_TaskCreate(&g_testTaskID02, &task1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    /* wait for task to schedule */
    TestBusyTaskDelay(10); // 10, set delay time

    /* take this core back to control */
    ICUNIT_GOTO_EQUAL(g_testCount, LOSCFG_KERNEL_CORE_NUM + 1, g_testCount, EXIT);

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        ret = LOS_TaskDelete(testTaskIDSmp[i]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    }

    return LOS_OK;

EXIT:
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        LOS_TaskDelete(testTaskIDSmp[i]);
    }

    return LOS_NOK;
}

void ItSmpLosTask034(void)
{
    TEST_ADD_CASE("ItSmpLosTask034", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
