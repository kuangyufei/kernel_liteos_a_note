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
#include "It_test_IO.h"
#include <langinfo.h>
#include <locale.h>

static UINT32 testcase(VOID)
{
    /* set the locale info */
    setenv("MUSL_LOCPATH", "/storage", 1);
    printf("getenv MUSL_LOCPATH=%s\n", getenv("MUSL_LOCPATH"));
    printf("Locale is: %s\n", setlocale(LC_TIME, "zh_CN.UTF-8"));

    setlocale(LC_CTYPE, "");
    setlocale(LC_NUMERIC, "");

    /* echo the nl_langinfo */
    printf("%s\n", nl_langinfo(CODESET));
    printf("%s\n", nl_langinfo(RADIXCHAR));
    printf("%s\n", nl_langinfo(D_T_FMT));
    printf("%s\n", nl_langinfo(D_FMT));
    printf("%s\n", nl_langinfo(T_FMT));
    printf("%s\n", nl_langinfo(DAY_1));
    printf("%s\n", nl_langinfo(DAY_7));
    printf("%s\n", nl_langinfo(ABDAY_1));
    printf("%s\n", nl_langinfo(ABDAY_7));
    printf("%s\n", nl_langinfo(MON_1));
    printf("%s\n", nl_langinfo(MON_12));
    printf("%s\n", nl_langinfo(ABMON_1));
    printf("%s\n", nl_langinfo(ABMON_12));
    printf("%s\n", nl_langinfo(THOUSEP));
    printf("%s\n", nl_langinfo(YESEXPR));
    printf("%s\n", nl_langinfo(NOEXPR));
    printf("%s\n", nl_langinfo(CRNCYSTR));

    /* set the locale info */
    setenv("MUSL_LOCPATH", "/storage", 1);
    printf("getenv MUSL_LOCPATH=%s\n", getenv("MUSL_LOCPATH"));
    printf("Locale is: %s\n", setlocale(LC_TIME, "en_US.UTF-8"));

    setlocale(LC_CTYPE, "");
    setlocale(LC_NUMERIC, "");

    /* echo the nl_langinfo */
    printf("%s\n", nl_langinfo(CODESET));
    printf("%s\n", nl_langinfo(RADIXCHAR));
    printf("%s\n", nl_langinfo(D_T_FMT));
    printf("%s\n", nl_langinfo(D_FMT));
    printf("%s\n", nl_langinfo(T_FMT));
    printf("%s\n", nl_langinfo(DAY_1));
    printf("%s\n", nl_langinfo(DAY_7));
    printf("%s\n", nl_langinfo(ABDAY_1));
    printf("%s\n", nl_langinfo(ABDAY_7));
    printf("%s\n", nl_langinfo(MON_1));
    printf("%s\n", nl_langinfo(MON_12));
    printf("%s\n", nl_langinfo(ABMON_1));
    printf("%s\n", nl_langinfo(ABMON_12));
    printf("%s\n", nl_langinfo(THOUSEP));
    printf("%s\n", nl_langinfo(YESEXPR));
    printf("%s\n", nl_langinfo(NOEXPR));
    printf("%s\n", nl_langinfo(CRNCYSTR));

    char *string = nl_langinfo(CRNCYSTR);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(string, NULL, string);

    return LOS_OK;
}

VOID IO_TEST_NL_LANGINFO_001(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
