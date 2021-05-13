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

static VOID ItQueueHead040F01(VOID)
{
    UINT32 ret;
    CHAR buff2[8] = "";

    g_testCount++;

    ret = LOS_QueueRead(g_testQueueID01, &buff2, sizeof(UINTPTR), 0xf);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    g_testCount++;

    ret = LOS_QueueWriteHead(g_testQueueID01, buff2, sizeof(UINTPTR), 0xf);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_QueueWriteHead(g_testQueueID01, buff2, sizeof(UINTPTR), 0xf);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    g_testCount++;

EXIT:
    LOS_TaskDelete(g_testTaskID01);
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    CHAR buff1[8] = "fgh";
    TSK_INIT_PARAM_S task1 = { 0 };

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)ItQueueHead040F01;
    task1.pcName = "TskName53";
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.usTaskPrio = 23; // 23, Set task priority.

    g_testCount = 0;

    ret = LOS_QueueCreate("Q1", 1, &g_testQueueID01, 0, sizeof(UINTPTR));
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_QueueDelete(LOSCFG_BASE_IPC_QUEUE_CONFIG + 1);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_NOT_FOUND, ret, EXIT);

    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    TestExtraTaskDelay(TEST_TASKDELAY_2TICK);

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);

    ret = LOS_QueueDelete(g_testQueueID01);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_IN_TSKUSE, ret, EXIT);

    ret = LOS_QueueWriteHead(g_testQueueID01, buff1, sizeof(UINTPTR), 0xf);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    TestExtraTaskDelay(TEST_TASKDELAY_2TICK);
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, Here, assert that g_testCount is equal to 2.

    ret = LOS_QueueDelete(g_testQueueID01);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_IN_TSKUSE, ret, EXIT);

    ret = LOS_QueueRead(g_testQueueID01, buff1, sizeof(UINTPTR), 0xf);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    TestExtraTaskDelay(TEST_TASKDELAY_2TICK);
    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, Here, assert that g_testCount is equal to 3.

    ret = LOS_QueueDelete(g_testQueueID01);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_QueueDelete(g_testQueueID01);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_NOT_CREATE, ret, EXIT);

EXIT:
    LOS_QueueDelete(g_testQueueID01);
    return LOS_OK;
}

VOID ItLosQueueHead040(VOID)
{
    TEST_ADD_CASE("ItLosQueueHead040", Testcase, TEST_LOS, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
