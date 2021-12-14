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
#include "los_cir_buf.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_FS_VFS

/* Define two fixed console id for Console ID. | 默认两种固定的控制台id */
#define CONSOLE_SERIAL 1	///< 串行方式
#define CONSOLE_TELNET 2	///< 远程登录
//POSIX 定义了 STDIN_FILENO、STDOUT_FILENO 和 STDERR_FILENO 来代表 0、1、2 
#define LOSCFG_PLATFORM_CONSOLE
#define STDIN  0	///< 标准输入
#define STDOUT 1	///< 标准输出
#define STDERR 2	///< 错误

#define CONSOLE  "/dev/console"	
#define CONSOLE_NAMELEN 16
#define CONSOLE_RD_BLOCK               1
#define CONSOLE_RD_NONBLOCK            0
#define CONSOLE_SHELL_KEY_EVENT        0x112	///< shell 键盘事件
#define CONSOLE_SHELL_EXITED           0x400	///< shell 退出事件
#define CONSOLE_FIFO_SIZE              0x400	///< 1K
#define CONSOLE_NUM                    2

#define CONSOLE_CIRCBUF_SIZE 0x400	///< 大小 1K

/**
 * @brief 发送环形buf控制块,通过事件发送
 */
typedef struct {
    CirBuf cirBufCB;        /* Circular buffer CB | 循环缓冲控制块 */
    EVENT_CB_S sendEvent;   /* Inform telnet send task | 例如: 给SendToSer任务发送事件*/
} CirBufSendCB;

/**
 * @brief 控制台控制块(描述符)
 */
typedef struct {
    UINT32 consoleID;	///< 控制台ID    例如 : 1 | 串口 , 2 | 远程登录
    UINT32 consoleType;	///< 控制台类型
    UINT32 consoleSem;	///< 控制台信号量
    UINT32 consoleMask;	///< 控制台掩码
    struct Vnode *devVnode;	///< 索引节点
    CHAR *name;	///< 名称 例如: /dev/console1 
    INT32 fd;	///< 系统文件句柄, 由内核分配
    UINT32 refCount;	///< 引用次数,用于判断控制台是否被占用
    UINT32 shellEntryId; ///<  负责接受来自终端信息的 "ShellEntry"任务,这个值在运行过程中可能会被换掉,它始终指向当前正在运行的shell客户端
    INT32 pgrpId;	///< 进程组ID
    BOOL isNonBlock; ///< 是否无锁方式		
#ifdef LOSCFG_SHELL
    VOID *shellHandle;	///< shell句柄,本质是 shell控制块 ShellCB
#endif
    UINT32 sendTaskID;	///< 创建任务通过事件接收数据, 见于OsConsoleBufInit
    CirBufSendCB *cirBufSendCB;	///< 循环缓冲发送控制块
    UINT8 fifo[CONSOLE_FIFO_SIZE]; ///< termios 规范模式(ICANON mode )下使用 size:1K
    UINT32 fifoOut;	///< 对fifo的标记,输出位置
    UINT32 fifoIn;	///< 对fifo的标记,输入位置
    UINT32 currentLen;	///< 当前fifo位置
    struct termios consoleTermios; ///< 行规程
} CONSOLE_CB;

/**
 * @brief https://man7.org/linux/man-pages/man3/tcflow.3.html
 termios 是在POSIX规范中定义的标准接口，表示终端设备，包括虚拟终端、串口等。串口通过termios进行配置。
	struct termios
	{
		unsigned short c_iflag;  // 输入模式标志
		unsigned short c_oflag;  // 输出模式标志
		unsigned short c_cflag;  // 控制模式标志
		unsigned short c_lflag;  // 本地模式标志 例如: 设置非规范模式 tios.c_lflag = ~(ICANON | ECHO | ECHOE | ISIG);
		unsigned char c_line;	 // 线路规程
		unsigned char c_cc[NCC]; // 控制特性
		speed_t c_ispeed;		 // 输入速度
		speed_t c_ospeed;		 // 输出速度
	}
	终端有三种工作模式：
		规范模式（canonical mode）、
		非规范模式（non-canonical mode）
		原始模式（raw mode）。
	https://www.jianshu.com/p/fe5812469801
	https://blog.csdn.net/wumenglu1018/article/details/53098794
 */

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
#ifdef LOSCFG_KERNEL_SMP
extern VOID OsWaitConsoleSendTaskPend(UINT32 taskID);
extern VOID OsWakeConsoleSendTask(VOID);
#endif
extern VOID KillPgrp(UINT16 consoleId);

/* console ioctl | 控制台常见控制操作*/
#define CONSOLE_IOC_MAGIC   'c'
#define CONSOLE_CMD_RD_BLOCK_SERIAL    _IO(CONSOLE_IOC_MAGIC, 1) ///< 设置串口方式
#define CONSOLE_CMD_RD_BLOCK_TELNET    _IO(CONSOLE_IOC_MAGIC, 2) ///< 设置远程登录方式
#define CONSOLE_CONTROL_RIGHTS_CAPTURE _IO(CONSOLE_IOC_MAGIC, 3) ///< 
#define CONSOLE_CONTROL_RIGHTS_RELEASE _IO(CONSOLE_IOC_MAGIC, 4)
#define CONSOLE_CONTROL_CAPTURE_LINE   _IO(CONSOLE_IOC_MAGIC, 5)
#define CONSOLE_CONTROL_CAPTURE_CHAR   _IO(CONSOLE_IOC_MAGIC, 6)
#define CONSOLE_CONTROL_REG_USERTASK   _IO(CONSOLE_IOC_MAGIC, 7) ///< 注册用户任务 shell 客户端

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _CONSOLE_H */
