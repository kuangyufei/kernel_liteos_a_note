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
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include "osTest.h"
#include <stdio.h>
#include <string.h>
#include "lt_timer_test.h"

static int g_sigHdlCnt01;
static int g_sigHdlCnt02;
static int g_sigHdlCnt03;

static void TempSigHandler(union sigval v)
{
    LogPrintln("This is TempSigHandler ...\r\n");
    (*(void(*)(void))(v.sival_ptr))();
}

static void TempSigHandler01(void)
{
    g_sigHdlCnt01++;
}

static void TempSigHandler02(void)
{
    g_sigHdlCnt02++;
}

static int TimerTest(void)
{
    timer_t timerid01, timerid02, timerid03;
    struct sigevent sev;
    struct itimerspec its;
    int ret;
    int i;

    (void)memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = TempSigHandler;
    sev.sigev_value.sival_ptr = (void *)TempSigHandler01;

    /* Start the timer */
    its.it_value.tv_sec = 3; // 3, timer time 3 seconds.
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    ret = timer_create(CLOCK_REALTIME, &sev, &timerid01);
    LogPrintln("timer_settime %p: %d", timerid01, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = timer_settime(timerid01, 0, &its, nullptr);
    LogPrintln("timer_create %p: %d", timerid01, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    its.it_value.tv_sec = 4; // 4, timer time 4 seconds.
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    sev.sigev_value.sival_ptr = (void *)TempSigHandler02;
    ret = timer_create(CLOCK_REALTIME, &sev, &timerid02);
    LogPrintln("timer_settime %p: %d", timerid02, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = timer_settime(timerid02, 0, &its, nullptr);
    LogPrintln("timer_settime %p: %d", timerid02, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    its.it_value.tv_sec = 5; // 5, timer time 5 seconds.
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    sleep(20); // 20, sleep seconds for timer.
    ret = timer_delete(timerid01);
    LogPrintln("timer_delete %p %d", timerid01, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = timer_delete(timerid02);
    LogPrintln("timer_delete %p %d", timerid02, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_NOT_EQUAL(g_sigHdlCnt01, 0, g_sigHdlCnt01);
    ICUNIT_ASSERT_NOT_EQUAL(g_sigHdlCnt02, 0, g_sigHdlCnt02);
    return 0;

}

void TimerTest005(void)
{
    TEST_ADD_CASE(__FUNCTION__, TimerTest, TEST_POSIX, TEST_SWTMR, TEST_LEVEL0, TEST_FUNCTION);
}
