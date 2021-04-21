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
#include "los_cir_buf_pri.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_FS_VFS

/* Define two fixed console id for Console ID. */ //两种固定的控制台id
#define CONSOLE_SERIAL 1	//串行方式
#define CONSOLE_TELNET 2	//远程登录
//POSIX 定义了 STDIN_FILENO、STDOUT_FILENO 和 STDERR_FILENO 来代替 0、1、2 
#define LOSCFG_PLATFORM_CONSOLE
#define STDIN  0	//标准输入
#define STDOUT 1	//标准输出
#define STDERR 2	//错误
/**********************************************************
https://www.cnblogs.com/sparkdev/p/11460821.html

TTY 是 Teletype 或 Teletypewriter 的缩写，字符设备的通称,原来是指电传打字机，
后来这种设备逐渐键盘和显示器取代。不管是电传打字机还是键盘,显示器，
都是作为计算机的终端设备存在的，所以 TTY 也泛指计算机的终端(terminal)设备。
为了支持这些 TTY 设备，Linux 实现了一个叫做 TTY 的子系统。所以 TTY 既指终端，也指 Linux 的 TTY 子系统

/dev/console是一个虚拟的tty，在鸿蒙它映射到真正的dev/ttyS0(UART0)上
能直接显示系统消息的那个终端称为控制台，其他的则称为终端
**********************************************************/
#define CONSOLE  "/dev/console"
#define CONSOLE_NAMELEN 16
#define CONSOLE_RD_BLOCK               1
#define CONSOLE_RD_NONBLOCK            0
#define CONSOLE_SHELL_KEY_EVENT        0x112
#define CONSOLE_SHELL_EXITED           0x400
#define CONSOLE_FIFO_SIZE              0x400
#define CONSOLE_NUM                    2

#define CONSOLE_CIRCBUF_SIZE 0x400

typedef struct {
    CirBuf cirBufCB;        /* Circular buffer CB */ //循环缓冲控制块
    EVENT_CB_S sendEvent;   /* Inform telnet send task */ //通知telnet发送任务
} CirBufSendCB;
//控制台控制块(描述符)
typedef struct {
    UINT32 consoleID;	//控制台ID
    UINT32 consoleType;	//控制台类型
    UINT32 consoleSem;	//控制台信号量
    UINT32 shellEntryId;//shell的入口ID
    UINT32 consoleMask;	//控制台掩码
    struct Vnode *devVnode;
    CHAR *name;	//名称
    INT32 fd;	//文件描述符
    UINT32 refCount;	//引用次数
    BOOL isNonBlock;	
#ifdef LOSCFG_SHELL
    VOID *shellHandle;	//shell句柄,本质是 shell控制块 ShellCB
#endif
    UINT32 sendTaskID;	//发送任务ID
    CirBufSendCB *cirBufSendCB;	//循环缓冲描述符
    UINT8 fifo[CONSOLE_FIFO_SIZE];
    UINT32 fifoOut;
    UINT32 fifoIn;
    UINT32 currentLen;
    struct termios consoleTermios; //控制台条款
} CONSOLE_CB;

extern INT32 system_console_init(const CHAR *deviceName);
extern INT32 system_console_deinit(const CHAR *deviceName);
extern BOOL SetSerialNonBlock(const CONSOLE_CB *consoleCB);
extern BOOL SetSerialBlock(const CONSOLE_CB *consoleCB);
extern BOOL SetTelnetNonBlock(const CONSOLE_CB *consoleCB);
extern BOOL SetTelnetBlock(const CONSOLE_CB *consoleCB);
extern CONSOLE_CB *OsGetConsoleByID(INT32 consoleID);
extern CONSOLE_CB *OsGetConsoleByTaskID(UINT32 taskID);
extern INT32 ConsoleTaskReg(INT32 consoleID, UINT32 taskID);
extern INT32 ConsoleUpdateFd(VOID);
extern BOOL ConsoleEnable(VOID);
extern BOOL is_nonblock(const CONSOLE_CB *consoleCB);
extern BOOL IsConsoleOccupied(const CONSOLE_CB *consoleCB);
extern INT32 FilepOpen(struct file *filep, const struct file_operations_vfs *fops);
extern INT32 FilepClose(struct file *filep, const struct file_operations_vfs *fops);
extern INT32 FilepRead(struct file *filep, const struct file_operations_vfs *fops, CHAR *buffer, size_t bufLen);
extern INT32 FilepWrite(struct file *filep, const struct file_operations_vfs *fops, const CHAR *buffer, size_t bufLen);
extern INT32 FilepPoll(struct file *filep, const struct file_operations_vfs *fops, poll_table *fds);
extern INT32 FilepIoctl(struct file *filep, const struct file_operations_vfs *fops, INT32 cmd, unsigned long arg);
extern INT32 GetFilepOps(const struct file *filep, struct file **privFilep, const struct file_operations_vfs **fops);
#if (LOSCFG_KERNEL_SMP == YES)
extern VOID OsWaitConsoleSendTaskPend(UINT32 taskID);
extern VOID OsWakeConsoleSendTask(VOID);
#endif

/* console ioctl */
#define CONSOLE_IOC_MAGIC   'c'
#define CONSOLE_CMD_RD_BLOCK_SERIAL    _IO(CONSOLE_IOC_MAGIC, 1)
#define CONSOLE_CMD_RD_BLOCK_TELNET    _IO(CONSOLE_IOC_MAGIC, 2)
#define CONSOLE_CONTROL_RIGHTS_CAPTURE _IO(CONSOLE_IOC_MAGIC, 3)
#define CONSOLE_CONTROL_RIGHTS_RELEASE _IO(CONSOLE_IOC_MAGIC, 4)
#define CONSOLE_CONTROL_CAPTURE_LINE   _IO(CONSOLE_IOC_MAGIC, 5)
#define CONSOLE_CONTROL_CAPTURE_CHAR   _IO(CONSOLE_IOC_MAGIC, 6)
#define CONSOLE_CONTROL_REG_USERTASK   _IO(CONSOLE_IOC_MAGIC, 7)

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _CONSOLE_H */
