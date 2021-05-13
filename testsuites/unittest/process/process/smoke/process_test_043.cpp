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

static int Testcase(VOID)
{
    int priority;
    int err;
    int ret;
    int currProcessPri = getpriority(PRIO_PROCESS, getpid());
    int count = 0;
    priority = getpriority(PRIO_PGRP, 0);
    struct sched_param param = { 0 };
    err = errno;

    priority = getpriority(PRIO_PGRP, 0);
    err = errno;
    ICUNIT_ASSERT_EQUAL(priority, -1, priority);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    priority = getpriority(PRIO_USER, 0);
    err = errno;
    ICUNIT_ASSERT_EQUAL(priority, -1, priority);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    for (count = 1; count < 1000; ++count) { // 1000, Number of cycles.
        priority = getpriority(PRIO_USER + count, 0);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        priority = setpriority(PRIO_USER + count, getpid(), currProcessPri);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        priority = getpriority(-count, 0);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        priority = setpriority(-count, getpid(), currProcessPri);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        priority = sched_get_priority_max(SCHED_RR - count);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        priority = sched_get_priority_max(SCHED_RR + count);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        priority = sched_get_priority_min(SCHED_RR - count);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        priority = sched_get_priority_min(SCHED_RR + count);
        err = errno;
        ICUNIT_ASSERT_EQUAL(priority, -1, priority);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        ret = sched_setscheduler(getpid(), SCHED_RR - count, &param);
        err = errno;
        ICUNIT_ASSERT_EQUAL(ret, -1, ret);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

        ret = sched_setscheduler(getpid(), SCHED_RR + count, &param);
        err = errno;
        ICUNIT_ASSERT_EQUAL(ret, -1, ret);
        ICUNIT_ASSERT_EQUAL(err, EINVAL, err);
    }

    priority = setpriority(PRIO_PGRP, getpid(), currProcessPri);
    err = errno;
    ICUNIT_ASSERT_EQUAL(priority, -1, priority);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    priority = setpriority(PRIO_USER, getpid(), currProcessPri);
    err = errno;
    ICUNIT_ASSERT_EQUAL(priority, -1, priority);
    ICUNIT_ASSERT_EQUAL(err, EINVAL, err);

    return 0;
}

void ItTestProcess043(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_043", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
