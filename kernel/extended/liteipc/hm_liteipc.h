/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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
#include "los_task_pri.h"
#include "los_typedef.h"
#include "los_vm_map.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/****************************************************
什么是句柄?
从形象意义的理解,跟门的把柄一样,握住门柄就控制了整个大门.
句柄是给用户程序使用的一个数字凭证,能以小博大,通过柄
能牵动内核模块工作.
****************************************************/
#define LITEIPC_DRIVER "/dev/lite_ipc"	//设备位置
#define LITEIPC_DRIVER_MODE 0644
#define MAX_SERVICE_NUM LOSCFG_BASE_CORE_TSK_LIMIT //最大服务数等于任务数 默认128
#define USE_TIMESTAMP YES

typedef enum { //句柄状态
    HANDLE_NOT_USED,	//未使用
    HANDLE_REGISTING,	//注册中
    HANDLE_REGISTED		//已注册
} HandleStatus;

typedef struct {//句柄信息
    HandleStatus status;	//状态
    UINT32       taskID;	//任务ID,以任务标识句柄
    UINTPTR      maxMsgSize;//最大消息大小
} HandleInfo;

typedef struct {// ipc池
    VOID   *uvaddr;	//虚拟地址,指向进程的LiteIPC线性区基地址
    VOID   *kvaddr;	//注意这里指的是物理地址
    UINT32 poolSize;//ipc池大小
} IpcPool;
//见于进程结构体:	LosProcessCB.ipcInfo
typedef struct {//进程IPC信息
    IpcPool pool;	//ipc池
    UINT32 ipcTaskID;	//
    UINT32 access[LOSCFG_BASE_CORE_TSK_LIMIT];	//访问的任务数组
} ProcIpcInfo;

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

typedef enum {	//消息的类型
    MT_REQUEST,	//请求
    MT_REPLY,	//回复
    MT_FAILED_REPLY,//回复失败
    MT_DEATH_NOTIFY,//通知死亡
    MT_NUM
} MsgType;

/* lite ipc ioctl */// 控制命令
#define IPC_IOC_MAGIC       'i'
#define IPC_SET_CMS         _IO(IPC_IOC_MAGIC, 1)
#define IPC_CMS_CMD         _IOWR(IPC_IOC_MAGIC, 2, CmsCmdContent)
#define IPC_SET_IPC_THREAD  _IO(IPC_IOC_MAGIC, 3)
#define IPC_SEND_RECV_MSG   _IOWR(IPC_IOC_MAGIC, 4, IpcContent)

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
    MsgType        type;       /**< cmd type, decide the data structure below*/	//命令类型，决定下面的数据结构
    SvcIdentity    target;    /**< serviceHandle or targetTaskId, depending on type */	//因命令类型不同而异
    UINT32         code;      /**< service function code */	//服务功能代码
    UINT32         flag;	//标签
#if (USE_TIMESTAMP == YES)
    UINT64         timestamp;	//时间戳
#endif
    UINT32         dataSz;    /**< size of data */ //消息内容大小
    VOID           *data;	//消息的内容,真正要传递的消息
    UINT32         spObjNum;	// ..
    VOID           *offsets;	// ..
    UINT32         processID; /**< filled by kernel, processId of sender/reciever */ //由内核填充,发送/接收消息的进程ID
    UINT32         taskID;    /**< filled by kernel, taskId of sender/reciever */	//由内核填充,发送/接收消息的任务ID
#ifdef LOSCFG_SECURITY_CAPABILITY	
    UINT32         userID;	//用户ID
    UINT32         gid;		//组ID
#endif
} IpcMsg;

typedef struct {	//IPC 内容节点
    IpcMsg         msg;	//内容体
    LOS_DL_LIST    listNode;//通过它挂到LosTaskCB.msgListHead链表上
} IpcListNode;

#define SEND (1 << 0)	//发送
#define RECV (1 << 1)	//接收
#define BUFF_FREE (1 << 2) //空闲状态

typedef struct {	//IPC消息内容回路,记录消息周期
    UINT32               flag;      /**< size of writeData */ //IPC标签 (SEND,RECV,BUFF_FREE)
    IpcMsg               *outMsg;   /**< data to send to target */ 	//发给给目标任务的消息内容
    IpcMsg               *inMsg;    /**< data reply by target */	//目标任务回复的消息内容
    VOID                 *buffToFree;
} IpcContent;

/* init liteipc driver */
extern UINT32 OsLiteIpcInit(VOID);

/* init process liteipc memory pool */
extern UINT32 LiteIpcPoolInit(ProcIpcInfo *ipcInfo);

/* reinit process liteipc memory pool, using in fork situation */
extern UINT32 LiteIpcPoolReInit(ProcIpcInfo *childIpcInfo, const ProcIpcInfo *parentIpcInfo);

/* delete process liteipc memory pool */
extern VOID LiteIpcPoolDelete(ProcIpcInfo *ipcInfo, UINT32 processID);

/* remove service handle and send death notify */
extern VOID LiteIpcRemoveServiceHandle(LosTaskCB *taskCB);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
