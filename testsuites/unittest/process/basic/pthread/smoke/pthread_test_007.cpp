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
#include "it_pthread_test.h"


static pthread_t g_th;
VOID *PthreadTest115(VOID *arg)
{
    g_testCount++;
    sleep(1);
    g_testCount++;

    return NULL;
}

VOID *PthreadFunc1115(VOID *arg)
{
    int ret = 0;
    g_testCount++;
    usleep(1000 * 10 * 10); // 1000 * 10 * 10, delay for timimg control.

    ret = pthread_join(g_th, NULL);
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);
    g_testCount++;
EXIT:
    return NULL;
}

VOID *PthreadFunc2115(VOID *arg)
{
    int ret = 0;
    g_testCount++;
    usleep(1000 * 10 * 15); // 1000 * 10 * 15, delay for timimg control.

    ret = pthread_join(g_th, NULL);
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);
    g_testCount++;
EXIT:
    return NULL;
}

VOID *PthreadFunc3115(VOID *arg)
{
    int ret = 0;
    g_testCount++;
    usleep(1000 * 10 * 6); // 1000 * 10 * 6, delay for timimg control.

    ret = pthread_join(g_th, NULL);
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);
    g_testCount++;
    ;

EXIT:
    return NULL;
}

static UINT32 Testcase(VOID)
{
    int rc;

    int ret = 0;
    pthread_t th1;
    pthread_t th2;
    pthread_t th3;
    pthread_attr_t attr;
    g_testCount = 0;
    pthread_attr_init(&attr);
    pthread_create(&g_th, &attr, PthreadTest115, NULL);

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th1, &attr, PthreadFunc1115, NULL);
    pthread_create(&th2, &attr, PthreadFunc2115, NULL);
    pthread_create(&th3, &attr, PthreadFunc3115, NULL);
    ret = pthread_join(g_th, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    usleep(1000 * 10 * 50);                           // 1000 * 10 * 50, delay for timimg control.
    ICUNIT_ASSERT_EQUAL(g_testCount, 8, g_testCount); // 8, assert the exit code.
    return 0;
}

void ItTestPthread007(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_007", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
