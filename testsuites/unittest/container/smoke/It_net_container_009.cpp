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
#include <net/if.h>
#include <net/route.h>
#include "It_container_test.h"

static const char *NETMASK = "255.255.255.0";
static const char *GW = "192.168.100.1";
static const char *IFNAME = "veth0";
static const char *SERVER_IP = "192.168.100.6";
static const int SERVER_PORT = 8005;
static const char *PEER_IP = "192.168.100.5";
static const char *SERVER_MSG = "===Hi, I'm Server.===";
static const char *PEER_MSG = "===Hi, I'm Peer.===";
static const int TRY_COUNT = 5;
static const int DATA_LEN = 128;
static const int IpLen = 16;

static int TcpClient(void)
{
    int ret = 0;
    int peer;
    int try_count = TRY_COUNT;
    char recv_data[DATA_LEN];
    struct sockaddr_in server_addr;

    peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (peer < 0) {
        return EXIT_CODE_ERRNO_1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);
    (void)memset_s(&(server_addr.sin_zero), sizeof(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

    do {
        sleep(1);
        ret = connect(peer, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&server_addr)),
                      sizeof(struct sockaddr));
    } while (ret !=0 && (try_count--));

    if (ret < 0) {
        (void)close(peer);
        printf("[ERR][%s:%d] connect failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_2;
    }

    ret = send(peer, PEER_MSG, strlen(PEER_MSG) + 1, 0);
    if (ret < 0) {
        (void)close(peer);
        printf("[ERR][%s:%d] send failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_3;
    }

    ret = recv(peer, recv_data, sizeof(recv_data), 0);
    if (ret < 0) {
        (void)close(peer);
        printf("[ERR][%s:%d] recv failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_4;
    }
    (void)close(peer);

    ret = strcmp(recv_data, SERVER_MSG);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_5;
    }
    return 0;
}

static int TcpServer(void)
{
    int ret = 0;
    int server;
    int peer;
    char recv_data[DATA_LEN];
    struct sockaddr_in server_addr;

    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0) {
        return EXIT_CODE_ERRNO_1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);
    (void)memset_s(&(server_addr.sin_zero), sizeof(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

    ret = bind(server, const_cast<struct sockaddr *>(reinterpret_cast<struct sockaddr *>(&server_addr)),
               sizeof(struct sockaddr));
    if (ret != 0) {
        printf("[ERR][%s:%d] bind failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_2;
    }

    ret = listen(server, 1);
    if (ret < 0) {
        printf("[ERR][%s:%d] listen failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_3;
    }

    peer = accept(server, nullptr, nullptr);
    if (peer < 0) {
        printf("[ERR][%s:%d] accept failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_4;
    }

    (void)close(server);

    ret = recv(peer, recv_data, sizeof(recv_data), 0);
    if (ret < 0) {
        printf("[ERR][%s:%d] recv failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_5;
    }

    ret = strcmp(recv_data, PEER_MSG);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_6;
    }

    ret = send(peer, SERVER_MSG, strlen(SERVER_MSG) + 1, 0);
    if (ret < 0) {
        printf("[ERR][%s:%d] send failed, %s!\n", __FUNCTION__, __LINE__, strerror(errno));
        return EXIT_CODE_ERRNO_7;
    }

    (void)close(peer);
    return 0;
}

static void *TcpClientThread(void *arg)
{
    char newIp[IpLen] = {NULL};
    int ret = NetContainerResetNetAddr(IFNAME, PEER_IP, NETMASK, GW);
    if (ret != 0) {
        return (void *)(intptr_t)EXIT_CODE_ERRNO_1;
    }

    ret = NetContainerGetLocalIP(IFNAME, newIp, IpLen);
    if (ret != 0) {
        return (void *)(intptr_t)EXIT_CODE_ERRNO_2;
    }

    if (strncmp(PEER_IP, newIp, strlen(PEER_IP)) != 0) {
        return (void *)(intptr_t)EXIT_CODE_ERRNO_3;
    }
    printf("######## [%s:%d] %s: %s ########\n", __FUNCTION__, __LINE__, IFNAME, newIp);

    ret = TcpClient();
    return (void *)(intptr_t)ret;
}

static int ChildFunc(void *arg)
{
    char newIp[IpLen] = {NULL};
    int ret = NetContainerResetNetAddr(IFNAME, SERVER_IP, NETMASK, GW);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_8;
    }

    ret = NetContainerGetLocalIP(IFNAME, newIp, IpLen);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_9;
    }

    if (strncmp(SERVER_IP, newIp, strlen(SERVER_IP)) != 0) {
        return EXIT_CODE_ERRNO_10;
    }

    printf("######## [%s:%d] %s: %s ########\n", __FUNCTION__, __LINE__, IFNAME, newIp);
    return TcpServer();
}

VOID ItNetContainer009(void)
{
    int ret = 0;
    int status;
    void *tret = nullptr;
    pthread_t srv;
    pthread_attr_t attr;

    char *stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;

    int arg = CHILD_FUNC_ARG;
    int pid = clone(ChildFunc, stackTop, SIGCHLD | CLONE_NEWNET, &arg);
    ASSERT_NE(pid, -1);

    ret = pthread_attr_init(&attr);
    ASSERT_EQ(ret, 0);

    ret = pthread_create(&srv, &attr, TcpClientThread, nullptr);
    ASSERT_EQ(ret, 0);

    ret = pthread_join(srv, &tret);
    ASSERT_EQ(ret, 0);

    ret = (uintptr_t)tret;
    ASSERT_EQ(ret, 0);

    ret = pthread_attr_destroy(&attr);
    ASSERT_EQ(ret, 0);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
