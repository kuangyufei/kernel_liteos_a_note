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
#include "It_process_fs_test.h"

static int const configLen = 16;
static int const invalidNum = 2;
static const int CHILD_FUNC_ARG = 0x2088;
const int  STACK_SIZE = (1024 * 1024);

static int childFunc(void *arg)
{
    (void)arg;
    sleep(2); /* 2: delay 2s */

    return 0;
}

void ItProcessFs016(void)
{
    std::string path = "/proc/sys/user/max_uts_container";
    int fd = open(path.c_str(), O_WRONLY);
    ASSERT_NE(fd, -1);

    char buf[configLen];
    (void)sprintf(buf, "%d", invalidNum);
    size_t ret = write(fd, buf, (strlen(buf) + 1));
    ASSERT_NE(ret, -1);

    int arg = CHILD_FUNC_ARG;

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;

    auto pid = clone(childFunc, stackTop, CLONE_NEWUTS, &arg);
    ASSERT_NE(pid, -1);
    pid = clone(childFunc, stackTop, CLONE_NEWUTS, &arg);
    ASSERT_NE(pid, -1);

    pid = clone(childFunc, stackTop, CLONE_NEWUTS, &arg);
    ASSERT_EQ(pid, -1);

    (void)close(fd);
}
