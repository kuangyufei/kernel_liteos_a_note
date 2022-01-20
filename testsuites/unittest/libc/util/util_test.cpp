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
#include "It_test_util.h"

using namespace testing::ext;
namespace OHOS {
class UtilTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: IT_TEST_UTIL_001
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil001, TestSize.Level0)
{
    ItTestUtil001();
}

/* *
 * @tc.name: IT_TEST_UTIL_002
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil002, TestSize.Level0)
{
    ItTestUtil002();
}

/* *
 * @tc.name: IT_TEST_UTIL_003
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil003, TestSize.Level0)
{
    ItTestUtil003();
}

/* *
 * @tc.name: IT_TEST_UTIL_004
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil004, TestSize.Level0)
{
    ItTestUtil004();
}

/* *
 * @tc.name: IT_TEST_UTIL_005
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil005, TestSize.Level0)
{
    ItTestUtil005();
}

/* *
 * @tc.name: IT_TEST_UTIL_006
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil006, TestSize.Level0)
{
    ItTestUtil006();
}

/* *
 * @tc.name: IT_TEST_UTIL_007
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil007, TestSize.Level0)
{
    ItTestUtil007();
}

/* *
 * @tc.name: IT_TEST_UTIL_101
 * @tc.desc: function for UtilTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(UtilTest, ItTestUtil101, TestSize.Level0)
{
    ItTestUtil101();
}

#endif
} // namespace OHOS
