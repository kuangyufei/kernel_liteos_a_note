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

#include "it_test_liteipc.h"
#include "sys/wait.h"

#include "unistd.h"
#include "liteipc.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "sys/time.h"
#include "sys/ioctl.h"
#include "fcntl.h"

#include "smgr_demo.h"

#define NEED_BREAK YES

static int g_ipcFd;
char g_serviceName[] = "ohos.testservice";

static int CallTestServiceLoop(uint32_t id)
{
    IpcContent data1;
    IpcMsg dataIn;
    IpcMsg dataOut;
    uint32_t ret;
    void *retptr = nullptr;
    uint32_t num = 0;
    uint32_t *ptr = nullptr;
    struct timeval test_time;
    struct timeval test_time2;
    unsigned int serviceHandle;

    printf("i am the client%d process, my process id is %d\n", id, getpid());
    ret = GetService(g_ipcFd, g_serviceName, sizeof(g_serviceName), &serviceHandle);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    retptr = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, g_ipcFd, 0);
    ICUNIT_ASSERT_NOT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    ret = GetService(g_ipcFd, g_serviceName, sizeof(g_serviceName), &serviceHandle);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    while (1) {
        num++;
#if (NEED_BREAK == YES)
        if (num > 50000) {
            break;
        }
#endif
        data1.flag = SEND | RECV;
        data1.outMsg = &dataOut;
        memset(data1.outMsg, 0, sizeof(IpcMsg));
        data1.outMsg->type = MT_REQUEST;
        data1.outMsg->target.handle = serviceHandle;
        data1.outMsg->dataSz = 4;
        data1.outMsg->data = &num;
        ret = ioctl(g_ipcFd, IPC_SEND_RECV_MSG, &data1);
        if (ret != 0) {
            printf("client%d CallTestServiceLoop ioctl ret:%d, errno:%d, num:%d\n", id, ret, errno, num);
            ICUNIT_ASSERT_EQUAL(errno, ETIME, errno);
            goto EXIT;
        } else {
            ptr = (uint32_t*)(data1.inMsg->data);
            ICUNIT_ASSERT_EQUAL(ptr[0], 0, ptr[0]);
            ICUNIT_ASSERT_EQUAL(ptr[1], 2 * num, ptr[1]);
            FreeBuffer(g_ipcFd, data1.inMsg);
        }
    }
    char tmpBuff[2048];
    data1.flag = SEND | RECV;
    data1.outMsg = &dataOut;
    memset(data1.outMsg, 0, sizeof(IpcMsg));
    data1.outMsg->type = MT_REQUEST;
    data1.outMsg->target.handle = serviceHandle;
    data1.outMsg->dataSz = 1024;
    data1.outMsg->data = tmpBuff;
    ret = ioctl(g_ipcFd, IPC_SEND_RECV_MSG, &data1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    FreeBuffer(g_ipcFd, data1.inMsg);
    data1.outMsg->dataSz = 2048;
    data1.outMsg->data = tmpBuff;
    ret = ioctl(g_ipcFd, IPC_SEND_RECV_MSG, &data1);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    data1.outMsg->spObjNum = 300;
    data1.outMsg->offsets = tmpBuff;
    ret = ioctl(g_ipcFd, IPC_SEND_RECV_MSG, &data1);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
EXIT:
    exit(0);
    return 0;
}

static void HandleRequest(IpcMsg *data)
{
    uint32_t *ptr = (uint32_t*)data->data;
    SendReply(g_ipcFd, data, 0, 2 * ptr[0]);
}

static int TestServiceLoop(void)
{
    IpcContent data1;
    IpcMsg dataIn;
    IpcMsg dataOut;
    int ret;
    void *retptr = nullptr;
    struct timeval last_time;
    struct timeval test_time;
    int cnt = 0;
    unsigned int serviceHandle;

    printf("i am the test service process, my process id is %d\n", getpid());
    ret = RegService(g_ipcFd, g_serviceName, sizeof(g_serviceName), &serviceHandle);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    retptr = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, g_ipcFd, 0);
    ICUNIT_ASSERT_NOT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    ret = RegService(g_ipcFd, g_serviceName, sizeof(g_serviceName), &serviceHandle);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    gettimeofday(&last_time, 0);
    while (1) {
        cnt++;
#if (NEED_BREAK == YES)
        if (cnt > 100000 - 10) {
            printf("TestServiceLoop break!\n");
            break;
        }
#endif
        data1.flag = RECV;
        ret = ioctl(g_ipcFd, IPC_SEND_RECV_MSG, &data1);
        if ((cnt % 1000000) == 0) {
            gettimeofday(&test_time, 0);
            printf("LiteIPC cnt:%d, time used:%d sec\n", cnt, test_time.tv_sec - last_time.tv_sec);
        }
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        switch (data1.inMsg->type) {
            case MT_REQUEST:
                HandleRequest(data1.inMsg);
                break;
            default:
                printf("request not support:%d!\n", data1.inMsg->type);
                FreeBuffer(g_ipcFd, data1.inMsg);
                break;
        }
    }
    sleep(6);
    exit(0);
    return 0;
}

static int LiteIpcTest(void)
{

    int count = 0;
    unsigned int ret;
    void *retptr = nullptr;
    pid_t farPid, sonPid, pid;
    int status;

    retptr = mmap(NULL, 16 * 4096, PROT_READ, MAP_PRIVATE, g_ipcFd, 0);
    ICUNIT_GOTO_NOT_EQUAL((int)(intptr_t)retptr, -1, retptr, EXIT1);

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT1);
    if (pid == 0) {
        TestServiceLoop();
        exit(-1);
    }
    sleep(1);//wait server start

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT1);
    if (pid == 0) {
        CallTestServiceLoop(1);
        exit(-1);
    }

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT1);
    if (pid == 0) {
        CallTestServiceLoop(2);
        exit(-1);
    }

    ret = waitpid(-1, &status, 0);
    printf("waitpid1 ret:%d\n", ret);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 0, status, EXIT2);

    ret = waitpid(-1, &status, 0);
    printf("waitpid2 ret:%d\n", ret);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 0, status, EXIT2);

    ret = waitpid(-1, &status, 0);
    printf("waitpid3 ret:%d\n", ret);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 0, status, EXIT2);

    return 0;
EXIT1:
    return 1;
EXIT2:
    printf("EXIT2 status:%d\n", status);
    return status;
}

static int TestCase(void)
{
    int ret;
    int status;
    g_ipcFd = open(LITEIPC_DRIVER, O_RDWR);
    ICUNIT_ASSERT_NOT_EQUAL(g_ipcFd, -1, g_ipcFd);

    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT);
    if (pid == 0) {
        sleep(1);//wait cms start
        ret = LiteIpcTest();
        StopCms(g_ipcFd);
        if (ret == 0) {
            exit(0);
        }
        if (ret == 1) {
            exit(-1);
        }
        exit(ret);
    }

    StartCms(g_ipcFd);

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 0, status, EXIT);

    return 0;
EXIT:
    return 1;
}

void ItPosixLiteIpc002(void)
{
    TEST_ADD_CASE("ItPosixLiteIpc002", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}

