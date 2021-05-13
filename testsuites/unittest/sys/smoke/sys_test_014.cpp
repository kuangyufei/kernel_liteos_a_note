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
#include "It_test_sys.h"

static void ShowFeExceptions(void)
{
    printf("current exceptions raised: ");
    if (fetestexcept(FE_DIVBYZERO)) {
        printf(" FE_DIVBYZERO");
    }
    if (fetestexcept(FE_INEXACT)) {
        printf(" FE_INEXACT");
    }
    if (fetestexcept(FE_INVALID)) {
        printf(" FE_INVALID");
    }
    if (fetestexcept(FE_OVERFLOW)) {
        printf(" FE_OVERFLOW");
    }
    if (fetestexcept(FE_UNDERFLOW)) {
        printf(" FE_UNDERFLOW");
    }
    if (fetestexcept(FE_ALL_EXCEPT) == 0) {
        printf(" none");
    }
    printf("\n");
}

static double X2(double x) /* times two */
{
    fenv_t currExcepts;

    /* Save and clear current f-p environment. */
    feholdexcept(&currExcepts);

    /* Raise inexact and overflow exceptions. */
    printf("In x2():  x = %f\n", x = x * 2.0);
    ShowFeExceptions();
    feclearexcept(FE_INEXACT); /* hide inexact exception from caller */

    /* Merge caller's exceptions (FE_INVALID)        */
    /* with remaining x2's exceptions (FE_OVERFLOW). */
    feupdateenv(&currExcepts);
    return x;
}

static UINT32 TestCase(VOID)
{
    feclearexcept(FE_ALL_EXCEPT);
    feraiseexcept(FE_INVALID); /* some computation with invalid argument */
    ShowFeExceptions();
    printf("x2(DBL_MAX) = %f\n", X2(DBL_MAX));
    ShowFeExceptions();

    return 0;
EXIT:
    return -1;
}


VOID ItTestSys014(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
