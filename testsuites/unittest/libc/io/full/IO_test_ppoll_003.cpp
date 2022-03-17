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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "It_test_IO.h"
#include "pthread.h"
#include "signal.h"

const int BUF_SIZE = 128;
const int DELAY_TIME = 200;

static INT32 pipeFdPpoll[2];
static INT32 g_step = 1;
static CHAR strBuf[] = "hello world.";
static struct pollfd pfd;
static sigset_t sigMask;
static UINT32 count = 0;

static void signalHandle(INT32 sigNum)
{
    g_step++;
    return;
}

static void *pthread_01(void *arg)
{
    INT32 retVal;
    CHAR buf[BUF_SIZE];

    (void)signal(SIGUSR1, signalHandle);

    while (1) {
        /* 执行ppoll监视文件描述符 */
        while (g_step < 2) { /* 2, 2nd step */
            usleep(DELAY_TIME);
        }
        g_step++;
        retVal = ppoll(&pfd, 1, NULL, &sigMask);

        ICUNIT_ASSERT_NOT_EQUAL_NULL(retVal, -1, retVal);
        
        /* 判断revents */
        if (pfd.revents & POLLIN) {
            (void)memset_s(buf, sizeof(buf), 0, sizeof(buf));
            retVal = read(pfd.fd, buf, BUF_SIZE);
            ICUNIT_ASSERT_NOT_EQUAL_NULL(retVal, -1, retVal);

            retVal = strcmp(strBuf, buf);
            ICUNIT_ASSERT_EQUAL_NULL(retVal, 0, retVal);

            count++;
        } else {
            ICUNIT_ASSERT_NOT_EQUAL_NULL(pfd.revents & POLLIN, 0, pfd.revents & POLLIN);
        }
        g_step++;
        
        if (g_step >= 7) { /* 7, 7th step */
            ICUNIT_ASSERT_EQUAL_NULL(count, 2, count); /* 2, 2nd step */
            pthread_exit(NULL);
        }
    }

    return LOS_OK;
}

static UINT32 testcase(VOID)
{
    INT32 retVal;
    pthread_t tid;

    /* 建立管道 */
    while (g_step < 1) {
        usleep(DELAY_TIME);
    }
    retVal = pipe(pipeFdPpoll);
    ICUNIT_ASSERT_NOT_EQUAL(retVal, -1, retVal);
    
    /* 设置pfd sigmask */
    pfd.fd = pipeFdPpoll[0];
    pfd.events = POLLIN;
    pfd.revents = 0x0;

    sigemptyset(&sigMask);
    sigaddset(&sigMask, SIGUSR1);

    /* 开辟线程执行 ppoll */
    retVal = pthread_create(&tid, NULL, pthread_01, NULL);
    ICUNIT_ASSERT_EQUAL(retVal, 0, retVal);
    g_step++;

    /* 向管道写入数据 */
    while (g_step < 3) { /* 3, 3ed step */
        usleep(DELAY_TIME);
    }
    sleep(1); /* 保证先挂起再写入数据 */
    retVal = write(pipeFdPpoll[1], "hello world.", sizeof(strBuf));
    ICUNIT_ASSERT_NOT_EQUAL(retVal, -1, retVal);

    /* 向线程发送信号 */
    while (g_step < 5) { /* 5, 5th step */
        usleep(DELAY_TIME);
    }
    sleep(1); /* 保证先挂起再发送信号 */
    retVal = pthread_kill(tid, SIGUSR1);
    ICUNIT_ASSERT_EQUAL(retVal, 0, retVal);

    /* 继续向管道写入数据 */
    ICUNIT_ASSERT_EQUAL(g_step, 5, g_step);    /* 5, sth。判断挂起解除之前信号没有被处理 */
    retVal = write(pipeFdPpoll[1], "hello world.", sizeof(strBuf));
    ICUNIT_ASSERT_NOT_EQUAL(retVal, -1, retVal);

    while (g_step < 7) { /* 7, 7th step */
        usleep(DELAY_TIME);
    }
    ICUNIT_ASSERT_EQUAL(count, 2, count); /* 2, 2nd step */
    /* 等待退出 */
    pthread_join(tid, NULL);

    return LOS_OK;
}

VOID IO_TEST_PPOLL_003(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}