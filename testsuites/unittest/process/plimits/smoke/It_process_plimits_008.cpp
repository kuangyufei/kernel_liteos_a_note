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
#include <climits>
#include <sys/types.h>
#include <gtest/gtest.h>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include "It_process_plimits.h"

static std::string GetFileMode(const char* filePath)
{
    const int MODE_COUNT = 11;
    char fileProperty[MODE_COUNT] = "----------";
    char fileMode[MODE_COUNT] = "-rwxrwxrwx";
    struct stat buf;
    stat(filePath, &buf);
    unsigned int off = 256;
    const int LOOP_VARY = 10;
    for (int i = 1; i < LOOP_VARY; i++) {
        if (buf.st_mode & (off >> (i - 1))) {
            fileProperty[i] = fileMode[i];
        }
    }
    return fileProperty;
}

static int IsFilePropertyR1(const char* filePath)
{
    std::string fileOrg = "-r--r--r--";
    std::string fileProperty = GetFileMode(filePath);
    return strcmp(fileProperty.c_str(), fileOrg.c_str());
}

void ItProcessPlimits008(void)
{
    std::string filePath = "/proc/plimits/";
    std::vector<std::string> fileName;
    fileName.push_back("plimits.procs");
    fileName.push_back("plimits.limiters");
    fileName.push_back("pids.max");
    fileName.push_back("sched.period");
    fileName.push_back("sched.quota");
    fileName.push_back("sched.stat");
    fileName.push_back("memory.failcnt");
    fileName.push_back("memory.limit");
    fileName.push_back("memory.peak");
    fileName.push_back("memory.usage");
    fileName.push_back("memory.stat");

    for (auto iter = fileName.begin(); iter != fileName.end(); ++iter) {
        std::string fileFullPath = filePath + *iter;
        int ret = IsFilePropertyR1(fileFullPath.c_str());
        ASSERT_EQ(ret, 0);
    }

    return;
}
