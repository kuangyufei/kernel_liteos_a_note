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
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <climits>
#include <sys/types.h>
#include <gtest/gtest.h>
#include <dirent.h>
#include "It_process_plimits.h"

static int g_processOs = 8;

void ItProcessPlimitsPid005(void)
{
    mode_t mode = 0;
    std::string path = "/proc/plimits/test";
    std::string filepath = "/proc/plimits/test/plimits.procs";
    int pid = getpid();
    int vprocessId = 90;
    std::string missPid = std::to_string(vprocessId);
    std::string runPid = std::to_string(pid);

    int ret = mkdir(path.c_str(), S_IFDIR | mode);
    ASSERT_EQ(ret, 0);

    int fd = open(filepath.c_str(), O_WRONLY);
    ASSERT_NE(fd, -1);

    for (int i = 1; i <= g_processOs; i++) {
        std::string fWrite = std::to_string(i);
        ret = write(fd, fWrite.c_str(), (fWrite.length()));
        ASSERT_EQ(ret, -1);
    }

    ret = write(fd, missPid.c_str(), (missPid.length()));
    ASSERT_EQ(ret, -1);

    ret = write(fd, path.c_str(), (path.length()));
    ASSERT_EQ(ret, -1);

    ret = write(fd, runPid.c_str(), (runPid.length()));
    ASSERT_NE(ret, -1);
    close(fd);
    ret = rmdir(path.c_str());
    ASSERT_EQ(ret, 0);
    return;
}
