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

#ifdef LOSCFG_KERNEL_SMP

static UINT32 g_ret1, g_ret2, g_ret3;
static UINT32 g_szId[3] = {0};

static VOID TaskF01(UINT32 index)
{
    UINT32 ret, i;
    PRINT_DEBUG("index = %d,cpuid=%d\n", index, ArchCurrCpuid());
    switch (index) {
        case 0:
            PRINT_DEBUG("---000---\n");
            for (i = 0; i < TRandom() % 500; i++) { // Random values of 0 to 500
            }
            LOS_AtomicInc(&g_testCount);
            g_ret1 = LOS_EventRead(&g_event, 0x11, LOS_WAITMODE_AND, LOS_WAIT_FOREVER);
            break;
        case 1:
            PRINT_DEBUG("---111---\n");
            for (i = 0; i < TRandom() % 500; i++) { // Random values of 0 to 500
            }
            LOS_AtomicInc(&g_testCount);
            g_ret2 = LOS_EventWrite(&g_event, 0x11);
            break;
        case 2: // 2,index
            PRINT_DEBUG("---222---\n");
            for (i = 0; i < TRandom() % 500; i++) { // Random values of 0 to 500
            }
            LOS_AtomicInc(&g_testCount);
            g_ret3 = LOS_EventDestroy(&g_event);
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

    for (i = 0; i < LOOP; i++) {
        g_testCount = 0;
        g_event.uwEventID = 0;
        g_ret1 = 0xff;
        g_ret2 = 0xff;
        g_ret3 = 0xff;
        ret = LOS_EventInit(&g_event);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

        for (j = 0; j < 3; j++) { // 3, max index
            TEST_TASK_PARAM_INIT(testTask, "it_event_032_task", TaskF01, TASK_PRIO_TEST - 1);
            testTask.auwArgs[0] = j;
            ret = LOS_TaskCreate(&g_szId[j], &testTask);
            ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
        }
        while (g_testCount < 3) { // 3, wait if g_testCount < 3 ,or do noting
        }

        LOS_TaskDelay(10); // 10, delay enouge time
        PRINT_DEBUG("ret1= 0x%x,ret2 = 0x%x,ret3=0x%x\n", g_ret1, g_ret2, g_ret3);

        if ((g_ret1 == 0x11) && (g_ret2 == LOS_OK) && (g_ret3 == LOS_OK)) { // pend-post-del ///post-pend-del
        } else if ((g_ret1 == 0x11) && (g_ret2 == LOS_OK) && (g_ret3 == LOS_OK)) { // del-pend-post//del-post-pend
        } else if ((g_ret1 == 0x11) && (g_ret2 == LOS_OK) &&
            (g_ret3 == LOS_ERRNO_EVENT_SHOULD_NOT_DESTROY)) { // pend-delete-post
        } else if ((g_ret1 == 0xff) && (g_ret2 == LOS_OK) && (g_ret3 == LOS_OK)) { // post-del-pend
            LOS_TaskDelete(g_szId[0]);                                             // delete the pend task
        } else if ((g_ret1 == 0x00) && (g_ret2 == LOS_OK) && (g_ret3 == LOS_OK)) {
            /*
             * (g_ret1 == 0) && (g_ret2 == 0) && (uwRet3 == 0)
             * try read -->write event ---resume read---> destroy event ---> read event [fail]
             */
        } else {
            ICUNIT_GOTO_EQUAL(1, 0, g_ret1, EXIT);
        }

        for (j = 0; j < 3; j++) { // 3, max index
            ret = OS_TCB_FROM_TID(g_szId[j])->taskStatus;
            ICUNIT_GOTO_NOT_EQUAL(ret & OS_TASK_STATUS_UNUSED, 0, ret, EXIT);
        }

        LOS_EventDestroy(&g_event);
    }

EXIT:
    for (i = 0; i < 3; i++) {  // 3, max index
        LOS_TaskDelete(g_szId[i]);
    }
    return LOS_OK;
}

VOID ItSmpLosEvent032(VOID)
{
    TEST_ADD_CASE("ItSmpLosEvent032", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
