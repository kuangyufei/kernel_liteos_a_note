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

#include "show.h"
#include "shmsg.h"
#include "shcmd.h"
#include "console.h"


STATIC BOOL g_shellSourceFlag = FALSE;//
//shell支持的命令初始化
STATIC UINT32 OsShellCmdInit(VOID)
{
    UINT32 ret = OsCmdInit();//命令初始化
    if (ret != LOS_OK) {
        return ret;
    }

    return OsShellSysCmdRegister();//系统自带的shell命令初始化
}
///创建shell服务端任务
STATIC UINT32 OsShellCreateTask(ShellCB *shellCB)
{
    UINT32 ret = ShellTaskInit(shellCB);//执行shell命令的任务初始化
    if (ret != LOS_OK) {
        return ret;
    }

    return ShellEntryInit(shellCB);//通过控制台接受shell命令的任务初始化
}
///shell资源初始化
STATIC UINT32 OsShellSourceInit(INT32 consoleId)
{
    UINT32 ret = LOS_NOK;
    CONSOLE_CB *consoleCB = OsGetConsoleByID(consoleId);//获取控制台信息
    if ((consoleCB == NULL) || (consoleCB->shellHandle != NULL)) {
        return LOS_NOK;
    }
    consoleCB->shellHandle = LOS_MemAlloc((VOID *)m_aucSysMem0, sizeof(ShellCB));
    if (consoleCB->shellHandle == NULL) {
        return LOS_NOK;
    }
    ShellCB *shellCB = (ShellCB *)consoleCB->shellHandle;
    if (memset_s(shellCB, sizeof(ShellCB), 0, sizeof(ShellCB)) != EOK) {
        goto ERR_OUT1;
    }

    shellCB->consoleID = (UINT32)consoleId;
    ret = (UINT32)pthread_mutex_init(&shellCB->keyMutex, NULL);
    if (ret != LOS_OK) {
        goto ERR_OUT1;
    }
    ret = (UINT32)pthread_mutex_init(&shellCB->historyMutex, NULL);
    if (ret != LOS_OK) {
        goto ERR_OUT2;
    }

    ret = OsShellKeyInit(shellCB);
    if (ret != LOS_OK) {
        goto ERR_OUT3;
    }
    if (strncpy_s(shellCB->shellWorkingDirectory, PATH_MAX, "/", 2) != EOK) { /* 2:space for "/" */
        ret = LOS_NOK;
        goto ERR_OUT4;
    }
#if !defined(LOSCFG_PLATFORM_ROOTFS)
    /*
     * In case of ROOTFS disabled but
     * serial console enabled, it is required
     * to create Shell task in kernel for it.
     */
    if (consoleId == CONSOLE_TELNET || consoleId == CONSOLE_SERIAL) {
#else
    if (consoleId == CONSOLE_TELNET) {
#endif
        ret = OsShellCreateTask(shellCB);
        if (ret != LOS_OK) {
            goto ERR_OUT4;
        }
    }

    return LOS_OK;
ERR_OUT4:
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB->cmdKeyLink);
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB->cmdHistoryKeyLink);
ERR_OUT3:
    (VOID)pthread_mutex_destroy(&shellCB->historyMutex);
ERR_OUT2:
    (VOID)pthread_mutex_destroy(&shellCB->keyMutex);
ERR_OUT1:
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB);
    consoleCB->shellHandle = NULL;
    return ret;
}
///shell初始化
UINT32 OsShellInit(INT32 consoleId)
{
    if (g_shellSourceFlag == FALSE) {
        UINT32 ret = OsShellCmdInit();
        if (ret == LOS_OK) {
            g_shellSourceFlag = TRUE;
        } else {
            return ret;
        }
    }
    return OsShellSourceInit(consoleId);
}
///shell结束
INT32 OsShellDeinit(INT32 consoleId)
{
    CONSOLE_CB *consoleCB = NULL;
    ShellCB *shellCB = NULL;

    consoleCB = OsGetConsoleByID(consoleId);//通过ID获取当前控制台
    if (consoleCB == NULL) {
        PRINT_ERR("shell deinit error.\n");
        return -1;
    }

    shellCB = (ShellCB *)consoleCB->shellHandle;//从当前控制台获取shell控制块
    consoleCB->shellHandle = NULL;//重置控制台的shell
    if (shellCB == NULL) {
        PRINT_ERR("shell deinit error.\n");
        return -1;
    }

    (VOID)LOS_TaskDelete(shellCB->shellEntryHandle);//删除接收控制台指令的shell任务
    (VOID)LOS_EventWrite(&shellCB->shellEvent, CONSOLE_SHELL_KEY_EVENT);//发送结束事件

    return 0;
}
///获取shell的工作目录
CHAR *OsShellGetWorkingDirectory(VOID)
{
    CONSOLE_CB *consoleCB = OsGetConsoleByTaskID(OsCurrTaskGet()->taskID);//获取当前任务控制台
    ShellCB *shellCB = NULL;

    if (consoleCB == NULL) {
        return NULL;
    }
    shellCB = (ShellCB *)consoleCB->shellHandle;//获取控制台的shell实体
    if (shellCB == NULL) {
        return NULL;
    }
    return shellCB->shellWorkingDirectory;//返回shell工作目录
}

