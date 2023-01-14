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
#include "los_typedef.h"
#include "los_vm_map.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 什么是句柄?

从形象意义的理解,跟门的把柄一样,握住门柄就控制了整个大门.句柄是给用户程序使用的一个数字凭证,
能以小博大,通过句柄能牵动内核模块工作.
 */
#define LITEIPC_DRIVER "/dev/lite_ipc"	///< 虚拟设备,文件访问读取
#define LITEIPC_DRIVER_MODE 0644 ///< 对虚拟设备的访问权限 110100100 表示只有所属用户才有读写权限,其余都只能读
#define MAX_SERVICE_NUM LOSCFG_BASE_CORE_TSK_LIMIT ///< 最大服务数等于任务数 默认128
#define USE_TIMESTAMP 1 	///< 使用时间戳

/**
 * @enum HandleStatus 
 * @brief 句柄状态
 */
typedef enum {
    HANDLE_NOT_USED,	///< 未使用
    HANDLE_REGISTING,	///< 注册中
    HANDLE_REGISTED		///< 已注册
} HandleStatus;

/**
 * @struct HandleStatus 
 * @brief 句柄信息
 */
typedef struct {
    HandleStatus status;	///< 状态
    UINT32       taskID;	///< 任务ID,以任务标识句柄
    UINTPTR      maxMsgSize; ///< 最大消息大小
} HandleInfo;
 
/**
 * @struct IpcPool | ipc池
 * @brief  LiteIPC的核心思想就是在内核态为每个Service任务维护一个IPC消息队列，该消息队列通过LiteIPC设备文件向上层
 * 用户态程序分别提供代表收取IPC消息的读操作和代表发送IPC消息的写操作。
 */
typedef struct {//用户空间和内核空间的消息传递通过偏移量计算
    VOID   *uvaddr;	///< 用户空间地址,由kvaddr映射而来的地址,这两个地址的关系一定要搞清楚,否则无法理解IPC的核心思想
    VOID   *kvaddr;	///< 内核空间地址,IPC申请的是内核空间,但是会通过 DoIpcMmap 将这个地址映射到用户空间
    UINT32 poolSize; ///< ipc池大小
} IpcPool;

/**
 * @struct ProcIpcInfo 
 * @brief 进程IPC信息,见于进程结构体:	LosProcessCB.ipcInfo
 */
typedef struct {
    IpcPool pool;				///< ipc内存池,IPC操作所有涉及内核空间分配的内存均有此池提供
    UINT32 ipcTaskID;			///< 指定能ServiceManager的任务ID
    LOS_DL_LIST ipcUsedNodelist;///< 已使用节点链表,上面挂 IpcUsedNode 节点, 申请IpcUsedNode的内存来自内核堆空间
    UINT32 access[LOSCFG_BASE_CORE_TSK_LIMIT];	///< 允许进程通过IPC访问哪些任务
} ProcIpcInfo;
typedef struct {
    LOS_DL_LIST     msgListHead;///< 上面挂的是一个个的 ipc节点 上面挂 IpcListNode,申请IpcListNode的内存来自进程IPC内存池
    BOOL            accessMap[LOSCFG_BASE_CORE_PROCESS_LIMIT]; ///< 此处是不是应该用 LOSCFG_BASE_CORE_PROCESS_LIMIT ? @note_thinking 
    				///< 任务是否可以给其他进程发送IPC消息
} IpcTaskInfo;

typedef enum {
    OBJ_FD,		///< 文件句柄
    OBJ_PTR,	///< 指针
    OBJ_SVC		///< 服务
} ObjType;

typedef struct {
    UINT32         buffSz;  ///< 大小
    VOID           *buff;	///< 内容 内核需要将内容从用户空间拷贝到内核空间的动作 
} BuffPtr;
/// SVC(service)服务身份证 
typedef struct {
    UINT32         handle;  //service 服务ID, 范围[0,最大任务ID]
    UINT32         token;	//
    UINT32         cookie;	//由应用层带入
} SvcIdentity;
/// 对象内容体,注意是个联合体
typedef union {
    UINT32      fd; 	///< 文件描述符
    BuffPtr     ptr;	///< 缓存的开始地址,即:指针,消息从用户空间来时,要将内容拷贝到内核空间
    SvcIdentity  svc;	///< 服务,用于设置访问权限
} ObjContent;
/// 指定对象
typedef struct { // IpcMsg->data 包含三种子消息,也要将它们读到内核空间
    ObjType     type; ///< 类型
    ObjContent  content;///< 内容
} SpecialObj;

/**
 * @brief 消息的类型
 */
typedef enum {	
    MT_REQUEST,	///< 请求
    MT_REPLY,	///< 回复
    MT_FAILED_REPLY,///< 回复失败
    MT_DEATH_NOTIFY,///< 死亡通知
    MT_NUM
} MsgType;

typedef struct {
    int32_t driverVersion;
} IpcVersion;
/* lite ipc ioctl | 控制命令*/
#define IPC_IOC_MAGIC       'i'
#define IPC_SET_CMS         _IO(IPC_IOC_MAGIC, 1) ///< 设置ServiceManager的命令
#define IPC_CMS_CMD         _IOWR(IPC_IOC_MAGIC, 2, CmsCmdContent)///< 控制命令,创建/删除 服务 添加服务权限
#define IPC_SET_IPC_THREAD  _IO(IPC_IOC_MAGIC, 3)	///< 为进程设置IPC任务
#define IPC_SEND_RECV_MSG   _IOWR(IPC_IOC_MAGIC, 4, IpcContent) ///< 对IPC的读写处理
#define IPC_GET_VERSION     _IOR(IPC_IOC_MAGIC, 5, IpcVersion)

typedef enum {//CMS 命令类型
    CMS_GEN_HANDLE, ///< 创建/注册服务
    CMS_REMOVE_HANDLE, ///< 删除服务
    CMS_ADD_ACCESS ///< 为服务添加权限
} CmsCmd;

typedef struct {//命令结构体
    CmsCmd        cmd;	///< 命令 例如注册命令:给taskID注册一个serviceHandle返回用户空间
    UINT32        taskID; ///< 任务ID 
    UINT32        serviceHandle;///< 服务ID
} CmsCmdContent;

typedef enum {
    LITEIPC_FLAG_DEFAULT = 0, // send and reply | 发送并回复
    LITEIPC_FLAG_ONEWAY,      // send message only | 仅发送信息
} IpcFlag;

typedef struct {//IPC 消息结构体
    MsgType        type;       	/**< cmd type, decide the data structure below | 命令类型，决定下面的数据结构*/
    SvcIdentity    target;    	/**< serviceHandle or targetTaskId, depending on type | 因命令类型不同而异*/
    UINT32         code;      	/**< service function code | 服务功能代码*/
    UINT32         flag;		///< 标签
#if (USE_TIMESTAMP == 1)
    UINT64         timestamp;	///< 时间戳,用于验证
#endif
    UINT32         dataSz;    	/**< size of data | 消息内容大小*/
    VOID           *data;		///< 消息的内容,真正要传递的消息,这个数据内容是指spObjNum个数据的内容,定位就靠offsets
    UINT32         spObjNum;	///< 对象数量, 例如 spObjNum = 3时,offsets = [0,35,79],代表从data中读取 0 - 35给第一个对象,依次类推
    VOID           *offsets;	///< 偏移量,注意这里有多少个spObjNum就会有多少个偏移量,详见 CopyDataFromUser 来理解
    UINT32         processID; 	/**< filled by kernel, processId of sender/reciever | 由内核提供,发送/接收消息的进程ID*/
    UINT32         taskID;    	/**< filled by kernel, taskId of sender/reciever | 由内核提供,发送/接收消息的任务ID*/
#ifdef LOSCFG_SECURITY_CAPABILITY	
    UINT32         userID;		///< 用户ID
    UINT32         gid;			///< 组ID
#endif
} IpcMsg;

typedef struct {	//IPC 内容节点
    IpcMsg         msg;			///< 内容体
    LOS_DL_LIST    listNode; ///< 通过它挂到LosTaskCB.msgListHead链表上
} IpcListNode;

#define SEND (1 << 0)	///< 发送
#define RECV (1 << 1)	///< 接收
#define BUFF_FREE (1 << 2) ///< 空闲状态

/**
 * @brief IPC消息内容结构体,记录消息周期
 */
typedef struct {
    UINT32               flag;      /**< size of writeData | IPC标签 (SEND,RECV,BUFF_FREE)*/
    IpcMsg               *outMsg;   /**< data to send to target | 发给目标任务的消息内容*/
    IpcMsg               *inMsg;    /**< data reply by target | 目标任务回复的消息内容*/
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
