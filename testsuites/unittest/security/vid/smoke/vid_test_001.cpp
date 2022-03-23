/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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
#include "it_test_vid.h"
#include "stdio.h"
#include "unistd.h"
#include "sys/stat.h"
#include "time.h"
#include "stdlib.h"

static VOID TimerFunc(int sig)
{
    return;
}

static void ChildFunc()
{
    timer_t tid;
    struct sigevent ent;
    INT32 *ret1 = NULL;
    int ret;
    struct itimerspec its;
    struct itimerspec its2;
    timer_t *tid2 = nullptr;
    int i = 0;
    signal(SIGUSR1, TimerFunc);
    ent.sigev_notify = SIGEV_SIGNAL;
    ent.sigev_signo = SIGUSR1;

    tid2 = (timer_t *)malloc(sizeof(UINTPTR) * 1024);
    if (tid2 == NULL) {
        return;
    }
    ret1 = (int *)malloc(sizeof(int) * 1024);
    if (ret1 == NULL) {
        free(tid2);
        return;
    }
    (void)memset_s(tid2, sizeof(char *) * 1024, 0, sizeof(char *) * 1024);
    (void)memset_s(ret1, sizeof(int) * 1024, 0xff, sizeof(int) * 1024);
    while (i < 1024) {
        *(ret1 + i) = timer_create(CLOCK_REALTIME, &ent, tid2 + i);
        if (*(ret1 + i) == 0) {
            ICUNIT_ASSERT_EQUAL_VOID(*(ret1 + i), 0, *(ret1 + i));
            ICUNIT_ASSERT_EQUAL_VOID(i, (int)(*(tid2 + i)), i);
        } else {
            ICUNIT_ASSERT_EQUAL_VOID(*(ret1 + i), -1, *(ret1 + i));
        }
        i++;
    }

    i = 0;
    while (*(ret1 + i) == 0) {
        ret = timer_delete(*(tid2 + i));
        ICUNIT_ASSERT_EQUAL_VOID(ret, 0, ret);
        i++;
    }

    i = 0;
    while (i < 1024) {
        *(ret1 + i) = timer_create(CLOCK_REALTIME, &ent, tid2 + i);
        if (*(ret1 + i) == 0) {
            ICUNIT_ASSERT_EQUAL_VOID(*(ret1 + i), 0, *(ret1 + i));
            ICUNIT_ASSERT_EQUAL_VOID(i, (int)(*(tid2 + i)), i);
        } else {
            ICUNIT_ASSERT_EQUAL_VOID(*(ret1 + i), -1, *(ret1 + i));
        }
        i++;
    }

    i = 0;
    while (*(ret1 + i) == 0) {
        ret = timer_delete(*(tid2 + i));
        ICUNIT_ASSERT_EQUAL_VOID(ret, 0, ret);
        i++;
    }

    free(tid2);
    ret1 = (INT32 *)timer_create(CLOCK_REALTIME, &ent, &tid);
    ICUNIT_ASSERT_EQUAL_VOID(ret1, 0, ret1);

    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    ret1 = (INT32 *)timer_settime(tid, 0, &its, NULL);
    ICUNIT_ASSERT_EQUAL_VOID(ret1, 0, ret1);
    sleep(1);
    ret1 = (INT32 *)timer_gettime(tid, &its2);
    ICUNIT_ASSERT_EQUAL_VOID(ret1, 0, ret1);
    ret1 = (INT32 *)timer_getoverrun(tid);
    ret1 = (INT32 *)(((int)(intptr_t)ret1 >= 0) ? 0 : -1);
    ICUNIT_ASSERT_EQUAL_VOID(ret1, 0, ret1);

    ret1 = (INT32 *)timer_delete(tid);
    ICUNIT_ASSERT_EQUAL_VOID(ret1, 0, ret1);

    exit((int)(intptr_t)tid);
}

static UINT32 TestCase(VOID)
{
    int pid;
    int status = 0;
    pid = fork();
    if (pid == 0) {
        ChildFunc();
    }

    waitpid(pid, &status, 0);
    return 0;
}

VOID ItSecVid001(VOID)
{
    TEST_ADD_CASE("IT_SEC_VID_001", TestCase, TEST_POSIX, TEST_SEC, TEST_LEVEL0, TEST_FUNCTION);
}
