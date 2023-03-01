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
#include <string>
#include <iostream>
#include <regex>
#include "It_container_test.h"

static std::string GenNetLinkPath(int pid)
{
    std::ostringstream buf;
    buf << "/proc/" << pid << "/container/net";
    return buf.str();
}

static std::string ReadlinkNet(int pid)
{
    auto path = GenNetLinkPath(pid);
    struct stat sb;

    int ret = lstat(path.data(), &sb);
    if (ret == -1) {
        perror("lstat");
        return std::string();
    }

    auto bufsiz = sb.st_size + 1;
    if (sb.st_size == 0) {
        bufsiz = PATH_MAX;
    }

    std::vector<char> buf(bufsiz);
    auto nbytes = readlink(path.c_str(), buf.data(), bufsiz);
    if (nbytes == -1) {
        perror("readlink");
        return std::string();
    }

    return buf.data();
}

void ItNetContainer003(void)
{
    std::string zerolink("'net:[0]'");
    auto netlink = ReadlinkNet(getpid());
    int ret = zerolink.compare(netlink);
    ASSERT_NE(ret, 0);

    std::regex reg("'net:\\[[0-9]+\\]'");
    ret = std::regex_match(netlink, reg);
    ASSERT_EQ(ret, 1);
}
