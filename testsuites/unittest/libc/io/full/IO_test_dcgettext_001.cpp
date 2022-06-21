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

const int domain_name_length = 10;
const int buffer_size = 50;

static UINT32 testcase(VOID)
{
    char *s = "";
    char domain[buffer_size], tmp[domain_name_length];
    srand(time(NULL));
    for (int i = 0, r = 0; i < domain_name_length; i++) {
        r = rand() % 36; // 36: 0-9 and a-z
        if (r < 10) {    // 10: 0-9
            tmp[i] = '0' + r;
        } else {
            tmp[i] = 'a' + r;
        }
    }
    int ret = sprintf_s(domain, sizeof(domain), "www.%s.com", tmp);
    if (ret == 0) {
        printf("sprinf_s failed\n");
        return LOS_NOK;
    }

    setlocale(LC_ALL, "");
    textdomain("gettext_demo");
    bindtextdomain("gettext_demo", ".");
    bind_textdomain_codeset("gettext_demo", "UTF-8");

    printf(dcgettext(domain, "TestString1\n", LC_MESSAGES));

    s = dcgettext(domain, "TestString1\n", LC_MESSAGES);
    printf("[INFO]%s:%d,%s,s=%s\n", __FILE__, __LINE__, __func__, s);
    ICUNIT_ASSERT_STRING_EQUAL(s, "TestString1\n", s);
    setlocale(LC_ALL, "C");

    return LOS_OK;
}

VOID IO_TEST_DCGETTEXT_001(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
