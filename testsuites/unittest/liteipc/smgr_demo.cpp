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
#include "signal.h"
#include "sys/wait.h"

#include "unistd.h"
#include "liteipc.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "sys/ioctl.h"
#include "sys/time.h"

#include "smgr_demo.h"

ServiceName g_serviceNameMap[MAX_SREVICE_NUM];
BOOL g_cmsRunningFlag = FALSE;

static void InitCms()
{
    memset(g_serviceNameMap, 0, sizeof(g_serviceNameMap));
}

uint32_t SetCms(int fd)
{
    return ioctl(fd, IPC_SET_CMS, 200);
}

void SendReply(int fd, IpcMsg *dataIn, uint32_t result, uint32_t serviceHandle)
{
    IpcContent data1;
    IpcMsg dataOut;
    unsigned int ret;
    uint32_t ptr[2];

    data1.flag = SEND | BUFF_FREE;
    data1.buffToFree = dataIn;
    data1.outMsg = &dataOut;
    memset(data1.outMsg, 0, sizeof(IpcMsg));
    data1.outMsg->type = MT_REPLY;
    data1.outMsg->target.handle = dataIn->taskID;
    data1.outMsg->target.token = dataIn->target.token;
    data1.outMsg->code = dataIn->code;
#if (USE_TIMESTAMP == YES)
    data1.outMsg->timestamp = dataIn->timestamp;
#endif
    ptr[0] = result;
    ptr[1] = serviceHandle;
    data1.outMsg->dataSz = 8;
    data1.outMsg->data = ptr;
    ret = ioctl(fd, IPC_SEND_RECV_MSG, &data1);
    if (ret) {
        printf("SendReply failed\n");
    }
}

void FreeBuffer(int fd, IpcMsg *dataIn)
{
    IpcContent data1;
    unsigned int ret;
    data1.flag = BUFF_FREE;
    data1.buffToFree = dataIn;
    ret = ioctl(fd, IPC_SEND_RECV_MSG, &data1);
    if (ret) {
        printf("FreeBuffer failed\n");
    }
}

static uint32_t SendCmsCmd(int fd, CmsCmdContent *content)
{
    unsigned int ret;
    ret = ioctl(fd, IPC_CMS_CMD, content);
    if (ret) {
        printf("SendCmsCmd failed\n");
    }
    return ret;
}

uint32_t RegService(int fd, char *serviceName, uint32_t nameLen, uint32_t *serviceHandle)
{
    IpcContent data1;
    IpcMsg dataIn;
    IpcMsg dataOut;
    uint32_t ret;
    uint32_t *ptr = nullptr;
    ServiceName name;

    if (nameLen > NAME_LEN_MAX) {
        return -1;
    }
    memcpy(name.serviceName, serviceName, nameLen);
    name.nameLen = nameLen;

    data1.flag = SEND | RECV;
    data1.outMsg = &dataOut;
    memset(data1.outMsg, 0, sizeof(IpcMsg));
    data1.outMsg->type = MT_REQUEST;
    data1.outMsg->target.handle = 0;
    data1.outMsg->code = REG_CODE;
    data1.outMsg->dataSz = sizeof(ServiceName);
    data1.outMsg->data = &name;

    ret = ioctl(fd, IPC_SEND_RECV_MSG, &data1);
    if (ret != 0) {
        printf("RegService failed\n");
        return ret;
    }
    ptr = (uint32_t*)(data1.inMsg->data);
    *serviceHandle = ptr[1];
    FreeBuffer(fd, data1.inMsg);
    return ptr[0];
}

uint32_t GetService(int fd, char *serviceName, uint32_t nameLen, uint32_t *serviceHandle)
{
    IpcContent data1;
    IpcMsg dataIn;
    IpcMsg dataOut;
    uint32_t ret;
    uint32_t *ptr = nullptr;
    ServiceName name;

    if (nameLen > NAME_LEN_MAX) {
        return -1;
    }
    memcpy(name.serviceName, serviceName, nameLen);
    name.nameLen = nameLen;

    data1.flag = SEND | RECV;
    data1.outMsg = &dataOut;
    memset(data1.outMsg, 0, sizeof(IpcMsg));
    data1.outMsg->type = MT_REQUEST;
    data1.outMsg->target.handle = 0;
    data1.outMsg->code = GET_CODE;
    data1.outMsg->dataSz = sizeof(ServiceName);
    data1.outMsg->data = &name;

    ret = ioctl(fd, IPC_SEND_RECV_MSG, &data1);
    if (ret != 0) {
        return ret;
    }
    ptr = (uint32_t*)(data1.inMsg->data);
    *serviceHandle = ptr[1];
    FreeBuffer(fd, data1.inMsg);
    return ptr[0];
}

