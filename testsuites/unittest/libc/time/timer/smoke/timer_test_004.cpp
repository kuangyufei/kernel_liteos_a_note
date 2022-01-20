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
#include <osTest.h>
#include "lt_timer_test.h"

/* signo should be in the range SIGRTMIN to SIGRTMAX when SA_SIGINFO flag is set. */
#define SIG SIGRTMIN
#define CLOCKID CLOCK_REALTIME

static int g_handlerFlag;
static int g_tmrOverrun;

static void SigHandler(int sig, siginfo_t *si, void *uc)
{
    if (si == nullptr) {
        LogPrintln("[ERROR]sig %d, si %p, uc %p\n", sig, si, uc);
        return;
    }

#ifdef TEST_ON_LINUX
    timer_t timerid = *(timer_t *)si->si_value.sival_ptr;
#else // SA_SIGINFO not compatible with POSIX on HMOS
    timer_t timerid = *(timer_t *)si;
#endif

    g_tmrOverrun += timer_getoverrun(timerid);
    LogPrintln("signo %d handlerFlag %d timer %p overrun %d\n", sig, ++g_handlerFlag, timerid, g_tmrOverrun);
}

static int SigInfoTimerTest(void)
{
    int interval = 3; // 3 seconds
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    sigset_t mask;
    struct sigaction sa;
    int ret;

    /* Install handler for timer signal. */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SigHandler;
    sigemptyset(&sa.sa_mask);
    ret = sigaction(SIG, &sa, nullptr);
    LogPrintln("sigaction %d: %d", SIG, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Block timer signal */
    sigemptyset(&mask);
    sigaddset(&mask, SIG);
    ret = sigprocmask(SIG_BLOCK, &mask, nullptr);
    LogPrintln("sigprocmask setmask %d: %d", SIG, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIG;
    sev.sigev_value.sival_ptr = &timerid;
    ret = timer_create(CLOCKID, &sev, &timerid);
    LogPrintln("timer_create %p: %d", timerid, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Start the timer */
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 990000000; // 990000000, 0.99s
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    ret = timer_settime(timerid, 0, &its, nullptr);
    LogPrintln("timer_settime %p: %d", timerid, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Sleep for a while */
    LogPrintln("sleep %ds", interval);
    sleep(interval); // should not be interrupted

    /* Get the timer's time */
    ret = timer_gettime(timerid, &its);
    LogPrintln("timer_gettime %p: %d", timerid, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Get the timer's overruns */
    ret = timer_getoverrun(timerid);
    LogPrintln("timer_getoverrun %p: %d", timerid, ret);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    /* Unlock the timer signal */
    ret = sigprocmask(SIG_UNBLOCK, &mask, nullptr);
    LogPrintln("sigprocmask unblock %d: %d", SIG, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LogPrintln("sleep another %ds", interval);
    sleep(interval); // should be interrupted
    LogPrintln("sleep time over, g_handlerFlag = %d", g_handlerFlag);

    ret = timer_delete(timerid);
    LogPrintln("timer_delete %p %d", timerid, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_NOT_EQUAL(g_handlerFlag, 0, g_handlerFlag);
    ICUNIT_ASSERT_NOT_EQUAL(g_tmrOverrun, 0, g_tmrOverrun);

    return 0;
}

void TimerTest004(void)
{
    TEST_ADD_CASE(__FUNCTION__, SigInfoTimerTest, TEST_POSIX, TEST_SWTMR, TEST_LEVEL0, TEST_FUNCTION);
}
