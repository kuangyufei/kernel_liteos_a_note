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

static int EtherHosttonTest(void)
{
    // suppose "0:0:0:0:0:0 localhost" in `/etc/ethers' file.
    struct ether_addr addr, *eaddr = &addr;
    int ret = ether_hostton("localhost", eaddr);

    ICUNIT_ASSERT_EQUAL(ret, 0, -1);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[0], 0x00, eaddr->ether_addr_octet[0]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[1], 0x00, eaddr->ether_addr_octet[1]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[2], 0x00, eaddr->ether_addr_octet[2]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[3], 0x00, eaddr->ether_addr_octet[3]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[4], 0x00, eaddr->ether_addr_octet[4]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[5], 0x00, eaddr->ether_addr_octet[5]);

    return ICUNIT_SUCCESS;
}

void NetResolvTest004(void)
{
    TEST_ADD_CASE(__FUNCTION__, EtherHosttonTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
