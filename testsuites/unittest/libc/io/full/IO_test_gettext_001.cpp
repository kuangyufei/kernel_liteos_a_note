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
#include <libintl.h>
#include <locale.h>

static UINT32 testcase1(VOID)
{
    char *s = nullptr;
    char *tmp = nullptr;

    /* set the locale. */
    setlocale(LC_ALL, "zh_CN.UTF-8");
    setenv("MUSL_LOCPATH", "/storage", 1);
    TEST_PRINT("[INFO]%s:%d,%s,getenv MUSL_LOCPATH=%s\n", __FILE__, __LINE__, __func__, getenv("MUSL_LOCPATH"));
    tmp = setlocale(LC_TIME, "zh_CN.UTF-8");
    TEST_PRINT("[INFO]%s:%d,%s,Locale is: %s\n", __FILE__, __LINE__, __func__, tmp);
    ICUNIT_GOTO_STRING_EQUAL(tmp, "zh_CN.UTF-8", -1, OUT);

    /* choose a domain for your program. */
    tmp = textdomain("messages");
    /* bind a directory,use PWD here. */
    bindtextdomain("messages", ".");
    /* set the output codeset.It is not needed usually */
    bind_textdomain_codeset("messages", "UTF-8");

    /* test gettext. */
    s = gettext("Monday/n\n");

    ICUNIT_ASSERT_STRING_EQUAL(s, "Monday/n\n", s);
    setlocale(LC_ALL, "C");

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase(VOID)
{
    testcase1();

    return LOS_OK;
}

VOID IO_TEST_GETTEXT_001(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
