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

#include "It_test_misc.h"

using namespace testing::ext;
namespace OHOS {
class MiscTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: IT_TEST_MISC_001
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc001, TestSize.Level0)
{
    ItTestMisc001();
}

/* *
 * @tc.name: IT_TEST_MISC_002
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc002, TestSize.Level0)
{
    ItTestMisc002();
}

/* *
 * @tc.name: IT_TEST_MISC_003
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc003, TestSize.Level0)
{
    ItTestMisc003();
}

/* *
 * @tc.name: IT_TEST_MISC_004
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc004, TestSize.Level0)
{
    ItTestMisc004();
}

/* *
 * @tc.name: IT_TEST_MISC_005
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc005, TestSize.Level0)
{
    ItTestMisc005();
}

/* *
 * @tc.name: IT_TEST_MISC_014
 * @tc.desc: function for tmpnam test
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, IT_TEST_MISC_014, TestSize.Level0)
{
    IT_TEST_MISC_014();
}
#endif

#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: IT_TEST_MISC_006
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc006, TestSize.Level0)
{
    ItTestMisc006();
}

/* *
 * @tc.name: IT_TEST_MISC_007
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc007, TestSize.Level0)
{
    ItTestMisc007();
}

/* *
 * @tc.name: IT_TEST_MISC_008
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc008, TestSize.Level0)
{
    ItTestMisc008();
}

/* *
 * @tc.name: IT_TEST_MISC_009
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc009, TestSize.Level0)
{
    ItTestMisc009();
}

/* *
 * @tc.name: IT_TEST_MISC_010
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
/*HWTEST_F(MiscTest, ItTestMisc010, TestSize.Level0)
{
    ItTestMisc010();
}*/

/* *
 * @tc.name: IT_TEST_MISC_011
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
/*HWTEST_F(MiscTest, ItTestMisc011, TestSize.Level0)
{
    ItTestMisc011();
}*/

/* *
 * @tc.name: IT_TEST_MISC_012
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(MiscTest, ItTestMisc012, TestSize.Level0)
{
    ItTestMisc012();
}

/* *
 * @tc.name: IT_TEST_MISC_013
 * @tc.desc: function for MiscTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
/*HWTEST_F(MiscTest, ItTestMisc013, TestSize.Level0)
{
    ItTestMisc013();
}*/
#endif

} // namespace OHOS
