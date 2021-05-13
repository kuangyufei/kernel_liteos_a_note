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

static int *g_shmptr = NULL;
static int g_ppid;
static int TestThread(void *arg)
{
    pid_t ppid = getppid();

    printf("TestThread ppid : %d g_ppid : %d\n", ppid, g_ppid);

    ICUNIT_ASSERT_EQUAL_NULL(ppid, g_ppid, g_ppid);

    *g_shmptr = 100; // 100, set shared num.
    return NULL;
}

// This testcase us used for undefination of LOSCFG_USER_TEST_SMP
static int Testcase(void)
{
    int arg = 0x2000;
    int status;
    void *stack;
    char *stackTop;
    int ret;
    int count;
    pid_t pid;
    
    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600); // 0600 config of shmget
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);

    g_shmptr = (int *)shmat(shmid, nullptr, 0);
    ICUNIT_ASSERT_NOT_EQUAL(g_shmptr, (int *)-1, g_shmptr);

    *g_shmptr = 0;

    g_ppid = getppid();
    printf("testcase ppid : %d\n", g_ppid);

    stack = malloc(arg);
    ICUNIT_GOTO_NOT_EQUAL(stack, NULL, stack, EXIT1);

    stackTop = (char *)((unsigned long)stack + arg);
    pid = clone(TestThread, (void *)stackTop, CLONE_PARENT, &arg);

    ret = waitpid(pid, &status, NULL);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT2);

    count = 0;
    while ((count < 100) || (*g_shmptr == 0)) { // 100, Number of cycles.
        usleep(1000 * 100); // 1000, 100, set usleep time.
        count++;
    }

    ICUNIT_GOTO_EQUAL(*g_shmptr, 100, *g_shmptr, EXIT2); // 100, assert g_shmptr equal to this.
EXIT2:
    free(stack);

EXIT1:
    shmdt(g_shmptr);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

void ItTestProcess050(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_050", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
