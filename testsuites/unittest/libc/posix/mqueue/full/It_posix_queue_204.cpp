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
#include "It_posix_queue.h"

CHAR g_mqname[MQUEUE_STANDARD_NAME_LENGTH] = {0};
struct mq_attr g_attr = { 0 };
STATIC volatile UINT32 g_itSync = 0;
mqd_t g_mqueue1, g_mqueue2;

static void *PthreadF01(void *arg)
{
    while (g_itSync != 1) {
    }

    struct mqarray *ptrMqcb = (struct mqarray *)NULL;
    g_mqueue1 = mq_open(g_mqname, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR, &g_attr);
    LosTaskDelay(2); // 2, Set delay time.
    if (g_mqueue1 != -1) {
        mq_close(g_mqueue1);
        mq_unlink(g_mqname);
    }
    return nullptr;
}

static void *PthreadF02(void *arg)
{
    g_itSync = 1;

    struct mqarray *ptrMqcb = (struct mqarray *)NULL;
    g_mqueue2 = mq_open(g_mqname, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR, &g_attr);
    LosTaskDelay(2); // 2, Set delay time.
    if (g_mqueue2 != -1) {
        mq_close(g_mqueue2);
        mq_unlink(g_mqname);
    }

    ICUNIT_GOTO_EQUAL(((g_mqueue1 == -1) && (g_mqueue2 != -1)) || ((g_mqueue1 != -1) && (g_mqueue2 == -1)), TRUE,
        g_mqueue1, EXIT);
EXIT:
    return NULL;
}

STATIC UINT32 Testcase(VOID)
{
    INT32 ret = MQUEUE_NO_ERROR, i;
    pthread_t pthread1, pthread2;
    pthread_attr_t attr1, attr2;

    snprintf(g_mqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq204_%d", LosCurTaskIDGet());
    g_attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    g_attr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    ret = pthread_attr_init(&attr1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ret = pthread_create(&pthread1, &attr1, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_attr_init(&attr2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ret = pthread_create(&pthread2, &attr2, PthreadF02, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    LosTaskDelay(10); // 10, Set delay time.

EXIT:
    (VOID)pthread_join(pthread1, NULL);
    (VOID)pthread_join(pthread2, NULL);
    return LOS_OK;
}

VOID ItPosixQueue204(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_204", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
