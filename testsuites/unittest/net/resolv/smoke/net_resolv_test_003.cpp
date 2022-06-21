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

const int buffer_size = 20;

static int EtherAtonrTest(void)
{
    struct ether_addr addr;
    struct ether_addr *eaddr = ether_aton_r("::01:EF", &addr);

    ICUNIT_ASSERT_EQUAL(eaddr, NULL, -1);

    char mac_addr[buffer_size], *tmp = mac_addr;
    int r[ETH_ALEN];
    srand(time(NULL));
    for (int i = 0; i < ETH_ALEN; i++) {
        r[i] = rand() % 0xff;
    }
    // 0, 1, 2, 3, 4, 5: 6 elements of mac address.
    (void)sprintf_s(mac_addr, sizeof(mac_addr), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", r[0], r[1], r[2], r[3], r[4], r[5]);
    eaddr = ether_aton_r(mac_addr, &addr);

    ICUNIT_ASSERT_EQUAL(eaddr, &addr, -1);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[0], r[0], eaddr->ether_addr_octet[0]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[1], r[1], eaddr->ether_addr_octet[1]);
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[2], r[2], eaddr->ether_addr_octet[2]); // 2: compare r[2] with eaddr
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[3], r[3], eaddr->ether_addr_octet[3]); // 3: compare r[3] with eaddr
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[4], r[4], eaddr->ether_addr_octet[4]); // 4: compare r[4] with eaddr
    ICUNIT_ASSERT_EQUAL(eaddr->ether_addr_octet[5], r[5], eaddr->ether_addr_octet[5]); // 5: compare r[5] with eaddr

    return ICUNIT_SUCCESS;
}

void NetResolvTest003(void)
{
    TEST_ADD_CASE(__FUNCTION__, EtherAtonrTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
