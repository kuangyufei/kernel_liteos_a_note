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


static pthread_mutex_t g_mutex1, g_mutex2;

/* pthread_mutex_init 1-1.c
 * Test that pthread_mutex_init()
 *   initializes a mutex referenced by 'mutex' with attributes specified
 *   by 'attr'.  If 'attr' is NULL, the default mutex attributes are used.
 *   The effect shall be the same as passing the address of a default
 *   mutex attributes.

 * NOTE: There is no direct way to judge if two mutexes have the same effect,
 *       thus this test does not cover the statement in the last sentence.
 *
 */
static UINT32 Testcase(VOID)
{
    pthread_mutexattr_t mta;
    int rc;

    /* Initialize a mutex attributes object */
    rc = pthread_mutexattr_init(&mta);
    ICUNIT_ASSERT_EQUAL(rc, ENOERR, rc);

    /* Initialize mutex1 with the default mutex attributes */
    rc = pthread_mutex_init(&g_mutex1, &mta);
    ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT1);

    /* Initialize mutex2 with NULL attributes */
    rc = pthread_mutex_init(&g_mutex2, NULL);
    ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT2);

    rc = pthread_mutexattr_destroy(&mta);
    ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT1);

    rc = pthread_mutex_destroy(&g_mutex1);
    ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT2);

    rc = pthread_mutex_destroy(&g_mutex2);
    ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT3);

    return LOS_OK;

EXIT1:
    pthread_mutexattr_destroy(&mta);

EXIT2:
    pthread_mutex_destroy(&g_mutex1);

EXIT3:
    pthread_mutex_destroy(&g_mutex2);
    return LOS_OK;
}


VOID ItPosixMux018(void)
{
    TEST_ADD_CASE("ItPosixMux018", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */