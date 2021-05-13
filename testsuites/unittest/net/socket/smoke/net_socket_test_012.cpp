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


#include <osTest.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <signal.h>

static struct iovec gIov[IOV_MAX + 1];

static int SocketNullTestInternal(int sfd)
{
    int ret;
    struct sockaddr addr = {0};
    struct sockaddr *bad = (struct sockaddr *)0xbad;
    socklen_t addrlen = sizeof(addr);
    socklen_t zero = 0;
    struct msghdr message = {0};
    void *badUserAddr = (void*)0x3effffff;

    /**
     * accept
     */
    ret = accept(sfd, NULL, NULL);
    LogPrintln("accept: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = accept(sfd, NULL, &addrlen);
    LogPrintln("accept: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = accept(sfd, bad, &zero);
    LogPrintln("accept: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = accept(sfd, &addr, NULL);
    LogPrintln("accept: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * bind
     */
    ret = bind(sfd, NULL, addrlen);
    LogPrintln("bind: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = bind(sfd, bad, 0);
    LogPrintln("bind: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * getpeername
     */
    ret = getpeername(sfd, NULL, NULL);
    LogPrintln("getpeername: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = getpeername(sfd, NULL, &addrlen);
    LogPrintln("getpeername: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = getpeername(sfd, &addr, NULL);
    LogPrintln("getpeername: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    zero = 0;
    ret = getpeername(sfd, bad, &zero);
    LogPrintln("getpeername: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);
    ICUNIT_ASSERT_EQUAL(errno, ENOTCONN, errno);

    /**
     * getsockname
     */
    ret = getsockname(sfd, NULL, NULL);
    LogPrintln("getsockname: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = getsockname(sfd, NULL, &addrlen);
    LogPrintln("getsockname: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = getsockname(sfd, &addr, NULL);
    LogPrintln("getsockname: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    zero = 0;
    ret = getsockname(sfd, bad, &zero);
    LogPrintln("getsockname: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    /**
     * getsockopt
     */
    ret = getsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, NULL, NULL);
    LogPrintln("getsockopt: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = getsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, NULL, &addrlen);
    LogPrintln("getsockopt: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = getsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &addr, NULL);
    LogPrintln("getsockopt: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    zero = 0;
    ret = getsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, bad, &zero);
    LogPrintln("getsockopt: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    /**
     * setsockopt
     */
    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, NULL, addrlen);
    LogPrintln("setsockopt: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, bad, 0);
    LogPrintln("setsockopt: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, bad, addrlen);
    LogPrintln("setsockopt: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * connect
     */
    ret = connect(sfd, NULL, addrlen);
    LogPrintln("connect: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = connect(sfd, bad, 0);
    LogPrintln("connect: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = connect(sfd, bad, addrlen);
    LogPrintln("connect: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * recv
     */
    ret = recv(sfd, NULL, 1, MSG_DONTWAIT);
    LogPrintln("recv: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * recvfrom
     */
    ret = recvfrom(sfd, NULL, 1, MSG_DONTWAIT, NULL, NULL);
    LogPrintln("recvfrom: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = recvfrom(sfd, NULL, 1, MSG_DONTWAIT, NULL, &addrlen);
    LogPrintln("recvfrom: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = recvfrom(sfd, NULL, 1, MSG_DONTWAIT, &addr, NULL);
    LogPrintln("recvfrom: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    zero = 0;
    ret = recvfrom(sfd, NULL, 1, MSG_DONTWAIT, bad, &zero);
    LogPrintln("recvfrom: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * recvmsg
     */
    ret = recvmsg(sfd, NULL, MSG_DONTWAIT);
    LogPrintln("recvmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = NULL;
    message.msg_iovlen = 1;
    ret = recvmsg(sfd, &message, MSG_DONTWAIT);
    LogPrintln("recvmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = gIov;
    message.msg_iovlen = 1 + IOV_MAX;
    ret = recvmsg(sfd, &message, MSG_DONTWAIT);
    LogPrintln("recvmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = (struct iovec *)((void *)0xbad);
    message.msg_iovlen = 1;
    ret = recvmsg(sfd, &message, MSG_DONTWAIT);
    LogPrintln("recvmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = (struct iovec *)((void *)0xbad);
    message.msg_iovlen = 0;
    ret = recvmsg(sfd, &message, MSG_DONTWAIT);
    LogPrintln("recvmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * send
     */
    ret = send(sfd, NULL, 1, MSG_NOSIGNAL);
    LogPrintln("send: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = send(sfd, bad, 0, MSG_NOSIGNAL);
    LogPrintln("send: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    /**
     * sendmsg
     */
    ret = sendmsg(sfd, NULL, MSG_NOSIGNAL);
    LogPrintln("sendmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = NULL;
    message.msg_iovlen = 1;
    ret = sendmsg(sfd, &message, MSG_NOSIGNAL);
    LogPrintln("sendmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = gIov;
    message.msg_iovlen = IOV_MAX + 1;
    ret = sendmsg(sfd, &message, MSG_NOSIGNAL);
    LogPrintln("sendmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = gIov;
    message.msg_iovlen = (~0UL / sizeof(struct iovec)) + 2; // Test overflow
    ret = sendmsg(sfd, &message, MSG_NOSIGNAL);
    LogPrintln("sendmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = (struct iovec *)0xbad;
    message.msg_iovlen = 1;
    ret = sendmsg(sfd, &message, MSG_NOSIGNAL);
    LogPrintln("sendmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    message.msg_iov = (struct iovec *)0xbad;
    message.msg_iovlen = 0;
    ret = sendmsg(sfd, &message, MSG_NOSIGNAL);
    LogPrintln("sendmsg: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * sendto
     */
    ret = sendto(sfd, NULL, 1, MSG_NOSIGNAL, NULL, addrlen);
    LogPrintln("sendto: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = sendto(sfd, NULL, 1, MSG_NOSIGNAL, &addr, addrlen);
    LogPrintln("sendto: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = sendto(sfd, bad, 0, MSG_NOSIGNAL, &addr, addrlen);
    LogPrintln("sendto: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = sendto(sfd, "NULL", 4, MSG_NOSIGNAL, NULL, addrlen);
    LogPrintln("sendto: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = sendto(sfd, "NULL", 4, MSG_NOSIGNAL, bad, 0);
    LogPrintln("sendto: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = sendto(sfd, "NULL", 4, MSG_NOSIGNAL, bad, addrlen);
    LogPrintln("sendto: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    /**
     * read/readv
     */
    ret = read(sfd, NULL, 1);
    LogPrintln("read: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = read(sfd, NULL, 0);
    LogPrintln("read: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno);

    ret = read(sfd, bad, 1);
    LogPrintln("read: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = read(sfd, badUserAddr, 1);
    LogPrintln("read: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = read(sfd, bad, 0);
    LogPrintln("read: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = readv(sfd, (struct iovec *)bad, 0);
    LogPrintln("readv: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = readv(sfd, (struct iovec *)bad, 1);
    LogPrintln("readv: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = readv(sfd, gIov, 0);
    LogPrintln("readv: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = readv(sfd, gIov, IOV_MAX + 1);
    LogPrintln("readv: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    gIov[0].iov_base = bad;
    gIov[0].iov_len = 1;
    ret = readv(sfd, gIov, 1);
    LogPrintln("readv: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    gIov[0].iov_base = bad;
    gIov[0].iov_len = 0;
    ret = readv(sfd, gIov, 1);
    LogPrintln("readv: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    /**
     * write/writev
     */
    ret = write(sfd, NULL, 1);
    LogPrintln("write: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = write(sfd, NULL, 0);
    LogPrintln("write: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = write(sfd, bad, 1);
    LogPrintln("write: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = write(sfd, badUserAddr, 1);
    LogPrintln("write: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = write(sfd, bad, 0);
    LogPrintln("write: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = writev(sfd, (struct iovec *)bad, 0);
    LogPrintln("writev: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = writev(sfd, (struct iovec *)bad, 1);
    LogPrintln("writev: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    ret = writev(sfd, gIov, 0);
    LogPrintln("writev: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    ret = writev(sfd, gIov, IOV_MAX + 1);
    LogPrintln("writev: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    gIov[0].iov_base = bad;
    gIov[0].iov_len = 1;
    ret = writev(sfd, gIov, 1);
    LogPrintln("writev: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, errno);

    gIov[0].iov_base = bad;
    gIov[0].iov_len = 0;
    ret = writev(sfd, gIov, 1);
    LogPrintln("writev: %d, errno=%d", ret, errno);
    ICUNIT_ASSERT_EQUAL((ret == 0 || ret == -1), 1, errno);

    return ICUNIT_SUCCESS;
}

static int SocketSetNonBlock(int sfd)
{
    int ret = fcntl(sfd, F_GETFL, 0);
    ICUNIT_ASSERT_EQUAL(ret < 0, 0, errno);

    ret = fcntl(sfd, F_SETFL, ret | O_NONBLOCK);
    ICUNIT_ASSERT_EQUAL(ret < 0, 0, errno);

    return ICUNIT_SUCCESS;
}

static int SocketNullTest(void)
{
    int sfd;

    sighandler_t oldHdl = signal(SIGPIPE, SIG_IGN);

    sfd = socket(PF_INET, SOCK_DGRAM, 0);
    ICUNIT_ASSERT_NOT_EQUAL(sfd, -1, errno);
    LogPrintln("UDP socket: %d", sfd);
    (void)SocketSetNonBlock(sfd);
    (void)SocketNullTestInternal(sfd);
    (void)close(sfd);

    sfd = socket(PF_INET, SOCK_STREAM, 0);
    ICUNIT_ASSERT_NOT_EQUAL(sfd, -1, errno);
    LogPrintln("TCP socket: %d", sfd);
    (void)SocketSetNonBlock(sfd);
    (void)SocketNullTestInternal(sfd);
    (void)close(sfd);

    (void)signal(SIGPIPE, oldHdl);

    return ICUNIT_SUCCESS;
}

void NetSocketTest012(void)
{
    TEST_ADD_CASE(__FUNCTION__, SocketNullTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}

