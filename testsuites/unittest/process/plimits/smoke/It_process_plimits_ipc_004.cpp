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
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <gtest/gtest.h>
#include "It_process_plimits.h"

void ItProcessPlimitsIpc004(void)
{
    mode_t mode;
    mode_t chmodMode = 0777;
    int ret;
    int fd;
    std::string rootplimitsPath = "/proc/plimits";
    std::string subPlimitsPath = "/proc/plimits/test";

    std::vector<std::string> ipcConfFileName;
    ipcConfFileName.push_back("ipc.mq_limit");
    ipcConfFileName.push_back("ipc.shm_limit");
    ipcConfFileName.push_back("ipc.stat");

    ret = mkdir(subPlimitsPath.c_str(), S_IFDIR | mode);
    ASSERT_EQ(ret, 0);

    for (auto iter = ipcConfFileName.begin(); iter != ipcConfFileName.end(); ++iter) {
        std::string fullPath = rootplimitsPath + "/" + *iter;
        ret = chmod(fullPath.c_str(), mode);
        ASSERT_EQ(ret, -1);
    }

    for (auto iter = ipcConfFileName.begin(); iter != ipcConfFileName.end(); ++iter) {
        std::string fullPath = subPlimitsPath + "/" + *iter;
        ret = chmod(fullPath.c_str(), mode);
        ASSERT_EQ(ret, -1);
    }

    ret = rmdir(subPlimitsPath.c_str());
    ASSERT_EQ(ret, 0);
    return;
}
