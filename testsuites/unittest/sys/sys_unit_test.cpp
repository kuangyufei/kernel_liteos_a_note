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

#include "It_test_sys.h"

using namespace testing::ext;
namespace OHOS {
class SysTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: IT_TEST_SYS_004
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys004, TestSize.Level0)
{
    ItTestSys004();
}

/* *
 * @tc.name: IT_TEST_SYS_005
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys005, TestSize.Level0)
{
    ItTestSys005();
}

/* *
 * @tc.name: IT_TEST_SYS_006
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys006, TestSize.Level0)
{
    ItTestSys006();
}

/* *
 * @tc.name: IT_TEST_SYS_007
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys007, TestSize.Level0)
{
    ItTestSys007();
}

/* *
 * @tc.name: IT_TEST_SYS_008
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys008, TestSize.Level0)
{
    ItTestSys008();
}

/* *
 * @tc.name: IT_TEST_SYS_009
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys009, TestSize.Level0)
{
    ItTestSys009();
}

/* *
 * @tc.name: IT_TEST_SYS_010
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys010, TestSize.Level0)
{
    ItTestSys010();
}

/* *
 * @tc.name: IT_TEST_SYS_012
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys012, TestSize.Level0)
{
    ItTestSys012();
}

/* *
 * @tc.name: IT_TEST_SYS_013
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys013, TestSize.Level0)
{
    ItTestSys013();
}

/* *
 * @tc.name: IT_TEST_SYS_014
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys014, TestSize.Level0)
{
    ItTestSys014();
}

/* *
 * @tc.name: IT_TEST_SYS_015
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys015, TestSize.Level0)
{
    ItTestSys015();
}

/* *
 * @tc.name: IT_TEST_SYS_016
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys016, TestSize.Level0)
{
    ItTestSys016();
}

/* *
 * @tc.name: IT_TEST_SYS_017
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys017, TestSize.Level0)
{
    ItTestSys017();
}

/* *
 * @tc.name: IT_TEST_SYS_029
 * @tc.desc: function for ftok exception test
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys029, TestSize.Level0)
{
    ItTestSys029();
}

/* *
 * @tc.name: IT_TEST_SYS_030
 * @tc.desc: function for sigsetjmp siglongjmp test
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, IT_TEST_SYS_030, TestSize.Level0)
{
    IT_TEST_SYS_030();
}

/* *
 * @tc.name: IT_TEST_SYS_031
 * @tc.desc: function for ctermid test
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, IT_TEST_SYS_031, TestSize.Level0)
{
    IT_TEST_SYS_031();
}
#endif

#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: IT_TEST_SYS_001
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys001, TestSize.Level0)
{
    ItTestSys001();
}

/* *
 * @tc.name: IT_TEST_SYS_018
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys018, TestSize.Level0)
{
    ItTestSys018();
}

/* *
 * @tc.name: IT_TEST_SYS_019
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys019, TestSize.Level0)
{
    ItTestSys019();
}

/* *
 * @tc.name: IT_TEST_SYS_020
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys020, TestSize.Level0)
{
    ItTestSys020();
}

/* *
 * @tc.name: IT_TEST_SYS_021
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys021, TestSize.Level0)
{
    ItTestSys021();
}

/* *
 * @tc.name: IT_TEST_SYS_022
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys022, TestSize.Level0)
{
    ItTestSys022();
}

/* *
 * @tc.name: IT_TEST_SYS_023
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys023, TestSize.Level0)
{
    ItTestSys023();
}

/* *
 * @tc.name: IT_TEST_SYS_024
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys024, TestSize.Level0)
{
    ItTestSys024();
}

/* *
 * @tc.name: IT_TEST_SYS_025
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys025, TestSize.Level0)
{
    ItTestSys025();
}

/* *
 * @tc.name: IT_TEST_SYS_025
 * @tc.desc: function for SysTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys026, TestSize.Level0)
{
    ItTestSys026();
}

/* *
 * @tc.name: IT_TEST_SYS_027
 * @tc.desc: function for ftok normal test
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, ItTestSys027, TestSize.Level0)
{
    ItTestSys027();
}

/* *
 * @tc.name: IT_TEST_SYS_028
 * @tc.desc: function for nice:set pthread priority
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SysTest, IT_TEST_SYS_028, TestSize.Level0)
{
    IT_TEST_SYS_028();
}
#endif
} // namespace OHOS
