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
/*!
 * @file    console.h
 * @brief
 * @link
   @verbatim
   https://www.cnblogs.com/sparkdev/p/11460821.html
   
   TTY 是 Teletype 或 Teletypewriter 的缩写，字符设备的通称,原来是指电传打字机，
   后来这种设备逐渐键盘和显示器取代。不管是电传打字机还是键盘,显示器，
   都是作为计算机的终端设备存在的，所以 TTY 也泛指计算机的终端(terminal)设备。
   为了支持这些 TTY 设备，Linux 实现了一个叫做 TTY 的子系统。所以 TTY 既指终端，也指 Linux 的 TTY 子系统
   
   /dev/console是一个虚拟的tty，在鸿蒙它映射到真正的dev/ttyS0(UART0)上
   能直接显示系统消息的那个终端称为控制台，其他的则称为终端

   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-12-8
 */

#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "sys/ioctl.h"
#include "los_config.h"
#ifdef LOSCFG_FS_VFS
#include "termios.h"
#ifdef LOSCFG_NET_TELNET
#include "telnet_dev.h"
#endif
#include "virtual_serial.h"
#include "los_cir_buf.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_FS_VFS
/**
 * @defgroup console_define 控制台基本定义
 * @brief 控制台设备类型、标准IO和操作模式定义
 * @{ 
 */

/**
 * @brief 控制台ID定义
 * @details 定义两种固定的控制台设备ID，用于标识不同类型的控制台
 */
#define CONSOLE_SERIAL 1       /* 串行控制台ID */
#define CONSOLE_TELNET 2       /* Telnet网络控制台ID */

#define LOSCFG_PLATFORM_CONSOLE /* 平台控制台配置开关 */

/**
 * @brief 标准IO文件描述符定义
 * @details 遵循POSIX标准的文件描述符编号
 */
#define STDIN  0               /* 标准输入 */
#define STDOUT 1               /* 标准输出 */
#define STDERR 2               /* 标准错误输出 */

/**
 * @brief 控制台设备路径与属性定义
 */
#define CONSOLE  "/dev/console" /* 控制台设备文件路径 */
#define CONSOLE_NAMELEN 16     /* 控制台名称最大长度 */

/**
 * @brief 控制台读取模式定义
 */
#define CONSOLE_RD_BLOCK        1   /* 阻塞读取模式 */
#define CONSOLE_RD_NONBLOCK     0   /* 非阻塞读取模式 */

/**
 * @brief 控制台事件与缓冲区定义
 */
#define CONSOLE_SHELL_KEY_EVENT 0x112   /* Shell键盘事件标识 */
#define CONSOLE_SHELL_EXITED    0x400   /* Shell退出状态标识 */
#define CONSOLE_FIFO_SIZE       0x400   /* FIFO缓冲区大小，1024字节 */
#define CONSOLE_NUM             2       /* 支持的最大控制台数量 */
#define CONSOLE_CIRCBUF_SIZE    0x400   /* 环形缓冲区大小，1024字节 */

/** @} */ // console_define

/**
 * @defgroup console_struct 控制台数据结构
 * @brief 控制台功能实现相关的数据结构定义
 * @{ 
 */

/**
 * @brief 环形缓冲区发送控制块
 * @details 用于管理控制台输出数据的环形缓冲区和发送事件
 */
typedef struct {
    CirBuf cirBufCB;            /* 环形缓冲区控制块 */
    EVENT_CB_S sendEvent;       /* 通知telnet发送任务的事件结构 */
} CirBufSendCB;

/**
 * @brief 控制台控制块
 * @details 存储控制台设备的配置信息、状态和操作句柄
 */
typedef struct {
    UINT32 consoleID;           /* 控制台唯一标识符 */
    UINT32 consoleType;         /* 控制台类型，对应CONSOLE_SERIAL或CONSOLE_TELNET */
    UINT32 consoleSem;          /* 控制台访问信号量 */
    UINT32 consoleMask;         /* 控制台事件掩码 */
    struct Vnode *devVnode;     /* 控制台设备的VFS节点指针 */
    CHAR *name;                 /* 控制台名称 */
    INT32 fd;                   /* 控制台文件描述符 */
    UINT32 refCount;            /* 引用计数 */
    UINT32 shellEntryId;        /* Shell入口任务ID */
    INT32 pgrpId;               /* 进程组ID */
    BOOL isNonBlock;            /* 是否非阻塞模式，TRUE表示非阻塞 */
#ifdef LOSCFG_SHELL
    VOID *shellHandle;          /* Shell句柄，仅在启用Shell时有效 */
#endif
    UINT32 sendTaskID;          /* 发送任务ID */
    CirBufSendCB *cirBufSendCB; /* 环形缓冲区发送控制块指针 */
    UINT8 fifo[CONSOLE_FIFO_SIZE]; /* FIFO缓冲区 */
    UINT32 fifoOut;             /* FIFO读指针 */
    UINT32 fifoIn;              /* FIFO写指针 */
    UINT32 currentLen;          /* FIFO中当前数据长度 */
    struct termios consoleTermios; /* 终端属性结构体 */
} CONSOLE_CB;

