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
#include <netinet/in.h>
#include <arpa/inet.h>
#include "It_container_test.h"

static const char *LOCALHOST = "127.0.0.1";
static const int LOCALPORT = 8003;
static const char *MSG = "Tis is UDP Test!";
static const int DATA_LEN = 128;

static int ChildFunc(void *arg)
{
    int ret;
    int sock;
    int recv_data_len;
    char recv_data[DATA_LEN];
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return EXIT_CODE_ERRNO_1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(LOCALHOST);
    addr.sin_port = htons(LOCALPORT);
    (void)memset_s(&(addr.sin_zero), sizeof(addr.sin_zero), 0, sizeof(addr.sin_zero));

    ret = bind(sock, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&addr)),
               sizeof(struct sockaddr));
    if (ret < 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = sendto(sock, MSG, DATA_LEN, 0, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&addr)),
                 (socklen_t)sizeof(addr));
    if (ret < 0) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = recvfrom(sock, recv_data, DATA_LEN, 0, nullptr, nullptr);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_4;
    }

    ret = strcmp(recv_data, MSG);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_5;
    }

    (void)close(sock);

    return 0;
}

void ItNetContainer007(void)
{
    int ret = 0;
    int status;

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;

    int arg = CHILD_FUNC_ARG;
    auto pid = clone(ChildFunc, stackTop, SIGCHLD | CLONE_NEWNET, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
