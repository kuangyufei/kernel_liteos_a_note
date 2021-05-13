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


#include "It_los_sem.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static UINT32 g_sz[LOSCFG_KERNEL_CORE_NUM] = {0};
static UINT32 TestAbs(UINT32 num1, UINT32 num2)
{
    return (num1 > num2) ? num1 - num2 : num2 - num1;
}
static VOID TaskF01(VOID)
{
    UINT32 ret;

    LOS_AtomicInc(&g_testCount);
    ret = LOS_SemPend(g_semID, 10); // 10 ticks timeout
    if ((ret != LOS_OK) && (ret != LOS_ERRNO_SEM_TIMEOUT)) {
        ICUNIT_ASSERT_EQUAL_VOID(1, 0, ret);
    }

    TestDumpCpuid();
    LOS_AtomicInc(&g_testCount);
    return;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret, currCpuid, i;
    TSK_INIT_PARAM_S testTask;

    g_testCount = 0;

    ret = LOS_SemCreate(0, &g_semID);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        TEST_TASK_PARAM_INIT_AFFI(testTask, "it_sem_031_task1", TaskF01, TASK_PRIO_TEST - 1,
            CPUID_TO_AFFI_MASK((ArchCurrCpuid() + i + 1) %
            LOSCFG_KERNEL_CORE_NUM)); // let current cpu 's task create at the last
        ret = LOS_TaskCreate(&g_sz[i], &testTask);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    LOS_TaskDelay(5); // 5, delay enouge time
    ICUNIT_GOTO_EQUAL(g_testCount, LOSCFG_KERNEL_CORE_NUM, g_testCount, EXIT);

    ret = LOS_SemPost(g_semID); // post sem before 10 ticks timeout
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    LOS_TaskDelay(1);
    ICUNIT_GOTO_EQUAL(g_testCount, LOSCFG_KERNEL_CORE_NUM + 1, g_testCount, EXIT);

    LOS_TaskDelay(10); // 10, delay enouge time

    ICUNIT_GOTO_EQUAL(g_testCount, LOSCFG_KERNEL_CORE_NUM * 2, g_testCount, EXIT); // 2, Here, assert that g_testCount is equal to

EXIT:
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        LOS_TaskDelete(g_sz[i]);
    }
    ret = LOS_SemDelete(g_semID);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;
}

VOID ItSmpLosSem031(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItSmpLosSem031", Testcase, TEST_LOS, TEST_SEM, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
