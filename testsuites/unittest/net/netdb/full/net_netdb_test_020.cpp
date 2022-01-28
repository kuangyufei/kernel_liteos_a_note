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

static int GetNetEntTest(void)
{
    char network_file[] = "# symbolic names for networks, see networks(5) for more information\n"
                          "link-local 169.254.0.0\n"
                          "example  192.168.1.0 network example-network\n"
                          "test1";
    char *pathList[] = {"/etc/networks"};
    char *streamList[] = {static_cast<char *>(network_file)};
    int streamLen[] = {sizeof(network_file)};
    const int file_number = 1;
    int flag = PrepareFileEnv(pathList, streamList, streamLen, file_number);
    if (flag != 0) {
        RecoveryFileEnv(pathList, file_number);
        return -1;
    }

    struct netent *se1 = nullptr;
    struct netent *se2 = nullptr;
    struct netent *se3 = nullptr;

    se1 = getnetent();
    ICUNIT_ASSERT_NOT_EQUAL(se1, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->n_name, "link-local", -1);
    ICUNIT_ASSERT_EQUAL(se1->n_addrtype, AF_INET, -1);
    ICUNIT_ASSERT_EQUAL(se1->n_net, inet_network("169.254.0.0"), -1);
    ICUNIT_ASSERT_EQUAL(se1->n_aliases[0], nullptr, -1);

    endnetent();
    se2 = getnetent();
    ICUNIT_ASSERT_NOT_EQUAL(se2, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->n_name, se2->n_name, -1);
    ICUNIT_ASSERT_EQUAL(se1->n_addrtype, se2->n_addrtype, -1);
    ICUNIT_ASSERT_EQUAL(se1->n_net, se2->n_net, -1);

    setnetent(0);
    se3 = getnetent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->n_name, se3->n_name, -1);
    ICUNIT_ASSERT_EQUAL(se1->n_addrtype, se3->n_addrtype, -1);
    ICUNIT_ASSERT_EQUAL(se1->n_net, se3->n_net, -1);

    se1 = getnetent();
    ICUNIT_ASSERT_NOT_EQUAL(se1, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->n_name, "example", -1);
    ICUNIT_ASSERT_EQUAL(se1->n_addrtype, AF_INET, -1);
    ICUNIT_ASSERT_EQUAL(se1->n_net, inet_network("192.168.1.0"), -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->n_aliases[0], "network", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->n_aliases[1], "example-network", -1);
    ICUNIT_ASSERT_EQUAL(se1->n_aliases[2], nullptr, -1);

    se1 = getnetent();
    ICUNIT_ASSERT_EQUAL(se1, nullptr, -1);
    endnetent();

    RecoveryFileEnv(pathList, file_number);
    return ICUNIT_SUCCESS;
}

void NetNetDbTest020(void)
{
    TEST_ADD_CASE(__FUNCTION__, GetNetEntTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
