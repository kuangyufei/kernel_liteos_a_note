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
#include "sys/shm.h"
#include "it_test_signal.h"
#include "signal.h"

static void SigPrint(int signum)
{
    printf("Pipe break\n");
}

static int PipecommonWrite()
{
    int pipefd[2]; // 2, array subscript
    pid_t pid;
    int retValue = -1;
    int *sharedflag = NULL;
    int shmid;
    int *readFd = &pipefd[0];
    int *writeFd = &pipefd[1];
    char sentence[] = "Hello World";
    char readbuffer[100];
    int status, ret;

    retValue = pipe(pipefd);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    if (signal(SIGPIPE, SigPrint) == SIG_ERR) {
        printf("signal error\n");
    }

    shmid = shmget((key_t)IPC_PRIVATE, sizeof(int), 0666 | IPC_CREAT); // 0666 the authority of the shm
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);
    sharedflag = (int *)shmat(shmid, NULL, 0);
    *sharedflag = 0;

    pid = fork();
    if (pid == -1) {
        printf("Fork Error!\n");
        return -1;
    } else if (pid == 0) {
        sharedflag = (int *)shmat(shmid, NULL, 0);
        close(*readFd);
        retValue = write(*writeFd, sentence, strlen(sentence) + 1);
        ICUNIT_ASSERT_EQUAL(retValue, strlen(sentence) + 1, retValue);
        *sharedflag = 1;
        // 2 waitting for the father process close the pipe's read port
        while (*sharedflag != 2) {
            usleep(1);
        }
        retValue = write(*writeFd, sentence, strlen(sentence) + 1);
        ICUNIT_ASSERT_EQUAL(retValue, -1, retValue);
        ICUNIT_ASSERT_EQUAL(errno, EPIPE, errno);
        exit(0);
    } else {
        close(*writeFd);
        // 1 waitting for the sub process has written the sentence first
        while (*sharedflag != 1) {
            usleep(1);
        }
        close(*readFd);
        // 2 father process close the pipe's read port
        *sharedflag = 2;
        ret = waitpid(pid, &status, 0);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    }
    return 0;
}

void ItPosixPipe005(void)
{
    TEST_ADD_CASE(__FUNCTION__, PipecommonWrite, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
