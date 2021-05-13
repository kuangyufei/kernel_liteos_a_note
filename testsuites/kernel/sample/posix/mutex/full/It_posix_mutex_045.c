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

static pthread_mutex_t g_mutex045;

/* pthread_mutex_trylock 1-2.c
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 * This sample test aims to check the following assertion:
 *
 * The pthread_mutex_trylock() function locks the mutex object
 * when it is unlocked.

 * The steps are:
 *
 * -> For each kind of mutex,
 *   -> trylock the mutex. It shall suceed.
 *   -> trylock the mutex again. It shall fail (except in case of recursive mutex).
 *   -> create a new child (either thread or process)
 *      -> the new child trylock the mutex. It shall fail.
 *   -> undo everything.
 */
static VOID *TaskF01(void *argument)
{
    int ret;
    ret = pthread_mutex_trylock(&g_mutex045);
    ICUNIT_GOTO_EQUAL(ret, EBUSY, ret, EXIT);

    return NULL;

EXIT:
    return NULL;
}

static UINT32 Testcase(VOID)
{
    pthread_mutexattr_t mta;
    int ret;

    pthread_attr_t attr;
    pthread_t newTh;

    ret = pthread_mutexattr_init(&mta);
    ret = pthread_mutex_init(&g_mutex045, &mta);

    ret = pthread_mutex_trylock(&g_mutex045);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_trylock(&g_mutex045);
    ICUNIT_ASSERT_EQUAL(ret, EBUSY, ret);

    ret = PosixPthreadInit(&attr, 4); // 4, Set thread priority.
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh, &attr, TaskF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_unlock(&g_mutex045);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    pthread_mutex_destroy(&g_mutex045);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newTh, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return LOS_OK;
}


VOID ItPosixMux045(void)
{
    TEST_ADD_CASE("ItPosixMux045", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */