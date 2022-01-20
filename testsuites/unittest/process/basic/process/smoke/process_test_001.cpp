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

#include "it_test_process.h"
 
static const int PROCESS_PRIORITY_MAX = 10;
static const int PROCESS_PRIORITY_MIN = 31;
static const int PROCESS_SCHED_RR_INTERVAL = 20000000;

static int Testcase(VOID)
{
    int ret;
    struct sched_param param = { 0 };
    struct timespec ts = { 0 };
    int err;
    ret = sched_getparam(getpid(), NULL);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_getparam(-1, &param);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_setparam(getpid(), NULL);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_setparam(-1, &param);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    param.sched_priority = PROCESS_PRIORITY_MIN + 1;
    ret = sched_setparam(getpid(), &param);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    param.sched_priority = 15; // 15, set pthread priority.
    ret = sched_setparam(60, &param); // 60, set the param.
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, ESRCH, err);

    ret = getpriority(PRIO_USER, getpid());
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_get_priority_min(SCHED_RR);
    ICUNIT_ASSERT_EQUAL(ret, PROCESS_PRIORITY_MAX, ret);

    ret = sched_get_priority_max(SCHED_FIFO);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_get_priority_max(SCHED_OTHER);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_get_priority_max(SCHED_RR);
    ICUNIT_ASSERT_EQUAL(ret, PROCESS_PRIORITY_MIN, ret);

    ret = sched_get_priority_min(SCHED_OTHER);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_get_priority_min(SCHED_FIFO);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    param.sched_priority = PROCESS_PRIORITY_MAX - 1;
    ret = sched_setparam(getpid(), &param);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    param.sched_priority = PROCESS_PRIORITY_MAX - 1;
    ret = sched_setscheduler(getpid(), SCHED_RR, &param);
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    param.sched_priority = 11; // 11, set pthread priority.
    ret = sched_setscheduler(1000, SCHED_RR, &param); // 1000, input the pid.
    err = errno;
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    param.sched_priority = PROCESS_PRIORITY_MAX - 1;
    ret = setpriority(PRIO_PROCESS, getpid(), param.sched_priority);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    err = errno;
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_getscheduler(getpid());
    ICUNIT_ASSERT_EQUAL(ret, SCHED_RR, ret);

    ret = sched_getscheduler(10000); // 10000, input the pid.
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    err = errno;
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    ret = sched_getscheduler(-1);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    err = errno;
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    param.sched_priority = PROCESS_PRIORITY_MAX - 1;
    ret = sched_setscheduler(getpid(), SCHED_FIFO, &param);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    err = errno;
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    return 0;

ERROR_OUT:
    return -1;
}

void ItTestProcess001(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_001", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
