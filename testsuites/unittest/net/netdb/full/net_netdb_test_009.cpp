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

#include "lt_net_netdb.h"

static int GetHostByName2RTest(void)
{
    struct hostent addr, *result = NULL;
    char buf[1024];
    char buf1[1];
    int err, ret;

    ret = gethostbyname2_r("localhost", AF_INET, &addr, buf, sizeof buf, &result, &err);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_NOT_EQUAL(result, NULL, -1);
    ICUNIT_ASSERT_EQUAL(strcmp(result->h_name, "localhost"), 0, -1);
    ICUNIT_ASSERT_EQUAL(result->h_addrtype, AF_INET, result->h_addrtype);
    ICUNIT_ASSERT_NOT_EQUAL(result->h_length, 0, result->h_length);

    ret = gethostbyname2_r("127.0.0.1", AF_INET, &addr, buf, sizeof buf, &result, &err);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_NOT_EQUAL(result, NULL, -1);
    ICUNIT_ASSERT_EQUAL(strcmp(result->h_name, "127.0.0.1"), 0, -1);
    ICUNIT_ASSERT_EQUAL(result->h_addrtype, AF_INET, result->h_addrtype);
    ICUNIT_ASSERT_NOT_EQUAL(result->h_length, 0, result->h_length);

    ret = gethostbyname2_r("127.0.0.1", AF_INET, &addr, buf1, sizeof buf1, &result, &err);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ret = gethostbyname2_r("127.0.0.1.1", AF_INET, &addr, buf, sizeof buf, &result, &err);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ret = gethostbyname2_r("lo", AF_INET, &addr, buf, sizeof buf, &result, &err);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    return ICUNIT_SUCCESS;
}

void NetNetDbTest009(void)
{
    TEST_ADD_CASE(__FUNCTION__, GetHostByName2RTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
