/*!
 * @file    shell.h
 * @brief Shell命令开发指导
 * @link
   @verbatim
   	OpenHarmony内核提供的Shell支持调试常用的基本功能，包含系统、文件、网络和动态加载相关命令。
   	同时OpenHarmony内核的Shell支持添加新的命令，可以根据需求来进行定制。

	系统相关命令：提供查询系统任务、内核信号量、系统软件定时器、CPU占用率、当前中断等相关信息的能力。

	文件相关命令：支持基本的ls、cd等功能。

	网络相关命令：支持查询接到开发板的其他设备的IP、查询本机IP、测试网络连接、设置开发板的AP和station模式等相关功能。

	在使用Shell功能的过程中，需要注意以下几点：

		Shell功能支持使用exec命令来运行可执行文件。

		Shell功能支持默认模式下英文输入。如果出现用户在UTF-8格式下输入了中文字符的情况，只能通过回退三次来删除。

		Shell功能支持shell命令、文件名及目录名的Tab键联想补全。若有多个匹配项，则根据共同字符， 打印多个匹配项。
		对于过多的匹配项（打印多于24行），将会进行打印询问（Display all num possibilities?（y/n）），
		用户可输入y选择全部打印，或输入n退出打印，选择全部打印并打印超过24行后，会进行--More--提示，
		此时按回车键继续打印，按q键退出（支持Ctrl+c退出)。

		Shell端工作目录与系统工作目录是分开的，即通过Shell端cd pwd等命令是对Shell端工作目录进行操作，
		通过chdir getcwd等命令是对系统工作目录进行操作，两个工作目录相互之间没有联系。当文件系统操作命令入参是相对路径时要格外注意。

		在使用网络Shell指令前，需要先调用tcpip_init函数完成网络初始化并完成telnet连接后才能起作用，内核默认不初始化tcpip_init。

		不建议使用Shell命令对/dev目录下的设备文件进行操作，这可能会引起不可预知的结果。
	输入Shell命令，有两种输入方式：

		1. 在串口工具中直接输入Shell命令。

		2. 在telnet工具中输入Shell命令（telnet使用方式详见telnet）。
			http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-debug-shell-net-telnet.html
   @endverbatim
 * @attention
 	Shell功能不符合POSIX标准，仅供调试使用。
 	Shell功能仅供调试使用，在Debug版本中开启（使用时通过menuconfig在配置项中开启"LOSCFG_DEBUG_VERSION"编译开关进行相关控制），
 	商用产品中禁止包含该功能。
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-22
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

#ifndef _HWLITEOS_SHELL_H
#define _HWLITEOS_SHELL_H

#include "pthread.h"
#include "limits.h"
#include "los_base.h"
#include "los_event.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_ERRNO_SHELL_NO_HOOK                  LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x00)
#define OS_ERRNO_SHELL_CMDREG_PARA_ERROR        LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x01)
#define OS_ERRNO_SHELL_CMDREG_CMD_ERROR         LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x02)
#define OS_ERRNO_SHELL_CMDREG_CMD_EXIST         LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x03)
#define OS_ERRNO_SHELL_CMDREG_MEMALLOC_ERROR    LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x04)
#define OS_ERRNO_SHELL_SHOW_HOOK_NULL           LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x05)
#define OS_ERRNO_SHELL_SHOW_HOOK_EXIST          LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x06)
#define OS_ERRNO_SHELL_SHOW_HOOK_TOO_MUCH       LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x07)
#define OS_ERRNO_SHELL_NOT_INIT                 LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x08)
#define OS_ERRNO_SHELL_CMD_HOOK_NULL            LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x09)
#define OS_ERRNO_SHELL_FIFO_ERROR               LOS_ERRNO_OS_ERROR(LOS_MOD_SHELL, 0x10)

/* Max len of show str */
#define SHOW_MAX_LEN CMD_MAX_LEN

#define XARGS 0xFFFFFFFF /* default args */

