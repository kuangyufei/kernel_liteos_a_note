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
#include "it_pthread_test.h"
#include <time.h>

extern int nanosleep(const struct timespec *req, struct timespec *rem);

static pthread_mutex_t g_mux = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_testAtforkCount = 0;
static int g_testAtforkPrepare = 0;
static int g_testAtforkParent = 0;

static void *Doit(void *arg)
{
    int err;
    struct timespec ts = { 2, 0 }; // 2, set time 2s.

    ICUNIT_GOTO_EQUAL(g_testAtforkCount, 1, g_testAtforkCount, EXIT);

    err = pthread_mutex_lock(&g_mux);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT);

    (void)nanosleep(&ts, NULL);
    err = pthread_mutex_unlock(&g_mux);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT);

EXIT:
    return NULL;
}

static void *Doit1(void *arg)
{
    int err;
    struct timespec ts = { 2, 0 }; // 2, set time 2s.

    ICUNIT_GOTO_EQUAL(g_testAtforkCount, 1, g_testAtforkCount, EXIT);

    err = pthread_mutex_lock(&g_mux);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT);

    (void)nanosleep(&ts, NULL);
    err = pthread_mutex_unlock(&g_mux);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT);

EXIT:
    return NULL;
}

static void Prepare(void)
{
    int err;

    err = pthread_mutex_unlock(&g_mux);
    ICUNIT_ASSERT_EQUAL_VOID(err, 0, err);
    g_testAtforkPrepare++;
}

static void Parent(void)
{
    int err = pthread_mutex_lock(&g_mux);
    ICUNIT_ASSERT_EQUAL_VOID(err, 0, err);
    g_testAtforkParent++;
}

static void *PthreadAtforkTest(void *arg)
{
    int err, ret;
    int pid;
    int status = 0;
    struct timespec ts = { 1, 0 };
    pthread_t tid;

    g_testAtforkCount = 0;
    g_testAtforkPrepare = 0;
    g_testAtforkParent = 0;

    err = pthread_atfork(Prepare, Parent, NULL);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT);
    g_testAtforkCount++;
    err = pthread_create(&tid, NULL, Doit, NULL);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT);

    nanosleep(&ts, NULL);
    pid = fork();
    ICUNIT_GOTO_EQUAL(g_testAtforkPrepare, 1, g_testAtforkPrepare, EXIT);
    if (pid == 0) {
        Doit1(NULL);
        exit(10); // 10, set exit status.
    }

    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT_WAIT); // 100000, The pid will never exceed 100000.
    ICUNIT_GOTO_EQUAL(g_testAtforkParent, 1, g_testAtforkParent, EXIT_WAIT);

    err = pthread_join(tid, NULL);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT_WAIT);


    err = waitpid(pid, &status, 0);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(err, pid, err, EXIT);
    ICUNIT_GOTO_EQUAL(status, 10, status, EXIT); // 10, get exit status.

EXIT:
    return NULL;

EXIT_WAIT:
    (void)waitpid(pid, 0, 0);
    return NULL;
}

static int Testcase(void)
{
    int ret;
    pthread_t newPthread;
    int curThreadPri, curThreadPolicy;
    pthread_attr_t a = { 0 };
    struct sched_param param = { 0 };

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    curThreadPri = param.sched_priority;

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = curThreadPri + 2; // 2, adjust the priority.
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, PthreadAtforkTest, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestPthreadAtfork002(void)
{
    TEST_ADD_CASE("IT_PTHREAD_ATFORK_002", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
