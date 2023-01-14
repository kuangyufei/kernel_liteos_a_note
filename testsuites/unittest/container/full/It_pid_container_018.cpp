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

const int TEST_COUNT = 5;
const int TEST_PARENT_SLEEP_TIME = 2;
const int TEST_CHILD_SLEEP_TIME  = 5;
static int ChildShell(void *p)
{
    printf("\n###################### %s pid container process info ####################\n", (char *)p);
    int ret = execl("/bin/shell", "shell", "task", "-a", (char *)0);
    if (ret < 0) {
        return errno;
    }
    return 0;
}

static void *PthreadFunc(void *arg)
{
    (void)arg;
    sleep(TEST_CHILD_SLEEP_TIME);
    return NULL;
}

static int Child2(void *p)
{
    (void)p;
    sleep(TEST_CHILD_SLEEP_TIME);
    exit(0);
}

static int Child1(void *p)
{
    (void)p;
    int ret;
    pid_t pid = fork();
    if (pid == 0) {
        sleep(TEST_CHILD_SLEEP_TIME);
        exit(0);
    }

    sleep(TEST_CHILD_SLEEP_TIME);

    ret = waitpid(pid, NULL, 0);
    if (ret != pid) {
        exit(EXIT_CODE_ERRNO_5);
    }
    return 0;
}

static int ChildFun(void *p)
{
    (void)p;
    int ret;

    pid_t pid1 = fork();
    if (pid1 == 0) {
        ret = Child1(NULL);
        exit(ret);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        (void)Child2(NULL);
    }

    pid_t pid3 = fork();
    if (pid3 == 0) {
        ret = ChildShell(static_cast<void *>(const_cast<char*>("Child")));
        if (ret != 0) {
            exit(EXIT_CODE_ERRNO_1);
        }
        exit(0);
    }

    sleep(TEST_CHILD_SLEEP_TIME);

    ret = waitpid(pid1, NULL, 0);
    if (ret != pid1) {
        exit(EXIT_CODE_ERRNO_2);
    }

    ret = waitpid(pid2, NULL, 0);
    if (ret != pid2) {
        exit(EXIT_CODE_ERRNO_3);
    }

    ret = waitpid(pid3, NULL, 0);
    if (ret != pid3) {
        exit(EXIT_CODE_ERRNO_4);
    }

    return 0;
}

void ItPidContainer018(void)
{
    int status;
    int ret;
    pid_t pid;
    void *pstk = malloc(STACK_SIZE);
    ASSERT_TRUE(pstk != NULL);
    int childPid = clone(ChildFun, (char *)pstk + STACK_SIZE, CLONE_NEWPID | SIGCHLD, NULL);
    free(pstk);
    ASSERT_NE(childPid, -1);

    sleep(TEST_PARENT_SLEEP_TIME);

    pid = fork();
    if (pid == 0) {
        ret = ChildShell(static_cast<void *>(const_cast<char*>("Parent")));
        ASSERT_EQ(ret, 0);
    }

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, 0);

    ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, 0);
}
