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

#include "lt_timer_test.h"

using namespace testing::ext;
namespace OHOS {
class TimeTimerTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: TimerTest001
 * @tc.desc: function for TimeTimerTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(TimeTimerTest, TimerTest001, TestSize.Level0)
{
    TimerTest001();
}

/* *
 * @tc.name: TimerTest002
 * @tc.desc: function for TimeTimerTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(TimeTimerTest, TimerTest002, TestSize.Level0)
{
    TimerTest002();
}

/* *
 * @tc.name: TimerTest003
 * @tc.desc: function for TimeTimerTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(TimeTimerTest, TimerTest003, TestSize.Level0)
{
    TimerTest003();
}

/* *
 * @tc.name: TimerTest004
 * @tc.desc: function for TimeTimerTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
/*HWTEST_F(TimeTimerTest, TimerTest004, TestSize.Level0)
{
    TimerTest004(); // TODO: musl sigaction handler have only one param.
}*/

/* *
 * @tc.name: TimerTest005
 * @tc.desc: function for timer_create SIGEV_THREAD.
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(TimeTimerTest, TimerTest005, TestSize.Level0)
{
    TimerTest005();
}

/* *
 * @tc.name: TIME_TEST_TZSET_001
 * @tc.desc: function for TIME_TEST_TZSET_001
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(TimeTimerTest, TIME_TEST_TZSET_001, TestSize.Level0)
{
    TIME_TEST_TZSET_001();
}

/* *
 * @tc.name: TIME_TEST_TZSET_002
 * @tc.desc: function for TimeTimerTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(TimeTimerTest, TIME_TEST_TZSET_002, TestSize.Level0)
{
    TIME_TEST_TZSET_002();
}
#endif
} // namespace OHOS
