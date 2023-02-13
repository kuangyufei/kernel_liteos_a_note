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

static int ChildFunClone3(void *p)
{
    (void)p;
    auto pid = getpid();
    if (pid != CONTAINER_SECOND_PID) {
        return EXIT_CODE_ERRNO_1;
    }
    return 0;
}

static int ChildFunClone2(void *p)
{
    (void)p;
    auto pid = getpid();
    if (pid != CONTAINER_FIRST_PID) {
        return EXIT_CODE_ERRNO_1;
    }
    int ret;
    int status;
    void *pstk = malloc(STACK_SIZE);
    if (pstk == NULL) {
        return EXIT_CODE_ERRNO_2;
    }
    int childPid = clone(ChildFunClone3, (char *)pstk + STACK_SIZE, SIGCHLD, NULL);
    if (childPid == -1) {
        free(pstk);
        return EXIT_CODE_ERRNO_3;
    }

    ret = waitpid(childPid, &status, 0);
    ret = WIFEXITED(status);
    ret = WEXITSTATUS(status);
    if (ret != 0) {
        free(pstk);
        return EXIT_CODE_ERRNO_4;
    }

    free(pstk);
    return 0;
}


static int ChildFunClone1(void *p)
{
    (void)p;
    int ret;
    int status;
    const char *containerType = "pid";
    const char *containerType1 = "pid_for_children";

    auto pid = getpid();
    ret = unshare(CLONE_NEWPID);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_1;
    }

    auto pid1 = getpid();
    if (pid != pid1) {
        return EXIT_CODE_ERRNO_2;
    }

    auto linkBuffer = ReadlinkContainer(pid, containerType);
    auto linkBuffer1 = ReadlinkContainer(pid, containerType1);
    ret = linkBuffer.compare(linkBuffer1);
    if (ret == 0) {
        printf("linkBuffer: %s linkBuffer1: %s\n", linkBuffer.c_str(), linkBuffer1.c_str());
        return EXIT_CODE_ERRNO_3;
    }

    void *pstk = malloc(STACK_SIZE);
    if (pstk == NULL) {
        return EXIT_CODE_ERRNO_4;
    }
    int childPid = clone(ChildFunClone2, (char *)pstk + STACK_SIZE, SIGCHLD, NULL);
    free(pstk);
    if (childPid == -1) {
        return EXIT_CODE_ERRNO_5;
    }

    ret = unshare(CLONE_NEWPID);
    if (ret != -1) {
        return EXIT_CODE_ERRNO_6;
    }

    ret = waitpid(childPid, &status, 0);
    if (ret != childPid) {
        return EXIT_CODE_ERRNO_7;
    }
    ret = WIFEXITED(status);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_8;
    }
    ret = WEXITSTATUS(status);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_9;
    }
    return 0;
}

void ItPidContainer027(void)
{
    void *pstk = malloc(STACK_SIZE);
    ASSERT_TRUE(pstk != NULL);

    int childPid = clone(ChildFunClone1, (char *)pstk + STACK_SIZE, SIGCHLD, NULL);
    free(pstk);
    ASSERT_NE(childPid, -1);

    int status;
    int ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, 0);
}
