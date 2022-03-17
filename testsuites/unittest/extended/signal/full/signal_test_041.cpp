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
#include "it_test_signal.h"
#include "signal.h"

#include "pthread.h"

static void Sigprint(int sig)
{
    (void)sig;
    printf("enter sighandle : -------------------------\n");
}
static void Sigprint111(int sig)
{
    (void)sig;
    printf("enter sighandle : ---111----------------------\n");
}

static void *ThreadFunc7(void *arg)
{
    (void)arg;
    int retval;
    sigset_t set, oldset;
    sighandler_t sigret;
    printf("-----------------------------\n");
    sleep(1);
    raise(SIGUSR2);
    sleep(1);
    sigret = signal(SIGUSR2, Sigprint);

    sleep(1);
    raise(SIGUSR2);
    retval = sigemptyset(&set);
    if (retval == -1) {
        exit(-1);
    }
    printf("line = %d\n", __LINE__);
    retval = sigemptyset(&oldset);
    if (retval == -1) {
        exit(-1);
    }
    printf("line = %d\n", __LINE__);
    retval = sigprocmask(SIG_SETMASK, &set, &oldset);
    if (retval == -1) {
        exit(-1);
    }
    retval = sigismember(&oldset, SIGTERM);
    printf("th SIGTERM is in oldset:%d\n", retval);
    if (retval != 1) {
        exit(-1);
    }
    retval = sigismember(&oldset, SIGUSR1);
    printf("th SIGUSR1 is in oldset:%d\n", retval);
    if (retval != 1) {
        exit(-1);
    }
    retval = sigismember(&oldset, SIGUSR2);
    printf("th SIGUSR2 is in oldset:%d\n", retval);
    if (retval != 0) {
        exit(-1);
    }
    retval = sigprocmask(SIG_SETMASK, &oldset, &set);
    if (retval == -1) {
        exit(-1);
    }
    sleep(1);
    printf("-----------------------------\n");
    retval = sigemptyset(&set);
    if (retval == -1) {
        exit(-1);
    }
    retval = sigemptyset(&oldset);
    if (retval == -1) {
        exit(-1);
    }
    retval = sigprocmask(SIG_SETMASK, &set, &oldset);
    if (retval == -1) {
        exit(-1);
    }
    retval = sigismember(&oldset, SIGTERM);
    printf("SIGTERM is in oldset:%d\n", retval);
    if (retval != 1) {
        exit(-1);
    }
    retval = sigismember(&oldset, SIGUSR1);
    printf("SIGUSR1 is in oldset:%d\n", retval);
    if (retval != 1) {
        exit(-1);
    }
    retval = sigismember(&oldset, SIGUSR2);
    printf("SIGUSR2 is in oldset:%d\n", retval);
    if (retval != 0) {
        exit(-1);
    }

    printf("-----------------newthread7 is finished------------\n");
    return nullptr;
}

static UINT32 TestCase(VOID)
{
    int ret, retval;
    void *res = nullptr;
    pthread_t newPthread, newPthread1;
    sigset_t set, oldset;
    sighandler_t sigret;
    sigret = signal(SIGUSR2, Sigprint111);
    retval = sigemptyset(&set);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    retval = sigemptyset(&oldset);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    retval = sigaddset(&set, SIGTERM);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    retval = sigismember(&set, SIGTERM);
    printf("SIGTERM is in set:%d\n", retval);
    ICUNIT_ASSERT_EQUAL(retval, 1, retval);
    retval = sigaddset(&set, SIGUSR1);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    retval = sigismember(&set, SIGUSR1);
    printf("SIGUSR1 is in set:%d\n", retval);
    ICUNIT_ASSERT_EQUAL(retval, 1, retval);
    retval = sigismember(&set, SIGUSR2);
    ICUNIT_ASSERT_EQUAL(retval, 0, retval);
    printf("SIGUSR2 is in set:%d\n", retval);
    retval = sigprocmask(SIG_SETMASK, &set, &oldset);
    printf("line = %d\n", __LINE__);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    printf("line = %d\n", __LINE__);
    raise(SIGUSR2);
    printf("line = %d\n", __LINE__);
    sleep(1);
    ret = pthread_create(&newPthread1, nullptr, ThreadFunc7, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    printf("line = %d\n", __LINE__);
    sleep(1);
    raise(SIGUSR2);
    retval = sigaddset(&set, SIGUSR2);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    printf("line = %d\n", __LINE__);
    retval = sigdelset(&set, SIGUSR1);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    printf("line = %d\n", __LINE__);
    retval = sigismember(&set, SIGTERM);
    printf("f SIGTERM is in set:%d\n", retval);
    ICUNIT_ASSERT_EQUAL(retval, 1, retval);
    retval = sigismember(&set, SIGUSR1);
    printf("f SIGUSR1 is in set:%d\n", retval);
    ICUNIT_ASSERT_EQUAL(retval, 0, retval);
    retval = sigismember(&set, SIGUSR2);
    printf("f SIGUSR2 is in set:%d\n", retval);
    ICUNIT_ASSERT_EQUAL(retval, 1, retval);
    retval = sigprocmask(SIG_SETMASK, &set, nullptr);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }
    ret = pthread_join(newPthread1, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    retval = sigprocmask(SIG_SETMASK, &oldset, &set);
    if (retval == -1) {
        printf("error: sigemptyset \n");
        return -1;
    }

    printf("-----------------main is finished------------\n");

    return 0;
}

void ItPosixSignal041(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
