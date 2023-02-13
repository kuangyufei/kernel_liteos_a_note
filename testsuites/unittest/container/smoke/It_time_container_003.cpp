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

static int childFunc(void *arg)
{
    int ret;
    char *containerType = "time";
    char *containerType1 = "time_for_children";
    char pid_link[100]; /* 100: test len */
    char targetpath[100]; /* 100: test len */
    auto linkBuffer = ReadlinkContainer(getppid(), containerType);
    int fd;

    ret = sprintf_s(pid_link, 100, "%s", linkBuffer.c_str()); /* 100: test len */
    if (ret <= 0) {
        return EXIT_CODE_ERRNO_4;
    }
    ret = sprintf_s(targetpath, 100, "/proc/%d/container/time", getppid()); /* 100: test len */
    if (ret <= 0) {
        return EXIT_CODE_ERRNO_5;
    }
    fd = open(targetpath, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        return EXIT_CODE_ERRNO_1;
    }

    ret = setns(fd, CLONE_NEWTIME);
    if (ret != 0) {
        close(fd);
        return EXIT_CODE_ERRNO_2;
    }
    close(fd);
    auto linkBuffer1 = ReadlinkContainer(getpid(), containerType);
    auto linkBuffer2 = ReadlinkContainer(getpid(), containerType1);
    ret = linkBuffer1.compare(linkBuffer2);
    EXPECT_EQ(ret, 0);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }

    return 0;
}

static void TimeContainerUnshare(void)
{
    int ret;
    int status;
    char *containerType = "time";
    char *containerType1 = "time_for_children";

    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;

    ret = unshare(CLONE_NEWTIME);
    ASSERT_EQ(ret, 0);

    auto linkBuffer = ReadlinkContainer(getpid(), containerType1);

    int arg = CHILD_FUNC_ARG;
    auto pid = clone(childFunc, stackTop, CLONE_NEWTIME | SIGCHLD, &arg);
    ASSERT_TRUE(pid != -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_TRUE(ret != 0);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
    exit(0);
}

void ItTimeContainer003(void)
{
    int status = 0;
    auto pid = fork();
    ASSERT_TRUE(pid != -1);
    if (pid == 0) {
        TimeContainerUnshare();
        exit(EXIT_CODE_ERRNO_1);
    }
    auto ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, 0);
}

