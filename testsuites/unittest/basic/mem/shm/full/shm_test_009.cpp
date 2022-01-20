/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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
#include "it_test_shm.h"
#include "sys/types.h"
#include <sched.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

static int *g_shmptr = NULL;

static void ChildProcess(void)
{
    struct sched_param param;

    param.sched_priority = sched_get_priority_max(SCHED_RR);
    if(sched_setparam(getpid(), &param) != 0) {
        printf("An error occurs when calling sched_setparam()");
        return;
    }

    /* to avoid blocking */
    alarm(2);
    while(1);
}

static void TestProcess(void)
{
    /* to avoid blocking */
    alarm(5);

    while(1) {
        (*g_shmptr)++;
        sched_yield();
    }
}

static void ExitChildren(int sig)
{
    exit(0);
}

static void KillChildren(int childPid)
{
    kill(childPid, SIGTERM);
    sleep(1);  //wait for kill child finish.
}

static int Testcase(void)
{
    int childPid, oldcount, newcount, shmid;
    struct sched_param param = {0};
    struct sched_param paramCopy = {0};
    int processPolicy = 0;
    int threadPrio = 0;
    int ret;
    int pid;

    void *ptr = (void *)signal(SIGTERM, ExitChildren);
    ICUNIT_ASSERT_NOT_EQUAL(ptr, NULL, ptr);

    shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);

    g_shmptr = (int *)shmat(shmid, 0, 0);
    ICUNIT_ASSERT_NOT_EQUAL(g_shmptr, (int *)-1, g_shmptr);

    *g_shmptr = 0;

    processPolicy = sched_getscheduler(getpid());
    ret = sched_getparam(getpid(), &paramCopy);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    param.sched_priority = (sched_get_priority_min(SCHED_RR) +
                            sched_get_priority_max(SCHED_RR)) / 2;
    ret = sched_setscheduler(getpid(), SCHED_RR, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_getschedparam(pthread_self(), &processPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    threadPrio = param.sched_priority;
    ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    childPid = fork();
    ICUNIT_GOTO_NOT_EQUAL(childPid, -1, childPid, OUT_SCHEDULER);

    if (childPid == 0) {
        TestProcess();
        exit(0);
    }
    sleep(1);

    param.sched_priority = sched_get_priority_min(SCHED_RR);
    oldcount = *g_shmptr;
    ret = sched_setparam(childPid, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, OUT_SCHEDULER);

    ret = 1;
    newcount = *g_shmptr;
    ICUNIT_GOTO_NOT_EQUAL(oldcount, newcount, newcount, OUT);

    ret = 0;
OUT:
    KillChildren(childPid);
    pid = waitpid(childPid, NULL, 0);
    ICUNIT_ASSERT_EQUAL(pid, childPid, pid);
    (void)sched_setparam(getpid(), &paramCopy);
OUT_SCHEDULER:
    (void)sched_setscheduler(getpid(), processPolicy, &paramCopy);
    param.sched_priority = threadPrio;
    pthread_setschedparam(pthread_self(), SCHED_RR, &param);

    ret = shmdt(g_shmptr);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    ret = shmctl(shmid, IPC_RMID, NULL);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    return ret;
}

void ItTestShm009(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_009", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
