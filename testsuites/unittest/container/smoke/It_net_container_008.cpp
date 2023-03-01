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
#include <sys/socket.h>
#include <poll.h>
#include "It_container_test.h"

static const int PORT = 8004;
static const char *LOCALHOST = "127.0.0.1";

static int UdpTcpBind(int *sock1, int *sock2)
{
    int ret;
    int udp_sock;
    int tcp_sock;

    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        return EXIT_CODE_ERRNO_1;
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr(LOCALHOST);
    sa.sin_port = htons(PORT);

    ret = bind(udp_sock, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&sa)), sizeof(sa));
    if (ret != 0) {
        return EXIT_CODE_ERRNO_2;
    }

    tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_sock < 0) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = bind(tcp_sock, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&sa)), sizeof(sa));
    if (ret != 0) {
        return EXIT_CODE_ERRNO_4;
    }

    (*sock1) = udp_sock;
    (*sock2) = tcp_sock;

    return 0;
}

static int ClientFunc(void *param)
{
    (void)param;
    int ret;
    int udp_sock;
    int tcp_sock;

    ret = UdpTcpBind(&udp_sock, &tcp_sock);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_1;
    }

    (void)close(udp_sock);
    (void)close(tcp_sock);

    return ret;
}

void ItNetContainer008(void)
{
    int ret;
    int status;
    int udp_sock;
    int tcp_sock;

    ret = UdpTcpBind(&udp_sock, &tcp_sock);
    ASSERT_EQ(ret, 0);

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    int arg = CHILD_FUNC_ARG;
    int pid = clone(ClientFunc, stackTop, SIGCHLD | CLONE_NEWNET, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);

    (void)close(udp_sock);
    (void)close(tcp_sock);
}
