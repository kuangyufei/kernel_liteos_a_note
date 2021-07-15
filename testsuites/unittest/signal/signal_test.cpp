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
#include <climits>
#include <gtest/gtest.h>
#include "it_test_signal.h"

using namespace testing::ext;
namespace OHOS {
class SignalTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: IT_POSIX_SIGNAL_002
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal002, TestSize.Level0)
{
    ItPosixSignal002();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_009
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal009, TestSize.Level0)
{
    ItPosixSignal009();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_013
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal013, TestSize.Level0)
{
    ItPosixSignal013();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_014
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal014, TestSize.Level0)
{
    ItPosixSignal014();
}


/* *
 * @tc.name: IT_POSIX_SIGNAL_021
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal021, TestSize.Level0)
{
    ItPosixSignal021();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_022
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal022, TestSize.Level0)
{
    ItPosixSignal022();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_023
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal023, TestSize.Level0)
{
    ItPosixSignal023();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_024
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal024, TestSize.Level0)
{
    ItPosixSignal024();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_031
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal031, TestSize.Level0)
{
    ItPosixSignal031();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_032
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal032, TestSize.Level0)
{
    ItPosixSignal032();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_035
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal035, TestSize.Level0)
{
    ItPosixSignal035();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_036
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal036, TestSize.Level0)
{
    ItPosixSignal036();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_037
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal037, TestSize.Level0)
{
    ItPosixSignal037();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_039
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal039, TestSize.Level0)
{
    ItPosixSignal039();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_042
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal042, TestSize.Level0)
{
    ItPosixSignal042();
}

/* *
 * @tc.name: ItPosixPipe002
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixPipe002, TestSize.Level0)
{
    ItPosixPipe002();
}

/* *
 * @tc.name: ItPosixMkfifo002
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixMkfifo002, TestSize.Level0)
{
    ItPosixMkfifo002();
}

/* *
 * @tc.name: IT_IPC_FD_ISSET_001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcFdIsset001, TestSize.Level0)
{
    ItIpcFdIsset001();
}
#endif

#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: IT_IPC_FD_CLR_001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcFdClr001, TestSize.Level0)
{
    ItIpcFdClr001();
}

/* *
 * @tc.name: IT_IPC_FD_SET_001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcFdSet001, TestSize.Level0)
{
    ItIpcFdSet001();
}

/* *
 * @tc.name: IT_IPC_FD_ZERO_001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcFdZero001, TestSize.Level0)
{
    ItIpcFdZero001();
}

/*
 * @tc.name: IT_IPC_SIGACTION_001^M
 * @tc.desc: function for SignalTest^M
 * @tc.type: FUNC^M
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcSigaction001, TestSize.Level0)
{
    ItIpcSigaction001();
}

/* *
 * @tc.name: IT_IPC_SIGPAUSE_001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcSigpause001, TestSize.Level0)
{
    ItIpcSigpause001();
}

/* *
 * @tc.name: IT_IPC_SIGPROMASK_001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcSigpromask001, TestSize.Level0)
{
    ItIpcSigpromask001();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal001, TestSize.Level0)
{
    ItPosixSignal001();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_003
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal003, TestSize.Level0)
{
    ItPosixSignal003();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_004
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal004, TestSize.Level0)
{
    ItPosixSignal004();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_005
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal005, TestSize.Level0)
{
    ItPosixSignal005();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_006
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal006, TestSize.Level0)
{
    ItPosixSignal006();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_007
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal007, TestSize.Level0)
{
    ItPosixSignal007();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_008
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal008, TestSize.Level0)
{
    ItPosixSignal008();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_010
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal010, TestSize.Level0)
{
    ItPosixSignal010();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_011
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal011, TestSize.Level0)
{
    ItPosixSignal011();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_012
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal012, TestSize.Level0)
{
    ItPosixSignal012();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_015
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal015, TestSize.Level0)
{
    ItPosixSignal015();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_016
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal016, TestSize.Level0)
{
    ItPosixSignal016();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_017
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal017, TestSize.Level0)
{
    ItPosixSignal017();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_018
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal018, TestSize.Level0)
{
    ItPosixSignal018();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_019
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal019, TestSize.Level0)
{
    ItPosixSignal019();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_020
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal020, TestSize.Level0)
{
    ItPosixSignal020();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_025
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal025, TestSize.Level0)
{
    ItPosixSignal025();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_026
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal026, TestSize.Level0)
{
    ItPosixSignal026();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_028
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal028, TestSize.Level0)
{
    ItPosixSignal028();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_029
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal029, TestSize.Level0)
{
    ItPosixSignal029();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_030
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal030, TestSize.Level0)
{
    ItPosixSignal030();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_033
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal033, TestSize.Level0)
{
    ItPosixSignal033();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_038
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal038, TestSize.Level0)
{
    ItPosixSignal038();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_040
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal040, TestSize.Level0)
{
    ItPosixSignal040();
}

/* *
 * @tc.name: IT_POSIX_SIGNAL_041
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixSignal041, TestSize.Level0)
{
    ItPosixSignal041();
}

/* *
 * @tc.name: IT_IPC_PIPE_002
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcPipe002, TestSize.Level0)
{
    ItIpcPipe002();
}

/* *
 * @tc.name: IT_IPC_PIPE_003
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItIpcPipe003, TestSize.Level0)
{
    ItIpcPipe003();
}

/* *
 * @tc.name: ItPosixPipe001
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixPipe001, TestSize.Level0)
{
    ItPosixPipe001();
}

/* *
 * @tc.name: ItPosixPipe003
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixPipe003, TestSize.Level0)
{
    ItPosixPipe003();
}

/* *
 * @tc.name: ItPosixPipe004
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixPipe004, TestSize.Level0)
{
    ItPosixPipe004();
}

/* *
 * @tc.name: ItPosixPipe005
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixPipe005, TestSize.Level0)
{
    ItPosixPipe005();
}

/* *
 * @tc.name: ItPosixPipe006
 * @tc.desc: function for SignalTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(SignalTest, ItPosixPipe006, TestSize.Level0)
{
    ItPosixPipe006();
}

#endif
} // namespace OHOS
