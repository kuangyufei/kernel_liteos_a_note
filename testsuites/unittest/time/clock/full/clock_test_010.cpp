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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/times.h>
#include <errno.h>
#include <inttypes.h>
#include "lt_clock_test.h"
#include <osTest.h>

static int g_failCnt = 0;

static int SleepTest(int64_t expectTime)
{
    clockid_t clk = CLOCK_REALTIME;
    struct timespec tp, oldtp;
    int ret;
    int64_t escapeTime;

    /* get current real time */
    ret = clock_gettime(clk, &oldtp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    tp.tv_sec = expectTime / (long)1e9;
    tp.tv_nsec = expectTime % (long)1e9;
    ret = clock_nanosleep(clk, 0, &tp, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* get current real time again */
    ret = clock_gettime(clk, &tp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    escapeTime = (tp.tv_sec - oldtp.tv_sec) * (int64_t)1e9  + (tp.tv_nsec - oldtp.tv_nsec);
    LogPrintln("slept time (expected --> actual): %" PRId64 "ns --> %" PRId64 "ns, delta: %" PRId64 "ns\n", expectTime,
        escapeTime, escapeTime - expectTime);

    g_failCnt += (escapeTime < expectTime);

    return 0;
}

static void *ClockTestThread(void *arg)
{
    (void)SleepTest(0);
    (void)SleepTest(2); // 2, ns.
    (void)SleepTest(3); // 3, ns.
    (void)SleepTest(40e3);     // 40us
    (void)SleepTest(50e3);     // 50us
    (void)SleepTest(50e3 + 1); // 50us+1ns
    (void)SleepTest(60e3);     // 60us
    (void)SleepTest(65e3);     // 65us
    (void)SleepTest(5e6);      // 5ms
    (void)SleepTest(10e6);     // 10ms
    (void)SleepTest(10e6 + 1); // 10ms+1ns
    (void)SleepTest(25e6);     // 25ms
    (void)SleepTest(1e9);      // 1s

    return NULL;
}

static int ClockTest(void)
{
    int ret;
    pthread_t thread;
    struct sched_param param = { 0 };
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(&thread, &attr, ClockTestThread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(thread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_failCnt, 0, g_failCnt);
    return 0;
}

void ClockTest010(void)
{
    TEST_ADD_CASE(__FUNCTION__, ClockTest, TEST_POSIX, TEST_TIMES, TEST_LEVEL0, TEST_FUNCTION);
}
