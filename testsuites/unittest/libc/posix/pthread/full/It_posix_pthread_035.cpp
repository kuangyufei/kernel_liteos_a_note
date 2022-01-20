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
static volatile int g_testCnt1;

static VOID *pthread_f01(void *argument)
{
    UINT32 j = 0;
    INT32 value;
    ICUNIT_GOTO_EQUAL(g_testCnt1, 0, g_testCnt1, EXIT);
    g_testCnt1++;

    while (g_testCnt1 != 3) {
        j++;
        j = j * 2 - 4;
        j = 2001 + 4001 * 1 - 2001;
        usleep(100);
    }
    g_testCnt1++;
    ICUNIT_GOTO_EQUAL(g_testCnt1, 4, g_testCnt1, EXIT);
EXIT:
    return (void *)9;
}

static VOID *pthread_f02(void *argument)
{
    UINT32 j = 0;
    INT32 value;
    ICUNIT_GOTO_EQUAL(g_testCnt1, 1, g_testCnt1, EXIT);
    g_testCnt1++;

    while (g_testCnt1 != 4) {
        j++;
        j = j * 2 - 4;
        j = 2001 + 4001 * 1 - 2001;
        usleep(100);
    }

    g_testCnt1++;
    ICUNIT_GOTO_EQUAL(g_testCnt1, 5, g_testCnt1, EXIT);
EXIT:
    return (void *)8;
}

static VOID *PthreadF03(void *argument)
{
    UINT32 j = 0;
    INT32 value;
    ICUNIT_GOTO_EQUAL(g_testCnt1, 2, g_testCnt1, EXIT);
    g_testCnt1++;

    while (g_testCnt1 != 5) {
        j++;
        j = j * 2 - 4;
        j = 2001 + 4001 * 1 - 2001;
        usleep(10);
    }

    g_testCnt1++;
    ICUNIT_GOTO_EQUAL(g_testCnt1, 6, g_testCnt1, EXIT);

EXIT:
    return (void *)7;
}
static UINT32 Testcase(VOID)
{
    pthread_t newTh, newTh2, newTh3;

    UINT32 ret;
    UINTPTR uwtemp = 1;

    g_testCnt1 = 0;

    ret = pthread_create(&newTh, NULL, pthread_f01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh2, NULL, pthread_f02, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh3, NULL, PthreadF03, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    while (g_testCnt1 < 6)
        sleep(1);

    ICUNIT_ASSERT_EQUAL(g_testCnt1, 6, g_testCnt1);

    ret = pthread_join(newTh, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newTh2, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newTh3, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread035(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_035", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
