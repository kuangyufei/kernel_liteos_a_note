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

static int ChildFun(void *p)
{
    (void)p;
    return getpid();
}

static int ChildFunClone2()
{
    void *pstk = malloc(STACK_SIZE);
    if (pstk == NULL) {
        return -1;
    }
    int childPid = clone(ChildFun, (char *)pstk + STACK_SIZE, SIGCHLD, NULL);

    free(pstk);
    return childPid;
}

static int ChildFunClone1(void *p)
{
    (void)p;
    pid_t pid = getpid();
    const int COUNT = 100;
    int childPid;
    int childFunRet = (int)pid;
    int processCount = 0;
    int ret;
    int status;

    for (int i = 0; i < COUNT; i++) {
        childPid = ChildFunClone2();
        if (childPid != -1) {
            processCount++;
        } else {
            ret = wait(&status);
            if (ret > 0) {
                processCount--;
            } else {
                sleep(1);
            }
            continue;
        }
    }

    ret = 0;
    while (processCount > 0) {
        ret = wait(&status);
        if (ret > 0) {
            processCount--;
        }
    }

    return childFunRet;
}

void ItPidContainer003(void)
{
    int status;
    int ret;
    void *pstk = malloc(STACK_SIZE);
    ASSERT_TRUE(pstk != NULL);
    int childPid = clone(ChildFunClone1, (char *)pstk + STACK_SIZE, CLONE_NEWPID | SIGCHLD, NULL);
    ASSERT_NE(childPid, -1);

    ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, CONTAINER_FIRST_PID);
}
