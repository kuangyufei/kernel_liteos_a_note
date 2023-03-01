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

#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include <cstring>
#include <gtest/gtest.h>
#include "It_process_plimits.h"

void ItProcessPlimitsDevices009(void)
{
    int fd;
    int ret;
    mode_t mode;
    std::string test_dev = "/dev/hi_mipi";
    std::string device_a = "a";
    std::string path = "/proc/plimits/test";
    std::string devicesDenyPath = "/proc/plimits/test/devices.deny";

    ret = mkdir(path.c_str(), S_IFDIR | mode);
    ASSERT_EQ(ret, 0);

    ret = WriteFile(devicesDenyPath.c_str(), device_a.c_str());
    ASSERT_NE(ret, -1);

    fd = open(test_dev.c_str(), O_RDONLY|O_CREAT);
    ASSERT_EQ(fd, -1);
    (void)close(fd);

    fd = open(test_dev.c_str(), O_WRONLY|O_CREAT);
    ASSERT_EQ(fd, -1);
    (void)close(fd);

    ret = rmdir(path.c_str());
    ASSERT_EQ(ret, 0);
}
