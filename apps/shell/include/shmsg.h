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

#ifndef _SHMSG_H
#define _SHMSG_H

#include "shell_list.h"
#include "shell.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @file shmsg.h
 * @brief Shell消息定义头文件，包含栈大小、命令字符串、控制命令和颜色定义
 */

/**
 * @brief Shell入口函数栈大小
 * @note 十六进制0x1000等于十进制4096字节
 */
#define SHELL_ENTRY_STACKSIZE   0x1000  // 十进制值：4096，表示入口函数栈大小为4KB

/**
 * @brief Shell任务栈大小
 * @note 十六进制0x3000等于十进制12288字节
 */
#define SHELL_TASK_STACKSIZE    0x3000  // 十进制值：12288，表示任务栈大小为12KB

/**
 * @brief 执行命令字符串
 * @note 用于在Shell中执行外部程序
 */
#define SHELL_EXEC_COMMAND          "exec"

/**
 * @brief 执行命令字符串长度
 * @note 不包含字符串结束符'\0'
 */
#define SHELL_EXEC_COMMAND_BYTES    4       // 十进制值：4，"exec"字符串长度为4字节

/**
 * @brief 带空格的执行命令字符串
 * @note 用于命令行解析时匹配带参数的执行命令
 */
#define CMD_EXEC_COMMAND            SHELL_EXEC_COMMAND" "

/**
 * @brief 带空格的执行命令字符串长度
 * @note 包含空格字符，不包含字符串结束符'\0'
 */
#define CMD_EXEC_COMMAND_BYTES      (SHELL_EXEC_COMMAND_BYTES + 1)  // 十进制值：5，"exec "字符串长度为5字节

/**
 * @brief 退出命令字符串
 * @note 用于退出当前Shell会话
 */
#define CMD_EXIT_COMMAND            "exit"

/**
 * @brief 退出命令字符串长度
 * @note 不包含字符串结束符'\0'
 */
#define CMD_EXIT_COMMAND_BYTES      4       // 十进制值：4，"exit"字符串长度为4字节

/**
 * @brief 退出码基数（十进制）
 * @note 用于解析退出命令后的状态码
 */
#define CMD_EXIT_CODE_BASE_DEC      10      // 十进制值：10，表示退出码为十进制数

/**
 * @brief 控制台IO控制命令魔数
 * @note 用于区分不同设备的IO控制命令
 */
#define CONSOLE_IOC_MAGIC   'c'             // ASCII值：99，表示控制台设备的魔数

/**
 * @brief 控制台注册用户任务控制命令
 * @note 通过ioctl调用注册用户任务到控制台
 */
#define CONSOLE_CONTROL_REG_USERTASK _IO(CONSOLE_IOC_MAGIC, 7)  // 控制台控制命令：注册用户任务

/**
 * @brief 无颜色ANSI转义序列
 * @note 用于重置终端文本颜色为默认
 */
#define COLOR_NONE     "\e[0m"            // ANSI转义序列：重置文本属性

/**
 * @brief 红色ANSI转义序列
 * @note 用于设置终端文本为红色（正常强度）
 */
#define COLOR_RED      "\e[0;31m"         // ANSI转义序列：设置文本为红色

/**
 * @brief 亮红色ANSI转义序列
 * @note 用于设置终端文本为亮红色（高强度）
 */
#define COLOR_L_RED    "\e[1;31m"         // ANSI转义序列：设置文本为亮红色

/**
 * @brief Shell提示符定义
 * @note 使用亮红色显示"OHOS #"提示符，之后重置为默认颜色
 */
#define SHELL_PROMPT   COLOR_L_RED"OHOS # "COLOR_NONE  // Shell提示符：亮红色"OHOS # "后跟默认颜色

typedef void (* OutputFunc)(const char *fmt, ...);
extern int ShellTaskInit(ShellCB *shellCB);
extern void ChildExec(const char *cmdName, char *const paramArray[], bool foreground);
extern void ShellCmdLineParse(char c, OutputFunc outputFunc, ShellCB *shellCB);
extern int ShellNotify(ShellCB *shellCB);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _SHMSG_H */
