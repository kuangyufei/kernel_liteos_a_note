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
    UINT32 ret, i, j;
    UINT32 index;
    const UINT32 len = 1000;
    const UINT32 count = sizeof(UINTPTR);
    CHAR filebuf[260] = "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
                        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
                        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR readbuf[260] = "";
    QUEUE_INFO_S queueInfo;

    ret = LOS_QueueCreate("Q1", 1000, &g_testQueueID01, 0, count); // 1000, Set the queue length.
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_QueueInfoGet(g_testQueueID01, &queueInfo);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    ICUNIT_GOTO_EQUAL(queueInfo.usQueueLen, len, queueInfo.usQueueLen, EXIT);
    ICUNIT_GOTO_EQUAL(queueInfo.uwQueueID, g_testQueueID01, queueInfo.uwQueueID, EXIT);

    for (j = 0; j < 100; j++) { // 100, The loop frequency.
        for (i = 0; i < 1000; i++) { // 1000, The loop frequency.
            ret = LOS_QueueWrite(g_testQueueID01, filebuf, QUEUE_SHORT_BUFFER_LENGTH, 0);
            ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
        }
        ret = LOS_QueueWrite(g_testQueueID01, filebuf, QUEUE_SHORT_BUFFER_LENGTH, 0);
        ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_ISFULL, ret, EXIT);

        for (i = 0; i < 1000; i++) { // 1000, The loop frequency.
            ret = memset_s(readbuf, 260, 0, 260); // 260, Read buf size.
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
            ret = LOS_QueueRead(g_testQueueID01, readbuf, QUEUE_SHORT_BUFFER_LENGTH, 0);
            ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
        }
        ret = LOS_QueueRead(g_testQueueID01, readbuf, QUEUE_SHORT_BUFFER_LENGTH, 0);
        ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_ISEMPTY, ret, EXIT);
    }
    ret = LOS_QueueWrite(g_testQueueID01, filebuf, QUEUE_SHORT_BUFFER_LENGTH, 0);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_QueueRead(g_testQueueID01, readbuf, QUEUE_SHORT_BUFFER_LENGTH, 0);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_QueueInfoGet(g_testQueueID01, &queueInfo);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    ICUNIT_GOTO_EQUAL(queueInfo.usQueueLen, len, queueInfo.usQueueLen, EXIT);
    ICUNIT_GOTO_EQUAL(queueInfo.uwQueueID, g_testQueueID01, queueInfo.uwQueueID, EXIT);

    ret = LOS_QueueRead(g_testQueueID01, readbuf, QUEUE_SHORT_BUFFER_LENGTH, 0);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_QUEUE_ISEMPTY, ret, EXIT);

    ret = LOS_QueueDelete(g_testQueueID01);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    return LOS_OK;

EXIT:
    LOS_QueueDelete(g_testQueueID01);
    return LOS_OK;
}

VOID ItLosQueue089(VOID)
{
    TEST_ADD_CASE("ItLosQueue089", Testcase, TEST_LOS, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
