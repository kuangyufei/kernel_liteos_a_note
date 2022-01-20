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
#include <string.h>

static UINT32 testcase1(VOID)
{
    char *a = "aBcDeF";
    char *b = "AbCdEf";
    int n = 600;
    int ret = 0;
    locale_t loc;
    errno = 0;

    setenv("MUSL_LOCPATH", "/storage", 1);
    TEST_PRINT("[INFO]%s:%d,%s,Locale is: %s\n", __FILE__, __LINE__, __func__,setlocale(LC_TIME, "zh_CN.UTF-8"));

    errno = 0;
    ret = strncasecmp_l(a, b, n, loc);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, 0, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase2(VOID)
{
    char *a = "aBcDeF";
    char *b = "AbCdEi";
    int n = -6;
    int ret = 0;
    locale_t loc;
    errno = 0;

    setenv("MUSL_LOCPATH", "/storage", 1);
    TEST_PRINT("[INFO]%s:%d,%s,Locale is: %s\n", __FILE__, __LINE__, __func__,setlocale(LC_TIME, "zh_CN.UTF-8"));

    errno = 0;
    ret = strncasecmp_l(a, b, n, loc);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -3, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, 0, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase3(VOID)
{
    char *a = "aBcDeF";
    char *b = "AbCdAx";
    int n = 0;
    int ret = 0;
    locale_t loc;
    errno = 0;

    setenv("MUSL_LOCPATH", "/storage", 1);
    TEST_PRINT("[INFO]%s:%d,%s,Locale is: %s\n", __FILE__, __LINE__, __func__,setlocale(LC_TIME, "zh_CN.UTF-8"));

    errno = 0;
    ret = strncasecmp_l(a, b, n, loc);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, 0, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase4(VOID)
{
    char *a = "34342aBcDeF";
    char *b = "AbCdAx";
    int n = 4;
    int ret = 0;
    locale_t loc;
    errno = 0;

    setenv("MUSL_LOCPATH", "/storage", 1);
    TEST_PRINT("[INFO]%s:%d,%s,Locale is: %s\n", __FILE__, __LINE__, __func__,setlocale(LC_TIME, "zh_CN.UTF-8"));

    errno = 0;
    ret = strncasecmp_l(a, b, n, loc);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -46, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, 0, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase(VOID)
{
    testcase1();
    testcase2();
    testcase3();
    testcase4();

    return LOS_OK;
}

VOID IO_TEST_STRNCASECMP_L_002(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
