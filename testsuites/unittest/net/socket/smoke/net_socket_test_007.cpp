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

#include <arpa/inet.h>
#include <osTest.h>

#define SRV_MSG                 "Hi, I am TCP server"
#define INVALID_USER_ADDR       0X1200000
#define INVALID_KERNEL_ADDR     0x48000000

static int TcpTest()
{
    int ret;
    int lsfd = -1;
    struct msghdr msg = { 0 };

    lsfd = socket(AF_INET, SOCK_STREAM, 0);
    ICUNIT_ASSERT_NOT_EQUAL(lsfd, -1, lsfd);

    ret = bind(lsfd, (const struct sockaddr *)INVALID_USER_ADDR, sizeof(struct sockaddr_in));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = bind(lsfd, (const struct sockaddr *)INVALID_KERNEL_ADDR, sizeof(struct sockaddr_in));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = connect(lsfd, (struct sockaddr*)INVALID_USER_ADDR, sizeof(struct sockaddr_in));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = connect(lsfd, (struct sockaddr*)INVALID_KERNEL_ADDR, sizeof(struct sockaddr_in));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = accept(lsfd, (struct sockaddr*)INVALID_USER_ADDR, (socklen_t *)INVALID_USER_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = accept(lsfd, (struct sockaddr*)INVALID_KERNEL_ADDR, (socklen_t *)INVALID_KERNEL_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = getsockname(lsfd, (struct sockaddr*)INVALID_USER_ADDR, (socklen_t *)INVALID_USER_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = getsockname(lsfd, (struct sockaddr*)INVALID_KERNEL_ADDR, (socklen_t *)INVALID_KERNEL_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = getpeername(lsfd, (struct sockaddr*)INVALID_USER_ADDR, (socklen_t *)INVALID_USER_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = getpeername(lsfd, (struct sockaddr*)INVALID_KERNEL_ADDR, (socklen_t *)INVALID_KERNEL_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = send(lsfd, (char *)INVALID_USER_ADDR, strlen(SRV_MSG), 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = send(lsfd, (char *)INVALID_KERNEL_ADDR, strlen(SRV_MSG), 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = sendto(lsfd, (char *)INVALID_USER_ADDR, strlen(SRV_MSG), 0, (struct sockaddr*)INVALID_USER_ADDR,
        (socklen_t)sizeof(struct sockaddr_in));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = sendto(lsfd, (char *)INVALID_KERNEL_ADDR, strlen(SRV_MSG), 0, (struct sockaddr*)INVALID_KERNEL_ADDR,
        (socklen_t)sizeof(struct sockaddr_in));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = recv(lsfd, (char *)INVALID_USER_ADDR, sizeof(SRV_MSG), 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = recv(lsfd, (char *)INVALID_KERNEL_ADDR, sizeof(SRV_MSG), 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = recvfrom(lsfd, (char *)INVALID_USER_ADDR, sizeof(SRV_MSG), 0, (struct sockaddr*)INVALID_USER_ADDR,
        (socklen_t *)INVALID_USER_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = recvfrom(lsfd, (char *)INVALID_KERNEL_ADDR, sizeof(SRV_MSG), 0, (struct sockaddr*)INVALID_KERNEL_ADDR,
        (socklen_t *)INVALID_KERNEL_ADDR);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = setsockopt(lsfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)INVALID_USER_ADDR, (socklen_t)sizeof(struct timeval));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = setsockopt(lsfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)INVALID_KERNEL_ADDR, (socklen_t)sizeof(struct timeval));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = getsockopt(lsfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)INVALID_USER_ADDR, (socklen_t*)sizeof(struct timeval));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = getsockopt(lsfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)INVALID_KERNEL_ADDR, (socklen_t*)sizeof(struct timeval));
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = sendmsg(lsfd, (struct msghdr *)INVALID_USER_ADDR, 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = sendmsg(lsfd, (struct msghdr *)INVALID_KERNEL_ADDR, 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    msg.msg_name = (char *)INVALID_USER_ADDR;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = (struct iovec *)INVALID_KERNEL_ADDR;
    msg.msg_iovlen = 2;
    ret = recvmsg(lsfd, (struct msghdr *)&msg, 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = recvmsg(lsfd, (struct msghdr *)&msg, 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

#ifdef TEST_BIG_MEM
    const int bufSiz = 0x1400000; // 20M
    void *buf = malloc(bufSiz);
    if (!buf) {
        printf("malloc 20M fail\n");
    } else {
        printf("malloc 20M success\n");
        ret = memset_s(buf, bufSiz, 0, bufSiz);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
        ret = send(lsfd, buf, bufSiz, 0);
        printf("send ret = %d, errno :%d\n", ret, errno);
        ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT);
    }

EXIT:
    if (buf != NULL) {
        free(buf);
    }
#endif
    close(lsfd);
    return 0;
}

void NetSocketTest007(void)
{
    TEST_ADD_CASE(__FUNCTION__, TcpTest, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