#define CMD_MAX_PARAS           32	///< 命令参数最大个数
#define CMD_KEY_LEN             16U	///< 关键字长度 例如: 'ls'的长度为2
#define CMD_MAX_LEN             (256U + CMD_KEY_LEN)
#define CMD_KEY_NUM             32	//
#define CMD_HISTORY_LEN         10	///< 历史记录数量
#define CMD_MAX_PATH            256	///< 最大路径
#define DEFAULT_SCREEN_WIDTH    80	///< 屏幕的宽
#define DEFAULT_SCREEN_HEIGHT   24	///< 屏幕的高

#define SHELL_MODE              0	///< shell模式
#define OTHER_MODE              1	///< 其他模式

#define SWITCH_QUOTES_STATUS(qu) do {   \
    if ((qu) == TRUE) {                 \
        (qu) = FALSE;                   \
    } else {                            \
        (qu) = TRUE;                    \
    }                                   \
} while (0)

#define QUOTES_STATUS_CLOSE(qu) ((qu) == FALSE)
#define QUOTES_STATUS_OPEN(qu)  ((qu) == TRUE)
/*! shell 控制块 */
typedef struct {
    UINT32   consoleID;	        ///< 控制台ID,shell必须捆绑一个控制台(串口或远程登录),以便接收输入和发送执行结果信息
    UINT32   shellTaskHandle;	///< shell服务端任务(负责解析和执行来自客户端的信息)
    UINT32   shellEntryHandle;	///< shell客户端任务(负责接受来自串口或远程登录的信息)
    VOID     *cmdKeyLink;		///< 待处理的shell命令链表
    VOID     *cmdHistoryKeyLink; ///< 已处理的命令历史记录链表,去重,10个
    VOID     *cmdMaskKeyLink;	///< 主要用于方向键上下遍历命令历史
    UINT32   shellBufOffset;	///< buf偏移量
    UINT32   shellKeyType;	    ///< 按键类型
    EVENT_CB_S shellEvent;	    ///< 事件类型触发
    pthread_mutex_t keyMutex;	///< 按键互斥量
    pthread_mutex_t historyMutex;	    ///< 命令历史记录互斥量
    CHAR     shellBuf[SHOW_MAX_LEN];	///< shell命令buf,接受键盘的输入,需要对输入字符解析.
    CHAR     shellWorkingDirectory[PATH_MAX]; ///< shell的工作目录
} ShellCB;

/*! All support cmd types | 所有支持的类型 */
typedef enum {
    CMD_TYPE_SHOW = 0,	///< 用户怎么输入就怎么显示出现,包括 \n \0 这些字符也都会存在
    CMD_TYPE_STD = 1,	///< 支持的标准命令参数输入，所有输入的字符都会通过命令解析后被传入。
    CMD_TYPE_EX = 2,	///< 不支持标准命令参数输入，会把用户填写的命令关键字屏蔽掉，例如：输入ls /ramfs，传入给注册函数的参数只有/ramfs，而ls命令关键字并不会被传入。
    CMD_TYPE_BUTT
} CmdType;
/*! 四个方向上下左右键*/
typedef enum {
    CMD_KEY_UP = 0,
    CMD_KEY_DOWN = 1,
    CMD_KEY_RIGHT = 2,
    CMD_KEY_LEFT = 4,
    CMD_KEY_BUTT
} CmdKeyDirection;

/*
 * Hook for user-defined debug function
 * Unify differnt module's func for registration
 */
typedef UINT32 (*CmdCallBackFunc)(UINT32 argc, const CHAR **argv);//命令回调函数,

/* External interface, need reserved */
typedef CmdCallBackFunc CMD_CBK_FUNC;
typedef CmdType CMD_TYPE_E;
/// 以动态方式注册命令
extern UINT32 osCmdReg(CmdType cmdType, const CHAR *cmdKey, UINT32 paraNum, CmdCallBackFunc cmdProc);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _HWLITEOS_SHELL_H */
