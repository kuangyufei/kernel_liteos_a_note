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
#include "It_container_test.h"

const int CHILD_FUNC_EXITCODE = 3;
const int PATH_LENGTH = 100;

static int ChildFun(void *p)
{
    (void)p;
    const int sleepSecond = 2;
    (void)getpid();
    (void)sleep(sleepSecond);
    return CHILD_FUNC_EXITCODE;
}

void ItMntContainer008(void)
{
    pid_t parentPid;
    int childPid;
    void *pstk = nullptr;
    int fd;
    int ret;
    int status;
    char targetpath[PATH_LENGTH];
    char old_mnt_link[PATH_LENGTH];
    char new_mnt_link[PATH_LENGTH];
    std::string containerType = "mnt";
    int setFlag = CLONE_NEWNS;

    parentPid = getpid();
    pstk = malloc(STACK_SIZE);
    ASSERT_NE(pstk, nullptr);

    childPid = clone(ChildFun, (char *)pstk + STACK_SIZE, CLONE_NEWNS, nullptr);
    ASSERT_NE(childPid, -1);

    auto linkBuffer = ReadlinkContainer(parentPid, containerType.c_str());
    ret = sprintf_s(old_mnt_link, PATH_LENGTH, "%s", linkBuffer.c_str());
    ASSERT_GT(ret, 0);
    ret = sprintf_s(targetpath, PATH_LENGTH, "/proc/%d/container/mnt", childPid);
    ASSERT_GT(ret, 0);
    fd = open(targetpath, O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);

    ret = setns(fd, setFlag);
    ASSERT_EQ(ret, 0);

    ret = close(fd);
    ASSERT_EQ(ret, 0);

    linkBuffer = ReadlinkContainer(parentPid, containerType.c_str());

    ret = sprintf_s(new_mnt_link, PATH_LENGTH, "%s", linkBuffer.c_str());
    ASSERT_GT(ret, 0);
    ASSERT_STRNE(old_mnt_link, new_mnt_link);

    ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, CHILD_FUNC_EXITCODE);
}
