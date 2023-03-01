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
#include <arpa/inet.h>
#include <net/if.h>
#include "It_container_test.h"

using namespace std;
const int IpLen = 16;

static int GetLocalIP(char *ip)
{
    struct ifreq ifr;
    int inet_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (inet_sock < 0) {
        return -1;
    }
    int ret = strcpy_s(ifr.ifr_name, sizeof(ifr.ifr_name), "eth0");
    if (ret != EOK) {
        (void)close(inet_sock);
        return -1;
    }
    ioctl(inet_sock, SIOCGIFADDR, &ifr);
    ret = strcpy_s(ip, IpLen, inet_ntoa((reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_addr))->sin_addr));
    if (ret != EOK) {
        (void)close(inet_sock);
        return -1;
    }
    ret = close(inet_sock);
    if (ret != 0) {
        return -1;
    }
    return 0;
}

static int SetIP(char *ip)
{
    struct ifreq ifr;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) {
        return -1;
    }
    int ret = strncpy_s(ifr.ifr_name, sizeof(ifr.ifr_name), "eth0", IFNAMSIZ);
    if (ret != EOK) {
        (void)close(fd);
        return -1;
    }
    ifr.ifr_addr.sa_family = AF_INET;
    inet_pton(AF_INET, ip, ifr.ifr_addr.sa_data + 2); /* 2: offset */
    ioctl(fd, SIOCSIFADDR, &ifr);
    ret = close(fd);
    if (ret != 0) {
        return -1;
    }
    return 0;
}

static int ChildFunc(void *arg)
{
    (void)arg;
    int ret;
    char oldIp[IpLen] = {NULL};
    char newIp[IpLen] = {NULL};

    ret = GetLocalIP(oldIp);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_1;
    }

    ret = SetIP("192.168.1.233");
    if (ret != 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = GetLocalIP(newIp);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = strcmp(oldIp, newIp);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_4;
    }

    printf("%s %d\n", __FUNCTION__, __LINE__);
    return 0;
}

void ItNetContainer001(void)
{
    int ret;
    char oldIp[IpLen] = {NULL};
    char newIp[IpLen] = {NULL};

    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    EXPECT_STRNE(stack, NULL);
    char *stackTop = stack + STACK_SIZE;

    ret = GetLocalIP(oldIp);
    ASSERT_EQ(ret, 0);

    int arg = CHILD_FUNC_ARG;
    auto pid = clone(ChildFunc, stackTop, SIGCHLD | CLONE_NEWNET, &arg);
    ASSERT_NE(pid, -1);

    int status;
    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);

    ret = GetLocalIP(newIp);
    ASSERT_EQ(ret, 0);

    ret = strcmp(oldIp, newIp);
    ASSERT_EQ(ret, 0);
}
