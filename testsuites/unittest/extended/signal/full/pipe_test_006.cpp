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
#include "it_test_signal.h"
#include "signal.h"

static const int MAX_PIPES = 32;

static int PipeUnlinkTest()
{
    int pipefd[MAX_PIPES][2]; // 2, array subscript
    int tmpFd[2];
    pid_t pid;
    int retValue = -1;

    int status, ret;
    char devName[60]; // 60, array subscript
    pid = fork();
    if (pid == -1) {
        printf("Fork Error!\n");
        return -1;
    } else if (pid == 0) {
        for (int i = 0; i < MAX_PIPES; i++) {
            retValue = pipe(pipefd[i]);
            ICUNIT_ASSERT_EQUAL_EXIT(retValue, 0, __LINE__);
        }
        retValue = pipe(tmpFd);
        ICUNIT_ASSERT_EQUAL_EXIT(retValue, -1, __LINE__);
        for (int i = 0; i < MAX_PIPES; i++) {
            snprintf(devName, 60, "/dev/pipe%d", i); // 60, len of max size
            retValue = unlink(devName);
            ICUNIT_ASSERT_EQUAL_EXIT(retValue, -1, __LINE__);
            retValue = close(pipefd[i][0]);
            ICUNIT_ASSERT_EQUAL_EXIT(retValue, 0, __LINE__);
            retValue = unlink(devName);
            ICUNIT_ASSERT_EQUAL_EXIT(retValue, -1, __LINE__);
            retValue = close(pipefd[i][1]);
            ICUNIT_ASSERT_EQUAL_EXIT(retValue, 0, __LINE__);
            retValue = unlink(devName);
            ICUNIT_ASSERT_EQUAL_EXIT(retValue, 0, __LINE__);
        }
        exit(0);
    }
    usleep(15000); // 15000, Used to calculate the delay time.
    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    return 0;
}


void ItPosixPipe006(void)
{
    TEST_ADD_CASE(__FUNCTION__, PipeUnlinkTest, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
