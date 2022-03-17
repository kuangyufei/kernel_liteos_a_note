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

#include "It_posix_mutex.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */


struct _scenar_028 {
    int m_type;    /* Mutex type to use */
    int m_pshared; /* 0: mutex is process-private (default) ~ !0: mutex is process-shared, if supported */
    char *descr;   /* Case description */
} g_scenarii028[] = {
    {PTHREAD_MUTEX_DEFAULT, 0, "Default mutex"},
    {PTHREAD_MUTEX_NORMAL, 0, "Normal mutex"},
    {PTHREAD_MUTEX_ERRORCHECK, 0, "Errorcheck mutex"},
    {PTHREAD_MUTEX_RECURSIVE, 0, "Recursive mutex"},
    {PTHREAD_MUTEX_DEFAULT, 1, "Pshared mutex"},
    {PTHREAD_MUTEX_NORMAL, 1, "Pshared Normal mutex"},
    {PTHREAD_MUTEX_ERRORCHECK, 1, "Pshared Errorcheck mutex"},
    {PTHREAD_MUTEX_RECURSIVE, 1, "Pshared Recursive mutex"}
};

#define NSCENAR (sizeof(g_scenarii028) / sizeof(g_scenarii028[0]))

static UINT32 Testcase(VOID)
{
    int ret;
    int i, j;
    pthread_mutex_t mtx;
    pthread_mutexattr_t ma[NSCENAR + 1];
    pthread_mutexattr_t *pma[NSCENAR + 2];
    long pshared;

    pshared = sysconf(_SC_THREAD_PROCESS_SHARED);

    for (i = 0; i < NSCENAR; i++) {
        ret = pthread_mutexattr_init(&ma[i]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    }

    ret = pthread_mutexattr_init(&ma[i]);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    for (i = 0; i < NSCENAR + 2; i++) // 2, The loop frequency.
        /* NULL pointer */
        pma[i] = NULL;

    for (i = 0; i < NSCENAR + 2; i++) { // 2, The loop frequency.
        for (j = 0; j < NSCENAR + 2; j++) { // 2, The loop frequency.
            ret = pthread_mutex_init(&mtx, pma[i]);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);

            ret = pthread_mutex_destroy(&mtx);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);

            ret = pthread_mutex_init(&mtx, pma[j]);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);

            ret = pthread_mutex_destroy(&mtx);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        }
    }
    for (i = 0; i < NSCENAR + 1; i++) {
        ret = pthread_mutexattr_destroy(&ma[i]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    }

    return LOS_OK;
}


VOID ItPosixMux026(void)
{
    TEST_ADD_CASE("ItPosixMux026", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
