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

static void *ThreadF01(void *arg)
{
    pthread_exit(NULL);
    return NULL;
}
static UINT32 Testcase(VOID)
{
    pthread_t aThread;
    pthread_t ptid = pthread_self();
    pthread_t a = 0;
    pthread_t b = 0;
    int tmp;
    pthread_attr_t aa = { 0 };
    int detachstate;
    pthread_mutex_t c;
    UINT32 ret;

    pthread_create(&aThread, NULL, ThreadF01, NULL);

    tmp = pthread_equal(a, b);

    // pthread_join(a, NULL);

    // pthread_detach(a);

    pthread_attr_init(&aa);

    pthread_attr_getdetachstate(&aa, &detachstate);

    pthread_attr_setdetachstate(&aa, PTHREAD_CREATE_DETACHED);

    pthread_attr_destroy(&aa);

    // pthread_mutex_init(&c, NULL);

    // pthread_mutex_destroy(&c);

    // pthread_mutex_lock(&c);

    // pthread_mutex_trylock(&c);

    // pthread_mutex_unlock(&c);

    // pthread_mutexattr_init(&c);

    // pthread_mutexattr_destroy(&c);
    ret = pthread_join(aThread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread003(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_003", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL0, TEST_FUNCTION);
}
