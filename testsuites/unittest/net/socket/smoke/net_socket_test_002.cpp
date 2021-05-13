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
#define MSG "Hi, I am UDP"
#define BUF_SIZE (1024 * 8)

static char g_buf[BUF_SIZE + 1] = { 0 };

static int UdpTest(void)
{
    int sfd;
    struct sockaddr_in srvAddr = { 0 };
    struct sockaddr_in clnAddr = { 0 };
    socklen_t clnAddrLen = sizeof(clnAddr);
    int ret = 0, i = 0;
    struct msghdr msg = { 0 };
    struct iovec iov[2] = { };

    /* socket creation */
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    ICUNIT_ASSERT_NOT_EQUAL(sfd, -1, sfd);

    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(STACK_IP);
    srvAddr.sin_port = htons(STACK_PORT);
    ret = bind(sfd, (struct sockaddr*)&srvAddr, sizeof(srvAddr));
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* send */
    clnAddr.sin_family = AF_INET;
    clnAddr.sin_addr.s_addr = inet_addr(PEER_IP);
    clnAddr.sin_port = htons(PEER_PORT);
    ret = memset_s(g_buf, BUF_SIZE, 0, BUF_SIZE);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = strcpy_s(g_buf, BUF_SIZE, MSG);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = sendto(sfd, g_buf, strlen(MSG), 0, (struct sockaddr*)&clnAddr,
        (socklen_t)sizeof(clnAddr));
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    /* recv */
    ret = memset_s(g_buf, BUF_SIZE, 0, BUF_SIZE);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = recvfrom(sfd, g_buf, sizeof(g_buf), 0, (struct sockaddr*)&clnAddr,
        &clnAddrLen);
    ICUNIT_ASSERT_EQUAL(ret, strlen(MSG), ret);

    /* sendmsg */
    clnAddr.sin_family = AF_INET;
    clnAddr.sin_addr.s_addr = inet_addr(PEER_IP);
    clnAddr.sin_port = htons(PEER_PORT);
    ret = memset_s(g_buf, BUF_SIZE, 0, BUF_SIZE);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = strcpy_s(g_buf, BUF_SIZE, MSG);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    msg.msg_name = &clnAddr;
    msg.msg_namelen = sizeof(clnAddr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    iov[0].iov_base = g_buf;
    iov[0].iov_len = strlen(MSG);
    iov[1].iov_base = g_buf;
    iov[1].iov_len = strlen(MSG);
    ret = sendmsg(sfd, &msg, 0);
    ICUNIT_ASSERT_EQUAL(ret, 2 * strlen(MSG), ret);

    /* recvmsg */
    ret = memset_s(g_buf, BUF_SIZE, 0, BUF_SIZE);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    msg.msg_name = &clnAddr;
    msg.msg_namelen = sizeof(clnAddr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    iov[0].iov_base = g_buf;
    iov[0].iov_len = sizeof(g_buf);
    ret = recvmsg(sfd, &msg, 0);
    ICUNIT_ASSERT_EQUAL(ret, 2 * strlen(MSG), ret);

    /* close socket */
    ret = close(sfd);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    return 0;
}

void NetSocketTest002(void)
{
    TEST_ADD_CASE(__FUNCTION__, UdpTest, TEST_POSIX, TEST_UDP, TEST_LEVEL0, TEST_FUNCTION);
}
