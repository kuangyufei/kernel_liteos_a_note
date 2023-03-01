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
#include <semaphore.h>
#include <sys/socket.h>
#include <poll.h>
#include "It_container_test.h"

static const char *SERVER_IP = "127.0.0.1";
static const int SERV_PORT = 8002;
static const char *SERVER_MSG = "Hi, I'm Tcp Server!";
static const char *PEER_MSG = "Hi, I'm Tcp Client!";
static const int DATA_LEN = 128;

static int TcpClient(void *arg)
{
    int ret;
    int client;
    char buffer[DATA_LEN];
    struct sockaddr_in server_addr;

    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client < 0) {
        return EXIT_CODE_ERRNO_1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(SERV_PORT);
    (void)memset_s(&(server_addr.sin_zero), sizeof(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

    ret = connect(client, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&server_addr)),
                  sizeof(server_addr));
    if (ret < 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = send(client, PEER_MSG, strlen(PEER_MSG) + 1, 0);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = recv(client, buffer, sizeof(buffer), 0);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_4;
    }

    ret = strcmp(buffer, SERVER_MSG);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_5;
    }

    (void)close(client);

    return 0;
}

static int ChildFunc(void *)
{
    int ret;
    int server;
    int status;
    char *stack = nullptr;
    char *stackTop = nullptr;
    char buffer[DATA_LEN];
    struct sockaddr_in server_addr;

    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0) {
        return EXIT_CODE_ERRNO_1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(SERV_PORT);
    (void)memset_s(&(server_addr.sin_zero), sizeof(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

    ret = bind(server, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&server_addr)),
               sizeof(server_addr));
    if (ret < 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = listen(server, 1);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_3;
    }

    stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, nullptr);
    stackTop = stack + STACK_SIZE;

    int arg = CHILD_FUNC_ARG;
    int pid = clone(TcpClient, stackTop, SIGCHLD, &arg);
    if (pid == -1) {
        return EXIT_CODE_ERRNO_4;
    }

    int client = accept(server, nullptr, nullptr);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_5;
    }

    ret = recv(client, buffer, sizeof(buffer), 0);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_6;
    }

    ret = strcmp(buffer, PEER_MSG);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_7;
    }

    ret = send(client, SERVER_MSG, strlen(SERVER_MSG) + 1, 0);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_8;
    }

    (void)close(client);
    (void)close(server);

    ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        return EXIT_CODE_ERRNO_9;
    }

    int exitCode = WEXITSTATUS(status);
    if (exitCode != 0) {
        return EXIT_CODE_ERRNO_10;
    }

    return 0;
}

void ItNetContainer006(void)
{
    int ret = 0;
    int status;

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;

    int arg = CHILD_FUNC_ARG;
    int pid = clone(ChildFunc, stackTop, SIGCHLD | CLONE_NEWNET, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
