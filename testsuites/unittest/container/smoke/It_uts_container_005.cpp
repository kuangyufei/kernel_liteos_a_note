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

const int TIMER_INTERVAL_3S = 2;

static int ChildFun(void *p)
{
    (void)p;
    sleep(TIMER_INTERVAL_3S);
    return EXIT_CODE_ERRNO_3;
}

void ItUtsContainer005(void)
{
    pid_t callerPid;
    int childPid;
    int fd = -1;
    int ret;
    int status;
    int setFlag;
    char targetpath[100];
    char old_uts_link[100];
    char new_uts_link[100];
    const char *containerType = "uts";

    callerPid = getpid();
    childPid = clone(ChildFun, NULL, CLONE_NEWUTS | SIGCHLD, NULL);
    ASSERT_NE(childPid, -1);

    auto linkBuffer = ReadlinkContainer(callerPid, containerType);
    ASSERT_TRUE(linkBuffer.c_str() != NULL);
    ret = sprintf_s(old_uts_link, sizeof(old_uts_link), "%s", linkBuffer.c_str());
    ASSERT_NE(ret, -1);

    ret = sprintf_s(targetpath, sizeof(targetpath), "/proc/%d/container/uts", childPid);
    ASSERT_NE(ret, -1);
    fd = open(targetpath, O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);

    setFlag = CLONE_NEWUTS;
    ret = setns(fd, setFlag);
    ASSERT_NE(ret, -1);

    /* NOTE: close fd, otherwise test fail */
    ret = close(fd);
    fd = -1;
    ASSERT_NE(ret, -1);

    linkBuffer = ReadlinkContainer(callerPid, containerType);

    ret = sprintf_s(new_uts_link, sizeof(new_uts_link), "%s", linkBuffer.c_str());
    ASSERT_NE(ret, -1);
    ASSERT_STRNE(old_uts_link, new_uts_link);

    ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, EXIT_CODE_ERRNO_3);

    ret = setns(fd, setFlag);
    ASSERT_EQ(ret, -1);
}
