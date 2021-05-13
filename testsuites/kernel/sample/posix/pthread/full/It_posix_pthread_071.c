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

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static UINT32 Testcase(VOID)
{
    pthread_condattr_t condattr;
    int rc;
    pthread_cond_t cond1, cond2;
    pthread_cond_t cond3 = PTHREAD_COND_INITIALIZER;

    /* Initialize a condition variable attribute object */
    rc = pthread_condattr_init(&condattr);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Initialize cond1 with the default condition variable attribute */
    rc = pthread_cond_init(&cond1, &condattr);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Initialize cond2 with NULL attributes */
    rc = pthread_cond_init(&cond2, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Destroy the condition variable attribute object */
    rc = pthread_condattr_destroy(&condattr);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Destroy cond1 */
    rc = pthread_cond_destroy(&cond1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Destroy cond2 */
    rc = pthread_cond_destroy(&cond2);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Destroy cond3 */
    rc = pthread_cond_destroy(&cond3);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread071(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread071", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */