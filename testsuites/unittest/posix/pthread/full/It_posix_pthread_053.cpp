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

static VOID *pthread_f01(void *argument)
{
    UINT32 temp = 200;
    UINT32 temp1 = 20000;
    char str[11] = "1234567890";
    UINT32 ret;
    VOID *result;

    ICUNIT_GOTO_EQUAL(g_testCount, 0, g_testCount, EXIT);
    g_testCount++;

    result = pthread_getspecific(g_key);
    ICUNIT_GOTO_EQUAL(result, NULL, result, EXIT);

    result = pthread_getspecific(g_key1);
    ICUNIT_GOTO_EQUAL(result, NULL, result, EXIT);

    result = pthread_getspecific(g_key2);
    ICUNIT_GOTO_EQUAL(result, NULL, result, EXIT);

    ret = pthread_setspecific(g_key, (void *)&temp);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_setspecific(g_key1, (void *)&temp1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_setspecific(g_key2, (void *)str);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = *((UINT32 *)pthread_getspecific(g_key));
    ICUNIT_GOTO_EQUAL(ret, 200, ret, EXIT);

    ret = *((UINT32 *)pthread_getspecific(g_key1));
    ICUNIT_GOTO_EQUAL(ret, 20000, ret, EXIT);

    result = pthread_getspecific(g_key2);
    ICUNIT_GOTO_STRING_EQUAL((char *)result, "1234567890", (char *)result, EXIT);

    g_testCount++;
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT);
EXIT:
    return (void *)9;
}

static VOID *pthread_f02(void *argument)
{
    UINT32 temp = 100;
    UINT32 temp1 = 10000;
    char str[11] = "abcdefghjk";
    UINT32 ret;
    VOID *result;

    ICUNIT_GOTO_EQUAL(g_testCount, 5, g_testCount, EXIT);
    g_testCount++;

    result = pthread_getspecific(g_key);
    ICUNIT_GOTO_EQUAL(result, NULL, result, EXIT);

    result = pthread_getspecific(g_key1);
    ICUNIT_GOTO_EQUAL(result, NULL, result, EXIT);

    result = pthread_getspecific(g_key2);
    ICUNIT_GOTO_EQUAL(result, NULL, result, EXIT);

    ret = pthread_setspecific(g_key, (void *)&temp);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_setspecific(g_key1, (void *)&temp1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_setspecific(g_key2, (void *)str);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = *((UINT32 *)pthread_getspecific(g_key));
    ICUNIT_GOTO_EQUAL(ret, 100, ret, EXIT);

    ret = *((UINT32 *)pthread_getspecific(g_key1));
    ICUNIT_GOTO_EQUAL(ret, 10000, ret, EXIT);

    result = pthread_getspecific(g_key2);
    ICUNIT_GOTO_STRING_EQUAL((char *)result, "abcdefghjk", (char *)result, EXIT);

    g_testCount++;
    ICUNIT_GOTO_EQUAL(g_testCount, 7, g_testCount, EXIT);
EXIT:
    return (void *)8;
}
static void PthreadKeyF01(void *threadLog)
{
    g_testCount++;
}

static UINT32 Testcase(VOID)
{
    pthread_t newTh, newTh2;
    UINT32 ret;
    UINTPTR uwtemp = 1;
    VOID *result;

    g_testCount = 0;

    ret = pthread_key_create(&g_key, PthreadKeyF01);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_key_create(&g_key1, PthreadKeyF01);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_key_create(&g_key2, PthreadKeyF01);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh, NULL, pthread_f01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LosTaskDelay(20);

    ret = pthread_create(&newTh2, NULL, pthread_f02, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LosTaskDelay(20);
    ICUNIT_ASSERT_EQUAL(g_testCount, 10, g_testCount);

    result = pthread_getspecific(g_key);
    ICUNIT_ASSERT_EQUAL(result, NULL, result);

    result = pthread_getspecific(g_key1);
    ICUNIT_ASSERT_EQUAL(result, NULL, result);

    result = pthread_getspecific(g_key2);
    ICUNIT_ASSERT_EQUAL(result, NULL, result);

    ret = pthread_join(newTh, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newTh2, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_key_delete(g_key);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_key_delete(g_key1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_key_delete(g_key2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return PTHREAD_NO_ERROR;
}


VOID ItPosixPthread053(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_053", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
