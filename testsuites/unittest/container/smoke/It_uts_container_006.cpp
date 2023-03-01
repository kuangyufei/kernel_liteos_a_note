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
#include "It_container_test.h"

static int UtsContainerTest(void *arg)
{
    (void)arg;
    std::string containerType = "uts";

    int parentPid = getpid();
    auto parentlink = ReadlinkContainer(parentPid, containerType);

    int childsPid = CloneWrapper(ChildFunction, CLONE_NEWUTS, NULL);
    if (childsPid == -1) {
        return EXIT_CODE_ERRNO_1;
    }
    auto childlink = ReadlinkContainer(childsPid, containerType);

    std::string filePath = GenContainerLinkPath(childsPid, containerType);
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd == -1) {
        return EXIT_CODE_ERRNO_2;
    }

    int ret = setns(fd, CLONE_NEWUTS);
    (void)close(fd);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_3;
    }

    auto parentlink1 = ReadlinkContainer(parentPid, containerType);

    ret = parentlink.compare(parentlink1);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_4;
    }
    ret = parentlink1.compare(childlink);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_5;
    }

    int status;
    ret = waitpid(childsPid, &status, 0);
    if (ret != childsPid) {
        return EXIT_CODE_ERRNO_6;
    }

    int exitCode = WEXITSTATUS(status);
    if (exitCode != 0) {
        return EXIT_CODE_ERRNO_7;
    }
    return 0;
}

void ItUtsContainer006(void)
{
    int ret;

    int arg = CHILD_FUNC_ARG;
    auto pid = CloneWrapper(UtsContainerTest, CLONE_NEWUTS, &arg);
    ASSERT_NE(pid, -1);

    int status;
    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
