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


#ifndef _SHCMD_H
#define _SHCMD_H

#include "string.h"
#include "stdlib.h"
#include "shell_list.h"
#include "shcmdparse.h"
#include "sherr.h"
#include "show.h"

#ifdef  __cplusplus
#if  __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */


/**
 * @brief   命令键链表节点结构
 * @details 用于存储命令历史记录或命令键相关信息的链表节点
 *          采用柔性数组设计，节省内存空间
 */
typedef struct {
    unsigned int count;       // 节点计数或引用次数
    SH_List list;             // 链表节点，用于连接多个CmdKeyLink结构
    char cmdString[0];        // 柔性数组，存储命令字符串（长度为0的数组不占内存空间）
} CmdKeyLink;

/**
 * @brief   判断是否需要换行
 * @param   timesPrint  当前已打印次数
 * @param   lineCap     每行可容纳的打印项数
 * @return  需要换行返回true，否则返回false
 */
#define NEED_NEW_LINE(timesPrint, lineCap) ((timesPrint) % (lineCap) == 0)

/**
 * @brief   判断屏幕是否已满
 * @param   timesPrint      当前已打印次数
 * @param   lineCap         每行可容纳的打印项数
 * @return  屏幕已满返回true，否则返回false
 * @note    DEFAULT_SCREEN_HEIGHT为默认屏幕高度常量
 */
#define SCREEN_IS_FULL(timesPrint, lineCap) ((timesPrint) >= ((lineCap) * DEFAULT_SCREEN_HEIGHT))


extern unsigned int OsCmdExec(CmdParsed *cmdParsed, char *cmdStr);
extern unsigned int OsCmdKeyShift(const char *cmdKey, char *cmdOut, unsigned int size);
extern int OsTabCompletion(char *cmdKey, unsigned int *len);
extern void OsShellCmdPush(const char *string, CmdKeyLink *cmdKeyLink);
extern void OsShellHistoryShow(unsigned int value, ShellCB *shellCB);
extern unsigned int OsShellKeyInit(ShellCB *shellCB);
extern void OsShellKeyDeInit(CmdKeyLink *cmdKeyLink);
extern int OsShellSetWorkingDirectory(const char *dir, size_t len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _SHCMD_H */
