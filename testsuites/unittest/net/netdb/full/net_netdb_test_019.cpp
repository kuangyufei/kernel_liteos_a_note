/*
 * opyright (c) 2021-2021, Huawei Technologies Co., Ltd. All rights reserved.
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
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "lt_net_netdb.h"
#include<arpa/nameser.h>

static int GetHostEntTest(void)
{
    char host_file[] = "127.0.0.1 localhost #localhost\n"
                       "::1      ip6-localhost\n"
                       "10.0.0.0 example example.com example.cn\n"
                       "10.0.0.0\n"
                       "10.0.0  example.com";
    char *pathList[] = {"/etc/hosts"};
    char *streamList[] = {static_cast<char *>(host_file)};
    int streamLen[] = {sizeof(host_file)};
    const int file_number = 1;
    int flag = PrepareFileEnv(pathList, streamList, streamLen, file_number);
    if (flag != 0) {
        RecoveryFileEnv(pathList, file_number);
        return -1;
    }

    struct hostent *se1 = nullptr;
    struct hostent *se2 = nullptr;
    struct hostent *se3 = nullptr;
    char addr[INET6_ADDRSTRLEN];

    se1 = gethostent();
    ICUNIT_ASSERT_NOT_EQUAL(se1, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->h_name, "localhost", -1);
    ICUNIT_ASSERT_EQUAL(se1->h_addrtype, AF_INET, -1);
    ICUNIT_ASSERT_EQUAL(se1->h_length, INADDRSZ, -1);
    ICUNIT_ASSERT_STRING_EQUAL("127.0.0.1", inet_ntop(AF_INET, se1->h_addr_list[0], addr, INET_ADDRSTRLEN), -1);
    ICUNIT_ASSERT_EQUAL(se1->h_aliases[0], nullptr, -1);

    endhostent();
    se2 = gethostent();
    ICUNIT_ASSERT_NOT_EQUAL(se2, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->h_name, se2->h_name, -1);
    ICUNIT_ASSERT_EQUAL(se1->h_addrtype, se2->h_addrtype, -1);
    ICUNIT_ASSERT_EQUAL(se1->h_length, se2->h_length, -1);

    sethostent(0);
    se3 = gethostent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->h_name, se3->h_name, -1);
    ICUNIT_ASSERT_EQUAL(se1->h_addrtype, se3->h_addrtype, -1);
    ICUNIT_ASSERT_EQUAL(se1->h_length, se3->h_length, -1);

    se3 = gethostent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_EQUAL(se3->h_addrtype, AF_INET6, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->h_name, "ip6-localhost", -1);
    ICUNIT_ASSERT_EQUAL(se3->h_length, IN6ADDRSZ, -1);
    ICUNIT_ASSERT_STRING_EQUAL("::1", inet_ntop(AF_INET6, se3->h_addr_list[0], addr, INET6_ADDRSTRLEN), -1);

    se3 = gethostent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_EQUAL(se3->h_addrtype, AF_INET, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->h_name, "example", -1);
    ICUNIT_ASSERT_STRING_EQUAL("10.0.0.0", inet_ntop(AF_INET, se3->h_addr_list[0], addr, INET_ADDRSTRLEN), -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->h_aliases[0], "example.com", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->h_aliases[1], "example.cn", -1);
    ICUNIT_ASSERT_EQUAL(se3->h_aliases[2], nullptr, -1);

    se3 = gethostent();
    ICUNIT_ASSERT_EQUAL(se3, nullptr, -1);
    endhostent();

    RecoveryFileEnv(pathList, file_number);
    return ICUNIT_SUCCESS;
}

void NetNetDbTest019(void)
{
    TEST_ADD_CASE(__FUNCTION__, GetHostEntTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
