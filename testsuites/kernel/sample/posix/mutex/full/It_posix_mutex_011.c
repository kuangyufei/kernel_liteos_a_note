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


/* pthread_mutexattr_getprotocol 1-2.c
 * Test that pthread_mutexattr_getprotocol()
 *
 * Gets the protocol attribute of a mutexattr object (which was prev. created
 * by the function pthread_mutexattr_init()).
 *
 */
static UINT32 Testcase(VOID)
{
    pthread_mutexattr_t mta;
    int protocol, protcls[3], i;
    int rc;

    /* Initialize a mutex attributes object */
    rc = pthread_mutexattr_init(&mta);
    ICUNIT_ASSERT_EQUAL(rc, ENOERR, rc);

    protcls[0] = PTHREAD_PRIO_NONE;
    protcls[1] = PTHREAD_PRIO_INHERIT;
    protcls[2] = PTHREAD_PRIO_PROTECT; // 2, buffer index.

    for (i = 0; i < 3; i++) { // 3, The loop frequency.
        /* Set the protocol to one of the 3 valid protocols. */
        rc = pthread_mutexattr_setprotocol(&mta, protcls[i]);
        ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT);

        /* Get the protocol mutex attr. */
        rc = pthread_mutexattr_getprotocol(&mta, &protocol);
        ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT);

        /* Make sure that the protocol set is the protocl we get when calling
         * pthread_mutexattr_getprocol() */
        if (protocol != protcls[i]) {
            ICUNIT_GOTO_EQUAL(1, 0, LOS_NOK, EXIT);
        }
    }

    rc = pthread_mutexattr_destroy(&mta);
    ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT);
    return LOS_OK;

EXIT:
    pthread_mutexattr_destroy(&mta);
    return LOS_OK;
}


VOID ItPosixMux011(void)
{
    TEST_ADD_CASE("ItPosixMux011", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */