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
#include "lt_clock_test.h"

static void *ThreadFuncTest(void *arg)
{
    printf("Subthread starting infinite loop\n");
    while (1) {
        pthread_testcancel();
    }
}

static int ThreadClock(const char *msg, clockid_t cid)
{
    struct timespec ts;
    int ret;

    printf("%s", msg);
    ret = clock_gettime(cid, &ts);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    printf("%lld.%03ld s\n", ts.tv_sec, ts.tv_nsec / 1000000); // 1000000, 1ms.
    return 0;
}

static int ClockTest(void)
{
    pthread_t thread;
    clockid_t clockid;
    int ret;
    struct timespec ts;

    ret = pthread_create(&thread, NULL, ThreadFuncTest, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    printf("Main thread sleeping\n");
    sleep(1);

    printf("Main thread consuming some CPU time...\n");
    usleep(400000); // 400000 delay for test

    /* get current pthread clockid */
    ret = pthread_getcpuclockid(pthread_self(), &clockid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = ThreadClock("Main thread CPU time:   ", clockid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* get create pthread clockid */
    ret = pthread_getcpuclockid(thread, &clockid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = ThreadClock("Subthread CPU time:   ", clockid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_cancel(thread);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ClockTest002(void)
{
    TEST_ADD_CASE(__FUNCTION__, ClockTest, TEST_POSIX, TEST_TIMES, TEST_LEVEL0, TEST_FUNCTION);
}
