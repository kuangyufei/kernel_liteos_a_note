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
#include <sys/syscall.h>
#include <unistd.h>

__attribute__((weak)) int Gettid()
{
    return syscall(SYS_gettid);
}

#include "it_mutex_test.h"

using namespace testing::ext;
namespace OHOS {
class ProcessMutexTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: it_test_pthread_mutex_001
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex001, TestSize.Level0)
{
    ItTestPthreadMutex001();
}

/* *
 * @tc.name: it_test_pthread_mutex_002
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex002, TestSize.Level0)
{
    ItTestPthreadMutex002();
}

/* *
 * @tc.name: it_test_pthread_mutex_003
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex003, TestSize.Level0)
{
    ItTestPthreadMutex003();
}

/* *
 * @tc.name: it_test_pthread_mutex_004
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex004, TestSize.Level0)
{
    ItTestPthreadMutex004();
}

/* *
 * @tc.name: it_test_pthread_mutex_005
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex005, TestSize.Level0)
{
    ItTestPthreadMutex005();
}

/* *
 * @tc.name: it_test_pthread_mutex_006
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex006, TestSize.Level0)
{
    ItTestPthreadMutex006();
}

/* *
 * @tc.name: it_test_pthread_mutex_007
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex007, TestSize.Level0)
{
    ItTestPthreadMutex007();
}

/* *
 * @tc.name: it_test_pthread_mutex_008
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex008, TestSize.Level0)
{
    ItTestPthreadMutex008();
}

/* *
 * @tc.name: it_test_pthread_mutex_009
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex009, TestSize.Level0)
{
    ItTestPthreadMutex009();
}

/* *
 * @tc.name: it_test_pthread_mutex_010
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex010, TestSize.Level0)
{
    ItTestPthreadMutex010();
}

/* *
 * @tc.name: it_test_pthread_mutex_011
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex011, TestSize.Level0)
{
    ItTestPthreadMutex011();
}

/* *
 * @tc.name: it_test_pthread_mutex_012
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex012, TestSize.Level0)
{
    ItTestPthreadMutex012();
}

/* *
 * @tc.name: it_test_pthread_mutex_013
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex013, TestSize.Level0)
{
    ItTestPthreadMutex013();
}

/* *
 * @tc.name: it_test_pthread_mutex_014
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex014, TestSize.Level0)
{
    ItTestPthreadMutex014();
}

/* *
 * @tc.name: it_test_pthread_mutex_015
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex015, TestSize.Level0)
{
    ItTestPthreadMutex015();
}

/* *
 * @tc.name: it_test_pthread_mutex_016
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex016, TestSize.Level0)
{
    ItTestPthreadMutex016();
}

/* *
 * @tc.name: it_test_pthread_mutex_017
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex017, TestSize.Level0)
{
    ItTestPthreadMutex017();
}

#ifndef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_pthread_mutex_019
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex019, TestSize.Level0)
{
    ItTestPthreadMutex019();
}

#endif
/* *
 * @tc.name: it_test_pthread_mutex_020
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex020, TestSize.Level0)
{
    ItTestPthreadMutex020();
}

/* *
 * @tc.name: it_test_pthread_mutex_021
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex021, TestSize.Level0)
{
    ItTestPthreadMutex021();
}

/* *
 * @tc.name: it_test_pthread_mutex_022
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex022, TestSize.Level0)
{
    ItTestPthreadMutex022();
}
#endif

#if defined(LOSCFG_USER_TEST_FULL)
#ifndef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_pthread_mutex_018
 * @tc.desc: function for ProcessMutexTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex018, TestSize.Level0)
{
    ItTestPthreadMutex018();
}
#endif

/* *
 * @tc.name: it_test_pthread_mutex_023
 * @tc.desc: function for test mutexattr robust
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex023, TestSize.Level0)
{
    ItTestPthreadMutex023();
}

/* *
 * @tc.name: it_test_pthread_mutex_024
 * @tc.desc: function for test mutexattr robust:error return value
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex024, TestSize.Level0)
{
    ItTestPthreadMutex024();
}

/* *
 * @tc.name: it_test_pthread_mutex_025
 * @tc.desc: test mutexattr robust:robustness product deadlock is not set
 * @tc.type: FUNC
 */
HWTEST_F(ProcessMutexTest, ItTestPthreadMutex025, TestSize.Level0)
{
    ItTestPthreadMutex025();
}
#endif
} // namespace OHOS
