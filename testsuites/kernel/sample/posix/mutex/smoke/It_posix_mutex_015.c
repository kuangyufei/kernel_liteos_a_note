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


/* pthread_mutexattr_getprioceiling 1-1.c
 * Test that pthread_mutexattr_getprioceiling()
 *
 * Gets the priority ceiling attribute of a mutexattr object (which was prev. created
 * by the function pthread_mutexattr_init()).
 *
 * Steps:
 * 1.  Initialize a pthread_mutexattr_t object with pthread_mutexattr_init()
 * 2.  Call pthread_mutexattr_getprioceiling() to obtain the prioceiling.
 *
 */
static UINT32 Testcase(VOID)
{
    pthread_mutexattr_t ma;
    int prioceiling, maxPrio, minPrio, ret;

    /* Initialize a mutex attributes object */
    ret = pthread_mutexattr_init(&ma);
    ICUNIT_ASSERT_EQUAL(ret, ENOERR, ret);

    ret = pthread_mutexattr_setprotocol(&ma, PTHREAD_PRIO_PROTECT);
    ICUNIT_GOTO_EQUAL(ret, ENOERR, ret, EXIT);

    /* Get the prioceiling mutex attr. */
    ret = pthread_mutexattr_getprioceiling(&ma, &prioceiling);
    ICUNIT_GOTO_EQUAL(ret, ENOERR, ret, EXIT);

    /* Get the max and min according to SCHED_FIFO */
    maxPrio = OS_TASK_PRIORITY_HIGHEST; // 0
    minPrio = OS_TASK_PRIORITY_LOWEST;  // 31

    /* Ensure that prioceiling is within legal limits. */
    if ((prioceiling > minPrio) || (prioceiling < maxPrio)) {
        ICUNIT_GOTO_EQUAL(1, 0, LOS_NOK, EXIT);
    }

    ret = pthread_mutexattr_destroy(&ma);
    ICUNIT_GOTO_EQUAL(ret, ENOERR, ret, EXIT);

    return LOS_OK;

EXIT:
    pthread_mutexattr_destroy(&ma);
    return LOS_OK;
}


VOID ItPosixMux015(void)
{
    TEST_ADD_CASE("ItPosixMux015", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */