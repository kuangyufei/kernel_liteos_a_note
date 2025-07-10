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

/**
 * @file shell.h
 * @brief Shell模块核心头文件，定义Shell控制块、命令类型和相关宏
 */

/**
 * @brief 显示字符串的最大长度
 * @note 与命令最大长度保持一致
 */
#define SHOW_MAX_LEN                    CMD_MAX_LEN

/**
 * @brief Shell进程的初始优先级
 * @note 优先级数值越低，实际优先级越高
 */
#define SHELL_PROCESS_PRIORITY_INIT     15  // 十进制值：15

/**
 * @brief 文件路径的最大长度
 * @note 符合POSIX标准的路径长度限制
 */
#define PATH_MAX                        256 // 十进制值：256，表示路径最大长度为256字节

/**
 * @brief 命令的最大参数个数
 */
#define CMD_MAX_PARAS                   32  // 十进制值：32，支持最多32个命令参数

/**
 * @brief 命令键的长度
 */
#define CMD_KEY_LEN                     16U // 十进制值：16，表示命令键长度为16字节

/**
 * @brief 命令的最大长度
 * @note 包含命令键长度和实际命令内容长度
 */
#define CMD_MAX_LEN                     (256U + CMD_KEY_LEN) // 十进制值：272，256字节命令内容+16字节命令键

/**
 * @brief 命令键的最大数量
 */
#define CMD_KEY_NUM                     32  // 十进制值：32，最多支持32个命令键

/**
 * @brief 命令历史记录的最大长度
 */
#define CMD_HISTORY_LEN                 10  // 十进制值：10，最多保存10条命令历史

/**
 * @brief 命令的最大路径长度
 */
#define CMD_MAX_PATH                    256 // 十进制值：256，表示命令路径最大长度为256字节

/**
 * @brief 默认屏幕宽度
 */
#define DEFAULT_SCREEN_WIDTH            80  // 十进制值：80，默认屏幕宽度为80列

/**
 * @brief 默认屏幕高度
 */
#define DEFAULT_SCREEN_HEIGHT           24  // 十进制值：24，默认屏幕高度为24行

/**
 * @brief 切换引号状态（打开/关闭）
 * @param qu 引号状态变量指针，类型为bool
 * @note 使用do-while(0)结构确保宏可以像函数一样使用
 */
#define SWITCH_QUOTES_STATUS(qu) do {   \
    if ((qu) == TRUE) {                 \
        (qu) = FALSE;                   \
    } else {                            \
        (qu) = TRUE;                    \
    }                                   \
} while (0)

/**
 * @brief 判断引号状态是否为关闭
 * @param qu 引号状态变量
 * @return 引号关闭返回true，否则返回false
 */
#define QUOTES_STATUS_CLOSE(qu) ((qu) == FALSE)

/**
 * @brief 判断引号状态是否为打开
 * @param qu 引号状态变量
 * @return 引号打开返回true，否则返回false
 */
#define QUOTES_STATUS_OPEN(qu)  ((qu) == TRUE)

/**
 * @brief Shell控制块结构体，存储Shell运行时的关键信息
 * @note 每个Shell实例对应一个ShellCB结构体
 */
typedef struct {
    unsigned int   consoleID;           // 控制台ID，标识当前Shell所属的控制台
    pthread_t      shellTaskHandle;     // Shell任务句柄，用于线程管理
    pthread_t      shellEntryHandle;    // Shell入口任务句柄，用于主入口线程管理
    void     *cmdKeyLink;               // 命令键链表指针，存储命令键信息
    void     *cmdHistoryKeyLink;        // 命令历史键链表指针，存储历史命令键信息
    void     *cmdMaskKeyLink;           // 命令掩码键链表指针，存储命令掩码键信息
    unsigned int   shellBufOffset;      // Shell缓冲区偏移量，记录当前缓冲区位置
    unsigned int   shellKeyType;        // Shell按键类型，标识当前按键的类型
    sem_t           shellSem;           // Shell信号量，用于线程同步
    pthread_mutex_t keyMutex;           // 按键互斥锁，保护按键操作的线程安全
    pthread_mutex_t historyMutex;       // 历史记录互斥锁，保护历史记录操作的线程安全
    char     shellBuf[SHOW_MAX_LEN];    // Shell缓冲区，用于存储输入输出数据，大小为SHOW_MAX_LEN
    char     shellWorkingDirectory[PATH_MAX]; // Shell工作目录，存储当前工作路径，大小为PATH_MAX
} ShellCB;

/**
 * @brief 命令类型枚举，定义所有支持的命令类型
 */
typedef enum {
    CMD_TYPE_SHOW = 0,                  // 显示类型命令，用于输出信息
    CMD_TYPE_STD = 1,                   // 标准类型命令，常规执行命令
    CMD_TYPE_EX = 2,                    // 扩展类型命令，扩展功能命令
    CMD_TYPE_BUTT                       // 命令类型结束标记，用于遍历命令类型
} CmdType;

/**
 * @brief 命令按键方向枚举，定义方向键类型
 */
typedef enum {
    CMD_KEY_UP = 0,                     // 上方向键
    CMD_KEY_DOWN = 1,                   // 下方向键
    CMD_KEY_RIGHT = 2,                  // 右方向键
    CMD_KEY_LEFT = 4,                   // 左方向键
    CMD_KEY_BUTT                        // 按键方向结束标记，用于遍历方向键
} CmdKeyDirection;

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _SHELL_H */
