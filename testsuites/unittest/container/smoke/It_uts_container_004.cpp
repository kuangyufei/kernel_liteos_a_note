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
#include "sys/utsname.h"

static int ChildFunc(void *arg)
{
    (void)arg;
    int ret;

    ret = unshare(CLONE_NEWUTS);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_1;
    }

    struct utsname newName;
    ret = uname(&newName);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_2;
    }

    const char *name = "TestHostName";
    ret = sethostname(name, strlen(name));
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }

    struct utsname newName1;
    ret = uname(&newName1);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_4;
    }

    ret = strcmp(newName.nodename, newName1.nodename);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_5;
    }

    return 0;
}

void ItUtsContainer004(void)
{
    int ret;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    ASSERT_TRUE(stack != NULL);
    char *stackTop = stack + STACK_SIZE;
    struct utsname oldName;

    ret = uname(&oldName);
    ASSERT_EQ(ret, 0);

    auto pid = clone(ChildFunc, stackTop, SIGCHLD, NULL);
    (void)munmap(stack, STACK_SIZE);
    ASSERT_NE(pid, -1);

    int status;
    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);

    struct utsname oldName1;
    ret = uname(&oldName1);
    ASSERT_EQ(ret, 0);

    ret = strcmp(oldName.nodename, oldName1.nodename);
    ASSERT_EQ(ret, 0);
}
