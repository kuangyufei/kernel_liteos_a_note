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

#ifndef _HWLITEOS_SHELL_SHCMD_H
#define _HWLITEOS_SHELL_SHCMD_H

#include "string.h"
#include "stdlib.h"
#include "los_base.h"
#include "los_list.h"
#include "shcmdparse.h"
#include "show.h"

#include "los_tables.h"
#include "console.h"

#ifdef  __cplusplus
#if  __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef BOOL (*CmdVerifyTransID)(UINT32 transID);

typedef struct {//命令项
    CmdType cmdType;	///< 命令类型
					    ///< CMD_TYPE_EX：不支持标准命令参数输入，会把用户填写的命令关键字屏蔽掉，例如：输入ls /ramfs，传入给注册函数的参数只有/ramfs，而ls命令关键字并不会被传入。
					    ///< CMD_TYPE_STD：支持的标准命令参数输入，所有输入的字符都会通过命令解析后被传入。
    const CHAR *cmdKey;	///< 命令关键字，例如:ls 函数在Shell中访问的名称。
    UINT32 paraNum;		///< 调用的执行函数的入参最大个数，暂不支持。
    CmdCallBackFunc cmdHook;///<命令执行函数地址，即命令实际执行函数。
} CmdItem;
/*! 命令节点*/
typedef struct {
    LOS_DL_LIST list;	///< 双向链表
    CmdItem *cmd;	///< 命令项
} CmdItemNode;

/*! global info for shell module | shell 模块的全局信息*/
typedef struct {
    CmdItemNode cmdList;	///< 命令项节点
    UINT32 listNum; ///<节点数量
    UINT32 initMagicFlag; ///<初始魔法标签 0xABABABAB
    LosMux muxLock;	///< 操作链表互斥锁
    CmdVerifyTransID transIdHook; ///< 暂不知何意.
} CmdModInfo;
/*! 一个shell命令的结构体,命令有长有短,鸿蒙采用了可变数组的方式实现 */
typedef struct {
    UINT32 count;	    ///< 字符数量
    LOS_DL_LIST list;	///< 双向链表
    CHAR cmdString[0];	///< 字符串,可变数组的一种实现方式.
} CmdKeyLink;
/// shell 静态宏方式注册
#define SHELLCMD_ENTRY(l, cmdType, cmdKey, paraNum, cmdHook)    \
    CmdItem l LOS_HAL_TABLE_ENTRY(shellcmd) = {                 \
        cmdType,                                                \
        cmdKey,                                                 \
        paraNum,                                                \
        cmdHook                                                 \
    }
/// 是否需要新开一行
#define NEED_NEW_LINE(timesPrint, lineCap) ((timesPrint) % (lineCap) == 0)
#define SCREEN_IS_FULL(timesPrint, lineCap) ((timesPrint) >= ((lineCap) * DEFAULT_SCREEN_HEIGHT))

extern UINT32 OsCmdInit(VOID);
extern CmdModInfo *OsCmdInfoGet(VOID);
extern UINT32 OsCmdExec(CmdParsed *cmdParsed, CHAR *cmdStr);
extern UINT32 OsCmdKeyShift(const CHAR *cmdKey, CHAR *cmdOut, UINT32 size);
extern INT32 OsTabCompletion(CHAR *cmdKey, UINT32 *len);
extern VOID OsShellCmdPush(const CHAR *string, CmdKeyLink *cmdKeyLink);
extern VOID OsShellHistoryShow(UINT32 value, ShellCB *shellCB);
extern UINT32 OsShellKeyInit(ShellCB *shellCB);
extern VOID OsShellKeyDeInit(CmdKeyLink *cmdKeyLink);
extern UINT32 OsShellSysCmdRegister(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _HWLITEOS_SHELL_SHCMD_H */
