/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <osTest.h>

#define localhost "127.0.0.1"
#define STACK_IP localhost
#define STACK_PORT 2277
#define PEER_PORT STACK_PORT
#define PEER_IP localhost
#define BUF_SIZE (1024 * 8)
#define SRV_MSG "Hi, I am TCP server"
#define CLI_MSG "Hi, I am TCP client"

static pthread_barrier_t gBarrier;
#define Wait() pthread_barrier_wait(&gBarrier)

static int SampleTcpServer()
{
    static char gBuf[BUF_SIZE + 1] = { 0 };
    int sfd = -1, lsfd = -1;
    struct sockaddr_in srvAddr = { 0 };
    struct sockaddr_in clnAddr = { 0 };
    socklen_t clnAddrLen = sizeof(clnAddr);
    struct msghdr msg = { 0 };
    struct iovec iov[2] = { };
    int ret = 0, i = 0;

    /* tcp server */
    lsfd = socket(AF_INET, SOCK_STREAM, 0);
    LogPrintln("create listen socket inet stream: %d", lsfd);
    ICUNIT_ASSERT_NOT_EQUAL(lsfd, -1, lsfd);

    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(STACK_IP);
    srvAddr.sin_port = htons(STACK_PORT);
    ret = bind(lsfd, (struct sockaddr*)&srvAddr, sizeof(srvAddr));
    LogPrintln("bind socket %d to %s:%d: %d", lsfd, inet_ntoa(srvAddr.sin_addr), ntohs(srvAddr.sin_port), ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, Wait() + ret);

    ret = listen(lsfd, 0);
    LogPrintln("listen socket %d: %d", lsfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    Wait();

    sfd = accept(lsfd, (struct sockaddr*)&clnAddr, &clnAddrLen);
    LogPrintln("accept socket %d: %d <%s:%d>", lsfd, sfd, inet_ntoa(clnAddr.sin_addr), ntohs(clnAddr.sin_port));
    ICUNIT_ASSERT_NOT_EQUAL(sfd, -1, sfd);

    /* send */
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = strcpy_s(gBuf, BUF_SIZE - 1, SRV_MSG);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = send(sfd, gBuf, strlen(SRV_MSG), 0);
    LogPrintln("send on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, strlen(SRV_MSG), ret);

    /* recv */
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = recv(sfd, gBuf, sizeof(gBuf), 0);
    LogPrintln("recv on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, strlen(CLI_MSG), ret);

    Wait();

    /* sendmsg */
    clnAddr.sin_family = AF_INET;
    clnAddr.sin_addr.s_addr = inet_addr(PEER_IP);
    clnAddr.sin_port = htons(PEER_PORT);
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = strcpy_s(gBuf, BUF_SIZE - 1, SRV_MSG);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    msg.msg_name = &clnAddr;
    msg.msg_namelen = sizeof(clnAddr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    iov[0].iov_base = gBuf;
    iov[0].iov_len = strlen(SRV_MSG);
    iov[1].iov_base = gBuf;
    iov[1].iov_len = strlen(SRV_MSG);
    ret = sendmsg(sfd, &msg, 0);
    LogPrintln("sendmsg on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, 2 * strlen(SRV_MSG), ret);

    Wait();

    /* recvmsg */
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    msg.msg_name = &clnAddr;
    msg.msg_namelen = sizeof(clnAddr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    iov[0].iov_base = gBuf;
    iov[0].iov_len = sizeof(gBuf);
    ret = recvmsg(sfd, &msg, 0);
    LogPrintln("recvmsg on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, 2 * strlen(CLI_MSG), ret);

    ret = shutdown(sfd, SHUT_RDWR);
    LogPrintln("shutdown socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    close(sfd);
    close(lsfd);
    return 0;
}

static int SampleTcpClient()
{
    static char gBuf[BUF_SIZE + 1] = { 0 };
    int sfd = -1;
    struct sockaddr_in srvAddr = { 0 };
    struct sockaddr_in clnAddr = { 0 };
    socklen_t clnAddrLen = sizeof(clnAddr);
    int ret = 0, i = 0;
    struct msghdr msg = { 0 };
    struct iovec iov[2] = { };
    struct sockaddr addr;
    socklen_t addrLen = sizeof(addr);

    /* tcp client connection */
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    LogPrintln("create client socket inet stream: %d", sfd);
    ICUNIT_ASSERT_NOT_EQUAL(sfd, -1, sfd);

    Wait();

    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(PEER_IP);
    srvAddr.sin_port = htons(PEER_PORT);
    ret = connect(sfd, (struct sockaddr*)&srvAddr, sizeof(srvAddr));
    LogPrintln("connect socket %d to %s:%d: %d", sfd, inet_ntoa(srvAddr.sin_addr), ntohs(srvAddr.sin_port), ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* test getpeername */
    ret = getpeername(sfd, &addr, &addrLen);
    LogPrintln("getpeername %d %s:%d: %d", sfd, inet_ntoa(((struct sockaddr_in*)&addr)->sin_addr), ntohs(((struct sockaddr_in*)&addr)->sin_port), ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(addrLen, sizeof(struct sockaddr_in), addrLen);
    ICUNIT_ASSERT_EQUAL(((struct sockaddr_in*)&addr)->sin_addr.s_addr,
        inet_addr(PEER_IP), ((struct sockaddr_in*)&addr)->sin_addr.s_addr);

    /* test getsockname */
    ret = getsockname(sfd, &addr, &addrLen);
    LogPrintln("getsockname %d %s:%d: %d", sfd, inet_ntoa(((struct sockaddr_in*)&addr)->sin_addr), ntohs(((struct sockaddr_in*)&addr)->sin_port), ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(addrLen, sizeof(struct sockaddr_in), addrLen);
    ICUNIT_ASSERT_EQUAL(((struct sockaddr_in*)&addr)->sin_addr.s_addr,
        inet_addr(STACK_IP), ((struct sockaddr_in*)&addr)->sin_addr.s_addr);

    /* send */
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = strcpy_s(gBuf, BUF_SIZE - 1, CLI_MSG);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = send(sfd, gBuf, strlen(CLI_MSG), 0);
    LogPrintln("send on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, strlen(CLI_MSG), ret);

    /* recv */
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = recv(sfd, gBuf, sizeof(gBuf), 0);
    LogPrintln("recv on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, strlen(SRV_MSG), ret);

    Wait();

    /* sendmsg */
    clnAddr.sin_family = AF_INET;
    clnAddr.sin_addr.s_addr = inet_addr(PEER_IP);
    clnAddr.sin_port = htons(PEER_PORT);
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = strcpy_s(gBuf, BUF_SIZE - 1, CLI_MSG);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    msg.msg_name = &clnAddr;
    msg.msg_namelen = sizeof(clnAddr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    iov[0].iov_base = gBuf;
    iov[0].iov_len = strlen(CLI_MSG);
    iov[1].iov_base = gBuf;
    iov[1].iov_len = strlen(CLI_MSG);
    ret = sendmsg(sfd, &msg, 0);
    LogPrintln("sendmsg on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, 2 * strlen(CLI_MSG), ret);

    Wait();

    /* recvmsg */
    ret = memset_s(gBuf, BUF_SIZE - 1, 0, BUF_SIZE - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gBuf[BUF_SIZE - 1] = '\0';
    ret = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    msg.msg_name = &clnAddr;
    msg.msg_namelen = sizeof(clnAddr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    iov[0].iov_base = gBuf;
    iov[0].iov_len = sizeof(gBuf);
    ret = recvmsg(sfd, &msg, 0);
    LogPrintln("recvmsg on socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, 2 * strlen(SRV_MSG), ret);

    ret = shutdown(sfd, SHUT_RDWR);
    LogPrintln("shutdown socket %d: %d", sfd, ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    close(sfd);
    return 0;
}

static void* TcpServerRoutine(void *p)
{
    int ret = SampleTcpServer();
    return (void*)(intptr_t)ret;
}

static void* TcpClientRoutine(void *p)
{
    int ret = SampleTcpClient();
    return (void*)(intptr_t)ret;
}

static int TcpTest()
{
    int ret;
    void *sret = NULL;
    void *cret = NULL;
    pthread_t srv, cli;
    pthread_attr_t attr;

    ret = pthread_barrier_init(&gBarrier, 0, 2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&srv, &attr, TcpServerRoutine, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&cli, &attr, TcpClientRoutine, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(cli, &cret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LogPrintln("client finish");

    ret = pthread_join(srv, &sret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LogPrintln("server finish");

    ret = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_barrier_destroy(&gBarrier);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return (int)(intptr_t)(sret) + (int)(intptr_t)(cret);
}

void NetSocketTest003(void)
{
    TEST_ADD_CASE(__FUNCTION__, TcpTest, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
