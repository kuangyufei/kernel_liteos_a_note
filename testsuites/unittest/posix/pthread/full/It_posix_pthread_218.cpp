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
#include "It_posix_pthread.h"

static VOID *pthread_f01(VOID *argument)
{
    INT32 ret = 0;
    const INT32 keyValue = 1000;

    ret = pthread_setspecific(g_pthreadKeyTest[g_testCount], (const void *)(keyValue));
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    pthread_exit(0);

    return NULL;
}


static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    VOID *valuePtr;
    INT32 ret = 0;

    g_testCount = 0;

    for (g_testCount = 0; g_testCount < PTHREAD_KEY_NUM; g_testCount++) {
        ret = pthread_key_create(&g_pthreadKeyTest[g_testCount], NULL);
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    }

    for (g_testCount = 0; g_testCount < PTHREAD_KEY_NUM; g_testCount++) {
        ret = pthread_create(&newTh, NULL, pthread_f01, NULL);
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

        ret = pthread_join(newTh, &valuePtr);
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
        ICUNIT_ASSERT_EQUAL(valuePtr, 0, valuePtr);
    }

    for (g_testCount = 0; g_testCount < PTHREAD_KEY_NUM; g_testCount++) {
        ret = pthread_key_delete(g_pthreadKeyTest[g_testCount]);
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    }

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread218(VOID)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_218", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
