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
#include "pthread.h"

const int BUF_SIZE = 128;
const int DELAY_TIME = 200;

static INT32 pipeFdPpoll[2];
static INT32 g_step = 1;
static CHAR strBuf[] = "hello world.";
static struct pollfd pfd;

static void *pthread_01(void *arg)
{
    INT32 retVal;
    CHAR buf[BUF_SIZE];
    
    /* 执行ppoll监视文件描述符 */
    while (g_step < 3) { /* 3, 3rd step */
        usleep(DELAY_TIME);
    }
    g_step++;
    retVal = ppoll(&pfd, 1, NULL, NULL);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(retVal, -1, retVal);
    
    while (g_step < 5) { /* 5, 5th step */
        usleep(DELAY_TIME);
    }
    g_step++;
    
    /* 判断revents */
    if (pfd.revents & POLLIN) {
        memset_s(buf, sizeof(buf), 0, sizeof(buf));
        retVal = read(pfd.fd, buf, BUF_SIZE);
        ICUNIT_ASSERT_NOT_EQUAL_NULL(retVal, -1, retVal);

        retVal = strcmp(strBuf, buf);
        ICUNIT_ASSERT_EQUAL_NULL(retVal, 0, retVal);
    }
    
    while (g_step < 6) { /* 6, 6th step */
        usleep(DELAY_TIME);
    }
    pthread_exit(NULL);
}

STATIC UINT32 testcase(VOID)
{
    INT32 retVal;
    pthread_t tid;
    
    /* 建立管道 */
    while (g_step < 1) { /* 1, 1st step */
        usleep(DELAY_TIME);
    }
    retVal = pipe(pipeFdPpoll);
    ICUNIT_ASSERT_NOT_EQUAL(retVal, -1, retVal);
    g_step++;
    
    /* 设置pfd */
    pfd.fd = pipeFdPpoll[0];
    pfd.events = POLLIN;
    
    /* 开辟线程执行 ppoll */
    while (g_step < 2) { /* 2, 2nd step */
        usleep(DELAY_TIME);
    }
    retVal = pthread_create(&tid, NULL, pthread_01, NULL);
    ICUNIT_ASSERT_EQUAL(retVal, 0, retVal);
    g_step++;
    
    /* 向管道写入数据 */
    while (g_step < 4) { /* 4, 4th step */
        usleep(DELAY_TIME);
    }
    sleep(1);
    
    retVal = write(pipeFdPpoll[1], "hello world.", sizeof(strBuf));
    ICUNIT_ASSERT_NOT_EQUAL(retVal, -1, retVal);
    g_step++;

    pthread_join(tid, NULL);
    
    return LOS_OK;
}

VOID IO_TEST_PPOLL_001(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}