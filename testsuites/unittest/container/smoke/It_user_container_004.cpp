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

#include <sched.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include "It_container_test.h"

const int EXIT_TRUE_CODE = 255;
const int MAX_PID_RANGE = 100000;

static int childFunc(void *arg)
{
    (void)arg;
    std::string containerType = "user";
    std::string childlink;
    std::string filePath;
    std::string parentlink1;
    int fd;
    int ret;
    int status;
    int parentPid = getpid();

    auto parentlink = ReadlinkContainer(parentPid, containerType);
    int childsPid = CloneWrapper(ChildFunction, CLONE_NEWUSER, NULL);
    if (childsPid == -1) {
        return EXIT_CODE_ERRNO_1;
    }

    childlink = ReadlinkContainer(childsPid, containerType);
    filePath = GenContainerLinkPath(childsPid, containerType);
    fd = open(filePath.c_str(), O_RDONLY);
    if (fd == -1) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = setns(fd, CLONE_NEWUSER);
    if (ret == -1) {
        close(fd);
        return EXIT_CODE_ERRNO_3;
    }

    parentlink1 = ReadlinkContainer(parentPid, containerType);

    close(fd);

    ret = waitpid(childsPid, &status, 0);
    if (ret != childsPid) {
        return EXIT_CODE_ERRNO_4;
    }

    ret = parentlink.compare(parentlink1);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_5;
    }

    ret = parentlink1.compare(childlink);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_6;
    }

    exit(EXIT_TRUE_CODE);
}

void ItUserContainer004(void)
{

    int ret;
    int status = 0;
    int arg = CHILD_FUNC_ARG;
    auto pid = CloneWrapper(childFunc, SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    status = WEXITSTATUS(status);
    ASSERT_EQ(status, EXIT_TRUE_CODE);
}
