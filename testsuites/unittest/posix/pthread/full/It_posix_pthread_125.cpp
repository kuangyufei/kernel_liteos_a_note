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

static VOID *pthread_f01(VOID *num)
{
    int *i, j;

    i = (int *)num;

    for (j = 0; j < PTHREAD_THREADS_NUM; j++) {
        printf("Passed argument %d for thread\n", i[j]);
        ICUNIT_TRACK_EQUAL(i[j], j + 1, i[j]);
    }

    pthread_exit(0);

    return NULL;
}

static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    INT32 i[PTHREAD_THREADS_NUM], j, ret;

    for (j = 0; j < PTHREAD_THREADS_NUM; j++)
        i[j] = j + 1;

    ret = pthread_create(&newTh, NULL, pthread_f01, (void *)&i);
    ICUNIT_GOTO_EQUAL(ret, PTHREAD_NO_ERROR, ret, EXIT);

    /* Wait for thread to end execution */
    ret = pthread_join(newTh, NULL);
    ICUNIT_GOTO_EQUAL(ret, PTHREAD_NO_ERROR, ret, EXIT);

    return PTHREAD_NO_ERROR;
EXIT:
    pthread_detach(newTh);
    return PTHREAD_NO_ERROR;
}


VOID ItPosixPthread125(VOID)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_125", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
