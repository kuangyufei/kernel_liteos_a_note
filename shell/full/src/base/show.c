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

/**
 * @brief 全局标志，指示shell命令源是否已初始化
 */
STATIC BOOL g_shellSourceFlag = FALSE;  // shell命令源初始化状态标志，FALSE表示未初始化

/**
 * @brief 初始化shell命令系统
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsShellCmdInit(VOID)
{
    UINT32 ret = OsCmdInit();  // 初始化命令解析器
    if (ret != LOS_OK) {       // 检查命令解析器初始化结果
        return ret;            // 初始化失败，返回错误码
    }

    return OsShellSysCmdRegister();  // 注册shell系统命令
}

/**
 * @brief 创建shell任务
 * @param shellCB shell控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsShellCreateTask(ShellCB *shellCB)
{
    UINT32 ret = ShellTaskInit(shellCB);  // 初始化shell任务
    if (ret != LOS_OK) {                  // 检查任务初始化结果
        return ret;                       // 初始化失败，返回错误码
    }

    return ShellEntryInit(shellCB);  // 初始化shell入口
}

/**
 * @brief 初始化shell资源
 * @param consoleId 控制台ID
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsShellSourceInit(INT32 consoleId)
{
    UINT32 ret = LOS_NOK;  // 返回值初始化
    CONSOLE_CB *consoleCB = OsGetConsoleByID(consoleId);  // 根据ID获取控制台控制块
    if ((consoleCB == NULL) || (consoleCB->shellHandle != NULL)) {  // 检查控制台有效性及shell句柄
        return LOS_NOK;  // 控制台无效或shell已初始化，返回失败
    }
    consoleCB->shellHandle = LOS_MemAlloc((VOID *)m_aucSysMem0, sizeof(ShellCB));  // 分配shell控制块内存
    if (consoleCB->shellHandle == NULL) {  // 检查内存分配结果
        return LOS_NOK;                    // 分配失败，返回错误
    }
    ShellCB *shellCB = (ShellCB *)consoleCB->shellHandle;  // 获取shell控制块指针
    if (memset_s(shellCB, sizeof(ShellCB), 0, sizeof(ShellCB)) != EOK) {  // 初始化shell控制块内存
        goto ERR_OUT1;  // 初始化失败，跳转到错误处理
    }

    shellCB->consoleID = (UINT32)consoleId;  // 设置控制台ID
    ret = (UINT32)pthread_mutex_init(&shellCB->keyMutex, NULL);  // 初始化按键互斥锁
    if (ret != LOS_OK) {  // 检查互斥锁初始化结果
        goto ERR_OUT1;    // 初始化失败，跳转到错误处理
    }
    ret = (UINT32)pthread_mutex_init(&shellCB->historyMutex, NULL);  // 初始化历史记录互斥锁
    if (ret != LOS_OK) {  // 检查互斥锁初始化结果
        goto ERR_OUT2;    // 初始化失败，跳转到错误处理
    }

    ret = OsShellKeyInit(shellCB);  // 初始化shell按键
    if (ret != LOS_OK) {            // 检查按键初始化结果
        goto ERR_OUT3;              // 初始化失败，跳转到错误处理
    }
    if (strncpy_s(shellCB->shellWorkingDirectory, PATH_MAX, "/", 2) != EOK) { /* 2:space for "/" */
        ret = LOS_NOK;              // 设置工作目录失败
        goto ERR_OUT4;              // 跳转到错误处理
    }
