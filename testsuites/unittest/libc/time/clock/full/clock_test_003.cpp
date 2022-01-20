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

static int ClockTest(void)
{
    clockid_t clockid;
    struct timespec ts;
    int ret;
    pid_t pid = 0;
    int passflag = 0;

    /* get kernel process 1 clockid */
    pid = 1;
    ret = clock_getcpuclockid(1, &clockid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(clockid, CLOCK_GET_CPU_CLOCKID(pid), clockid);

    /* get clockid time */
    ret = clock_gettime(clockid, &ts);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    printf("Obtaining the process running time: %lld second, %ld nanosecond\n", ts.tv_sec, ts.tv_nsec);

    /* get user process 2 clockid */
    pid = 2; // 2, test the process 2 (An existent pid)
    ret = clock_getcpuclockid(2, &clockid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(clockid, CLOCK_GET_CPU_CLOCKID(pid), clockid);

    /* get clockid time */
    ret = clock_gettime(clockid, &ts);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    printf("Obtaining the process running time: %lld second, %ld nanosecond\n", ts.tv_sec, ts.tv_nsec);

    /* get current process clockid */
    pid = getpid();
    ret = clock_getcpuclockid(getpid(), &clockid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(clockid, CLOCK_GET_CPU_CLOCKID(pid), clockid);

    /* get clockid time */
    ret = clock_gettime(clockid, &ts);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    printf("Obtaining the process running time: %lld second, %ld nanosecond\n", ts.tv_sec, ts.tv_nsec);

    return 0;
}

void ClockTest003(void)
{
    TEST_ADD_CASE(__FUNCTION__, ClockTest, TEST_POSIX, TEST_TIMES, TEST_LEVEL0, TEST_FUNCTION);
}
