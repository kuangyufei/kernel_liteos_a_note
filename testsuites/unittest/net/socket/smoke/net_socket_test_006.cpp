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

#include <osTest.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int InetTest(void)
{
    struct in_addr in;
    int ret = inet_pton(AF_INET, "300.10.10.10", &in);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = inet_pton(AF_INET, "10.11.12.13", &in);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ICUNIT_ASSERT_EQUAL(in.s_addr, 0x0d0c0b0a, in.s_addr);
#else
    ICUNIT_ASSERT_EQUAL(in.s_addr, 0x0a0b0c0d, in.s_addr);
#endif

    // host order
    in_addr_t lna = inet_lnaof(in);
    ICUNIT_ASSERT_EQUAL(lna, 0x000b0c0d, lna);

    // host order
    in_addr_t net = inet_netof(in);
    ICUNIT_ASSERT_EQUAL(net, 0x0000000a, net);

    in = inet_makeaddr(net, lna);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ICUNIT_ASSERT_EQUAL(in.s_addr, 0x0d0c0b0a, in.s_addr);
#else
    ICUNIT_ASSERT_EQUAL(in.s_addr, 0x0a0b0c0d, in.s_addr);
#endif

    net = inet_network("300.10.10.10");
    ICUNIT_ASSERT_EQUAL(net, -1, net);

    // host order
    net = inet_network("10.11.12.13");
    ICUNIT_ASSERT_EQUAL(net, 0x0a0b0c0d, net);

    const char *p = inet_ntoa(in);
    ICUNIT_ASSERT_EQUAL(strcmp(p, "10.11.12.13"), 0, -1);

    char buf[32];
    p = inet_ntop(AF_INET, &in, buf, sizeof(buf));
    ICUNIT_ASSERT_EQUAL(p, buf, -1);
    ICUNIT_ASSERT_EQUAL(strcmp(p, "10.11.12.13"), 0, -1);

    return ICUNIT_SUCCESS;
}

void NetSocketTest006(void)
{
    TEST_ADD_CASE(__FUNCTION__, InetTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
