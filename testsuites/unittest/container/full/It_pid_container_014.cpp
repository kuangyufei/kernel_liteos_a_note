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
    int ret = 0;
    int val, currPolicy;
    struct sched_param param = { 0 };
    const int DEFALUTE_PRIORITY = 31;
    int testProcessPri = 0;
    int currProcessPri = getpriority(PRIO_PROCESS, getpid());
    if ((currProcessPri > DEFALUTE_PRIORITY) || (currProcessPri < 0)) {
        return EXIT_CODE_ERRNO_1;
    }

    testProcessPri = currProcessPri + 1;
    currPolicy = sched_getscheduler(getpid());
    if (currPolicy != SCHED_RR) {
        return EXIT_CODE_ERRNO_2;
    }

    val = getpriority(PRIO_PROCESS, 0);
    if (val != currProcessPri) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = sched_getparam(0, &param);
    if ((ret != 0) || (param.sched_priority != currProcessPri)) {
        return EXIT_CODE_ERRNO_4;
    }

    val = sched_getscheduler(0);
    if (val != currPolicy) {
        return EXIT_CODE_ERRNO_5;
    }

    ret = setpriority(PRIO_PROCESS, 0, testProcessPri);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_6;
    }

    ret = getpriority(PRIO_PROCESS, 0);
    if (ret != testProcessPri) {
        return EXIT_CODE_ERRNO_7;
    }

    param.sched_priority = currProcessPri;
    ret = sched_setparam(0, &param);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_8;
    }

    ret = getpriority(PRIO_PROCESS, getpid());
    if (ret != currProcessPri) {
        return EXIT_CODE_ERRNO_9;
    }

    ret = sched_setscheduler(0, SCHED_FIFO, &param);
    if ((ret != -1) || (errno != EINVAL)) {
        return EXIT_CODE_ERRNO_10;
    }

    ret = sched_setscheduler(0, SCHED_RR, &param);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_11;
    }

    ret = sched_setscheduler(1, SCHED_FIFO, &param);
    if ((ret != -1) || (errno != EINVAL)) {
        return EXIT_CODE_ERRNO_12;
    }

    ret = sched_setscheduler(2, SCHED_FIFO, &param); // 2, input the pid.
    if ((ret != -1) || (errno != ESRCH)) {
        return EXIT_CODE_ERRNO_13;
    }

    ret = sched_setparam(1, &param);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_14;
    }

    ret = sched_setparam(2, &param); // 2, set the param.
    if ((ret != -1) || (errno != ESRCH)) {
        return EXIT_CODE_ERRNO_15;
    }

    ret = setpriority(PRIO_PROCESS, 1, testProcessPri);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_16;
    }

    ret = setpriority(PRIO_PROCESS, 2, testProcessPri); // 2, Used to calculate priorities.
    if ((ret != -1) || (errno != ESRCH)) {
        return EXIT_CODE_ERRNO_255;
    }

    return 0;
}

void ItPidContainer014(void)
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
