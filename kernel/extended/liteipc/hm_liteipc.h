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

#define LITEIPC_DRIVER "/dev/lite_ipc"  // LiteIPC驱动设备路径
#define LITEIPC_DRIVER_MODE 0644        // LiteIPC驱动文件权限
#define MAX_SERVICE_NUM LOSCFG_BASE_CORE_TSK_LIMIT  // 最大服务数量，与系统任务限制相同
#define USE_TIMESTAMP 1                  // 是否启用时间戳功能：1-启用，0-禁用

/**
 * @brief 句柄状态枚举
 * @details 表示LiteIPC服务句柄的注册状态
 */
typedef enum {
    HANDLE_NOT_USED,                     // 句柄未使用
    HANDLE_REGISTING,                    // 句柄正在注册中
    HANDLE_REGISTED                      // 句柄已注册完成
} HandleStatus;

/**
 * @brief 句柄信息结构体
 * @details 存储LiteIPC服务句柄的详细信息
 */
typedef struct {
    HandleStatus status;                 // 句柄状态，取值为HandleStatus枚举
    UINT32       taskID;                 // 关联的任务ID
    UINTPTR      maxMsgSize;             // 最大消息大小（字节）
} HandleInfo;

/**
 * @brief IPC内存池结构体
 * @details 描述用户空间与内核空间共享的内存池信息
 */
typedef struct {
    VOID   *uvaddr;                      // 用户空间虚拟地址
    VOID   *kvaddr;                      // 内核空间虚拟地址
    UINT32 poolSize;                     // 内存池大小（字节）
} IpcPool;

/**
 * @brief 进程IPC信息结构体
 * @details 存储特定进程的IPC相关信息
 */
typedef struct {
    IpcPool pool;                        // IPC内存池
    UINT32 ipcTaskID;                    // IPC处理任务ID
    LOS_DL_LIST ipcUsedNodelist;         // IPC已使用节点链表
    UINT32 access[LOSCFG_BASE_CORE_TSK_LIMIT];  // 访问权限数组，索引为任务ID
} ProcIpcInfo;

/**
 * @brief 任务IPC信息结构体
 * @details 存储特定任务的IPC消息队列信息
 */
typedef struct {
    LOS_DL_LIST     msgListHead;         // 消息链表头
    BOOL            accessMap[LOSCFG_BASE_CORE_PROCESS_LIMIT];  // 进程访问映射表
} IpcTaskInfo;

/**
 * @brief 对象类型枚举
 * @details 表示IPC消息中特殊对象的类型
 */
typedef enum {
    OBJ_FD,                              // 文件描述符对象
    OBJ_PTR,                             // 指针缓冲区对象
    OBJ_SVC                              // 服务身份对象
} ObjType;

/**
 * @brief 缓冲区指针结构体
 * @details 描述一个带大小的缓冲区
 */
typedef struct {
    UINT32         buffSz;               // 缓冲区大小（字节）
    VOID           *buff;                // 缓冲区指针
} BuffPtr;

/**
 * @brief 服务身份结构体
 * @details 唯一标识一个服务的身份信息
 */
typedef struct {
    UINT32         handle;               // 服务句柄
    UINT32         token;                // 服务令牌
    UINT32         cookie;               // 服务Cookie值
} SvcIdentity;

/**
 * @brief 对象内容联合体
 * @details 根据ObjType类型存储不同类型对象的内容
 */
typedef union {
    UINT32      fd;                      // 文件描述符（OBJ_FD类型）
    BuffPtr     ptr;                     // 缓冲区指针（OBJ_PTR类型）
    SvcIdentity  svc;                    // 服务身份（OBJ_SVC类型）
} ObjContent;

/**
 * @brief 特殊对象结构体
 * @details 包含对象类型和对应内容的联合体
 */
typedef struct {
    ObjType     type;                    // 对象类型，取值为ObjType枚举
    ObjContent  content;                 // 对象内容，根据type选择对应联合体成员
} SpecialObj;

/**
 * @brief 消息类型枚举
 * @details 表示IPC消息的类型
 */
