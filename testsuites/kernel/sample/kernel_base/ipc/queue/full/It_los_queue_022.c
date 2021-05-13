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


static UINT32 Testcase(VOID)
{
    UINT32 ret;
    UINT32 index;
    UINT32 queueID[LOSCFG_BASE_IPC_QUEUE_CONFIG + 1];
    CHAR buff1[QUEUE_SHORT_BUFFER_LENTH] = "UniDSP";
    CHAR buff2[QUEUE_SHORT_BUFFER_LENTH] = "";

    UINT32 exsitedQueue = QUEUE_EXISTED_NUM;
    for (index = 0; index < LOSCFG_BASE_IPC_QUEUE_CONFIG - exsitedQueue; index++) {
        ret = LOS_QueueCreate(NULL, 3, &queueID[index], 0, 8); // 3, Set the queue length; 8, Set the node size of the queue.
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    }

    ret = LOS_QueueCreate("Q1", 3, &queueID[LOSCFG_BASE_IPC_QUEUE_CONFIG], 0, 8); // 3, Set the queue length; 8, Set the node size of the queue.
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_CB_UNAVAILABLE, ret, EXIT);

    ret = LOS_QueueWrite(LOSCFG_BASE_IPC_QUEUE_CONFIG + 1, &buff1, 8, 0); // 8, Write the setting size of queue buffer.
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_INVALID, ret, EXIT);

    ret = LOS_QueueRead(LOSCFG_BASE_IPC_QUEUE_CONFIG + 1, &buff2, 8, 0); // 8, Read the setting size of queue buffer.
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_INVALID, ret, EXIT);

EXIT:
    for (index = 0; index < LOSCFG_BASE_IPC_QUEUE_CONFIG - exsitedQueue; index++) {
        ret = LOS_QueueDelete(queueID[index]);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    return LOS_OK;
}

VOID ItLosQueue022(VOID)
{
    TEST_ADD_CASE("ItLosQueue022", Testcase, TEST_LOS, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
