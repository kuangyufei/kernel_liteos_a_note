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
    int value = *((int*)arg);
    if (value != CHILD_FUNC_ARG) {
        return EXIT_CODE_ERRNO_1;
    }
    sleep(1);
    return 0;
}

void ItTimeContainer001(void)
{
    int ret;
    int status;
    char *containerType = "time";
    char *containerType1 = "time_for_children";

    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    std::cout << getpid() << std::endl;
    auto linkBuffer = ReadlinkContainer(getpid(), containerType);
    auto linkBuffer1 = ReadlinkContainer(getpid(), containerType1);
    ret = linkBuffer.compare(linkBuffer1);
    ASSERT_EQ(ret, 0);

    int arg = CHILD_FUNC_ARG;
    auto pid = clone(childFunc, stackTop, CLONE_NEWTIME | SIGCHLD, &arg);
    ASSERT_TRUE(pid != -1);

    auto linkBuffer2 = ReadlinkContainer(pid, containerType);
    ret = linkBuffer.compare(linkBuffer2);
    ASSERT_EQ(ret, 0);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
