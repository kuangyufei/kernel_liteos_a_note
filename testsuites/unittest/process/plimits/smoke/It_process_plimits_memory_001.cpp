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

#include "It_process_plimits.h"

static char *testStr1 = nullptr;

static int limitWrite(const char *filepath, const char *buf)
{
    int fd = open(filepath, O_WRONLY);
    size_t ret = write(fd, buf, strlen(buf));
    close(fd);
    return ret;
}

static int CloneOne(void *arg)
{
    const unsigned int testSize = 11534336;

    testStr1 = (char *)malloc(testSize);
    EXPECT_STRNE(testStr1, NULL);

    (void)memset_s(testStr1, testSize, 1, testSize);
    return 0;
}

void ItProcessPlimitsMemory001(void)
{
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    int ret, pid, status;
    std::string path = "/proc/plimits/test";
    std::string limitTestPath = "/proc/plimits/test/memory.limit";
    std::string usageTestPath = "/proc/plimits/test/memory.stat";
    const char *buffer = "10485760";
    char buf[512];
    int twoM = 2 * 1024 * 1024;

    char *stack = (char *)mmap(NULL, twoM, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    EXPECT_STRNE(stack, NULL);
    ret = access(path.c_str(), 0);
    EXPECT_EQ(ret, -1);
    ret = mkdir(path.c_str(), S_IFDIR | mode);
    EXPECT_EQ(ret, 0);
    ret = limitWrite(limitTestPath.c_str(), buffer);
    EXPECT_NE(ret, -1);
    pid = clone(CloneOne, stack, 0, NULL);
    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);
    ret = WIFEXITED(status);
    ASSERT_EQ(ret, 0);

    (void)memset_s(buf, sizeof(buf), 0, sizeof(buf));
    ret = ReadFile(usageTestPath.c_str(), buf);
    EXPECT_NE(ret, -1);
    printf("%s: %s\n", usageTestPath.c_str(), buf);

    ret = rmdir(path.c_str());
    EXPECT_EQ(ret, 0);
}
