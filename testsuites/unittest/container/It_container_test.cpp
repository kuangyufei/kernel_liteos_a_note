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
#include <climits>
#include <gtest/gtest.h>
#include <cstdio>
#include "It_container_test.h"

const char *USERDATA_DIR_NAME = "/userdata";
const char *ACCESS_FILE_NAME  = "/userdata/mntcontainertest";
const char *USERDATA_DEV_NAME = "/dev/mmcblk0p2";
const char *FS_TYPE           = "vfat";

const int BIT_ON_RETURN_VALUE  = 8;
const int STACK_SIZE           = 1024 * 1024;
const int CHILD_FUNC_ARG       = 0x2088;

int ChildFunction(void *args)
{
    (void)args;
    const int sleep_time = 2;
    sleep(sleep_time);
    return 0;
}

pid_t CloneWrapper(int (*func)(void *), int flag, void *args)
{
    pid_t pid;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    if (stack == MAP_FAILED) {
        return -1;
    }
    char *stackTop = stack + STACK_SIZE;

    pid = clone(func, stackTop, flag, args);
    munmap(stack, STACK_SIZE);
    return pid;
}

std::string GenContainerLinkPath(int pid, const std::string& containerType)
{
    std::ostringstream buf;
    buf << "/proc/" << pid << "/container/" << containerType;
    return buf.str();
}

std::string ReadlinkContainer(int pid, const std::string& containerType)
{
    char buf[PATH_MAX];
    auto path = GenContainerLinkPath(pid, containerType);

    struct stat sb;
    int ret = lstat(path.data(), &sb);
    if (ret == -1) {
        throw std::exception();
    }

    size_t bufsiz = sb.st_size + 1;
    if (sb.st_size == 0) {
        bufsiz = PATH_MAX;
    }

    (void)memset_s(buf, PATH_MAX, 0, PATH_MAX);
    ssize_t nbytes = readlink(path.c_str(), buf, bufsiz);
    if (nbytes == -1) {
        throw std::exception();
    }
    return buf;
}

using namespace testing::ext;
namespace OHOS {
class ContainerTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}

protected:
    virtual void SetUp();
    virtual void TearDown();
};

#if defined(LOSCFG_USER_TEST_SMOKE)
HWTEST_F(ContainerTest, ItContainer001, TestSize.Level0)
{
    ItContainer001();
}

#if defined(LOSCFG_USER_TEST_PID_CONTAINER)
/**
* @tc.name: Container_Pid_Test_023
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer023, TestSize.Level0)
{
    ItPidContainer023();
}
#endif
#if defined(LOSCFG_USER_TEST_UTS_CONTAINER)
/**
* @tc.name: Container_UTS_Test_001
* @tc.desc: uts container function test case
* @tc.type: FUNC
* @tc.require: issueI6A7C8
* @tc.author:
*/
HWTEST_F(ContainerTest, ItUtsContainer001, TestSize.Level0)
{
    ItUtsContainer001();
}
/**
* @tc.name: Container_UTS_Test_002
* @tc.desc: uts container function test case
* @tc.type: FUNC
* @tc.require: issueI6A7C8
* @tc.author:
*/
HWTEST_F(ContainerTest, ItUtsContainer002, TestSize.Level0)
{
    ItUtsContainer002();
}
#endif
#endif

#if defined(LOSCFG_USER_TEST_FULL)
#if defined(LOSCFG_USER_TEST_PID_CONTAINER)
/**
* @tc.name: Container_Pid_Test_001
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer001, TestSize.Level0)
{
    ItPidContainer001();
}

/**
* @tc.name: Container_Pid_Test_002
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer002, TestSize.Level0)
{
    ItPidContainer002();
}

/**
* @tc.name: Container_Pid_Test_003
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer003, TestSize.Level0)
{
    ItPidContainer003();
}

/**
* @tc.name: Container_Pid_Test_004
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer004, TestSize.Level0)
{
    ItPidContainer004();
}

/**
* @tc.name: Container_Pid_Test_006
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer006, TestSize.Level0)
{
    ItPidContainer006();
}

/**
* @tc.name: Container_Pid_Test_007
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer007, TestSize.Level0)
{
    ItPidContainer007();
}

/**
* @tc.name: Container_Pid_Test_008
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer008, TestSize.Level0)
{
    ItPidContainer008();
}

/**
* @tc.name: Container_Pid_Test_009
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer009, TestSize.Level0)
{
    ItPidContainer009();
}

/**
* @tc.name: Container_Pid_Test_010
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer010, TestSize.Level0)
{
    ItPidContainer010();
}

/**
* @tc.name: Container_Pid_Test_011
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer011, TestSize.Level0)
{
    ItPidContainer011();
}

/**
* @tc.name: Container_Pid_Test_012
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer012, TestSize.Level0)
{
    ItPidContainer012();
}

/**
* @tc.name: Container_Pid_Test_013
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer013, TestSize.Level0)
{
    ItPidContainer013();
}

/**
* @tc.name: Container_Pid_Test_014
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer014, TestSize.Level0)
{
    ItPidContainer014();
}

/**
* @tc.name: Container_Pid_Test_015
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer015, TestSize.Level0)
{
    ItPidContainer015();
}

/**
* @tc.name: Container_Pid_Test_016
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer016, TestSize.Level0)
{
    ItPidContainer016();
}

/**
* @tc.name: Container_Pid_Test_017
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer017, TestSize.Level0)
{
    ItPidContainer017();
}

/**
* @tc.name: Container_Pid_Test_018
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer018, TestSize.Level0)
{
    ItPidContainer018();
}

/**
* @tc.name: Container_Pid_Test_019
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer019, TestSize.Level0)
{
    ItPidContainer019();
}

/**
* @tc.name: Container_Pid_Test_020
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer020, TestSize.Level0)
{
    ItPidContainer020();
}

/**
* @tc.name: Container_Pid_Test_021
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer021, TestSize.Level0)
{
    ItPidContainer021();
}

/**
* @tc.name: Container_Pid_Test_022
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer022, TestSize.Level0)
{
    ItPidContainer022();
}

/**
* @tc.name: Container_Pid_Test_024
* @tc.desc: pid container function test case
* @tc.type: FUNC
* @tc.require: issueI68LVW
* @tc.author:
*/
HWTEST_F(ContainerTest, ItPidContainer024, TestSize.Level0)
{
    ItPidContainer024();
}
#endif
#if defined(LOSCFG_USER_TEST_UTS_CONTAINER)
/**
* @tc.name: Container_UTS_Test_003
* @tc.desc: uts container function test case
* @tc.type: FUNC
* @tc.require: issueI6A7C8
* @tc.author:
*/
HWTEST_F(ContainerTest, ItUtsContainer003, TestSize.Level0)
{
    ItUtsContainer003();
}
#endif
#endif
} // namespace OHOS

namespace OHOS {
void ContainerTest::SetUp()
{
    mode_t mode = 0;
    (void)mkdir(ACCESS_FILE_NAME, S_IFDIR | mode);
}
void ContainerTest::TearDown()
{
    (void)rmdir(ACCESS_FILE_NAME);
}
}
