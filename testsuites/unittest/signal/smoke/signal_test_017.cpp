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

static void SigPrint(int sig)
{
    printf("%s\n", __FUNCTION__, __LINE__);
}

static void *ThreadSetFunc2(void *arg)
{
    int retValue;

    sigset_t set;
    int sig;
    retValue = sigemptyset(&set);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGALRM);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGUSR1);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    int count;

    retValue = sigwait(&set, &sig);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
}

static void *ThreadSetDfl(void *arg)
{
    int retValue;

    sigset_t set;
    int sig;
    retValue = sigemptyset(&set);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGALRM);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGUSR1);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    int count;

    retValue = sigwait(&set, &sig);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
}

static void *ThreadKill(void *arg)
{
    int retValue;

    sigset_t set;
    int sig;
    retValue = sigemptyset(&set);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGALRM);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGUSR1);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
    int count;

    retValue = sigwait(&set, &sig);
    ICUNIT_ASSERT_EQUAL_NULL(retValue, 0, retValue);
}

static int TestMultiPthreadFatherProcessExit()
{
    int status = 0;
    int retValue;
    int fpid, fpids;

    pthread_t thread, thread1, thread2;
    int father;

    fpid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        fpids = fork();
        ICUNIT_ASSERT_WITHIN_EQUAL(fpids, 0, UINT_MAX, fpids);
        if (fpids == 0) {
            retValue = pthread_create(&thread, NULL, ThreadKill, 0);
            if (retValue != 0) {
                exit(retValue);
            }
            retValue = pthread_create(&thread2, NULL, ThreadSetFunc2, 0);
            if (retValue != 0) {
                exit(retValue);
            }
            retValue = pthread_create(&thread1, NULL, ThreadSetDfl, 0);
            if (retValue != 0) {
                exit(retValue);
            }

            father = getppid();
            printf("fatherPid = %d\n", father);
            sleep(1);
            printf("child kill father = %d\n", father);
            retValue = kill(father, SIGKILL);
            if (retValue != 0) {
                exit(retValue);
            }
        }
        sleep(1);
        retValue = waitpid(fpids, &status, 0);
        // grandchild process kill father process, so child process is recovered by init process
        // child process doesn't receive termination singal from grandchild process
        // so waitpid() returns -1
        ICUNIT_ASSERT_EQUAL(retValue, -1, retValue);
        exit(1);
    }
    sleep(1);
    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WIFEXITED(status), 0, WIFEXITED(status));
    ICUNIT_ASSERT_EQUAL(WIFSIGNALED(status), 1, WIFSIGNALED(status));
    ICUNIT_ASSERT_EQUAL(WTERMSIG(status), SIGKILL, WTERMSIG(status));
    return 0;
}

void ItPosixSignal017(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestMultiPthreadFatherProcessExit, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
