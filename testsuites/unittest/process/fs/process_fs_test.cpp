/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include <cstdio>
#include <climits>
#include <gtest/gtest.h>
#include "It_process_fs_test.h"

VOID PrintTest(const CHAR *fmt, ...)
{
#ifdef PRINT_TEST
    va_list ap;
    if (g_osLkHook != nullptr) {
        va_start(ap, fmt);
        printf(fmt, ap);
        va_end(ap);
    }
#endif
}

std::string GenProcPidPath(int pid)
{
    std::ostringstream buf;
    buf << "/proc/" << pid;
    return buf.str();
}

std::string GenProcPidContainerPath(int pid, char *name)
{
    std::ostringstream buf;
    buf << "/proc/" << pid << "/container/" << name;
    return buf.str();
}


using namespace testing::ext;
namespace OHOS {
class ProcessFsTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/**
* @tc.name: Process_fs_Test_001
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AEVV
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs001, TestSize.Level0)
{
    ItProcessFs001();
}

/**
* @tc.name: Process_fs_Test_002
* @tc.desc: System memory information acquisition test
* @tc.type: FUNC
* @tc.require: issueI6AMVG
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs002, TestSize.Level0)
{
    ItProcessFs002();
}

/**
* @tc.name: Process_fs_Test_003
* @tc.desc: Get the file system type information supported by the system test
* @tc.type: FUNC
* @tc.require: issueI6AMVG
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs003, TestSize.Level0)
{
    ItProcessFs003();
}

/**
* @tc.name: Process_fs_Test_004
* @tc.desc: Process memory information acquisition test
* @tc.type: FUNC
* @tc.require: issueI6AMVG
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs004, TestSize.Level0)
{
    ItProcessFs004();
}

/**
* @tc.name: Process_fs_Test_005
* @tc.desc: Process cpup information acquisition test
* @tc.type: FUNC
* @tc.require: issueI6AMVG
* @tc.author:
*/

HWTEST_F(ProcessFsTest, ItProcessFs005, TestSize.Level0)
{
    ItProcessFs005();
}

/**
* @tc.name: Process_fs_Test_007
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AEVV
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs007, TestSize.Level0)
{
    ItProcessFs007();
}

/**
* @tc.name: Process_fs_Test_008
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6APW2
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs008, TestSize.Level0)
{
    ItProcessFs008();
}

/**
* @tc.name: Process_fs_Test_010
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AEVV
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs010, TestSize.Level0)
{
    ItProcessFs010();
}

/**
* @tc.name: Process_fs_Test_011
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6E2LG
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs011, TestSize.Level0)
{
    ItProcessFs011();
}

/**
* @tc.name: Process_fs_Test_012
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AEVV
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs012, TestSize.Level0)
{
    ItProcessFs012();
}

/**
* @tc.name: Process_fs_Test_013
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AEVV
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs013, TestSize.Level0)
{
    ItProcessFs013();
}

/**
* @tc.name: Process_fs_Test_014
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AEVV
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs014, TestSize.Level0)
{
    ItProcessFs014();
}

/**
* @tc.name: Process_fs_Test_015
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AEVV
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs015, TestSize.Level0)
{
    ItProcessFs015();
}

/**
* @tc.name: Process_fs_Test_021
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6AVMY
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs021, TestSize.Level0)
{
    ItProcessFs021();
}

/**
* @tc.name: Process_fs_Test_022
* @tc.desc: Process mount directory test
* @tc.type: FUNC
* @tc.require: issueI6B0A3
* @tc.author:
*/
HWTEST_F(ProcessFsTest, ItProcessFs022, TestSize.Level0)
{
    ItProcessFs022();
}
#endif
}
