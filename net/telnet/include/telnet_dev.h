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

#define TELNET                 "/dev/telnet"	//远程登录文件路径

#define FIFO_MAX               1024	//缓冲区大小
#define TELNET_IOC_MAGIC       't'
#define CFG_TELNET_SET_FD      _IO(TELNET_IOC_MAGIC, 1)
#define CFG_TELNET_EVENT_PEND  CONSOLE_CMD_RD_BLOCK_TELNET
#define BLOCK_DISABLE          0
#define BLOCK_ENABLE           1
//远程登录接发数据结构体.管理缓冲区
typedef struct {
    UINT32 rxIndex;         /* index for receiving user's commands | 接收用户命令的索引位置 */
    UINT32 rxOutIndex;      /* index for taking out commands by a shell task to run | 用于通过 shell 任务取出命令以运行的索引 */
    UINT32 fifoNum;         /* unused size of the cmdBuf | 剩余buf大小*/
    UINT32 lock;			//锁用于保证buf数据一致性
    CHAR rxBuf[FIFO_MAX];   /* the real buffer to store user's commands | 存储用户命令的真实缓冲区 */
} TELNTE_FIFO_S;
//远程登录设备结构体
typedef struct {
    INT32 clientFd;	///< 客户端文件句柄
    UINT32 id;	
    BOOL eventPend;	///< 任务是否处于挂起
    EVENT_CB_S eventTelnet;	///< 远程登录事件
    wait_queue_head_t wait;
    TELNTE_FIFO_S *cmdFifo;  /* use a FIFO to store user's commands | 使用先进先出保存用户的命令*/
} TELNET_DEV_S;

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
