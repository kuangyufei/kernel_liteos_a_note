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

static int GetServByPortRTest(void)
{
    // refer to the `/etc/services' file.
    char serv_file[] = "ssh		22/tcp\n";
    char *pathList[] = {"/etc/services"};
    char *streamList[] = {static_cast<char *>(serv_file)};
    int streamLen[] = {sizeof(serv_file)};
    const int file_number = 1;
    int flag = PrepareFileEnv(pathList, streamList, streamLen, file_number);
    if (flag != 0) {
        RecoveryFileEnv(pathList, file_number);
        return -1;
    }

    struct servent se, *result = NULL;
    char buf[1024];
    char buf1[2];

    const int test_port_no = 22;   // ssh port number is 22
    int ret = getservbyport_r(htons(test_port_no), "tcp", &se, buf, sizeof buf, &result);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_NOT_EQUAL(result, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se.s_name, "ssh", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se.s_proto, "tcp", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se.s_aliases[0], "ssh", -1);
    ICUNIT_ASSERT_STRING_EQUAL(result->s_name, "ssh", -1);
    ICUNIT_ASSERT_STRING_EQUAL(result->s_proto, "tcp", -1);
    ICUNIT_ASSERT_STRING_EQUAL(result->s_aliases[0], "ssh", -1);

    ret = getservbyport_r(htons(test_port_no), "udp", &se, buf, sizeof buf, &result);
    ICUNIT_ASSERT_EQUAL(ret, ENOENT, -1);

    ret = getservbyport_r(htons(test_port_no), "udp", &se, buf1, sizeof buf1, &result);
    ICUNIT_ASSERT_EQUAL(ret, ERANGE, ret);

    ret = getservbyport_r(htons(test_port_no), "ud", &se, buf, sizeof buf, &result);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    RecoveryFileEnv(pathList, file_number);
    return ICUNIT_SUCCESS;
}

void NetNetDbTest012(void)
{
    TEST_ADD_CASE(__FUNCTION__, GetServByPortRTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
