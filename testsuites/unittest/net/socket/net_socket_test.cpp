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

#include "lt_net_socket.h"

using namespace testing::ext;
namespace OHOS {
class NetSocketTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        struct sched_param param = { 0 };
        int currThreadPolicy, ret;
        ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &param);
        ICUNIT_ASSERT_EQUAL_VOID(ret, 0, -ret);
        param.sched_priority = TASK_PRIO_TEST;
        ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
        ICUNIT_ASSERT_EQUAL_VOID(ret, 0, -ret);
    }
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE) && defined(LOSCFG_USER_TEST_NET_SOCKET)
/* *
 * @tc.name: NetSocketTest001
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest001, TestSize.Level0)
{
    NetSocketTest001();
}

/* *
 * @tc.name: NetSocketTest002
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest002, TestSize.Level0)
{
    NetSocketTest002();
}

#if TEST_ON_LINUX
/* *
 * @tc.name: NetSocketTest003
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest003, TestSize.Level0)
{
    NetSocketTest003(); // getifaddrs need PF_NETLINK which was not supported by lwip currently
}
#endif

/* *
 * @tc.name: NetSocketTest004
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest004, TestSize.Level0)
{
    NetSocketTest004();
}

/* *
 * @tc.name: NetSocketTest005
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest005, TestSize.Level0)
{
    NetSocketTest005();
}

/* *
 * @tc.name: NetSocketTest006
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest006, TestSize.Level0)
{
    NetSocketTest006();
}

#if TEST_ON_LINUX
/* *
 * @tc.name: NetSocketTest007
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest007, TestSize.Level0)
{
    NetSocketTest007();
}
#endif

/* *
 * @tc.name: NetSocketTest008
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest008, TestSize.Level0)
{
    NetSocketTest008();
}

/* *
 * @tc.name: NetSocketTest009
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest009, TestSize.Level0)
{
    NetSocketTest009();
}

/* *
 * @tc.name: NetSocketTest010
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest010, TestSize.Level0)
{
    NetSocketTest010();
}

/* *
 * @tc.name: NetSocketTest011
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest011, TestSize.Level0)
{
    NetSocketTest011();
}

/* *
 * @tc.name: NetSocketTest012
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(NetSocketTest, NetSocketTest012, TestSize.Level0)
{
    NetSocketTest012();
}

/* *
 * @tc.name: NetSocketTest013
 * @tc.desc: function for NetSocketTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
/*
HWTEST_F(NetSocketTest, NetSocketTest013, TestSize.Level0)
{
    //NetSocketTest013(); // broadcast to self to be supported.
}
*/
#endif
}
