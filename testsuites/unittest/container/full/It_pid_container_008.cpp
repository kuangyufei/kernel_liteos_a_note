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
    int ret;
    cpu_set_t cpuset;
    cpu_set_t initCpuset;

    CPU_ZERO(&initCpuset);
    ret = sched_getaffinity(getpid(), sizeof(cpu_set_t), &initCpuset);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_1;
    }

    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    ret = sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_2;
    }

    CPU_ZERO(&cpuset);
    ret = sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }
    ret = (CPU_ISSET(0, &cpuset) > 0) ? 1 : 0;
    if (ret != 0) {
        return EXIT_CODE_ERRNO_4;
    }
    ret = (CPU_ISSET(1, &cpuset) > 0) ? 1 : 0;
    if (ret != 1) {
        return EXIT_CODE_ERRNO_5;
    }

    return 0;
}

void ItPidContainer008(void)
{
    int status;
    int ret;
    void *pstk = malloc(STACK_SIZE);
    ASSERT_TRUE(pstk != NULL);
    int childPid = clone(ChildFun, (char *)pstk + STACK_SIZE, CLONE_NEWPID | SIGCHLD, NULL);
    free(pstk);
    ASSERT_NE(childPid, -1);

    ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);
    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);
    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, 0);
}
