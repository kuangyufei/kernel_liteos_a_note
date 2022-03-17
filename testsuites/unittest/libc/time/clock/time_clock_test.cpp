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

#include "lt_clock_test.h"

using namespace testing::ext;
namespace OHOS {
class TimeClockTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: ClockTestSmoke
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTestSmoke, TestSize.Level0)
{
    ClockTestSmoke();
}
#endif

#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: ClockTest001
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest001, TestSize.Level0)
{
    ClockTest001();
}

/* *
 * @tc.name: ClockTest002
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest002, TestSize.Level0)
{
    ClockTest002();
}

/* *
 * @tc.name: ClockTest003
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest003, TestSize.Level0)
{
    ClockTest003();
}

/* *
 * @tc.name: ClockTest004
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest004, TestSize.Level0)
{
    ClockTest004(); // clock_getcpuclockid not supported on HMOS currently
}

/* *
 * @tc.name: ClockTest005
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest005, TestSize.Level0)
{
    ClockTest005();
}

/* *
 * @tc.name: ClockTest006
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest006, TestSize.Level0)
{
    ClockTest006();
}

/* *
 * @tc.name: ClockTest007
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest007, TestSize.Level0)
{
    ClockTest007();
}

/* *
 * @tc.name: ClockTest008
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest008, TestSize.Level0)
{
    ClockTest008();
}

/* *
 * @tc.name: ClockTest009
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest009, TestSize.Level0)
{
    ClockTest009();
}

/* *
 * @tc.name: ClockTest010
 * @tc.desc: function for TimeClockTest
 * @tc.type: FUNC
 */
HWTEST_F(TimeClockTest, ClockTest010, TestSize.Level0)
{
    ClockTest010();
}

#endif
} // namespace OHOS