static void HandleServiceRegAndGet(int fd, IpcMsg *data)
{
    uint32_t ret, i;

    if (data->code == STOP_CODE) {
        g_cmsRunningFlag = FALSE;
        return;
    }
    
    ServiceName *info = (ServiceName*)(data->data);
    CmsCmdContent content;
    if ((info->nameLen == 0) || (info->serviceName == NULL)) {
        goto ERROR_EXIT;
    }
    for (i = 0; i < MAX_SREVICE_NUM; i++) {
        if (g_serviceNameMap[i].serviceName != NULL && g_serviceNameMap[i].nameLen == info->nameLen) {
            if(memcmp(g_serviceNameMap[i].serviceName, info->serviceName, info->nameLen) == 0) {
                break;
            }
        }
    }
    printf("recieve service request, code:%d, service name:%s\n", data->code, info->serviceName);
    switch (data->code) {
        case REG_CODE:
            if (i == MAX_SREVICE_NUM) {
                content.cmd = CMS_GEN_HANDLE;
                content.taskID = data->taskID;
                ret = SendCmsCmd(fd, &content);
                if (ret) {
                    goto ERROR_EXIT;
                }
                if (g_serviceNameMap[content.serviceHandle].serviceName != NULL && g_serviceNameMap[content.serviceHandle].nameLen == info->nameLen) {
                    printf("the task has already a service named:%s\n", g_serviceNameMap[content.serviceHandle].serviceName);
                    goto ERROR_REG;
                } else {
                    memcpy(g_serviceNameMap[content.serviceHandle].serviceName, info->serviceName, info->nameLen);
                    g_serviceNameMap[content.serviceHandle].nameLen = info->nameLen;
                    SendReply(fd, data, 0, content.serviceHandle);
                }
            }else {
                printf("this service already registed\n");
                goto ERROR_EXIT;
            }
            break;
        case GET_CODE:
            if (i == MAX_SREVICE_NUM) {
                goto ERROR_EXIT;
            }else {
                content.cmd = CMS_ADD_ACCESS;
                content.taskID = data->taskID;
                content.serviceHandle = i;
                SendCmsCmd(fd, &content);
                SendReply(fd, data, 0, i);
            }
            break;
        default:
            break;
    }
    return;
ERROR_REG:
    content.cmd = CMS_REMOVE_HANDLE;
    SendCmsCmd(fd, &content);
ERROR_EXIT:
    SendReply(fd, data, -1, 0);
}

static uint32_t CmsLoop(int fd)
{
    IpcContent data1;
    IpcMsg dataIn;
    IpcMsg dataOut;
    uint32_t ret;
    g_cmsRunningFlag = TRUE;
    while (g_cmsRunningFlag == TRUE) {
        data1.flag = RECV;
        ret = ioctl(fd, IPC_SEND_RECV_MSG, &data1);
        if (ret != 0) {
            printf("bad request!\n");
            continue;
        }
        switch (data1.inMsg->type) {
            case MT_REQUEST:
                HandleServiceRegAndGet(fd, data1.inMsg);
                break;
            default:
                printf("request not support:%d!\n", data1.inMsg->type);
                FreeBuffer(fd, data1.inMsg);
                break;
        }
    }
}

void StartCms(int fd)
{
    InitCms();
    CmsLoop(fd);
}

void StopCms(int fd)
{
    IpcContent data1;
    IpcMsg dataOut;
    int ret;

    data1.flag = SEND;
    data1.outMsg = &dataOut;
    memset(data1.outMsg, 0, sizeof(IpcMsg));
    data1.outMsg->type = MT_REQUEST;
    data1.outMsg->target.handle = 0;
    data1.outMsg->code = STOP_CODE;
    data1.outMsg->dataSz = 0;
    data1.outMsg->data = 0;

    ret = ioctl(fd, IPC_SEND_RECV_MSG, &data1);
    if (ret != 0) {
        printf("StopCms failed ioctl ret:%d!\n", ret);
    }
}

