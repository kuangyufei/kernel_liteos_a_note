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

static int Testcase(void)
{
    const int memSize = 1024;
    int shmid;
    char *shared = NULL;
    char testStr[] = "hello shmem";
    pid_t pid;
    int ret;
    int status;

    shmid = shmget((key_t)1234, memSize, 0666 | IPC_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);

    ret = fork();
    if (ret == 0) {
        usleep(100000);
        if ((shared = (char *)shmat(shmid, 0, 0)) == (void *)-1) {
            printf("child : error: shmat()\n");
            exit(1);
        }

        if (strncmp(shared, testStr, sizeof(testStr)) != 0) {
            printf("child : error strncmp() shared = %s\n", shared);
            exit(1);
        }

        if ((shmdt(shared)) < 0) {
            printf("child : error : shmdt()\n");
            exit(1);
        }

        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            printf("child : error : shmctl()\n");
            exit(1);
        }

        exit(0);
    } else {
        pid = ret;
        usleep(50000);

        shared = (char *)shmat(shmid, 0, 0);
        ICUNIT_ASSERT_NOT_EQUAL(shared, (void *)-1, shared);

        ret = strncpy_s(shared, memSize, testStr, sizeof(testStr));
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        ret = shmdt(shared);
        ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

        usleep(100000);

        ret = wait(&status);
        status = WEXITSTATUS(status);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(status, 0, status);
    }

    return 0;
}

void ItTestShm007(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_007", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
