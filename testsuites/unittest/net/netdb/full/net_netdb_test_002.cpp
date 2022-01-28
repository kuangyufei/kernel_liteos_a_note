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

static int AddrInfoTest(void)
{
    // Prerequisite: correct DNS servers must be configured.
    char host_file[] = "127.0.0.2 example.com\n", serv_file[] = "ftp		21/tcp\n";
    char *pathList[] = {"/etc/hosts", "/etc/services"};
    char *streamList[] = {host_file, serv_file};
    int streamLen[] = {sizeof(host_file), sizeof(serv_file)};
    const int file_number = 2;
    int flag = PrepareFileEnv(pathList, streamList, streamLen, file_number);
    if (flag != 0) {
        RecoveryFileEnv(pathList, file_number);
        return -1;
    }

    struct addrinfo *addr = NULL;
    int ret = getaddrinfo("example.com", "ftp", NULL, &addr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    freeaddrinfo(addr);

    ret = getaddrinfo("local", "ftp", NULL, &addr);
    ICUNIT_ASSERT_EQUAL(ret, EAI_AGAIN, ret);

    ret = getaddrinfo("localhost", "fp", NULL, &addr);
    ICUNIT_ASSERT_EQUAL(ret, EAI_SERVICE, ret);

    const char *p = gai_strerror(EAI_AGAIN);
    ICUNIT_ASSERT_NOT_EQUAL(p, NULL, -1);

    RecoveryFileEnv(pathList, file_number);
    return ICUNIT_SUCCESS;
}

void NetNetDbTest002(void)
{
    TEST_ADD_CASE(__FUNCTION__, AddrInfoTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
