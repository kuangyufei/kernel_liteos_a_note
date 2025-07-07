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

#ifndef _TELNET_DEV_H
#define _TELNET_DEV_H

#include "los_config.h"
#include "linux/wait.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_NET_TELNET
// ... existing code ...
/**
 * @brief telnet设备文件路径定义
 */
#define TELNET                 "/dev/telnet"  // telnet设备在文件系统中的路径

/**
 * @brief FIFO缓冲区大小定义
 */
#define FIFO_MAX               1024  // 命令缓冲区FIFO的最大容量（字节）

/**
 * @brief telnet IO控制命令魔数
 */
#define TELNET_IOC_MAGIC       't'  // telnet设备IO控制命令的魔数

/**
 * @brief 设置telnet客户端文件描述符的IO控制命令
 */
#define CFG_TELNET_SET_FD      _IO(TELNET_IOC_MAGIC, 1)  // 设置telnet客户端文件描述符的IOCTL命令

/**
 * @brief 设置telnet事件等待模式的IO控制命令
 */
#define CFG_TELNET_EVENT_PEND  CONSOLE_CMD_RD_BLOCK_TELNET  // 设置telnet事件阻塞/非阻塞模式的IOCTL命令

/**
 * @brief 阻塞模式禁用标志
 */
#define BLOCK_DISABLE          0  // 禁用阻塞模式

/**
 * @brief 阻塞模式启用标志
 */
#define BLOCK_ENABLE           1  // 启用阻塞模式

/**
 * @brief telnet命令FIFO结构体
 * @details 用于存储telnet客户端发送的命令，采用先进先出机制
 */
typedef struct {
    UINT32 rxIndex;         /* 接收用户命令的索引（写入指针） */
    UINT32 rxOutIndex;      /* shell任务取出命令的索引（读取指针） */
    UINT32 fifoNum;         /* cmdBuf的未使用大小（剩余空间） */
    UINT32 lock;            /* FIFO操作互斥锁 */
    CHAR rxBuf[FIFO_MAX];   /* 存储用户命令的实际缓冲区 */
} TELNTE_FIFO_S;

/**
 * @brief telnet设备结构体
 * @details 包含telnet设备的核心信息，包括客户端连接、事件、等待队列和命令缓冲区
 */
typedef struct {
    INT32 clientFd;         /* telnet客户端文件描述符 */
    UINT32 id;              /* 设备ID */
    BOOL eventPend;         /* 事件等待标志（TRUE表示阻塞等待事件） */
    EVENT_CB_S eventTelnet; /* telnet事件控制块 */
    wait_queue_head_t wait; /* 等待队列头，用于IO多路复用 */
    TELNTE_FIFO_S *cmdFifo; /* 存储用户命令的FIFO指针 */
} TELNET_DEV_S;
// ... existing code ...

extern INT32 TelnetTx(const CHAR *buf, UINT32 len);
extern INT32 TelnetDevInit(INT32 fd);
extern INT32 TelnetDevDeinit(VOID);
extern INT32 TelnetedRegister(VOID);
extern INT32 TelnetedUnregister(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
