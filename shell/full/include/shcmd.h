/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

typedef struct {	//cmd的条目项
    CmdType cmdType;	//命令类型
    const CHAR *cmdKey;	//命令关键字，函数在Shell中访问的名称。
    UINT32 paraNum;		//调用的执行函数的入参最大个数，鸿蒙暂不支持。
    CmdCallBackFunc cmdHook;	//命令执行函数地址，即命令实际执行函数。
} CmdItem; 

typedef struct {//cmd的条目项节点,增加一幅挂钩,让item挂在一起
    LOS_DL_LIST list;
    CmdItem *cmd;
} CmdItemNode;

/* global info for shell module */	//全局shell信息模块 对应全局变量 g_cmdInfo
typedef struct {
    CmdItemNode cmdList;	//所有item将挂在上面
    UINT32 listNum;			//item数量
    UINT32 initMagicFlag;	//初始化时魔法数字
    LosMux muxLock;			//互斥锁
    CmdVerifyTransID transIdHook;// @note_why 验证转换ID的函数地址,  尚未清楚是干什么用的,看懂了的请私信我完善.
} CmdModInfo;

typedef struct {
    UINT32 count;//数量
    LOS_DL_LIST list;//链表
    CHAR cmdString[0];
} CmdKeyLink;

/**************************************************************** 
*	Shell 注册命令。用户可以选择静态注册命令方式和系统运行时动态注册命令方式，
----------------------------------------------------------------------------------------
*	第一种:静态注册命令方式一般用在系统常用命令注册，动态注册命令方式一般用在用户命令注册。
*	SHELLCMD_ENTRY(watch_shellcmd, CMD_TYPE_EX, "watch", XARGS, (CmdCallBackFunc)OsShellCmdWatch);
* 	静态注册命令方式.通过宏的方式注册
*	参数:
*	l:			静态注册全局变量名（注意：不与系统中其他symbol重名）。
*	cmdType:	命令类型
*	cmdKey:		命令关键字，函数在Shell中访问的名称。
*	paraNum:	调用的执行函数的入参最大个数，暂不支持。
*	cmdHook:	命令执行函数地址，即命令实际执行函数。
*	在build/mk/liteos_tables_ldflags.mk中添加相应选项：
*	如：上述“watch”命令注册时，需在build/mk/liteos_tables_ldflags.mk中添加“-uwatch_shellcmd”。
*	其中-u后面跟SHELLCMD_ENTRY的第一个参数。
----------------------------------------------------------------------------------------
*	第二种:.动态注册命令方式：
*	注册函数原型：
*	UINT32 osCmdReg(CmdT ype cmdType, CHAR *cmdKey, UINT32 paraNum, CmdCallBackFunc cmdProc)	
----------------------------------------------------------------------------------------
*	输入Shell命令，有两种输入方式：
*	在串口工具中直接输入Shell命令。
*	在telnet工具中输入Shell命令
****************************************************************/
#define SHELLCMD_ENTRY(l, cmdType, cmdKey, paraNum, cmdHook)    \
    CmdItem l LOS_HAL_TABLE_ENTRY(shellcmd) = {                 \
        cmdType,                                                \
        cmdKey,                                                 \
        paraNum,                                                \
        cmdHook                                                 \
    }

#define NEED_NEW_LINE(timesPrint, lineCap) ((timesPrint) % (lineCap) == 0)
#define SCREEN_IS_FULL(timesPrint, lineCap) ((timesPrint) >= ((lineCap) * DEFAULT_SCREEN_HEIGNT))

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
