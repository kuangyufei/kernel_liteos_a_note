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
using namespace std;

static int OpendirCheck(void)
{
    DIR *dir = opendir("/proc");
    if (dir == nullptr) {
        return EXIT_CODE_ERRNO_1;
    }
    closedir(dir);
    return 0;
}

static int ChildFunc(void *arg)
{
    int ret = 0;
    ret = OpendirCheck();
    if (ret == 0) {
        return EXIT_CODE_ERRNO_1;
    }

    return 0;
}

static int TestFunc(void *arg)
{
    int ret = 0;
    int fd;
    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == nullptr) {
        return EXIT_CODE_ERRNO_1;
    }
    char *stackTop = stack + STACK_SIZE;

    ret = OpendirCheck();
    if (ret != 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = chroot("/system/etc");
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = OpendirCheck();
    if (ret == 0) {
        return EXIT_CODE_ERRNO_4;
    }
    fd = open("/PCID.sc", O_RDONLY);
    if (fd == -1) {
        return EXIT_CODE_ERRNO_5;
    }
    close(fd);
    sleep(1);

    auto pid = clone(ChildFunc, stackTop, SIGCHLD, arg);
    if (pid == -1) {
        return EXIT_CODE_ERRNO_6;
    }
    int status;
    ret = waitpid(pid, &status, 0);
    ret = WIFEXITED(status);
    int exitCode = WEXITSTATUS(status);
    if (exitCode != 0) {
        return EXIT_CODE_ERRNO_7;
    }

    return 0;
}

void ItContainerChroot002(void)
{
    int ret = 0;
    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    ASSERT_TRUE(stack != nullptr);
    char *stackTop = stack + STACK_SIZE;

    int arg = CHILD_FUNC_ARG;
    auto pid = clone(TestFunc, stackTop, SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    int status;
    ret = waitpid(pid, &status, 0);
    ret = WIFEXITED(status);
    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
