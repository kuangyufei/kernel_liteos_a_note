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


/* pthread_mutex_getprioceiling 3-2.c
 * Test that pthread_mutex_getprioceiling() fails because:
 *
 * [EINVAL]
 * The protocol attribute of mutex is PTHREAD_PRIO_NONE.
 *
 * by explicitly specifying the protocol as PTHREAD_PRIO_NONE.
 *
 * Steps:
 * 1.  Initialize a pthread_mutexattr_t object with pthread_mutexattr_init()
 * 2.  Explicitly set the protocol using PTHREAD_PRIO_NONE.
 * 3.  Call pthread_mutex_getprioceiling() to obtain the prioceiling.
 *
 */
static UINT32 Testcase(VOID)
{
    pthread_mutexattr_t mutexAttr;
    pthread_mutex_t mutex = TEST_MUTEX_INIT;
    int err, prioceiling;

    err = pthread_mutexattr_init(&mutexAttr);
    ICUNIT_ASSERT_EQUAL(err, ENOERR, err);

    /*
     * The default protocol is PTHREAD_PRIO_NONE according to
     * pthread_mutexattr_getprotocol.
     */
    err = pthread_mutexattr_setprotocol(&mutexAttr, PTHREAD_PRIO_NONE);
    ICUNIT_ASSERT_EQUAL(err, ENOERR, err);

    /* Initialize a mutex object */
    err = pthread_mutex_init(&mutex, &mutexAttr);
    ICUNIT_GOTO_EQUAL(err, ENOERR, err, EXIT1);

    /* Get the prioceiling of the mutex. */
    err = pthread_mutex_getprioceiling(&mutex, &prioceiling);
    ICUNIT_GOTO_EQUAL(err, ENOERR, err, EXIT2);

    err = pthread_mutexattr_destroy(&mutexAttr);
    ICUNIT_GOTO_EQUAL(err, ENOERR, err, EXIT1);

    err = pthread_mutex_destroy(&mutex);
    ICUNIT_GOTO_EQUAL(err, ENOERR, err, EXIT2);

    return LOS_OK;

EXIT2:
    pthread_mutex_destroy(&mutex);

EXIT1:
    pthread_mutexattr_destroy(&mutexAttr);
    return LOS_OK;
}


VOID ItPosixMux034(void)
{
    TEST_ADD_CASE("ItPosixMux034", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */