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
#include "it_test_process.h"
#include "sys/shm.h"

static volatile int *g_shmptr = NULL;

static void Handler(int sig)
{
    (*g_shmptr)++;
}

static int TestCase(void)
{
    int status;
    (void)signal(SIGUSR2, Handler);
    int ret;
    pid_t pid;

    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600); // 0600 config of shmget
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);

    g_shmptr = (int *)shmat(shmid, nullptr, 0);
    ICUNIT_GOTO_NOT_EQUAL(g_shmptr, (int *)-1, g_shmptr, EXIT);

    *g_shmptr = 0;

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.

    if (pid == 0) {
        pid_t pid1 = fork();
        ICUNIT_GOTO_WITHIN_EQUAL(pid1, 0, 100000, pid1, EXIT); // 100000, assert pid1 equal to this.

        if (pid1 == 0) {
            ret = killpg(getpgrp(), SIGUSR2);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);

            while (*g_shmptr < 3) { // 3, Number of cycles.
                sleep(1);
            }

            exit(11); // 11, exit args
        }

        while (*g_shmptr < 3) { // 3, Number of cycles.
            sleep(1);
        }

        ret = waitpid(pid1, &status, 0);
        ICUNIT_GOTO_EQUAL(ret, pid1, ret, EXIT);

        status = WEXITSTATUS(status);
        ICUNIT_GOTO_EQUAL(status, 11, status, EXIT); // 11, assert status equal to this.

        exit(10); // 10, exit args
    }

    while (*g_shmptr < 3) { // 3, wait function running.
        sleep(1);
    }
    ICUNIT_ASSERT_EQUAL(*g_shmptr, 3, *g_shmptr); // 3, assert that function Result is equal to this.

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);

    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 10, status, EXIT); // 10, assert that function Result is equal to this.

    shmdt((void *)g_shmptr);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
EXIT:
    shmdt((void *)g_shmptr);
    shmctl(shmid, IPC_RMID, NULL);
    return 1;
}

void ItTestProcess062(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_062", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}