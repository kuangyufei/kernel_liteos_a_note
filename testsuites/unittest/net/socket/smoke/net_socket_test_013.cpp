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
#include <net/if.h>
#include <sys/ioctl.h>

#define SERVER_PORT 9999
#define INVALID_SOCKET -1
#define CLIENT_NUM 1

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
    char buf[256];
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

static unsigned int GetIp(int sfd, const char *ifname)
{
    struct ifreq ifr;
    unsigned int ip = 0;
    int ret = strncpy_s(ifr.ifr_name, sizeof(ifr.ifr_name) - 1, ifname, sizeof(ifr.ifr_name) - 1);
    if (ret < 0) {
        return 0;
    }
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
    ret = ioctl(sfd, SIOCGIFADDR, &ifr);
    if (ret == 0) {
        ip = ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr;
    }
    return ip;
}

static unsigned int GetNetmask(int sfd, const char *ifname)
{
    struct ifreq ifr;
    unsigned int msk = 0;
    int ret = strncpy_s(ifr.ifr_name, sizeof(ifr.ifr_name) - 1, ifname, sizeof(ifr.ifr_name) - 1);
    if (ret < 0) {
        return 0;
    }
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
    ret = ioctl(sfd, SIOCGIFNETMASK, &ifr);
    if (ret == 0) {
        msk = ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr;
    }
    return msk;
}

static char *MyInetNtoa(unsigned int ip)
{
    struct in_addr in = {ip};
    return inet_ntoa(in);
}

static void *ClientsThread(void *param)
{
    int fd;
    int thrNo = (int)(intptr_t)param;
    int ret;
    const char *ifname[] = {"eth0", "wlan0", "et1", "wl1", "enp4s0f0"};
    unsigned int ip, msk, brdcast;

    LogPrintln("<%d>socket client thread started", thrNo);
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET) {
        perror("socket");
        return NULL;
    }

    int broadcast = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    if (ret != 0) {
        LogPrintln("[%d]<%d> set broadcast option fail", fd, thrNo);
        close(fd);
        return NULL;
    }

    for (int j = 0; j < sizeof(ifname) / sizeof(ifname[0]); ++j) {
        ip = GetIp(fd, ifname[j]);
        msk = GetNetmask(fd, ifname[j]);
        if (ip != 0) {
            LogPrintln("[%d]<%d>%s: ip %s", fd, thrNo, ifname[j], MyInetNtoa(ip));
            LogPrintln("[%d]<%d>%s: netmask %s", fd, thrNo, ifname[j], MyInetNtoa(msk));
            break;
        }
    }

    brdcast = ip | ~msk;
    LogPrintln("[%d]<%d>broadcast address %s", fd, thrNo, MyInetNtoa(brdcast));

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = brdcast;
    sa.sin_port = htons(SERVER_PORT);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        perror("connect");
        return NULL;
    }

    LogPrintln("[%d]<%d>connected to udp://%s:%d successful", fd, thrNo, inet_ntoa(sa.sin_addr), SERVER_PORT);

    const char *msg[] = {
            "hello, ",
            "ohos, ",
            "my name is net_socket_test_013, ",
            "see u next time, ",
            "Bye!"
    };

    for (int i = 0; i < sizeof(msg) / sizeof(msg[0]); ++i) {
        if (send(fd, msg[i], strlen(msg[i]), 0) < 0) {
            LogPrintln("[%d]<%d>send msg [%s] fail: errno %d", fd, thrNo, msg[i], errno);
        }
    }

    (void)shutdown(fd, SHUT_RDWR);
    (void)close(fd);
    return param;
}

static int StartClients(pthread_t *cli, int cliNum)
{
    int ret;
    pthread_attr_t attr;

    for (int i = 0; i < cliNum; ++i) {
        ret = pthread_attr_init(&attr);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_create(&cli[i], &attr, ClientsThread, (void *)(intptr_t)i);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_attr_destroy(&attr);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    }
    return 0;
}

static int UdpBrdcastSelectTest(void)
{
    struct sockaddr_in sa = {0};
    int lsfd;
    int ret;

    lsfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ICUNIT_ASSERT_NOT_EQUAL(lsfd, -1, errno);

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(SERVER_PORT);
    ret = bind(lsfd, (struct sockaddr *)&sa, sizeof(sa));
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, errno + CloseAllFd());

    int broadcast = 1;
    ret = setsockopt(lsfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    ICUNIT_ASSERT_EQUAL(ret, 0, errno + CloseAllFd());

    int loop = 0;
    socklen_t len = sizeof(loop);
    ret = getsockopt(lsfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, &len);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno + CloseAllFd());
    LogPrintln("IP_MULTICAST_LOOP default: %d", loop);

    loop = 0;
    ret = setsockopt(lsfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    ICUNIT_ASSERT_EQUAL(ret, 0, errno + CloseAllFd());

    ret = getsockopt(lsfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, &len);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno + CloseAllFd());
    LogPrintln("IP_MULTICAST_LOOP changed to: %d", loop);

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

void NetSocketTest013(void)
{
    TEST_ADD_CASE(__FUNCTION__, UdpBrdcastSelectTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
