/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#ifndef HM_LITEIPC_H
#define HM_LITEIPC_H

#include "sys/ioctl.h"
#include "los_config.h"
#include "los_typedef.h"
#include "los_vm_map.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define LITEIPC_DRIVER "/dev/lite_ipc"
#define LITEIPC_DRIVER_MODE 0644
#define MAX_SERVICE_NUM LOSCFG_BASE_CORE_TSK_LIMIT
#define USE_TIMESTAMP 1

typedef enum {
    HANDLE_NOT_USED,
    HANDLE_REGISTING,
    HANDLE_REGISTED
} HandleStatus;

typedef struct {
    HandleStatus status;
    UINT32       taskID;
    UINTPTR      maxMsgSize;
} HandleInfo;

typedef struct {
    VOID   *uvaddr;
    VOID   *kvaddr;
    UINT32 poolSize;
} IpcPool;

typedef struct {
    IpcPool pool;
    UINT32 ipcTaskID;
    LOS_DL_LIST ipcUsedNodelist;
    UINT32 access[LOSCFG_BASE_CORE_TSK_LIMIT];
} ProcIpcInfo;

typedef struct {
    LOS_DL_LIST     msgListHead;
    BOOL            accessMap[LOSCFG_BASE_CORE_PROCESS_LIMIT];
} IpcTaskInfo;

typedef enum {
    OBJ_FD,
    OBJ_PTR,
    OBJ_SVC
} ObjType;

typedef struct {
    UINT32         buffSz;
    VOID           *buff;
} BuffPtr;

typedef struct {
    UINT32         handle;
    UINT32         token;
    UINT32         cookie;
} SvcIdentity;

typedef union {
    UINT32      fd;
    BuffPtr     ptr;
    SvcIdentity  svc;
} ObjContent;

typedef struct {
    ObjType     type;
    ObjContent  content;
} SpecialObj;

typedef enum {
    MT_REQUEST,
    MT_REPLY,
    MT_FAILED_REPLY,
    MT_DEATH_NOTIFY,
    MT_NUM
} MsgType;

typedef struct {
    int32_t driverVersion;
} IpcVersion;

/* lite ipc ioctl */
#define IPC_IOC_MAGIC       'i'
#define IPC_SET_CMS         _IO(IPC_IOC_MAGIC, 1)
#define IPC_CMS_CMD         _IOWR(IPC_IOC_MAGIC, 2, CmsCmdContent)
#define IPC_SET_IPC_THREAD  _IO(IPC_IOC_MAGIC, 3)
#define IPC_SEND_RECV_MSG   _IOWR(IPC_IOC_MAGIC, 4, IpcContent)
#define IPC_GET_VERSION     _IOR(IPC_IOC_MAGIC, 5, IpcVersion)

typedef enum {
    CMS_GEN_HANDLE,
    CMS_REMOVE_HANDLE,
    CMS_ADD_ACCESS
} CmsCmd;

typedef struct {
    CmsCmd        cmd;
    UINT32        taskID;
    UINT32        serviceHandle;
} CmsCmdContent;

typedef enum {
    LITEIPC_FLAG_DEFAULT = 0, // send and reply
    LITEIPC_FLAG_ONEWAY,      // send message only
} IpcFlag;

typedef struct {
    MsgType        type;       /**< cmd type, decide the data structure below*/
    SvcIdentity    target;    /**< serviceHandle or targetTaskId, depending on type */
    UINT32         code;      /**< service function code */
    UINT32         flag;
#if (USE_TIMESTAMP == 1)
    UINT64         timestamp;
#endif
    UINT32         dataSz;    /**< size of data */
    VOID           *data;
    UINT32         spObjNum;
    VOID           *offsets;
    UINT32         processID; /**< filled by kernel, processId of sender/receiver */
    UINT32         taskID;    /**< filled by kernel, taskId of sender/receiver */
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32         userID;
    UINT32         gid;
#endif
} IpcMsg;

typedef struct {
    IpcMsg         msg;
    LOS_DL_LIST    listNode;
} IpcListNode;

#define SEND (1 << 0)
#define RECV (1 << 1)
#define BUFF_FREE (1 << 2)

typedef struct {
    UINT32               flag;      /**< size of writeData */
    IpcMsg               *outMsg;   /**< data to send to target */
    IpcMsg               *inMsg;    /**< data reply by target */
    VOID                 *buffToFree;
} IpcContent;

/* init liteipc driver */
extern UINT32 OsLiteIpcInit(VOID);

/* reinit process liteipc memory pool, using in fork situation */
extern ProcIpcInfo *LiteIpcPoolReInit(const ProcIpcInfo *parentIpcInfo);

/* remove service handle and send death notify */
extern VOID LiteIpcRemoveServiceHandle(UINT32 taskID);

extern UINT32 LiteIpcPoolDestroy(UINT32 processID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