/** @} */ // console_struct

/**
 * @defgroup console_api 控制台API函数
 * @brief 控制台初始化、配置和操作函数声明
 * @{ 
 */

/**
 * @brief 系统控制台初始化
 * @param deviceName [IN] 控制台设备名称
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 system_console_init(const CHAR *deviceName);

/**
 * @brief 系统控制台去初始化
 * @param deviceName [IN] 控制台设备名称
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 system_console_deinit(const CHAR *deviceName);

/**
 * @brief 设置串行控制台为非阻塞模式
 * @param consoleCB [IN] 控制台控制块指针
 * @return BOOL 操作结果，TRUE表示成功，FALSE表示失败
 */
extern BOOL SetSerialNonBlock(const CONSOLE_CB *consoleCB);

/**
 * @brief 设置串行控制台为阻塞模式
 * @param consoleCB [IN] 控制台控制块指针
 * @return BOOL 操作结果，TRUE表示成功，FALSE表示失败
 */
extern BOOL SetSerialBlock(const CONSOLE_CB *consoleCB);

/**
 * @brief 设置Telnet控制台为非阻塞模式
 * @param consoleCB [IN] 控制台控制块指针
 * @return BOOL 操作结果，TRUE表示成功，FALSE表示失败
 */
extern BOOL SetTelnetNonBlock(const CONSOLE_CB *consoleCB);

/**
 * @brief 设置Telnet控制台为阻塞模式
 * @param consoleCB [IN] 控制台控制块指针
 * @return BOOL 操作结果，TRUE表示成功，FALSE表示失败
 */
extern BOOL SetTelnetBlock(const CONSOLE_CB *consoleCB);

/**
 * @brief 通过控制台ID获取控制台控制块
 * @param consoleID [IN] 控制台ID
 * @return CONSOLE_CB* 成功返回控制台控制块指针，失败返回NULL
 */
extern CONSOLE_CB *OsGetConsoleByID(INT32 consoleID);

/**
 * @brief 通过任务ID获取控制台控制块
 * @param taskID [IN] 任务ID
 * @return CONSOLE_CB* 成功返回控制台控制块指针，失败返回NULL
 */
extern CONSOLE_CB *OsGetConsoleByTaskID(UINT32 taskID);

/**
 * @brief 注册任务到控制台
 * @param consoleID [IN] 控制台ID
 * @param taskID [IN] 任务ID
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 ConsoleTaskReg(INT32 consoleID, UINT32 taskID);

/**
 * @brief 更新控制台文件描述符
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 ConsoleUpdateFd(VOID);

/**
 * @brief 启用控制台
 * @return BOOL 操作结果，TRUE表示成功，FALSE表示失败
 */
extern BOOL ConsoleEnable(VOID);

/**
 * @brief 判断控制台是否为非阻塞模式
 * @param consoleCB [IN] 控制台控制块指针
 * @return BOOL 非阻塞模式返回TRUE，阻塞模式返回FALSE
 */
extern BOOL is_nonblock(const CONSOLE_CB *consoleCB);

/**
 * @brief 判断控制台是否被占用
 * @param consoleCB [IN] 控制台控制块指针
 * @return BOOL 被占用返回TRUE，未被占用返回FALSE
 */
extern BOOL IsConsoleOccupied(const CONSOLE_CB *consoleCB);

/**
 * @brief 文件操作：打开
 * @param filep [IN] 文件结构体指针
 * @param fops [IN] 文件操作函数集
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 FilepOpen(struct file *filep, const struct file_operations_vfs *fops);

/**
 * @brief 文件操作：关闭
 * @param filep [IN] 文件结构体指针
 * @param fops [IN] 文件操作函数集
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 FilepClose(struct file *filep, const struct file_operations_vfs *fops);

/**
 * @brief 文件操作：读取
 * @param filep [IN] 文件结构体指针
 * @param fops [IN] 文件操作函数集
 * @param buffer [OUT] 接收数据的缓冲区
 * @param bufLen [IN] 缓冲区长度
 * @return INT32 成功返回读取字节数，失败返回错误码
 */
extern INT32 FilepRead(struct file *filep, const struct file_operations_vfs *fops, CHAR *buffer, size_t bufLen);

/**
 * @brief 文件操作：写入
 * @param filep [IN] 文件结构体指针
 * @param fops [IN] 文件操作函数集
 * @param buffer [IN] 待写入数据的缓冲区
 * @param bufLen [IN] 写入数据长度
 * @return INT32 成功返回写入字节数，失败返回错误码
 */
extern INT32 FilepWrite(struct file *filep, const struct file_operations_vfs *fops, const CHAR *buffer, size_t bufLen);

/**
 * @brief 文件操作：轮询
 * @param filep [IN] 文件结构体指针
 * @param fops [IN] 文件操作函数集
 * @param fds [IN/OUT] 轮询文件描述符集
 * @return INT32 操作结果，成功返回满足条件的文件描述符数量
 */
extern INT32 FilepPoll(struct file *filep, const struct file_operations_vfs *fops, poll_table *fds);

/**
 * @brief 文件操作：IO控制
 * @param filep [IN] 文件结构体指针
 * @param fops [IN] 文件操作函数集
 * @param cmd [IN] IO控制命令
 * @param arg [IN/OUT] 命令参数
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 FilepIoctl(struct file *filep, const struct file_operations_vfs *fops, INT32 cmd, unsigned long arg);

/**
 * @brief 获取文件操作函数集
 * @param filep [IN] 文件结构体指针
 * @param privFilep [OUT] 私有文件结构体指针
 * @param fops [OUT] 文件操作函数集指针
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 */
extern INT32 GetFilepOps(const struct file *filep, struct file **privFilep, const struct file_operations_vfs **fops);

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief 等待控制台发送任务挂起
 * @param taskID [IN] 任务ID
 * @note 仅在SMP多核配置下有效
 */
extern VOID OsWaitConsoleSendTaskPend(UINT32 taskID);

/**
 * @brief 唤醒控制台发送任务
 * @note 仅在SMP多核配置下有效
 */
extern VOID OsWakeConsoleSendTask(VOID);
#endif

/**
 * @brief 终止进程组
 * @param consoleId [IN] 控制台ID
 */
extern VOID KillPgrp(UINT16 consoleId);

/** @} */ // console_api

/**
 * @defgroup console_ioctl 控制台IO控制命令
 * @brief 控制台设备的IO控制命令定义
 * @{ 
 */
#define CONSOLE_IOC_MAGIC   'c' /* 控制台IOCTL命令魔数 */

/**
 * @brief 设置串行控制台为阻塞读取模式
 * @details 命令码：_IO('c', 1)
 */
#define CONSOLE_CMD_RD_BLOCK_SERIAL    _IO(CONSOLE_IOC_MAGIC, 1)

/**
 * @brief 设置Telnet控制台为阻塞读取模式
 * @details 命令码：_IO('c', 2)
 */
#define CONSOLE_CMD_RD_BLOCK_TELNET    _IO(CONSOLE_IOC_MAGIC, 2)

/**
 * @brief 捕获控制台控制权
 * @details 命令码：_IO('c', 3)，获取控制台输入输出控制权
 */
#define CONSOLE_CONTROL_RIGHTS_CAPTURE _IO(CONSOLE_IOC_MAGIC, 3)

/**
 * @brief 释放控制台控制权
 * @details 命令码：_IO('c', 4)，释放控制台输入输出控制权
 */
#define CONSOLE_CONTROL_RIGHTS_RELEASE _IO(CONSOLE_IOC_MAGIC, 4)

/**
 * @brief 设置行捕获模式
 * @details 命令码：_IO('c', 5)，按行捕获控制台输入
 */
#define CONSOLE_CONTROL_CAPTURE_LINE   _IO(CONSOLE_IOC_MAGIC, 5)

/**
 * @brief 设置字符捕获模式
 * @details 命令码：_IO('c', 6)，按字符捕获控制台输入
 */
#define CONSOLE_CONTROL_CAPTURE_CHAR   _IO(CONSOLE_IOC_MAGIC, 6)

/**
 * @brief 注册用户任务到控制台
 * @details 命令码：_IO('c', 7)，将用户任务与控制台关联
 */
#define CONSOLE_CONTROL_REG_USERTASK   _IO(CONSOLE_IOC_MAGIC, 7)

/** @} */ // console_ioctl

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _CONSOLE_H */
