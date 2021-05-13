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
    char *s = "";

    setlocale(LC_ALL, "");
    textdomain("gettext_demo");
    bindtextdomain("gettext_demo", ".");
    bind_textdomain_codeset("gettext_demo", "UTF-8");

    errno = 0;
    s = dcgettext("www.huawei.com", "TestString1\n", LC_MESSAGES);
    TEST_PRINT("[INFO]%s:%d,%s,s=%s\n", __FILE__, __LINE__, __func__, s);
    TEST_PRINT("[INFO]%s:%d,%s,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, errno, strerror(errno));
    ICUNIT_GOTO_STRING_EQUAL(s, "TestString1\n", -1, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase2(VOID)
{
    char *s = "";

    setlocale(LC_ALL, "");
    textdomain("gettext_demo");
    bindtextdomain("gettext_demo", ".");
    bind_textdomain_codeset("gettext_demo", "UTF-8");

    errno = 0;
    s = dcgettext(nullptr, "Hello World!\n", LC_MESSAGES);
    TEST_PRINT("[INFO]%s:%d,%s,s=%s\n", __FILE__, __LINE__, __func__, s);
    TEST_PRINT("[INFO]%s:%d,%s,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, errno, strerror(errno));
    ICUNIT_GOTO_STRING_EQUAL(s, "Hello World!\n", -1, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase3(VOID)
{
    char *s = "";
    char *domain = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
12345678901234567890123456789012345678901234567890123456";
    char *string = "Domain is NAME_MAX+1 256 characters long!\n";

    setlocale(LC_ALL, "");
    textdomain("gettext_demo");
    bindtextdomain("gettext_demo", ".");
    bind_textdomain_codeset("gettext_demo", "UTF-8");

    errno = 0;
    s = dcgettext(domain, string, LC_MESSAGES);
    TEST_PRINT("[INFO]%s:%d,%s,s=%s\n", __FILE__, __LINE__, __func__, s);
    TEST_PRINT("[INFO]%s:%d,%s,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, errno, strerror(errno));
    ICUNIT_GOTO_STRING_EQUAL(s, string, -1, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase4(VOID)
{
    char *s = "";
    char *domain = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
123456789012345678901234567890123456789012345678901234567";
    char *string = "Domain is more than NAME_MAX+1(256) 257 characters long!\n";

    setlocale(LC_ALL, "");
    textdomain("gettext_demo");
    bindtextdomain("gettext_demo", ".");
    bind_textdomain_codeset("gettext_demo", "UTF-8");

    errno = 0;
    s = dcgettext(domain, string, LC_MESSAGES);
    TEST_PRINT("[INFO]%s:%d,%s,s=%s\n", __FILE__, __LINE__, __func__, s);
    TEST_PRINT("[INFO]%s:%d,%s,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, errno, strerror(errno));
    ICUNIT_GOTO_STRING_EQUAL(s, string, -1, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}
static UINT32 testcase(VOID)
{
    errno = 0;

    testcase1();
    testcase2();
    testcase3();
    testcase4();

    return LOS_OK;
}

VOID IO_TEST_DCGETTEXT_002(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
