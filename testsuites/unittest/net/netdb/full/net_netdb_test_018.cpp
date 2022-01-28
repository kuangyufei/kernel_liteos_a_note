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

static int GetServEntTest(void)
{
    char serv_file[] = "tcpmux		1/tcp    # TCP port service multiplexer\n"
                       "echo		7/tcp\n"
                       "echo		7/udp\n"
                       "discard		9/tcp		sink null\n"
                       "ssh         100000/tcp\n"
                       "ssh         /tcp\n"
                       "ssh         22/";
    char *pathList[] = {"/etc/services"};
    char *streamList[] = {static_cast<char *>(serv_file)};
    int streamLen[] = {sizeof(serv_file)};
    const int file_number = 1;
    int flag = PrepareFileEnv(pathList, streamList, streamLen, file_number);
    if (flag != 0) {
        RecoveryFileEnv(pathList, file_number);
        return -1;
    }
    
    /* tcpmux,echo,discard port number is 1,7,9 */
    const int tcpmux_port_no  = 1;
    const int echo_port_no    = 7;
    const int discard_port_no = 9;

    struct servent *se1 = nullptr;
    struct servent *se2 = nullptr;
    struct servent *se3 = nullptr;

    se1 = getservent();
    ICUNIT_ASSERT_NOT_EQUAL(se1, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_name, "tcpmux", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_proto, "tcp", -1);
    ICUNIT_ASSERT_EQUAL(se1->s_port, ntohs(tcpmux_port_no), -1);

    endservent();
    se2 = getservent();
    ICUNIT_ASSERT_NOT_EQUAL(se2, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_name, se2->s_name, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_proto, se2->s_proto, -1);
    ICUNIT_ASSERT_EQUAL(se1->s_port, se2->s_port, -1);

    setservent(0);
    se3 = getservent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_name, se3->s_name, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->s_proto, se3->s_proto, -1);
    ICUNIT_ASSERT_EQUAL(se1->s_port, se3->s_port, -1);

    se3 = getservent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_name, "echo", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_proto, "tcp", -1);
    ICUNIT_ASSERT_EQUAL(se3->s_port, ntohs(echo_port_no), -1);

    se3 = getservent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_name, "echo", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_proto, "udp", -1);
    ICUNIT_ASSERT_EQUAL(se3->s_port, ntohs(echo_port_no), -1);

    se3 = getservent();
    ICUNIT_ASSERT_NOT_EQUAL(se3, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_name, "discard", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_proto, "tcp", -1);
    ICUNIT_ASSERT_EQUAL(se3->s_port, ntohs(discard_port_no), -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_aliases[0], "sink", -1);
    ICUNIT_ASSERT_STRING_EQUAL(se3->s_aliases[1], "null", -1);
    ICUNIT_ASSERT_EQUAL(se3->s_aliases[2], nullptr, -1);

    se3 = getservent();
    ICUNIT_ASSERT_EQUAL(se3, nullptr, -1);
    endservent();
    
    RecoveryFileEnv(pathList, file_number);
    return ICUNIT_SUCCESS;
}

void NetNetDbTest018(void)
{
    TEST_ADD_CASE(__FUNCTION__, GetServEntTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
