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
#include <osTest.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include "lt_timer_test.h"

#define SIG SIGALRM
#define CLOCKID CLOCK_REALTIME

static int SetTimerTest(void)
{
    int ret = 0;
    int sig = 0;
    int failed = 0;
    timer_t timerid;
    sigset_t set, oldSet;
    struct sigevent sev;

    ret = sigemptyset(&set);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sigaddset(&set, SIG);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sigprocmask(SIG_BLOCK, &set, &oldSet);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIG;
    sev.sigev_value.sival_ptr = &timerid;
    ret = timer_create(CLOCKID, &sev, &timerid);
    LogPrintln("timer_create %p: %d", timerid, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    struct timespec testcases[] = {
        {0, 30000000},
        {1, 0},
        {1, 5},
        {1, 5000},
        {1, 30000000},
        {2, 0},
    }, zero = {0, 0};

    for (int i = 0; i < sizeof(testcases) / sizeof(testcases[0]); ++i) {
        struct timespec start, end;
        struct itimerspec its;
        int64_t expected, escaped;

        its.it_interval = zero;
        its.it_value = testcases[i];

        ret = clock_gettime(CLOCKID, &start);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = timer_settime(timerid, 0, &its, nullptr);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = sigwait(&set, &sig);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = clock_gettime(CLOCKID, &end);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        expected = its.it_value.tv_sec * (int64_t)1e9 + its.it_value.tv_nsec;
        escaped = end.tv_sec * (int64_t)1e9 + end.tv_nsec - start.tv_sec * (int64_t)1e9 - start.tv_nsec;

        failed += (escaped < expected || (escaped - expected) >= 20000000); // 20000000, 2 ticks.
    }

    ret = timer_delete(timerid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sigprocmask(SIG_SETMASK, &oldSet, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(failed, 0, failed);

    return 0;
}

void TimerTest003(void)
{
    TEST_ADD_CASE(__FUNCTION__, SetTimerTest, TEST_POSIX, TEST_SWTMR, TEST_LEVEL0, TEST_FUNCTION);
}
