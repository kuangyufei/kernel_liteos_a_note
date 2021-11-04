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

#ifndef _SHELL_H
#define _SHELL_H

#include "pthread.h"
#include "semaphore.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* Max len of show str */
#define SHOW_MAX_LEN                    CMD_MAX_LEN

#define SHELL_PROCESS_PRIORITY_INIT     15

#define PATH_MAX                        256
#define CMD_MAX_PARAS                   32
#define CMD_KEY_LEN                     16U
#define CMD_MAX_LEN                     (256U + CMD_KEY_LEN)
#define CMD_KEY_NUM                     32
#define CMD_HISTORY_LEN                 10	///< 最多保存十个命令行记录
#define CMD_MAX_PATH                    256
#define DEFAULT_SCREEN_WIDTH            80
#define DEFAULT_SCREEN_HEIGNT           24

#define SWITCH_QUOTES_STATUS(qu) do {   \
    if ((qu) == TRUE) {                 \
        (qu) = FALSE;                   \
    } else {                            \
        (qu) = TRUE;                    \
    }                                   \
} while (0)

#define QUOTES_STATUS_CLOSE(qu) ((qu) == FALSE)
#define QUOTES_STATUS_OPEN(qu)  ((qu) == TRUE)

typedef size_t bool;

typedef struct {
    unsigned int   consoleID;	///< 控制台ID
    pthread_t      shellTaskHandle;	///< shell服务端任务
    pthread_t      shellEntryHandle;///< shell客户端任务
    void     *cmdKeyLink;			///< 命令链表,所有敲过的命令链表
    void     *cmdHistoryKeyLink;	///< 命令的历史记录链表,去重,10个
    void     *cmdMaskKeyLink;		///< 主要用于方向键上下遍历历史命令
    unsigned int   shellBufOffset;	///< buf偏移量
    unsigned int   shellKeyType;	///< 按键类型
    sem_t           shellSem;		///< shell信号量
    pthread_mutex_t keyMutex;		///< 操作cmdKeyLink的互斥量
    pthread_mutex_t historyMutex;	///< 操作cmdHistoryKeyLink的互斥量
    char     shellBuf[SHOW_MAX_LEN];	///< 接受shell命令 buf大小
    char     shellWorkingDirectory[PATH_MAX];///< shell工作目录
} ShellCB;

/*! All support cmd types | 所有支持的类型*/
typedef enum {
    CMD_TYPE_SHOW = 0,	///< 用户怎么输入就怎么显示出现,包括 \n \0 这些字符也都会存在
    CMD_TYPE_STD = 1,	///< 支持的标准命令参数输入，所有输入的字符都会通过命令解析后被传入。
    CMD_TYPE_EX = 2,	///< 不支持标准命令参数输入，会把用户填写的命令关键字屏蔽掉，例如：输入ls /ramfs，传入给注册函数的参数只有/ramfs，而ls命令关键字并不会被传入。
    CMD_TYPE_BUTT
} CmdType;

typedef enum {
    CMD_KEY_UP = 0,		///< 方向上键 
    CMD_KEY_DOWN = 1,	///< 方向下键
    CMD_KEY_RIGHT = 2,	///< 方向左键
    CMD_KEY_LEFT = 4,	///< 方向右键
    CMD_KEY_BUTT
} CmdKeyDirection;

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _SHELL_H */
