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

#include "It_posix_pthread.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

typedef struct {
    sigset_t mask;
    sigset_t pending;
} testdata_t;

/* Thread function; which will check the signal mask and pending signals */
static VOID *PthreadF01(VOID *arg)
{
    INT32 ret;
    testdata_t *td = (testdata_t *)arg;

    /* Obtain the signal mask of this thread. */
    ret = pthread_sigmask(SIG_SETMASK, NULL, &(td->mask));
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    /* Obtain the pending signals of this thread. It should be empty. */
    ret = sigpending(&(td->pending));
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    /* Signal we're done (especially in case of a detached thread) */
    do {
        ret = sem_post(&g_scenarii[g_testCount].sem);
    } while ((ret == -1) && (errno == EINTR));
    ICUNIT_TRACK_NOT_EQUAL(ret, -1, ret);

    return arg;
}

static UINT32 Testcase(VOID)
{
    INT32 ret;
    pthread_t child;
    testdata_t tdParent, tdThread;

    g_testCount = 0;

#if PTHREAD_SIGNAL_SUPPORT == 1

    /* Initialize thread attribute objects */
    ScenarInit();

    /* Initialize the signal state */
    ret = sigemptyset(&(tdParent.mask));
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = sigemptyset(&(tdParent.pending));
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    /* Add SIGCONT, SIGUSR1 and SIGUSR2 to the set of blocked signals */
    ret = sigaddset(&(tdParent.mask), SIGUSR1);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = sigaddset(&(tdParent.mask), SIGUSR2);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    /* Block those signals. */
    ret = pthread_sigmask(SIG_SETMASK, &(tdParent.mask), NULL);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    /* Raise those signals so they are now pending. */
    ret = raise(SIGUSR1);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = raise(SIGUSR2);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    /* Do the testing for each thread */
    for (g_testCount = 0; g_testCount < NSCENAR; g_testCount++) {
        /* (re)initialize thread signal sets */
        ret = sigemptyset(&(tdThread.mask));
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

        ret = sigemptyset(&(tdThread.pending));
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

        ret = pthread_create(&child, &g_scenarii[g_testCount].ta, PthreadF01, &tdThread);
        switch (g_scenarii[g_testCount].result) {
            case 0: /* Operation was expected to succeed */
                ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
                break;

            case 1: /* Operation was expected to fail */
                ICUNIT_ASSERT_NOT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
                break;

            case 2: /* 2, We did not know the expected result */
            default:
                break;
        }
        if (ret == 0) { /* The new thread is running */
            if (g_scenarii[g_testCount].detached == 0) {
                ret = pthread_join(child, NULL);
                ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
            } else {
                /* Just wait for the thread to terminate */
                do {
                    ret = sem_wait(&g_scenarii[g_testCount].sem);
                } while ((ret == -1) && (errno == EINTR));
                ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
            }

            /* The thread has terminated its work, so we can now control */
            ret = sigismember(&(tdThread.mask), SIGUSR1);
            ICUNIT_ASSERT_EQUAL(ret, 1, ret);

            ret = sigismember(&(tdThread.mask), SIGUSR2);
            ICUNIT_ASSERT_EQUAL(ret, 1, ret);

            ret = sigismember(&(tdThread.pending), SIGUSR1);
            ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

            ret = sigismember(&(tdThread.pending), SIGUSR2);
            ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
        }
    }

    ScenarFini();

#endif

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread127(VOID)
{
    TEST_ADD_CASE("ItPosixPthread127", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
