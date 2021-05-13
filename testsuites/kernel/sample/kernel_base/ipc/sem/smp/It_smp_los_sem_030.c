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

#if (LOSCFG_KERNEL_SMP == YES)
static UINT32 g_ret1, g_ret2, g_ret3;
static UINT32 g_szId[3] = {0};

static VOID TaskF01(UINT32 index)
{
    UINT32 ret, i;
    switch (index) {
        case 0:
            for (i = 0; i < TRandom() % 500; i++) { // Gets a random value of 0-500
            }
            g_ret1 = LOS_SemPend(g_semID, LOS_WAIT_FOREVER);
            LOS_AtomicInc(&g_testCount);
            break;
        case 1:
            for (i = 0; i < TRandom() % 500; i++) { // Gets a random value of 0-500
            }
            g_ret2 = LOS_SemPost(g_semID);
            LOS_AtomicInc(&g_testCount);
            break;
        case 2: // 2, index
            for (i = 0; i < TRandom() % 500; i++) { // Gets a random value of 0-500
            }
            g_ret3 = LOS_SemDelete(g_semID);
            LOS_AtomicInc(&g_testCount);
            break;
        default:
            break;
    }

    TestDumpCpuid();
    return;
}
static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret;
    UINT32 i, j;

    g_testCount = 0;

    for (i = 0; i < LOOP; i++) {
        g_testCount = 0;
        g_ret1 = 0xff;
        g_ret2 = 0xff;
        g_ret3 = 0xff;
        ret = LOS_SemCreate(0, &g_semID);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

        for (j = 0; j < 3; j++) { // 3, max index
            TEST_TASK_PARAM_INIT(testTask, "it_sem_030_task", TaskF01, TASK_PRIO_TEST + 1);
            testTask.auwArgs[0] = j;
            ret = LOS_TaskCreate(&g_szId[j], &testTask);
            ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
        }

        LOS_TaskDelay(10); // 10, delay enouge time
        ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, Here, assert that g_testCount is equal to

        if ((g_ret1 == LOS_OK) && (g_ret2 == LOS_OK) && (g_ret3 == LOS_OK)) { // pend-post-del ///post-pend-del
        } else if ((g_ret1 == LOS_ERRNO_SEM_INVALID) && (g_ret2 == LOS_ERRNO_SEM_INVALID) &&
            (g_ret3 == LOS_OK)) { // del-pend-post//del-post-pend
        } else if ((g_ret1 == LOS_OK) && (g_ret2 == LOS_OK) && (g_ret3 == LOS_ERRNO_SEM_PENDED)) { // pend-delete-post
        } else if ((g_ret1 == LOS_ERRNO_SEM_INVALID) && (g_ret2 == LOS_OK) && (g_ret3 == LOS_OK)) { // post-del-pend
        } else {
            ICUNIT_GOTO_EQUAL(1, 0, g_ret1, EXIT);
        }
        for (j = 0; j < 3; j++) { // 3, max index
            ret = OS_TCB_FROM_TID(g_szId[j])->taskStatus;
            ICUNIT_GOTO_NOT_EQUAL(ret & OS_TASK_STATUS_UNUSED, 0, ret, EXIT);
        }
        LOS_SemDelete(g_semID);
    }

EXIT:
    for (i = 0; i < 3; i++) { // 3, max index
        LOS_TaskDelete(g_szId[i]);
    }
    LOS_SemDelete(g_semID);
    return LOS_OK;
}

VOID ItSmpLosSem030(VOID)
{
    TEST_ADD_CASE("ItSmpLosSem030", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
