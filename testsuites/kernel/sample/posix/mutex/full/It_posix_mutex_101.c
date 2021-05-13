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


static pthread_mutex_t g_mutexTest100 = { 0 };

static VOID TestMutexInit101(UINT8 type)
{
    int ret;
    pthread_mutexattr_t mta = { 0 };

    /* Initialize a mutex attributes object */
    ret = pthread_mutexattr_init(&mta);
    ICUNIT_ASSERT_EQUAL_VOID(ret, ENOERR, ret);

    mta.type = type;

    ret = pthread_mutex_init(&g_mutexTest100, &mta);
    ICUNIT_ASSERT_EQUAL_VOID(ret, 0, ret);

    (void)pthread_mutexattr_destroy(&mta);
}


static UINT32 Testcase(VOID)
{
    int ret;

    ICUNIT_ASSERT_EQUAL(OsCurrTaskGet()->priority, TASK_PRIO_TEST, OsCurrTaskGet()->priority);

    TestMutexInit101(PTHREAD_MUTEX_ERRORCHECK);

    ret = pthread_mutex_lock(&g_mutexTest100);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_mutex_setprioceiling(&g_mutexTest100, 21, NULL); // 21, task priority.
    ICUNIT_GOTO_EQUAL(ret, EDEADLK, ret, EXIT1);

    ret = pthread_mutex_unlock(&g_mutexTest100);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_mutex_destroy(&g_mutexTest100);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return LOS_OK;

EXIT1:
    ret = pthread_mutex_unlock(&g_mutexTest100);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

EXIT:
    ret = pthread_mutex_destroy(&g_mutexTest100);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return LOS_OK;
}

VOID ItPosixMux101(void)
{
    TEST_ADD_CASE("ItPosixMux101", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
