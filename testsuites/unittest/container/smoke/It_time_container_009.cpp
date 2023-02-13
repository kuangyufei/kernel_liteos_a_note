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

#include <iostream>
#include "It_container_test.h"

static std::string GenTimeLinkPath(int pid)
{
    std::ostringstream buf;
    buf << "/proc/" << pid << "/container/time";
    return buf.str();
}

static std::string ReadlinkTime(int pid)
{
    auto path = GenTimeLinkPath(pid);
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

void ItTimeContainer009(void)
{
    auto timelink = ReadlinkTime(getpid());
    std::cout << "Contents of the time link is: " << timelink << std::endl;

    std::regex reg("'time:\\[[0-9]+\\]'");
    bool ret = std::regex_match(timelink, reg);
    ASSERT_TRUE(ret);
}
