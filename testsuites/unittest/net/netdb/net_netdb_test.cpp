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
#include "stdio.h"
#include <climits>
#include <gtest/gtest.h>

#include "lt_net_netdb.h"

using namespace testing::ext;
namespace OHOS {
class NetDbTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        struct sched_param param = { 0 };
        int currThreadPolicy, ret;
        ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &param);
        param.sched_priority = TASK_PRIO_TEST;
        ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE) && defined(LOSCFG_USER_TEST_NET_NETDB)
/* *
 * @tc.name: NetNetDbTest001
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest001, TestSize.Level0)
{
    NetNetDbTest001();
}

/* *
 * @tc.name: NetNetDbTest013
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest013, TestSize.Level0)
{
    NetNetDbTest013();
}

#endif

#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: NetNetDbTest018
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest018, TestSize.Level0)
{
    NetNetDbTest018();
}

/* *
 * @tc.name: NetNetDbTest002
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest002, TestSize.Level0)
{
    NetNetDbTest002();
}

/* *
 * @tc.name: NetNetDbTest003
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest003, TestSize.Level0)
{
    NetNetDbTest003();
}

/* *
 * @tc.name: NetNetDbTest004
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest004, TestSize.Level0)
{
    NetNetDbTest004();
}

/* *
 * @tc.name: NetNetDbTest006
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest006, TestSize.Level0)
{
    NetNetDbTest006();
}

/* *
 * @tc.name: NetNetDbTest007
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest007, TestSize.Level0)
{
    NetNetDbTest007();
}

/* *
 * @tc.name: NetNetDbTest008
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest008, TestSize.Level0)
{
    NetNetDbTest008();
}

/* *
 * @tc.name: NetNetDbTest009
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest009, TestSize.Level0)
{
    NetNetDbTest009();
}

/* *
 * @tc.name: NetNetDbTest010
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest010, TestSize.Level0)
{
    NetNetDbTest010();
}

/* *
 * @tc.name: NetNetDbTest011
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest011, TestSize.Level0)
{
    NetNetDbTest011();
}

/* *
 * @tc.name: NetNetDbTest012
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest012, TestSize.Level0)
{
    NetNetDbTest012();
}

/* *
 * @tc.name: NetNetDbTest015
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest015, TestSize.Level0)
{
    NetNetDbTest015();
}

/* *
 * @tc.name: NetNetDbTest016
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest016, TestSize.Level0)
{
    NetNetDbTest016();
}

/* *
 * @tc.name: NetNetDbTest017
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest017, TestSize.Level0)
{
    NetNetDbTest017();
}

/* *
 * @tc.name: NetNetDbTest019
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest019, TestSize.Level0)
{
    NetNetDbTest019();
}

/* *
 * @tc.name: NetNetDbTest020
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest020, TestSize.Level0)
{
    NetNetDbTest020();
}

/* *
 * @tc.name: NetNetDbTest021
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest021, TestSize.Level0)
{
    NetNetDbTest021();
}

/* *
 * @tc.name: NetNetDbTest022
 * @tc.desc: function for NetDbTest
 * @tc.type: FUNC
 */
HWTEST_F(NetDbTest, NetNetDbTest022, TestSize.Level0)
{
    NetNetDbTest022();
}

#endif
}
