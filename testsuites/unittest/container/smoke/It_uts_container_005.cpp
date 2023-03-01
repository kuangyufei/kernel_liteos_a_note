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

static int UtsContainerTest(void *arg)
{
    (void)arg;
    pid_t callerPid;
    int childPid;
    int fd = -1;
    int ret, status, setFlag;
    char targetpath[100];
    const char *containerType = "uts";

    callerPid = getpid();
    childPid = clone(ChildFun, NULL, CLONE_NEWUTS | SIGCHLD, NULL);
    if (childPid == -1) {
        return EXIT_CODE_ERRNO_1;
    }

    auto linkBuffer1 = ReadlinkContainer(callerPid, containerType);
    if (linkBuffer1.c_str() == NULL) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = sprintf_s(targetpath, sizeof(targetpath), "/proc/%d/container/uts", childPid);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_4;
    }

    fd = open(targetpath, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        return EXIT_CODE_ERRNO_5;
    }

    setFlag = CLONE_NEWUTS;
    ret = setns(fd, setFlag);
    (void)close(fd);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_6;
    }

    auto linkBuffer2 = ReadlinkContainer(callerPid, containerType);

    ret = linkBuffer2.compare(linkBuffer1);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_7;
    }

    ret = waitpid(childPid, &status, 0);
    if (ret != childPid) {
        return EXIT_CODE_ERRNO_8;
    }

    int exitCode = WEXITSTATUS(status);
    if (exitCode != EXIT_CODE_ERRNO_3) {
        return EXIT_CODE_ERRNO_9;
    }

    ret = setns(fd, setFlag);
    if (ret != -1) {
        return EXIT_CODE_ERRNO_10;
    }
    return 0;
}

void ItUtsContainer005(void)
{
    int ret;

    int arg = CHILD_FUNC_ARG;
    auto pid = CloneWrapper(UtsContainerTest, CLONE_NEWUTS, &arg);
    ASSERT_NE(pid, -1);

    int status;
    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
