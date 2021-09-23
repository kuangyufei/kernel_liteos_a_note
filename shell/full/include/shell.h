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

#define CMD_MAX_PARAS           32	//命令参数最大个数
#define CMD_KEY_LEN             16U	//关键字长度 例如: 'ls'的长度为2
#define CMD_MAX_LEN             (256U + CMD_KEY_LEN)
#define CMD_KEY_NUM             32	//
#define CMD_HISTORY_LEN         10	//历史记录数量
#define CMD_MAX_PATH            256	//最大路径
#define DEFAULT_SCREEN_WIDTH    80	//屏幕的宽
#define DEFAULT_SCREEN_HEIGNT   24	//屏幕的高

#define SHELL_MODE              0	//shell模式
#define OTHER_MODE              1	//其他模式

#define SWITCH_QUOTES_STATUS(qu) do {   \
    if ((qu) == TRUE) {                 \
        (qu) = FALSE;                   \
    } else {                            \
        (qu) = TRUE;                    \
    }                                   \
} while (0)

#define QUOTES_STATUS_CLOSE(qu) ((qu) == FALSE)
#define QUOTES_STATUS_OPEN(qu)  ((qu) == TRUE)
//shell 控制块
typedef struct {
    UINT32   consoleID;	//控制台ID
    UINT32   shellTaskHandle;	//shell服务端任务
    UINT32   shellEntryHandle;	//shell客户端任务
    VOID     *cmdKeyLink;		//待处理的shell命令链表
    VOID     *cmdHistoryKeyLink;//已处理的历史记录链表,去重,10个
    VOID     *cmdMaskKeyLink;	//主要用于方向键上下遍历历史命令
    UINT32   shellBufOffset;	//buf偏移量
    UINT32   shellKeyType;	//按键类型
    EVENT_CB_S shellEvent;	//事件类型触发
    pthread_mutex_t keyMutex;	//按键互斥量
    pthread_mutex_t historyMutex;	//历史记录互斥量
    CHAR     shellBuf[SHOW_MAX_LEN];	//shell命令buf,接受键盘的输入,需要对输入字符解析.
    CHAR     shellWorkingDirectory[PATH_MAX];//shell的工作目录
} ShellCB;

/* All support cmd types */
typedef enum {//所有支持的类型
    CMD_TYPE_SHOW = 0,	//用户怎么输入就怎么显示出现,包括 \n \0 这些字符也都会存在
    CMD_TYPE_STD = 1,	//支持的标准命令参数输入，所有输入的字符都会通过命令解析后被传入。
    CMD_TYPE_EX = 2,	//不支持标准命令参数输入，会把用户填写的命令关键字屏蔽掉，例如：输入ls /ramfs，传入给注册函数的参数只有/ramfs，而ls命令关键字并不会被传入。
    CMD_TYPE_BUTT
} CmdType;

typedef enum {//四个方向上下左右键
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
//以动态方式注册命令
extern UINT32 osCmdReg(CmdType cmdType, const CHAR *cmdKey, UINT32 paraNum, CmdCallBackFunc cmdProc);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _HWLITEOS_SHELL_H */
