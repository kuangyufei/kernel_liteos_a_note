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

static int GetServByNameTest(void)
{
    char serv_file[] = "echo		7/tcp\n"
                       "echo		7/udp\n"
                       "discard		9/tcp		sink null\n"
                       "discard		9/udp		sink null\n"
                       "systat		11/tcp		users\n"
                       "ssh		22/tcp\n";

    char *pathList[] = {"/etc/services"};
    char *streamList[] = {static_cast<char *>(serv_file)};
    int streamLen[] = {sizeof(serv_file)};
    const int file_number = 1;
    int flag = PrepareFileEnv(pathList, streamList, streamLen, file_number);
    if (flag != 0) {
        RecoveryFileEnv(pathList, file_number);
        return -1;
    }

    struct servent *se1 = nullptr;
    struct servent *se2 = nullptr;

    se1 = getservbyname("discard", "tcp");
    ICUNIT_ASSERT_NOT_EQUAL(se1, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_name, "discard", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_proto, "tcp", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_aliases[0], "discard", -1);

    se1 = getservbyname("ssh", "tcp");
    ICUNIT_ASSERT_NOT_EQUAL(se1, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_name, "ssh", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_proto, "tcp", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_aliases[0], "ssh", -1);

    se2 = getservbyname("cho", "udp");
    ICUNIT_ASSERT_EQUAL(se2, nullptr, -1);

    se2 = getservbyname("systat", "udp");
    ICUNIT_ASSERT_EQUAL(se2, nullptr, -1);

    RecoveryFileEnv(pathList, file_number);
    return ICUNIT_SUCCESS;
}

void NetNetDbTest016(void)
{
    TEST_ADD_CASE(__FUNCTION__, GetServByNameTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
