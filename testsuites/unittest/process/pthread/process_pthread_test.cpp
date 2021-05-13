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
#include "it_pthread_test.h"
#include <sys/resource.h>

using namespace testing::ext;
namespace OHOS {
class ProcessPthreadTest : public testing::Test {
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

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: it_test_pthread_001
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread001, TestSize.Level0)
{
    ItTestPthread001();
}

#ifndef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_pthread_002
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread002, TestSize.Level0)
{
    ItTestPthread002();
}

#endif
/* *
 * @tc.name: it_test_pthread_003
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread003, TestSize.Level0)
{
    ItTestPthread003();
}

/* *
 * @tc.name: it_test_pthread_004
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread004, TestSize.Level0)
{
    ItTestPthread004();
}

/* *
 * @tc.name: it_test_pthread_005
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread005, TestSize.Level0)
{
    ItTestPthread005();
}

#ifndef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_pthread_006
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread006, TestSize.Level0)
{
    ItTestPthread006();
}
#endif

/* *
 * @tc.name: it_test_pthread_007
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread007, TestSize.Level0)
{
    ItTestPthread007();
}

/* *
 * @tc.name: it_test_pthread_008
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread008, TestSize.Level0)
{
    ItTestPthread008();
}

/* *
 * @tc.name: it_test_pthread_009
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread009, TestSize.Level0)
{
    ItTestPthread009();
}

#ifndef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_pthread_010
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread010, TestSize.Level0)
{
    ItTestPthread010();
}
#endif

/* *
 * @tc.name: it_test_pthread_011
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread011, TestSize.Level0)
{
    ItTestPthread011();
}

/* *
 * @tc.name: it_test_pthread_012
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread012, TestSize.Level0)
{
    ItTestPthread012();
}

/* *
 * @tc.name: it_test_pthread_013
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread013, TestSize.Level0)
{
    ItTestPthread013();
}

#ifndef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_pthread_014
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread014, TestSize.Level0)
{
    ItTestPthread014();
}
#endif

/* *
 * @tc.name: it_test_pthread_015
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread015, TestSize.Level0)
{
    ItTestPthread015();
}

/* *
 * @tc.name: it_test_pthread_016
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread016, TestSize.Level0)
{
    ItTestPthread016();
}

/* *
 * @tc.name: it_test_pthread_017
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread017, TestSize.Level0)
{
    ItTestPthread017();
}

/* *
 * @tc.name: it_test_pthread_018
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread018, TestSize.Level0)
{
    ItTestPthread018();
}

/* *
 * @tc.name: it_test_pthread_019
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthread019, TestSize.Level0)
{
    ItTestPthread019();
}

/* *
 * @tc.name: it_test_pthread_once_001
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthreadOnce001, TestSize.Level0)
{
    ItTestPthreadOnce001();
}

/* *
 * @tc.name: it_test_pthread_atfork_001
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthreadAtfork001, TestSize.Level0)
{
    ItTestPthreadAtfork001();
}

/* *
 * @tc.name: it_test_pthread_atfork_002
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthreadAtfork002, TestSize.Level0)
{
    ItTestPthreadAtfork002();
}

/* *
 * @tc.name: it_test_pthread_cond_001
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthreadCond001, TestSize.Level0)
{
    ItTestPthreadCond001();
}

/* *
 * @tc.name: it_test_pthread_cond_002
 * @tc.desc: function for ProcessPthreadTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(ProcessPthreadTest, ItTestPthreadCond002, TestSize.Level0)
{
    ItTestPthreadCond002();
}
#endif
} // namespace OHOS
