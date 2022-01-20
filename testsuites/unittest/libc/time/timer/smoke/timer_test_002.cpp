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
#include "lt_timer_test.h"

static int g_almHandlerFlag;
static void SigAlmHandler(int sig)
{
    g_almHandlerFlag++;
}

static int TimerTest(void)
{
    struct itimerval itv;
    int ret;

    ret = getitimer(ITIMER_REAL, &itv);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    itv.it_value.tv_sec = 2; // 2, seconds.
    itv.it_value.tv_usec = 500000; // 500000, 500ms.
    itv.it_interval.tv_sec = 1;
    itv.it_interval.tv_usec = 500000; // 500000, 500ms.
    (void)signal(SIGALRM, SigAlmHandler);

    ret = setitimer(ITIMER_REAL, &itv, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LogPrintln("sleep %ds", 3); // 3, this sleep may be interrupted by the timer
    sleep(3); // 3, this sleep may be interrupted by the timer
    LogPrintln("sleep end, almHandlerFlag %d\n", g_almHandlerFlag);
    ICUNIT_ASSERT_EQUAL(g_almHandlerFlag, 1, g_almHandlerFlag);

    LogPrintln("sleep %ds", 2); // 2, this sleep may be interrupted by the timer
    sleep(2); // 2, this sleep may be interrupted by the timer
    LogPrintln("sleep end, almHandlerFlag %d\n", g_almHandlerFlag);
    ICUNIT_ASSERT_EQUAL(g_almHandlerFlag, 2, g_almHandlerFlag); // 2, assert the g_almHandlerFlag.

    LogPrintln("sleep %ds", 2); // 2, this sleep may be interrupted by the timer
    sleep(2); // 2, this sleep may be interrupted by the timer
    LogPrintln("sleep end, almHandlerFlag %d\n", g_almHandlerFlag);
    ICUNIT_ASSERT_EQUAL(g_almHandlerFlag, 3, g_almHandlerFlag); // 3, assert the g_almHandlerFlag.

    ret = getitimer(ITIMER_REAL, &itv);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(itv.it_interval.tv_sec, 1, itv.it_interval.tv_sec);
    ICUNIT_ASSERT_EQUAL(itv.it_interval.tv_usec, 500000, itv.it_interval.tv_usec); // 500000, assert the tv_usec.

    /* stop this timer */
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 0;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    ret = setitimer(ITIMER_REAL, &itv, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void TimerTest002(void)
{
    TEST_ADD_CASE(__FUNCTION__, TimerTest, TEST_POSIX, TEST_SWTMR, TEST_LEVEL0, TEST_FUNCTION);
}