typedef enum {
    MT_REQUEST,                          // 请求消息
    MT_REPLY,                            // 回复消息
    MT_FAILED_REPLY,                     // 失败回复消息
    MT_DEATH_NOTIFY,                     // 死亡通知消息
    MT_NUM                               // 消息类型总数
} MsgType;

/**
 * @brief IPC版本结构体
 * @details 存储LiteIPC驱动的版本信息
 */
typedef struct {
    int32_t driverVersion;               // 驱动版本号
} IpcVersion;

/* lite ipc ioctl 命令定义 */
#define IPC_IOC_MAGIC       'i'           // IPC ioctl幻数
#define IPC_SET_CMS         _IO(IPC_IOC_MAGIC, 1)  // 设置CMS（通信管理服务）
#define IPC_CMS_CMD         _IOWR(IPC_IOC_MAGIC, 2, CmsCmdContent)  // CMS命令操作
#define IPC_SET_IPC_THREAD  _IO(IPC_IOC_MAGIC, 3)  // 设置IPC线程
#define IPC_SEND_RECV_MSG   _IOWR(IPC_IOC_MAGIC, 4, IpcContent)  // 发送接收消息
#define IPC_GET_VERSION     _IOR(IPC_IOC_MAGIC, 5, IpcVersion)  // 获取版本信息

/**
 * @brief CMS命令枚举
 * @details 表示通信管理服务（CMS）支持的命令类型
 */
typedef enum {
    CMS_GEN_HANDLE,                      // 生成服务句柄
    CMS_REMOVE_HANDLE,                   // 移除服务句柄
    CMS_ADD_ACCESS                       // 添加访问权限
} CmsCmd;

/**
 * @brief CMS命令内容结构体
 * @details 存储CMS命令的具体参数
 */
typedef struct {
    CmsCmd        cmd;                   // 命令类型，取值为CmsCmd枚举
    UINT32        taskID;                // 目标任务ID
    UINT32        serviceHandle;         // 服务句柄
} CmsCmdContent;

/**
 * @brief IPC标志枚举
 * @details 表示IPC消息的发送标志
 */
typedef enum {
    LITEIPC_FLAG_DEFAULT = 0,            // 默认模式：发送并等待回复
    LITEIPC_FLAG_ONEWAY,                 // 单向模式：仅发送消息不等待回复
} IpcFlag;

/**
 * @brief IPC消息结构体
 * @details 表示一个完整的IPC消息，包含消息头和数据
 */
typedef struct {
    MsgType        type;                 // 消息类型，决定后续数据结构
    SvcIdentity    target;               // 目标服务身份标识
    UINT32         code;                 // 服务功能代码
    UINT32         flag;                 // 消息标志，取值为IpcFlag枚举
#if (USE_TIMESTAMP == 1)                  // 如果启用时间戳功能
    UINT64         timestamp;            // 消息时间戳
#endif
    UINT32         dataSz;               // 数据大小（字节）
    VOID           *data;                // 数据指针
    UINT32         spObjNum;             // 特殊对象数量
    VOID           *offsets;             // 偏移量数组指针
    UINT32         processID;            // 发送/接收进程ID（由内核填充）
    UINT32         taskID;               // 发送/接收任务ID（由内核填充）
#ifdef LOSCFG_SECURITY_CAPABILITY        // 如果启用安全能力配置
    UINT32         userID;               // 用户ID
    UINT32         gid;                  // 用户组ID
#endif
} IpcMsg;

/**
 * @brief IPC链表节点结构体
 * @details 用于将IPC消息组织成链表
 */
typedef struct {
    IpcMsg         msg;                  // IPC消息
    LOS_DL_LIST    listNode;             // 链表节点
} IpcListNode;

// IPC操作标志定义
#define SEND (1 << 0)                     // 发送操作标志
#define RECV (1 << 1)                     // 接收操作标志
#define BUFF_FREE (1 << 2)                // 缓冲区释放标志

/**
 * @brief IPC内容结构体
 * @details 包含发送和接收消息的完整信息
 */
typedef struct {
    UINT32               flag;           // 操作标志：SEND/RECV/BUFF_FREE的组合
    IpcMsg               *outMsg;        // 发送给目标的消息指针
    IpcMsg               *inMsg;         // 目标回复的消息指针
    VOID                 *buffToFree;    // 需要释放的缓冲区指针
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
