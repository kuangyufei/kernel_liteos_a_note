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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <fcntl.h>

static char gDefaultNetif[IFNAMSIZ] = "eth0";

static void InitIfreq(struct ifreq *ifr)
{
    *ifr = (struct ifreq){{0}};
    (void)strncpy_s(ifr->ifr_name, sizeof(ifr->ifr_name) - 1, gDefaultNetif, sizeof(ifr->ifr_name) - 1);
    ifr->ifr_name[sizeof(ifr->ifr_name) - 1] = '\0';
}

static char *IfIndex2Name(int fd, unsigned index, char *name)
{
#if SUPPORT_IF_INDEX_TO_NAME
    return if_indextoname(index, name);
#else
    struct ifreq ifr;
    int ret;

    ifr.ifr_ifindex = index;
    ret = ioctl(fd, SIOCGIFNAME, &ifr);
    if (ret < 0) {
        return NULL;
    }
    ret = strncpy_s(name, IF_NAMESIZE - 1, ifr.ifr_name, IF_NAMESIZE - 1);
    if (ret < 0) {
        return NULL;
    }
    name[IF_NAMESIZE - 1] = '\0';
    return name;
#endif
}

static unsigned IfName2Index(int fd, const char *name)
{
#if SUPPORT_IF_NAME_TO_INDEX
    return if_nametoindex(name);
#else
    struct ifreq ifr;
    int ret;

    (void)strncpy_s(ifr.ifr_name, sizeof(ifr.ifr_name) - 1, name, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
    ret = ioctl(fd, SIOCGIFINDEX, &ifr);
    return ret < 0 ? 0 : ifr.ifr_ifindex;
#endif
}

static int IoctlTestInternal(int sfd)
{
    struct ifreq ifr = {{0}};
    char ifName[IFNAMSIZ] = {0}, *p = NULL;
    unsigned int loIndex = 0;
    unsigned int lanIndex = 0;
    int maxIndex = 256;
    int ret;

    for (int i = 0; i < maxIndex; ++i) {
        p = IfIndex2Name(sfd, i, ifName);
        if (p) {
            if (strcmp(p, "lo") == 0) {
                loIndex = i;
            } else {
                lanIndex = i;
            }
        }
    }

    LogPrintln("ifindex of lo: %u, ifindex of lan: %u", loIndex, lanIndex);
    ICUNIT_ASSERT_NOT_EQUAL(loIndex, 0, errno);
    ICUNIT_ASSERT_NOT_EQUAL(lanIndex, 0, errno);

    p = IfIndex2Name(sfd, loIndex, ifName);
    LogPrintln("ifindex %u: %s", loIndex, p);
    ICUNIT_ASSERT_NOT_EQUAL(p, NULL, errno);

    p = IfIndex2Name(sfd, lanIndex, ifName);
    LogPrintln("ifindex %u: %s", lanIndex, p);
    ICUNIT_ASSERT_NOT_EQUAL(p, NULL, errno);

    ret = strncpy_s(gDefaultNetif, sizeof(gDefaultNetif) -1, p, sizeof(gDefaultNetif) - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    gDefaultNetif[sizeof(gDefaultNetif) - 1] = '\0';

    ret = (int)IfName2Index(sfd, p);
    LogPrintln("index of %s: %d", p, ret);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, errno);

    ifr.ifr_ifindex = lanIndex;
    ret = ioctl(sfd, SIOCGIFNAME, &ifr);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno);
    LogPrintln("name of ifindex %u: %s", lanIndex, ifr.ifr_name);

    InitIfreq(&ifr);
    ret = ioctl(sfd, SIOCGIFINDEX, &ifr);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno);
    LogPrintln("index of ifname %s: %d", ifr.ifr_name, ifr.ifr_ifindex);

    InitIfreq(&ifr);
    ret = ioctl(sfd, SIOCGIFHWADDR, &ifr);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno);
    LogPrintln("hwaddr: %02hhX:%02hhX:%02hhX:XX:XX:XX", ifr.ifr_hwaddr.sa_data[0], ifr.ifr_hwaddr.sa_data[1], ifr.ifr_hwaddr.sa_data[2]);

    InitIfreq(&ifr);
    ret = ioctl(sfd, SIOCGIFFLAGS, &ifr);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno);
    LogPrintln("FLAGS of ifname %s: %#x, IFF_PROMISC: %d", ifr.ifr_name, ifr.ifr_flags, !!(ifr.ifr_flags & IFF_PROMISC));

    if (ifr.ifr_flags & IFF_PROMISC) {
        ifr.ifr_flags &= ~(IFF_PROMISC);
    } else {
        ifr.ifr_flags |= IFF_PROMISC;
    }
    LogPrintln("SIOCSIFFLAGS FLAGS: %#x", ifr.ifr_flags);
    ret = ioctl(sfd, SIOCSIFFLAGS, &ifr);
    if (ret == -1) {
        ICUNIT_ASSERT_EQUAL(errno, EPERM, errno);
    } else {
        ICUNIT_ASSERT_EQUAL(ret, 0, errno);
    }

    InitIfreq(&ifr);
    ret = ioctl(sfd, SIOCGIFFLAGS, &ifr);
    ICUNIT_ASSERT_EQUAL(ret, 0, errno);
    LogPrintln("FLAGS of ifname %s: %#x, IFF_PROMISC: %d", ifr.ifr_name, ifr.ifr_flags, !!(ifr.ifr_flags & IFF_PROMISC));

    ret = fcntl(sfd, F_GETFL, 0);
    ICUNIT_ASSERT_EQUAL(ret < 0, 0, errno);

    ret = fcntl(sfd, F_SETFL, ret | O_NONBLOCK);
    ICUNIT_ASSERT_EQUAL(ret < 0, 0, errno);

    return ICUNIT_SUCCESS;
}

static int IoctlTest(void)
{
    int sfd;

    sfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    LogPrintln("socket(PF_INET, SOCK_STREAM, IPPROTO_TCP): %d", sfd);
    ICUNIT_ASSERT_NOT_EQUAL(sfd, -1, errno);

    (void)IoctlTestInternal(sfd);

    (void)close(sfd);
    return ICUNIT_SUCCESS;
}

void NetSocketTest010(void)
{
    TEST_ADD_CASE(__FUNCTION__, IoctlTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
