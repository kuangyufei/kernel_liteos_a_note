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
#include <sys/stat.h>
#include <iostream>
#include <regex>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "It_container_test.h"

static const char *CONTAINER_TYPE = "net";
static const int SLEEP_SECONDS = 2;
static const int TEST_PATH_MAX = 100;

static int NewnetChildFun(void *)
{
    sleep(SLEEP_SECONDS);
    return 0;
}

static int ChildFun(void *p)
{
    (void)p;
    int ret;
    int status;
    char path[TEST_PATH_MAX];

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    int arg = CHILD_FUNC_ARG;
    auto childPid = clone(NewnetChildFun, stackTop, SIGCHLD | CLONE_NEWNET, &arg);
    if (childPid == -1) {
        return EXIT_CODE_ERRNO_1;
    }
    auto oldReadLink = ReadlinkContainer(getpid(), CONTAINER_TYPE);

    if (sprintf_s(path, TEST_PATH_MAX, "/proc/%d/container/net", childPid) < 0) {
        (void)waitpid(childPid, &status, 0);
        return EXIT_CODE_ERRNO_8;
    }
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = setns(fd, CLONE_NEWNET);
    if (ret != 0) {
        (void)close(fd);
        return EXIT_CODE_ERRNO_3;
    }

    ret = close(fd);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_4;
    }

    auto newReadLink = ReadlinkContainer(getpid(), CONTAINER_TYPE);

    ret = strcmp(oldReadLink.c_str(), newReadLink.c_str());
    if (ret == 0) {
        return EXIT_CODE_ERRNO_5;
    }

    ret = waitpid(childPid, &status, 0);
    if (ret != childPid) {
        return EXIT_CODE_ERRNO_6;
    }

    int exitCode = WEXITSTATUS(status);
    if (exitCode != 0) {
        return EXIT_CODE_ERRNO_7;
    }

    return 0;
}

void ItNetContainer004(void)
{
    int status;

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    int arg = CHILD_FUNC_ARG;
    auto childPid = clone(ChildFun, stackTop, SIGCHLD | CLONE_NEWNET, &arg);
    ASSERT_NE(childPid, -1);

    int ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
