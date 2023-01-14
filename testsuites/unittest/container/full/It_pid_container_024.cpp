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
#include "sys/resource.h"
#include "sys/wait.h"
#include "pthread.h"
#include "sched.h"

const int SLEEP_TIME_US = 1000;
const int LOOP_NUM = 1000;

static int ChildFunc(void *arg)
{
    (void)arg;
    usleep(SLEEP_TIME_US);
    exit(EXIT_CODE_ERRNO_5);
}

static int GroupProcess(void *arg)
{
    (void)arg;
    int ret;
    int status = 0;

    for (int i = 0; i < LOOP_NUM; i++) {
        int argTmp = CHILD_FUNC_ARG;
        auto pid = CloneWrapper(ChildFunc, CLONE_NEWPID, &argTmp);
        if (pid == -1) {
            return EXIT_CODE_ERRNO_1;
        }

        ret = waitpid(pid, &status, 0);
        status = WEXITSTATUS(status);
        if (status != EXIT_CODE_ERRNO_5) {
            return EXIT_CODE_ERRNO_2;
        }
    }

    exit(EXIT_CODE_ERRNO_5);
}

void ItPidContainer024(void)
{
    int ret;
    int status = 0;
    int arg = CHILD_FUNC_ARG;
    auto pid = CloneWrapper(GroupProcess, CLONE_NEWPID, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);
    status = WEXITSTATUS(status);
    ASSERT_EQ(status, EXIT_CODE_ERRNO_5);
}
