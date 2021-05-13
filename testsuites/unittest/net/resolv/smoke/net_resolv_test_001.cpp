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
#include <string.h>
static void dump(unsigned char *s, int len) {
    if (!s) return;
    for (int i = 0; i < len; ++i) {
        printf("%02x ", s[i]);
    }
    printf("\n");
}

static int DnCompTest(void)
{
    unsigned char comp_dn[100] = "example";
    unsigned char *dnptrs[6] = {comp_dn, 0};
    unsigned char **lastdnptr = &dnptrs[6];
    int offset, ret;

    offset = strlen((const char *)dnptrs[0])+1;
    ret = dn_comp("x.y.z.example.com", comp_dn+offset, sizeof(comp_dn)-offset, dnptrs, lastdnptr);
    dump(comp_dn+offset, ret);
    ICUNIT_ASSERT_EQUAL(ret, 19, ret);
    ICUNIT_ASSERT_EQUAL(dnptrs[1], comp_dn+offset, dnptrs[1]);

    offset += ret+1;
    ret = dn_comp("zz.example.com", comp_dn+offset, sizeof(comp_dn)-offset, dnptrs, lastdnptr);
    dump(comp_dn+offset, ret);
    ICUNIT_ASSERT_EQUAL(ret, 5, ret);
    ICUNIT_ASSERT_EQUAL(dnptrs[2], comp_dn+offset, dnptrs[2]);

    offset += ret+1;
    ret = dn_comp("a.example.com", comp_dn+offset, sizeof(comp_dn)-offset, dnptrs, lastdnptr);
    dump(comp_dn+offset, ret);
    ICUNIT_ASSERT_EQUAL(ret, 4, ret);
    ICUNIT_ASSERT_EQUAL(dnptrs[3], comp_dn+offset, dnptrs[3]);

    offset += ret+1;
    ret = dn_comp("example.com.cn", comp_dn+offset, sizeof(comp_dn)-offset, dnptrs, lastdnptr);
    dump(comp_dn+offset, ret);
    ICUNIT_ASSERT_EQUAL(ret, 16, ret);
    ICUNIT_ASSERT_EQUAL(dnptrs[4], comp_dn+offset, dnptrs[4]);

    offset += ret+1;
    ret = dn_comp("2example.com", comp_dn+offset, sizeof(comp_dn)-offset, dnptrs, lastdnptr);
    dump(comp_dn+offset, ret);
    ICUNIT_ASSERT_EQUAL(ret, 11, ret);
    ICUNIT_ASSERT_EQUAL(dnptrs[5], /*comp_dn+offset*/ NULL, dnptrs[5]); //last one is always NULL

    for (int i = 0; i < 6; ++i) {
        printf("%p: %s\n", dnptrs[i], dnptrs[i]);
    }
    ICUNIT_ASSERT_EQUAL(offset+ret<100, 1, ret);
    return ICUNIT_SUCCESS;
}

void NetResolvTest001(void)
{
    TEST_ADD_CASE(__FUNCTION__, DnCompTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
