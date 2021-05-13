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

static int GetNetByAddrtTest(void)
{
    struct netent *se1 = nullptr;
    struct netent *se2 = nullptr;
    struct netent *se3 = nullptr;

    se1 = getnetbyaddr(inet_network("169.254.0.0"), AF_INET);
    ICUNIT_ASSERT_NOT_EQUAL(se1, nullptr, -1);
    ICUNIT_ASSERT_STRING_EQUAL(se1->n_name, "link-local", -1);
    ICUNIT_ASSERT_EQUAL(se1->n_addrtype, 2, -1);
    ICUNIT_ASSERT_EQUAL(se1->n_net, inet_network("169.254.0.0"), -1);

    se2 = getnetbyaddr(inet_network("169.254.0.1"), AF_INET);
    ICUNIT_ASSERT_EQUAL(se2, nullptr, -1);

    se3 = getnetbyaddr(inet_network("169.254.0.1"), AF_INET6);
    ICUNIT_ASSERT_EQUAL(se3, nullptr, -1);

    return ICUNIT_SUCCESS;
}

void NetNetDbTest022(void)
{
    TEST_ADD_CASE(__FUNCTION__, GetNetByAddrtTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
