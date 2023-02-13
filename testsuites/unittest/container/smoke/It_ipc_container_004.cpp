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
using namespace std;

static const char *containerType = "ipc";

struct shared_use_st {
    char test[SHM_TEST_DATA_SIZE];
};

static int childFunc1(void *arg)
{
    struct shared_use_st *shared = NULL;
    const char testBuf[] = "child test shm";
    int ret;
    (void)arg;

    int shmid = shmget((key_t)SHM_TEST_KEY1, sizeof(struct shared_use_st), SHM_TEST_OPEN_PERM | IPC_CREAT);
    if (shmid == -1) {
        return EXIT_CODE_ERRNO_1;
    }

    void *shm = shmat(shmid, 0, 0);
    if (shm == reinterpret_cast<void *>(-1)) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_2;
    }
    shared = (struct shared_use_st *)shm;
    ret = strncmp(shared->test, testBuf, strlen(testBuf));
    if (ret != 0) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_3;
    }
    ret = shmdt(shm);
    if (ret ==  -1) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_4;
    }
    return 0;
}

static int childFunc(void *arg)
{
    const char testBuf[] = "parent test shm";
    const char testBuf1[] = "child test shm";
    int ret, status, pid, exitCode;
    (void)arg;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    if (stack == nullptr) {
        return EXIT_CODE_ERRNO_17;
    }
    char *stackTop = stack + STACK_SIZE;
    auto linkBuffer = ReadlinkContainer(getpid(), containerType);

    int shmid = shmget((key_t)SHM_TEST_KEY1, sizeof(struct shared_use_st), SHM_TEST_OPEN_PERM | IPC_CREAT);
    if (shmid == -1) {
        return EXIT_CODE_ERRNO_1;
    }

    void *shm = shmat(shmid, 0, 0);
    if (shm == reinterpret_cast<void *>(-1)) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_2;
    }

    struct shared_use_st *shared = (struct shared_use_st *)shm;
    ret = strncmp(shared->test, testBuf, strlen(testBuf));
    if (ret == 0) {
        ret = EXIT_CODE_ERRNO_3;
        goto EXIT;
    }

    ret = memcpy_s(shared->test, sizeof(struct shared_use_st), testBuf1, sizeof(testBuf1));
    if (ret != 0) {
        ret = EXIT_CODE_ERRNO_4;
        goto EXIT;
    }

    pid = clone(childFunc1, stackTop, SIGCHLD, &arg);
    if (pid == -1) {
        ret = EXIT_CODE_ERRNO_5;
        goto EXIT;
    }

    ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        ret = EXIT_CODE_ERRNO_6;
        goto EXIT;
    }

    ret = WIFEXITED(status);
    if (ret == 0) {
        ret = EXIT_CODE_ERRNO_7;
        goto EXIT;
    }

    exitCode = WEXITSTATUS(status);
    if (exitCode != 0) {
        ret = EXIT_CODE_ERRNO_8;
        goto EXIT;
    }

    ret = shmdt(shm);
    if (ret == -1) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_9;
    }

    ret = shmctl(shmid, IPC_RMID, 0);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_10;
    }

    return 0;
EXIT:
    shmdt(shm);
    shmctl(shmid, IPC_RMID, 0);
    return ret;
}

void ItIpcContainer004(void)
{
    const char testBuf[] = "parent test shm";
    int pid, exitCode, status, ret;
    void *shm = NULL;
    struct shmid_ds ds = {};
    struct shminfo info = {};

    int arg = CHILD_FUNC_ARG;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;

    int shmid = shmget((key_t)SHM_TEST_KEY1, sizeof(struct shared_use_st), SHM_TEST_OPEN_PERM | IPC_CREAT);
    ShmFinalizer ShmFinalizer(shm, shmid);
    ASSERT_NE(shmid, -1);

    shm = shmat(shmid, 0, 0);
    ASSERT_NE((int)shm, -1);

    struct shared_use_st *shared = (struct shared_use_st *)shm;
    ret = memcpy_s(shared->test, sizeof(struct shared_use_st), testBuf, sizeof(testBuf));
    ASSERT_EQ(ret, 0);

    pid = clone(childFunc, stackTop, CLONE_NEWIPC | SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);

    ret = shmctl(shmid, IPC_STAT, &ds);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(ds.shm_segsz, PAGE_SIZE);
    ASSERT_EQ(ds.shm_nattch, 1);
    ASSERT_EQ(ds.shm_cpid, getpid());
    ASSERT_EQ(ds.shm_lpid, getpid());
    ASSERT_EQ(ds.shm_perm.uid, getuid());

    ret = shmctl(shmid, SHM_STAT, &ds);
    ASSERT_NE(ret, -1);
    ASSERT_NE(ret, 0);

    ds.shm_perm.uid = getuid();
    ds.shm_perm.gid = getgid();
    ds.shm_perm.mode = 0;
    ret = shmctl(shmid, IPC_SET, &ds);
    ASSERT_EQ(ret, 0);

    ret = shmctl(shmid, IPC_INFO, (struct shmid_ds *)&info);
    ASSERT_EQ(ret, 192); /* 192: test value */
    ASSERT_EQ(info.shmmax, 0x1000000); /* 0x1000000: Shared memory information  */
    ASSERT_EQ(info.shmmin, 1); /* 1: Shared memory information */
    ASSERT_EQ(info.shmmni, 192); /* 192: Shared memory information */
    ASSERT_EQ(info.shmseg, 128); /* 128: Shared memory information */
    ASSERT_EQ(info.shmall, 0x1000); /* 0x1000: Shared memory information */

    ret = shmdt(shm);
    ASSERT_NE(ret, -1);

    ret = shmctl(shmid, IPC_RMID, NULL);
    ASSERT_EQ(ret, 0);
}
