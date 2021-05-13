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

#include "It_los_swtmr.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static UINT32 g_testTimes;

static UINT16 g_swtmrHandle[LOSCFG_KERNEL_CORE_NUM];

static VOID SwtmrF01(void)
{
    LOS_AtomicInc(&g_testCount);
}

static void TaskF01(UINTPTR swtmrId)
{
    UINT32 ret;

    ret = LOS_SwtmrStart(swtmrId);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    TestDumpCpuid();

    return;

EXIT:
    LOS_SwtmrDelete(swtmrId);
    return;
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, testid;
    UINT32 i;

    /* each core run swtmr for 10 times, period is 10 */
    g_testCount = 0;
    g_testPeriod = 10; // period is 10
    g_testTimes = 1;

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        ret = LOS_SwtmrCreate(g_testPeriod, LOS_SWTMR_MODE_PERIOD, (SWTMR_PROC_FUNC)SwtmrF01, &g_swtmrHandle[i], 0);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        TEST_TASK_PARAM_INIT(testTask, "it_swtmr_002_task", TaskF01,
            TASK_PRIO_TEST_SWTMR + 1); // not set cpuaffi
        testTask.auwArgs[0] = g_swtmrHandle[i];
        ret = LOS_TaskCreate(&testid, &testTask);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    LOS_TaskDelay(g_testPeriod * g_testTimes + 5); // g_testPeriod * g_testTimes + 5, set delay time

    ICUNIT_ASSERT_EQUAL(g_testCount, g_testTimes * LOSCFG_KERNEL_CORE_NUM, g_testCount);

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        ret = LOS_SwtmrStop(g_swtmrHandle[i]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

        ret = LOS_SwtmrDelete(g_swtmrHandle[i]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    }
    return LOS_OK;
EXIT:
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        LOS_SwtmrDelete(g_swtmrHandle[i]);
    }
}

VOID ItSmpLosSwtmr002(VOID)
{
    TEST_ADD_CASE("ItSmpLosSwtmr002", Testcase, TEST_LOS, TEST_SWTMR, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

