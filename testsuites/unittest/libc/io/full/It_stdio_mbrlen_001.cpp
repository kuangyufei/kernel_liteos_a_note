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


static UINT32 Testcase(VOID)
{
    // allow mbrlen() to work with UTF-8 multibyte encoding
    setlocale(LC_ALL, "en_US.utf8");
    const char *str = "水";
    size_t sz = strlen(str);
    int len1, len2, len3;

    mbstate_t mb;
    memset(&mb, 0, sizeof(mb));
    len1 = mbrlen(str, 1, &mb);
    ICUNIT_GOTO_EQUAL(len1, -2, len1, EXIT); // -2, except return value

    len2 = mbrlen(str + 1, sz - 1, &mb);
    ICUNIT_GOTO_EQUAL(len2, 2, len2, EXIT); // 2, except return value

    len3 = mbrlen(str + 1, sz - 1, &mb);
    ICUNIT_GOTO_EQUAL(len3, -1, len3, EXIT);
    setlocale(LC_ALL, "C");

    return LOS_OK;
EXIT:
    return LOS_NOK;
}

VOID ItStdioMbrlen001(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
