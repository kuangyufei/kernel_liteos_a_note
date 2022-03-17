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
    (void)sig;
    printf("%s%d\n", __FUNCTION__, __LINE__);
    return;
}

static void SigPrint1(int sig)
{
    (void)sig;
    printf("%s%d\n", __FUNCTION__, __LINE__);
    return;
}

static int g_sigCount = 0;
static void SigPrint2(int sig)
{
    (void)sig;
    g_sigCount = 1;
    printf("%s, count = %d\n", __FUNCTION__, g_sigCount);
    return;
}

static void *ThreadSetFunc2(void *arg)
{
    (void)arg;
    void (*retSig)(int);
    retSig = signal(SIGALRM, SigPrint2);
    ICUNIT_GOTO_NOT_EQUAL(retSig, SIG_ERR, retSig, EXIT);
    pthread_exit((void *)NULL);
    return NULL;
EXIT:
    pthread_exit((void *)-1);
    return (void *)-1;
}

static void *ThreadSetDfl(void *arg)
{
    (void)arg;
    void (*retSig)(int);
    retSig = signal(SIGALRM, SIG_DFL);
    ICUNIT_GOTO_NOT_EQUAL(retSig, SIG_ERR, retSig, EXIT);
    pthread_exit((void *)NULL);
    return NULL;
EXIT:
    pthread_exit((void *)-1);
    return (void *)-1;
}

static void *ThreadKill(void *arg)
{
    (void)arg;
    int retValue;

    retValue = raise(SIGALRM);
    ICUNIT_GOTO_EQUAL(retValue, 0, retValue, EXIT);
    pthread_exit((void *)NULL);
    return NULL;
EXIT:
    pthread_exit((void *)-1);
    return (void *)-1;
}

static int TestSigMultiPthread(void)
{
    int fpid;
    int status;
    int *status1 = nullptr;
    int ret;
    int count;
    pthread_t thread, thread1, thread2;

    fpid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        ret = pthread_create(&thread1, NULL, ThreadSetDfl, 0);
        if (ret != 0) {
            exit(ret);
        }
        ret = pthread_create(&thread2, NULL, ThreadSetFunc2, 0);
        if (ret != 0) {
            exit(ret);
        }
        ret = pthread_create(&thread, NULL, ThreadKill, 0);
        if (ret != 0) {
            exit(ret);
        }

        pthread_join(thread, (void **)&status1);
        if ((int)(intptr_t)status1 != 0) {
            exit(-1);
        }
        pthread_join(thread1, (void **)&status1);
        if ((int)(intptr_t)status1 != 0) {
            exit(-1);
        }
        pthread_join(thread2, (void **)&status1);
        if ((int)(intptr_t)status1 != 0) {
            exit(-1);
        }
        if (g_sigCount != 1) {
            exit(g_sigCount);
        }
        exit(0);
    }

    ret = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, fpid, ret);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    return 0;
}

void ItPosixSignal013(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestSigMultiPthread, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
