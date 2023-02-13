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

void ItUtsContainer006(void)
{
    std::string containerType = "uts";

    int parentPid = getpid();
    auto parentlink = ReadlinkContainer(parentPid, containerType);

    int childsPid = CloneWrapper(ChildFunction, CLONE_NEWUTS, NULL);
    ASSERT_NE(childsPid, -1);
    auto childlink = ReadlinkContainer(childsPid, containerType);

    std::string filePath = GenContainerLinkPath(childsPid, containerType);
    int fd = open(filePath.c_str(), O_RDONLY);
    ASSERT_NE(fd, -1);

    int ret = setns(fd, CLONE_NEWUTS);
    ASSERT_NE(ret, -1);
    (void)close(fd);

    auto parentlink1 = ReadlinkContainer(parentPid, containerType);

    ret = parentlink.compare(parentlink1);
    ASSERT_NE(ret, 0);
    ret = parentlink1.compare(childlink);
    ASSERT_EQ(ret, 0);

    int status;
    ret = waitpid(childsPid, &status, 0);
    ASSERT_EQ(ret, childsPid);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
