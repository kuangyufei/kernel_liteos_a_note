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


#include <lt_net_resolv.h>

static int EtherLineTest(void)
{
    struct ether_addr addr, *eaddr = &addr;
    char buf[100];
    int ret;

    ret = ether_line("localhost 01:02:03:04:05:06", eaddr, buf);
    ICUNIT_ASSERT_EQUAL(ret, -1, -1);

    ret = ether_line("01:02:03:04:05:06  localhost", eaddr, buf);
    ICUNIT_ASSERT_EQUAL(ret, 0, -1);

    ICUNIT_ASSERT_EQUAL(strcmp("localhost", buf), 0, -1);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[0], 0x01, eaddr->ether_addr_octet[0]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[1], 0x02, eaddr->ether_addr_octet[1]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[2], 0x03, eaddr->ether_addr_octet[2]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[3], 0x04, eaddr->ether_addr_octet[3]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[4], 0x05, eaddr->ether_addr_octet[4]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[5], 0x06, eaddr->ether_addr_octet[5]);

    return ICUNIT_SUCCESS;
}

void NetResolvTest005(void)
{
    TEST_ADD_CASE(__FUNCTION__, EtherLineTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
