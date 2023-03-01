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
#include "It_container_test.h"

static int const configLen = 16;
static const int MAX_CONTAINER = 10;
static const int g_buffSize = 512;
static const int g_arryLen = 4;
static const int g_readLen = 254;

static int childFunc(void *arg)
{
    (void)arg;

    int ret = unshare(CLONE_NEWUSER);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_1;
    }
    return 0;
}

void ItUserContainer007(void)
{
    std::string path = "/proc/sys/user/max_user_container";
    char *array[g_arryLen] = { nullptr };
    char buf[g_buffSize] = { 0 };
    int status = 0;

    int ret = ReadFile(path.c_str(), buf);
    ASSERT_NE(ret, -1);

    GetLine(buf, g_arryLen, g_readLen, array);

    int value = atoi(array[1] + strlen("limit: "));
    ASSERT_EQ(value, MAX_CONTAINER);

    int usedCount = atoi(array[2] + strlen("count: "));

    (void)memset_s(buf, configLen, 0, configLen);
    ret = sprintf_s(buf, configLen, "%d", usedCount + 1);
    ASSERT_GT(ret, 0);

    ret = WriteFile(path.c_str(), buf);
    ASSERT_NE(ret, -1);

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;

    auto pid1 = clone(childFunc, stackTop, CLONE_NEWUSER, NULL);
    ASSERT_NE(pid1, -1);

    ret = waitpid(pid1, &status, 0);
    ASSERT_EQ(ret, pid1);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, EXIT_CODE_ERRNO_1);

    (void)memset_s(buf, configLen, 0, configLen);
    ret = sprintf_s(buf, configLen, "%d", value);
    ASSERT_GT(ret, 0);

    ret = WriteFile(path.c_str(), buf);
    ASSERT_NE(ret, -1);

    (void)memset_s(buf, configLen, 0, configLen);
    ret = ReadFile(path.c_str(), buf);
    ASSERT_NE(ret, -1);

    GetLine(buf, g_arryLen, g_readLen, array);

    value = atoi(array[1] + strlen("limit: "));
    ASSERT_EQ(value, MAX_CONTAINER);
}
