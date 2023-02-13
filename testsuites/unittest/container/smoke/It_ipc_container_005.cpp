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

static const char testBuf[] = "test shm";
struct shared_use_st {
    char test[SHM_TEST_DATA_SIZE];
};

static int childFunc(void *arg)
{
    struct shared_use_st *shared = NULL;
    int ret;
    (void)arg;
    char *containerType = "ipc";
    auto linkBuffer = ReadlinkContainer(getpid(), containerType);

    ret = unshare(CLONE_NEWIPC);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_1;
    }

    auto linkBuffer1 = ReadlinkContainer(getpid(), containerType);
    ret = linkBuffer.compare(linkBuffer1);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_2;
    }

    int shmid = shmget((key_t)SHM_TEST_KEY1, sizeof(struct shared_use_st), SHM_TEST_OPEN_PERM | IPC_CREAT);
    if (shmid == -1) {
        return EXIT_CODE_ERRNO_3;
    }

    void *shm = shmat(shmid, 0, 0);
    if (shm == reinterpret_cast<void *>(-1)) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_4;
    }

    shared = (struct shared_use_st *)shm;
    ret = strncmp(shared->test, testBuf, strlen(testBuf));
    if (ret == 0) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_5;
    }

    ret = shmdt(shm);
    if (ret == -1) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_6;
    }

    ret = shmctl(shmid, IPC_RMID, 0);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_7;
    }
    return 0;
}

static int testChild(void *arg)
{
    (void)arg;
    int ret;
    struct shared_use_st *shared = NULL;
    int status;
    int exitCode;
    int pid;

    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    if (stack == nullptr) {
        return EXIT_CODE_ERRNO_17;
    }
    char *stackTop = stack + STACK_SIZE;

    int shmid = shmget((key_t)SHM_TEST_KEY1, sizeof(struct shared_use_st), SHM_TEST_OPEN_PERM | IPC_CREAT);
    if (shmid == -1) {
        return EXIT_CODE_ERRNO_8;
    }

    void *shm = shmat(shmid, 0, 0);
    if (shm == reinterpret_cast<void *>(-1)) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_9;
    }

    shared = (struct shared_use_st *)shm;
    ret = memcpy_s(shared->test, sizeof(struct shared_use_st), testBuf, sizeof(testBuf));
    if (ret != 0) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_10;
    }

    pid = clone(childFunc, stackTop, SIGCHLD, &arg);
    if (pid == -1) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_11;
    }

    ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_12;
    }

    ret = WIFEXITED(status);
    if (ret == 0) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_13;
    }

    exitCode = WEXITSTATUS(status);
    if (exitCode != 0) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_14;
    }

    ret = shmdt(shm);
    if (ret == -1) {
        shmctl(shmid, IPC_RMID, 0);
        return EXIT_CODE_ERRNO_15;
    }

    ret = shmctl(shmid, IPC_RMID, 0);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_16;
    }
    return 0;
}

void ItIpcContainer005(void)
{
    int ret;
    int status;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    ASSERT_NE(stack, nullptr);

    char *stackTop = stack + STACK_SIZE;

    int arg = CHILD_FUNC_ARG;
    auto pid = clone(testChild, stackTop, CLONE_NEWIPC | SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    int exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, 0);
}
