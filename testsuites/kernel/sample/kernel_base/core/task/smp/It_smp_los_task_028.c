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

static UINT32 g_testTaskIdsmp[LOSCFG_KERNEL_CORE_NUM];
static int g_runCpu[LOSCFG_KERNEL_CORE_NUM];
static void TaskF01(UINT32 i)
{
    g_runCpu[i] = OS_TCB_FROM_TID(g_testTaskIdsmp[i])->currCpu;
    while (1) {
    }
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    UINT32 gBitmap;
    TSK_INIT_PARAM_S task1 = { 0 };
    int i;

    g_testCount = 0;
    gBitmap = LOSCFG_KERNEL_CPU_MASK;

    TEST_TASK_PARAM_INIT(task1, "it_smp_task_028", (TSK_ENTRY_FUNC)TaskF01, TASK_PRIO_TEST_TASK + 1);

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        g_runCpu[i] = 3; // 3, It is used to calculate runcpu id.
        /* take control of every cores */
        task1.usTaskPrio = TASK_PRIO_TEST_TASK + i + 1;
        task1.usCpuAffiMask = 0;
        task1.auwArgs[0] = i;
        ret = LOS_TaskCreate(&g_testTaskIdsmp[i], &task1);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    TestBusyTaskDelay(10); // 10, set delay time

    /* Calculate the field on the other core */
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        ret = g_runCpu[i];
        g_testCount |= (0x1 << ret);
        ret = LOS_TaskDelete(g_testTaskIdsmp[i]);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    /* Calculate the field on the current core */
    ret = ArchCurrCpuid();
    g_testCount |= (0x1 << ret);

    ICUNIT_ASSERT_EQUAL(g_testCount, gBitmap, g_testCount);

    return LOS_OK;

EXIT:
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM - 1; i++) {
        LOS_TaskDelete(g_testTaskIdsmp[i]);
    }
}

void ItSmpLosTask028(void)
{
    TEST_ADD_CASE("ItSmpLosTask028", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
