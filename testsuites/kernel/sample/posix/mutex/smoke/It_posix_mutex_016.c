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


/* pthread_mutexattr_getprioceiling 1-2.c
 * Test that pthread_mutexattr_getprioceiling()
 *
 * Gets the priority ceiling attribute of a mutexattr object (which was prev. created
 * by the function pthread_mutexattr_init()).
 *
 * Steps:
 * 1.  Initialize a pthread_mutexattr_t object with pthread_mutexattr_init()
 * 2.  Get the min and max boundries for SCHED_FIFO of what prioceiling can be.
 * 3.  In a for loop, go through each valid SCHED_FIFO value, set the prioceiling, then
 * get the prio ceiling.  These should always be the same.  If not, fail the test.
 *
 */
static UINT32 Testcase(VOID)
{
    pthread_mutexattr_t mta;
    int prioceiling, maxPrio, minPrio, i;
    int rc;

    /* Initialize a mutex attributes object */
    rc = pthread_mutexattr_init(&mta);
    ICUNIT_ASSERT_EQUAL(rc, ENOERR, rc);

    /* Get the max and min prio according to SCHED_FIFO (posix scheduling policy) */
    maxPrio = OS_TASK_PRIORITY_HIGHEST; // 0
    minPrio = OS_TASK_PRIORITY_LOWEST;  // 31

    for (i = maxPrio; (i < minPrio + 1); i++) {
        /* Set the prioceiling to a priority number in the boundries
         * of SCHED_FIFO policy */
        rc = pthread_mutexattr_setprioceiling(&mta, i);
        ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT);

        /* Get the prioceiling mutex attr. */
        rc = pthread_mutexattr_getprioceiling(&mta, &prioceiling);
        ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT);

        /* Make sure that prioceiling is within the legal SCHED_FIFO boundries. */
        if (prioceiling != i) {
            rc = pthread_mutexattr_destroy(&mta);
            ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT);
            return LOS_NOK;
        }
    }
    rc = pthread_mutexattr_destroy(&mta);
    ICUNIT_GOTO_EQUAL(rc, ENOERR, rc, EXIT);

    return LOS_OK;

EXIT:
    pthread_mutexattr_destroy(&mta);
    return LOS_OK;
}


VOID ItPosixMux016(void)
{
    TEST_ADD_CASE("ItPosixMux016", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