#if !defined(LOSCFG_PLATFORM_ROOTFS)
    /*
     * In case of ROOTFS disabled but
     * serial console enabled, it is required
     * to create Shell task in kernel for it.
     */
    if (consoleId == CONSOLE_TELNET || consoleId == CONSOLE_SERIAL) {  // 当ROOTFS禁用时，处理Telnet和串口控制台
#else
    if (consoleId == CONSOLE_TELNET) {  // 当ROOTFS启用时，仅处理Telnet控制台
#endif
        ret = OsShellCreateTask(shellCB);  // 创建shell任务
        if (ret != LOS_OK) {               // 检查任务创建结果
            goto ERR_OUT4;                 // 创建失败，跳转到错误处理
        }
    }

    return LOS_OK;  // 初始化成功
ERR_OUT4:  // 按键初始化失败的错误处理
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB->cmdKeyLink);       // 释放命令按键链表内存
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB->cmdHistoryKeyLink);  // 释放历史按键链表内存
ERR_OUT3:  // 历史互斥锁初始化失败的错误处理
    (VOID)pthread_mutex_destroy(&shellCB->historyMutex);  // 销毁历史记录互斥锁
ERR_OUT2:  // 按键互斥锁初始化失败的错误处理
    (VOID)pthread_mutex_destroy(&shellCB->keyMutex);      // 销毁按键互斥锁
ERR_OUT1:  // 内存分配或初始化失败的错误处理
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB);     // 释放shell控制块内存
    consoleCB->shellHandle = NULL;                        // 重置shell句柄
    return ret;                                           // 返回错误码
}

/**
 * @brief 初始化shell
 * @param consoleId 控制台ID
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsShellInit(INT32 consoleId)
{
    if (g_shellSourceFlag == FALSE) {  // 检查shell命令源是否已初始化
        UINT32 ret = OsShellCmdInit();  // 初始化shell命令系统
        if (ret == LOS_OK) {            // 检查命令系统初始化结果
            g_shellSourceFlag = TRUE;   // 更新初始化状态标志
        } else {
            return ret;                 // 初始化失败，返回错误码
        }
    }
    return OsShellSourceInit(consoleId);  // 初始化shell资源
}

/**
 * @brief 反初始化shell
 * @param consoleId 控制台ID
 * @return 成功返回0，失败返回-1
 */
INT32 OsShellDeinit(INT32 consoleId)
{
    CONSOLE_CB *consoleCB = NULL;  // 控制台控制块指针
    ShellCB *shellCB = NULL;       // shell控制块指针

    consoleCB = OsGetConsoleByID(consoleId);  // 根据ID获取控制台控制块
    if (consoleCB == NULL) {                  // 检查控制台控制块有效性
        PRINT_ERR("shell deinit error.\n");  // 打印错误信息
        return -1;                            // 返回错误
    }

    shellCB = (ShellCB *)consoleCB->shellHandle;  // 获取shell控制块
    consoleCB->shellHandle = NULL;                // 重置shell句柄
    if (shellCB == NULL) {                        // 检查shell控制块有效性
        PRINT_ERR("shell deinit error.\n");      // 打印错误信息
        return -1;                                // 返回错误
    }

    (VOID)LOS_TaskDelete(shellCB->shellEntryHandle);  // 删除shell入口任务
    (VOID)LOS_EventWrite(&shellCB->shellEvent, CONSOLE_SHELL_KEY_EVENT);  // 写入shell事件

    return 0;  // 反初始化成功
}

/**
 * @brief 获取shell当前工作目录
 * @return 成功返回工作目录字符串，失败返回NULL
 */
CHAR *OsShellGetWorkingDirectory(VOID)
{
    CONSOLE_CB *consoleCB = OsGetConsoleByTaskID(OsCurrTaskGet()->taskID);  // 获取当前任务的控制台
    ShellCB *shellCB = NULL;                                               // shell控制块指针

    if (consoleCB == NULL) {  // 检查控制台控制块有效性
        return NULL;          // 无效，返回NULL
    }
    shellCB = (ShellCB *)consoleCB->shellHandle;  // 获取shell控制块
    if (shellCB == NULL) {                        // 检查shell控制块有效性
        return NULL;                              // 无效，返回NULL
    }
    return shellCB->shellWorkingDirectory;  // 返回工作目录
}