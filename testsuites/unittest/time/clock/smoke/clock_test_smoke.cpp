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

static int ClockSmokeTest(void)
{
    clockid_t clk = CLOCK_REALTIME;
    struct timespec res = {0,0}, setts = {0,0}, oldtp = {0,0}, ts = {0,0};
    int ret;
    int passflag = 0;

    /* get clock resolution */
    ret = clock_getres(clk, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(res.tv_sec, CLOCK_RES_SEC, res.tv_sec);
    ICUNIT_ASSERT_EQUAL(res.tv_nsec, CLOCK_RES_NSEC, res.tv_nsec);

    /* get clock realtime */
    ret = clock_gettime(clk, &oldtp);
    printf("the clock current time: %lld second, %ld nanosecond\n", oldtp.tv_sec, oldtp.tv_nsec);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* set clock realtime */
    setts.tv_sec = oldtp.tv_sec + 1;
    setts.tv_nsec = oldtp.tv_nsec;
    printf("the clock setting time: %lld second, %ld nanosecond\n", setts.tv_sec, setts.tv_nsec);
    ret = clock_settime(CLOCK_REALTIME, &setts);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = clock_gettime(clk, &ts);
    printf("obtaining the current time after setting: %lld second, %ld nanosecond\n", ts.tv_sec, ts.tv_nsec);
    passflag = (ts.tv_sec >= setts.tv_sec) && (ts.tv_sec <= setts.tv_sec + 1); // 1, means obtaining time's errno is 1 second.
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(passflag, true, passflag);
    return 0;
}

void ClockTestSmoke(void)
{
    TEST_ADD_CASE(__FUNCTION__, ClockSmokeTest, TEST_POSIX, TEST_TIMES, TEST_LEVEL0, TEST_FUNCTION);
}

