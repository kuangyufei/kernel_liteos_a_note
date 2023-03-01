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
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <climits>
#include "It_process_plimits.h"

void ItProcessPlimits007(void)
{
    mode_t mode = S_IRWXU;
    std::string path0 = "/proc/plimits/test";
    std::string path1 = "/proc/plimits/test/subtest";
    std::string path2 = "/proc/plimits/test/subtest/subtest";
    std::string path3 = "/proc/plimits/test/subtest/subtest/subtest";
    std::string path4 = "/proc/plimits/test/subtest/subtest/subtest/subtest";
    int ret = mkdir(path0.c_str(), mode);
    ASSERT_EQ(ret, 0);

    ret = mkdir(path1.c_str(), mode);
    ASSERT_EQ(ret, -1);

    ret = mkdir(path2.c_str(), mode);
    ASSERT_EQ(ret, -1);

    ret = mkdir(path3.c_str(), mode);
    ASSERT_EQ(ret, -1);

    ret = mkdir(path4.c_str(), mode);
    ASSERT_EQ(ret, -1);

    ret = rmdir(path0.c_str());
    ASSERT_EQ(ret, 0);
    return;
}
