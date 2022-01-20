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

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    int oldstate;
    int oldstype;

    // pthread_setcancelstate(int new, int *old): if new > 2U, return EINVAL, else return 0.
    ret = pthread_setcancelstate(0, &oldstate);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_setcancelstate(3, &oldstate);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(oldstate, PTHREAD_CANCEL_ENABLE, oldstate);

    // pthread_setcanceltype(int new, int *old): if new > 1U, return EINVAL,else return 0.
    ret = pthread_setcanceltype(0, &oldstype);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_setcanceltype(3, &oldstype);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldstate);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(oldstate, PTHREAD_CANCEL_ASYNCHRONOUS, oldstate);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread021(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_021", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL0, TEST_FUNCTION);
}
