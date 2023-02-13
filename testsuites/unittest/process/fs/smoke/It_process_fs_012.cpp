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
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include "It_process_fs_test.h"

static const int STACK_SIZE = 1024 * 1024;
static const int CHILD_FUNC_ARG = 0x2088;

static int child_process(void *arg)
{
    (void)arg;
    pid_t pid = getpid();
    std::ostringstream buf;
    buf << "/proc/" << pid;
    DIR *dirp = opendir(buf.str().data());
    if (dirp == nullptr) {
        return 1;
    }

    (void)closedir(dirp);
    return 0;
}

void ItProcessFs012(void)
{
    int ret = 0;
    int status;
    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    int arg = CHILD_FUNC_ARG;
    pid_t pid = clone(child_process, stackTop, SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);

    std::ostringstream buf;
    buf << "/proc/" << pid;
    DIR *dirp = opendir(buf.str().data());
    ASSERT_EQ(dirp, nullptr);
}
