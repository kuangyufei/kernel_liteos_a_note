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

#ifndef _LITEIPC_H
#define _LITEIPC_H

#include <stdint.h>

#define LITEIPC_DRIVER "/dev/lite_ipc"

typedef enum { OBJ_FD, OBJ_PTR, OBJ_SVC } ObjType;

typedef struct {
    uint32_t buffSz;
    void *buff;
} BuffPtr;

typedef struct {
    uint32_t handle;
    uint32_t token;
    uint32_t cookie;
} SvcIdentity;

typedef union {
    uint32_t fd;
    BuffPtr ptr;
    SvcIdentity svc;
} ObjContent;

typedef struct {
    ObjType type;
    ObjContent content;
} SpecialObj;

typedef enum { MT_REQUEST, MT_REPLY, MT_FAILED_REPLY, MT_DEATH_NOTIFY } MsgType;
typedef enum { CMS_GEN_HANDLE, CMS_REMOVE_HANDLE, CMS_ADD_ACCESS } CmsCmd;

/* lite ipc ioctl */
#define IPC_IOC_MAGIC   'i'
#define IPC_SET_CMS         _IO(IPC_IOC_MAGIC, 1)
#define IPC_CMS_CMD         _IOWR(IPC_IOC_MAGIC, 2, CmsCmdContent)
#define IPC_SET_IPC_THREAD  _IO(IPC_IOC_MAGIC, 3)
#define IPC_SEND_RECV_MSG   _IOWR(IPC_IOC_MAGIC, 4, IpcContent)

typedef struct {
    CmsCmd cmd;
    uint32_t taskID;
    uint32_t serviceHandle;
} CmsCmdContent;

typedef struct {
    MsgType type;       /**< cmd type, decide the data structure below */
    SvcIdentity target; /**< serviceHandle or targetTaskId, depending on type */
    uint32_t code;
    uint32_t flag;
#if (USE_TIMESTAMP == YES)
    uint64_t timestamp;
#endif
    uint32_t dataSz;    /**< size of data */
    void* data;
    uint32_t spObjNum;
    void* offsets;
    uint32_t processID; /**< filled by kernel, processId of sender/reciever */
    uint32_t taskID;    /**< filled by kernel, taskId of sender/reciever */
#ifdef LOSCFG_SECURITY_CAPABILITY
    uint32_t userID;
    uint32_t gid;
#endif
} IpcMsg;

#define SEND (1 << 0)
#define RECV (1 << 1)
#define BUFF_FREE (1 << 2)

typedef struct {
    uint32_t flag;
    IpcMsg *outMsg; /**< data to send to target */
    IpcMsg *inMsg;  /**< data reply by target */
    void *buffToFree;
} IpcContent;

#endif //_LITEIPC_H