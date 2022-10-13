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
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 7777
#define INVALID_SOCKET -1
#define CLIENT_NUM 25

static int gFds[FD_SETSIZE];
static int gBye;

static void InitFds()
{
    for (int i = 0; i < FD_SETSIZE; ++i) {
        gFds[i] = INVALID_SOCKET;
    }
}

static void GetReadfds(fd_set *fds, int *nfd)
{
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (gFds[i] == INVALID_SOCKET) {
            continue;
        }
        FD_SET(gFds[i], fds);
        if (*nfd < gFds[i]) {
            *nfd = gFds[i];
        }
    }
}

static int AddFd(int fd)
{
    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (gFds[i] == INVALID_SOCKET) {
            gFds[i] = fd;
            return 0;
        }
    }
    return -1;
}

static void DelFd(int fd)
{
    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (gFds[i] == fd) {
            gFds[i] = INVALID_SOCKET;
        }
    }
    (void)close(fd);
}

static int CloseAllFd(void)
{
    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (gFds[i] != INVALID_SOCKET) {
            (void)close(gFds[i]);
            gFds[i] = INVALID_SOCKET;
        }
    }
    return 0;
}

static int HandleRecv(int fd)
{
    char buf[128];
    int ret = recv(fd, buf, sizeof(buf)-1, 0);
    if (ret < 0) {
        LogPrintln("[%d]Error: %s", fd, strerror(errno));
        DelFd(fd);
    } else if (ret == 0) {
        LogPrintln("[%d]Closed", fd);
        DelFd(fd);
    } else {
        buf[ret] = 0;
        LogPrintln("[%d]Received: %s", fd, buf);
        if (strstr(buf, "Bye") != NULL) {
            gBye++;
        }
    }
    return -(ret < 0);
}

static int HandleReadfds(fd_set *fds, int lsfd)
{
    int ret = 0;
    for (int i = 0; i < FD_SETSIZE; ++i) {
        if (gFds[i] == INVALID_SOCKET || !FD_ISSET(gFds[i], fds)) {
            continue;
        }
        ret += HandleRecv(gFds[i]);
    }
    return ret;
}

static void *ClientsThread(void *param)
{
    int fd;
    int thrNo = (int)(intptr_t)param;

    LogPrintln("<%d>socket client thread started", thrNo);
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = htons(SERVER_PORT);
    if (connect(fd, reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa)) == -1) {
        perror("connect");
        return NULL;
    }

    LogPrintln("[%d]<%d>connected to udp://%s:%d successful", fd, thrNo, inet_ntoa(sa.sin_addr), SERVER_PORT);

    const char *msg[] = {
            "ohos, ",
            "hello, ",
            "my name is net_socket_test_011, ",
            "see u next time, ",
            "Bye!",
    };

    for (int i = 0; i < sizeof(msg) / sizeof(msg[0]); ++i) {
        if (send(fd, msg[i], strlen(msg[i]), 0) < 0) {
            LogPrintln("[%d]<%d>send msg [%s] fail", fd, thrNo, msg[i]);
        }
    }

    (void)shutdown(fd, SHUT_RDWR);
    (void)close(fd);
    return param;
}

static int StartClients(pthread_t *cli, int cliNum)
{
    int ret;
    pthread_attr_t attr = {0};
    struct sched_param param = { 0 };
    int policy;
    ret = pthread_getschedparam(pthread_self(), &policy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    for (int i = 0; i < cliNum; ++i) {
        ret = pthread_attr_init(&attr);
        param.sched_priority = param.sched_priority + 1;
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedparam(&attr, &param);

        ret = pthread_create(&cli[i], &attr, ClientsThread, (void *)(intptr_t)i);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        ret = pthread_attr_destroy(&attr);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    }
    return 0;
}

static int UdpSelectTest(void)
{
    struct sockaddr_in sa = {0};
    int lsfd;
    int ret;

    lsfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ICUNIT_ASSERT_NOT_EQUAL(lsfd, -1, errno);

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(SERVER_PORT);
    ret = bind(lsfd, reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa));
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, errno + CloseAllFd());

    InitFds();
    AddFd(lsfd);
    LogPrintln("[%d]Waiting for client to connect on UDP port %d", lsfd, SERVER_PORT);

    pthread_t clients[CLIENT_NUM];

    ret = StartClients(clients, CLIENT_NUM);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret + CloseAllFd());

    for ( ; ; ) {
        int nfd;
        fd_set readfds;
        struct timeval timeout;

        timeout.tv_sec = 3;
        timeout.tv_usec = 0;

        nfd = 0;
        FD_ZERO(&readfds);

        GetReadfds(&readfds, &nfd);

        ret = select(nfd + 1, &readfds, NULL, NULL, &timeout);
        LogPrintln("select %d", ret);
        if (ret == -1) {
            perror("select");
            break; // error occurred
        } else if (ret == 0) {
            break; // timed out
        }

        if (HandleReadfds(&readfds, lsfd) < 0) {
            break;
        }
    }

    for (int i = 0; i < CLIENT_NUM; ++i) {
        ret = pthread_join(clients[i], NULL);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret + CloseAllFd());
    }

    ICUNIT_ASSERT_EQUAL(gBye, CLIENT_NUM, gBye + CloseAllFd());
    (void)CloseAllFd();
    return ICUNIT_SUCCESS;
}

void NetSocketTest011(void)
{
    TEST_ADD_CASE(__FUNCTION__, UdpSelectTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
