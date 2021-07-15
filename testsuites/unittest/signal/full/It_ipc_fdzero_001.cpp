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


static const int TEST_LOOP_NUM = 4;
static const int TAR_STR_LEN = 12;

static UINT32 Testcase(VOID)
{
    int pipeFd[TEST_LOOP_NUM][2], ret, i; // 2, pipe return 2 file descirpter
    int fdmax = 0;
    struct timeval tv;
    fd_set reads;
    FD_ZERO(&reads);

    tv.tv_sec = 1;
    tv.tv_usec = 50; // 50, overtime setting
    for (i = 0; i < TEST_LOOP_NUM; i++) {
        ret = pipe(pipeFd[i]);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
        fdmax = pipeFd[i][1] > fdmax ? pipeFd[i][1] : fdmax;
        ret = write(pipeFd[i][1], "Aloha world", TAR_STR_LEN);
        printf("write first status: %d\n", ret);
        ICUNIT_GOTO_EQUAL(ret, TAR_STR_LEN, ret, EXIT);
    }

    ret = select(fdmax + 1, &reads, NULL, NULL, &tv);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    FD_SET(pipeFd[0][0], &reads);
    ret = select(fdmax + 1, &reads, NULL, NULL, &tv);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT1);
    ret = FD_ISSET(pipeFd[0][0], &reads);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT1);

    FD_SET(pipeFd[1][0], &reads);
    FD_SET(pipeFd[2][0], &reads); // 2, pipe return on the 2nd loop
    FD_SET(pipeFd[3][0], &reads); // 3, pipe return on the 3rd loop
    FD_ZERO(&reads);
    ret = select(fdmax + 1, &reads, NULL, NULL, &tv);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    for (i = 0; i < TEST_LOOP_NUM; i++) {
        ret = FD_ISSET(pipeFd[i][0], &reads);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    }

    for (i = 0; i < TEST_LOOP_NUM; i++) {
        close(pipeFd[i][0]);
        close(pipeFd[i][1]);
    }

    return LOS_OK;
EXIT1:
    i = TEST_LOOP_NUM - 1;
EXIT:
    for (i; i >= 0; i--) {
        close(pipeFd[i][0]);
        close(pipeFd[i][1]);
    }
    return LOS_NOK;
}

VOID ItIpcFdZero001(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
