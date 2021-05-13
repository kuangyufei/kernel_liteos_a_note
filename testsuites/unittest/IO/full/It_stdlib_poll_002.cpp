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
#include "It_test_IO.h"

int g_pipeFd[10][2]; // 2, read and write; 10, listen fd number.
static pthread_t g_tid = -1;
static const int LISTEN_FD_NUM = 10;

static void *Pthread01(void *arg)
{
    int totalNum = 0;
    int times = 3; // 3, loop number for test.
    int i, ret;
    struct pollfd fds[LISTEN_FD_NUM] = {0};
    char buffer[20]; // 20, enough space for test.
    const int pollEvents = 1;

    for (i = 0; i < LISTEN_FD_NUM; i++) {
        fds[i].fd = g_pipeFd[i][0];
        fds[i].events = pollEvents;
    }

    while (times--) {
        ret = poll(fds, LISTEN_FD_NUM, 1000); // 1000, wait time.
        totalNum += ((ret > 0) ? ret : 0);

        if (ret <= 0) {
            continue;
        }

        for (i = 0; i < LISTEN_FD_NUM; i++) {
            if (fds[i].revents & pollEvents) {
                ret = read(fds[i].fd, buffer, 12); // 12, "hello world" length and '\0'
                ICUNIT_GOTO_EQUAL(ret, 12, ret, EXIT); // 12, "hello world" length and '\0'
                ret = strcmp(buffer, "hello world");
                ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
            }
        }

        if (totalNum == LISTEN_FD_NUM) {
            break;
        }
    }

    ICUNIT_GOTO_EQUAL(totalNum, LISTEN_FD_NUM, -1, EXIT);

EXIT:
    return NULL;
}

static UINT32 Testcase(VOID)
{
    int i;
    int ret;

    for (i = 0; i < LISTEN_FD_NUM; i++) {
        ret = pipe(g_pipeFd[i]);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    }

    ret = pthread_create(&g_tid, NULL, Pthread01, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    for (i = 0; i < LISTEN_FD_NUM; i++) {
        ret = write(g_pipeFd[i][1], "hello world", 12); // 12, "hello world" length and '\0'
        ICUNIT_GOTO_EQUAL(ret, 12, ret, EXIT);          // 12, "hello world" length and '\0'
    }

    pthread_join(g_tid, NULL);

    for (i = 0; i < LISTEN_FD_NUM; i++) {
        close(g_pipeFd[i][0]);
        close(g_pipeFd[i][1]);
    }

    return LOS_OK;

EXIT:
    for (i = 0; i < LISTEN_FD_NUM; i++) {
        close(g_pipeFd[i][0]);
        close(g_pipeFd[i][1]);
    }
    return LOS_NOK;
}

VOID ItStdlibPoll002(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
