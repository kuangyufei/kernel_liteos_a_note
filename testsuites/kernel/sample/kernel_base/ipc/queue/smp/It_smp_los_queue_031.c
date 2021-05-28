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

#include "It_los_queue.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */


static CHAR g_buff1[QUEUE_SHORT_BUFFER_LENGTH] = "UniDSP";
static CHAR g_buff2[QUEUE_SHORT_BUFFER_LENGTH] = "";
static UINT32 g_ret = 0xff;
static VOID TaskF01(VOID)
{
    UINT32 ret, i;
    LOS_AtomicInc(&g_testCount);

    for (i = 0; i < TRandom() % 500; i++); // 500, Set this loop to not more than 500 cpu cycles.

    ret = LOS_QueueRead(g_testQueueID01, &g_buff2, 8, LOS_WAIT_FOREVER); // 8, Read the setting size of queue buffer.
    g_ret = ret;

    LOS_AtomicInc(&g_testCount);

    TestDumpCpuid();
    return;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret, currCpuid, i, j;
    TSK_INIT_PARAM_S testTask;
    LosTaskCB *cb = NULL;

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    for (i = 0; i < LOOP; i++) {
        g_testCount = 0;
        g_ret = 0xff;
        ret = LOS_QueueCreate("Q1", 1, &g_testQueueID01, 0, 8); // 8, Set the maximum data length of the message queue.
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

        TEST_TASK_PARAM_INIT_AFFI(testTask, "it_queue_031_task1", TaskF01, TASK_PRIO_TEST - 1,
            CPUID_TO_AFFI_MASK(currCpuid)); // other cpu
        ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

        TestAssertBusyTaskDelay(100, 1); // 100, Set the timeout of runtime; wait for task f01 run
        ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);

        for (j = 0; j < TRandom() % 500; j++); // 500, Set this loop to not more than 500 cpu cycles.

        ret = LOS_QueueDelete(g_testQueueID01); // delete queue in current cpu
        if ((g_ret == LOS_ERRNO_QUEUE_NOT_CREATE) && (ret == LOS_OK)) {
        } else if ((g_ret == 0xff) && (ret == LOS_OK)) {
        } else if ((g_ret == 0xff) && (ret == LOS_ERRNO_QUEUE_IN_TSKUSE)) {
        } else {
            ICUNIT_GOTO_EQUAL(1, 0, g_ret, EXIT); // report error
        }

        /* clear enviorment */
        LOS_TaskDelete(g_testTaskID01);
        LOS_TaskDelay(10); // 10, set delay time.
        ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;
        ICUNIT_GOTO_NOT_EQUAL(ret & OS_TASK_STATUS_UNUSED, 0, ret, EXIT);

        ret = LOS_QueueDelete(g_testQueueID01);
        PRINT_DEBUG("finally delRet = 0x%x\n", ret);
    }
EXIT:
    LOS_TaskDelete(g_testTaskID01);
    LOS_QueueDelete(g_testQueueID01);
    return LOS_OK;
}

VOID ItSmpLosQueue031(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItSmpLosQueue031", Testcase, TEST_LOS, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
