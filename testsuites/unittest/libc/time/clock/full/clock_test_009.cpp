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
#include "lt_clock_test.h"
#include <osTest.h>

/* When clock time is changed, timers for a relative interval are unaffected,
 * but timers for an absolute point in time are affected.
 */
static int ClockTest(void)
{
    clockid_t clk = CLOCK_MONOTONIC_RAW;
    struct timespec res, tp, oldtp;
    int ret;
    int passflag = 0;

    /* get clock resolution */
    ret = clock_getres(clk, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(res.tv_sec, CLOCK_RES_SEC, res.tv_sec);
    ICUNIT_ASSERT_EQUAL(res.tv_nsec, CLOCK_RES_NSEC, res.tv_nsec);

    /* get current monotonic raw time */
    ret = clock_gettime(clk, &oldtp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    printf("The current monotonic raw time: sec is %lld, nsec is %ld\n", oldtp.tv_sec, oldtp.tv_nsec);

    tp.tv_sec = 5 * res.tv_sec; // 5, times the number of seconds.
    tp.tv_nsec = res.tv_nsec;

    /* set monotonic raw time */
    ret = clock_settime(clk, &tp);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(errno, EOPNOTSUPP, errno);

    printf("get monotonic raw time clock again\n");

    /* get current monotonic raw time again */
    ret = clock_gettime(clk, &tp);
    passflag = (tp.tv_sec >= oldtp.tv_sec) && (tp.tv_sec <= oldtp.tv_sec + 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(passflag, 1, passflag);
    printf("The current monotonic raw time again: sec is %lld, nsec is %ld\n", tp.tv_sec, tp.tv_nsec);

    return 0;
}

void ClockTest009(void)
{
    TEST_ADD_CASE(__FUNCTION__, ClockTest, TEST_POSIX, TEST_TIMES, TEST_LEVEL0, TEST_FUNCTION);
}
