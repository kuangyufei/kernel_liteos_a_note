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

static const int BUF_SIZE = 100;

static int ChildFun2(void *p)
{
    (void)p;
    sleep(3); /* 3: delay 3s */
    return 0;
}

static int ChildFun1(void *p)
{
    (void)p;
    sleep(6); /* 6: delay 6s */
    return 0;
}

static int ChildFun(void *p)
{
    (void)p;
    int status, ret;
    const char *containerType = "pid";
    const char *containerType1 = "pid_for_children";
    char targetpath[BUF_SIZE] = {0};
    auto linkBuffer1 = ReadlinkContainer(getpid(), containerType);

    int childPid = clone(ChildFun1, NULL, CLONE_NEWPID | SIGCHLD, NULL);
    if (childPid == -1) {
        return EXIT_CODE_ERRNO_1;
    }
    auto linkBuffer2 = ReadlinkContainer(childPid, containerType);
    ret = linkBuffer2.compare(linkBuffer1);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = sprintf_s(targetpath, BUF_SIZE, "/proc/%d/container/pid", childPid);
    if (ret < 0) {
        return EXIT_CODE_ERRNO_16;
    }
    int fd = open(targetpath, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = setns(fd, CLONE_NEWPID);
    if (ret != 0) {
        close(fd);
        return EXIT_CODE_ERRNO_4;
    }

    close(fd);

    auto linkBuffer6 = ReadlinkContainer(getpid(), containerType);
    ret = linkBuffer6.compare(linkBuffer1);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_17;
    }

    auto linkBuffer3 = ReadlinkContainer(getpid(), containerType1);
    ret = linkBuffer3.compare(linkBuffer2);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_5;
    }

    int childPid1 = clone(ChildFun2, NULL, SIGCHLD, NULL);
    if (childPid1 == -1) {
        return EXIT_CODE_ERRNO_6;
    }

    auto linkBuffer4 = ReadlinkContainer(childPid1, containerType);
    ret = linkBuffer4.compare(linkBuffer3);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_7;
    }

    int childPid2 = clone(ChildFun2, NULL, CLONE_NEWUTS | SIGCHLD, NULL);
    if (childPid2 == -1) {
        return EXIT_CODE_ERRNO_8;
    }

    auto linkBuffer5 = ReadlinkContainer(childPid2, containerType);
    ret = linkBuffer5.compare(linkBuffer4);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_9;
    }

    ret = WaitChild(childPid2, &status, EXIT_CODE_ERRNO_10, EXIT_CODE_ERRNO_11);
    if (ret != 0) {
        return ret;
    }

    ret = WaitChild(childPid1, &status, EXIT_CODE_ERRNO_12, EXIT_CODE_ERRNO_13);
    if (ret != 0) {
        return ret;
    }
    ret = WaitChild(childPid, &status, EXIT_CODE_ERRNO_14, EXIT_CODE_ERRNO_15);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

void ItPidContainer031(void)
{
    int status;
    int childPid = clone(ChildFun, NULL, CLONE_NEWPID | SIGCHLD, NULL);
    ASSERT_NE(childPid, -1);

    int ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, 0);
}
