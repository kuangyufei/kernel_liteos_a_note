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
#include <poll.h>
#include "signal.h"
#include "pthread.h"

#define LISTEN_FD_NUM 10
#define POLL_EVENTS 1

int pipeFdPpoll[LISTEN_FD_NUM][2];
static pthread_t g_tid = -1;
extern int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigmask);

static void *pthread_01(void)
{
    static int count = 0;
    int total_num = 0;
    int times = 3;
    int i, ret;
    struct pollfd fds[LISTEN_FD_NUM] = { 0 };
    char buffer[20];
    struct timespec t = { 3,0 };
    sigset_t sigset;

    /* TEST_PRINT("[INFO]%s:%d,%s,Create thread %d\n", __FILE__, __LINE__, __func__, count); */
    count++;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);    /* 把SIGALRM 信号添加到sigset 信号集中 */
    sigaddset(&sigset, SIGUSR1);

    for (i = 0; i < LISTEN_FD_NUM; i++) {
        fds[i].fd = pipeFdPpoll[i][0];
        fds[i].events = POLL_EVENTS;
    }

    while (times--) {
        ret = ppoll(fds, LISTEN_FD_NUM, &t, &sigset);
        total_num += ((ret > 0) ? ret : 0);

        if (ret <= 0) {
            continue;
        }

        for (i = 0; i < LISTEN_FD_NUM; i++) {
            if (fds[i].revents & POLL_EVENTS) {
                ret = read(fds[i].fd, buffer, 12);
                ICUNIT_GOTO_EQUAL(ret, 12, ret, EXIT);
                ret = strcmp(buffer, "hello world");
                TEST_PRINT("[EVENT]%s:%d,%s,buffer=%s\n", __FILE__, __LINE__, __func__, buffer);
            }
        }

        if (total_num == LISTEN_FD_NUM) {
            break;
        }
    }

    /* ICUNIT_GOTO_EQUAL(total_num, 10, -1, EXIT); */
    /* TEST_PRINT("[INFO]%s:%d,%s,total_num=%d\n", __FILE__, __LINE__, __func__, total_num); */
EXIT:
    return nullptr;
}

static UINT32 testcase1(VOID)
{
    int i;
    int ret;
    int count = 0;

    for (i = 0; i < LISTEN_FD_NUM; i++) {
        ret = pipe(pipeFdPpoll[i]);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
        TEST_PRINT("[INFO]%s:%d,%s,ret=%d,pipeFdPpoll[%d][0]=%d\n", __FILE__, __LINE__, __func__, ret, i, pipeFdPpoll[i][0]);
        TEST_PRINT("[INFO]%s:%d,%s,ret=%d,pipeFdPpoll[%d][0]=%d\n", __FILE__, __LINE__, __func__, ret, i, pipeFdPpoll[i][1]);
    }

    errno = 0;
    ret = pthread_create(&g_tid, nullptr, (void *(*)(void *))pthread_01, nullptr);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    errno = 0;
    for (i = 0; i < LISTEN_FD_NUM; i++) {
        if ((pipeFdPpoll[i][1] != 9) && (pipeFdPpoll[i][1] != 10) && (pipeFdPpoll[i][1] != 11)) {
            ret = write(pipeFdPpoll[i][1], "hello world!", 12); /* 12, "hello world" length and '\0' */
        }
        ICUNIT_GOTO_EQUAL(ret, 12, ret, EXIT);
        TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    }

#if 1 /* write looply */
    while(1) {
        if (count++ > 3) {
            break;
        }
        sleep(1);
        for (i = 0; i < LISTEN_FD_NUM; i++) {
            errno = 0;
            ret = pthread_create(&g_tid, nullptr, (void *(*)(void *))pthread_01, nullptr);
            TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

            if ((pipeFdPpoll[i][1] == 13) || (pipeFdPpoll[i][1] == 14) || (pipeFdPpoll[i][1] == 15)) {
                errno = 0;
                ret = write(pipeFdPpoll[i][1], "World HELLO!", 12); /* 12, "hello world" length and '\0' */
                TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
                TEST_PRINT("[INFO]%s:%d,%s,ret=%d,pipeFdPpoll[%d][1]=%d,count=%d\n", __FILE__, __LINE__, __func__, ret,i, pipeFdPpoll[i][1], count);
            }
            /* ICUNIT_GOTO_EQUAL(ret, 12, ret, EXIT); */
        }
    }
#endif

    pthread_join(g_tid, nullptr);

    for (i = 0; i < LISTEN_FD_NUM; i++) {
        close(pipeFdPpoll[i][0]);
        close(pipeFdPpoll[i][1]);
    }

    return LOS_OK;

EXIT:
    for (i = 0; i < LISTEN_FD_NUM; i++) {
        close(pipeFdPpoll[i][0]);
        close(pipeFdPpoll[i][1]);
    }
    return LOS_NOK;
}

static UINT32 testcase(VOID)
{
    testcase1();

    return LOS_OK;
}

VOID IO_TEST_PPOLL_001(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
