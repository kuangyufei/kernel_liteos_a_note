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

#include "shmsg.h"
#include "shell_pri.h"
#include "shcmd.h"
#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "securec.h"
#include "los_base.h"
#include "los_task.h"
#include "los_event.h"
#include "los_list.h"
#include "los_printf.h"

#ifdef LOSCFG_FS_VFS
#include "console.h"
#endif
/**
 * @brief 获取Shell输入缓冲区内容
 * @param shellCB Shell控制块指针
 * @return 成功返回命令字符串指针，失败返回NULL
 */
CHAR *ShellGetInputBuf(ShellCB *shellCB)
{
    CmdKeyLink *cmdkey = shellCB->cmdKeyLink;  // 命令链表指针
    CmdKeyLink *cmdNode = NULL;                // 命令节点指针

    (VOID)pthread_mutex_lock(&shellCB->keyMutex);  // 加锁保护命令链表操作
    if ((cmdkey == NULL) || LOS_ListEmpty(&cmdkey->list)) {  // 检查链表是否为空
        (VOID)pthread_mutex_unlock(&shellCB->keyMutex);  // 解锁
        return NULL;  // 返回空
    }

    cmdNode = LOS_DL_LIST_ENTRY(cmdkey->list.pstNext, CmdKeyLink, list);  // 获取第一个命令节点
    LOS_ListDelete(&(cmdNode->list)); /* 'cmdNode'在历史记录保存过程中释放 */
    (VOID)pthread_mutex_unlock(&shellCB->keyMutex);  // 解锁

    return cmdNode->cmdString;  // 返回命令字符串
}

/**
 * @brief 保存命令历史记录
 * @param string 命令字符串
 * @param shellCB Shell控制块指针
 */
STATIC VOID ShellSaveHistoryCmd(const CHAR *string, ShellCB *shellCB)
{
    CmdKeyLink *cmdHistory = shellCB->cmdHistoryKeyLink;  // 历史命令链表指针
    CmdKeyLink *cmdkey = LOS_DL_LIST_ENTRY(string, CmdKeyLink, cmdString);  // 命令节点指针
    CmdKeyLink *cmdNxt = NULL;  // 下一个命令节点指针

    if ((string == NULL) || (strlen(string) == 0)) {  // 检查命令字符串是否为空
        return;  // 直接返回
    }

    (VOID)pthread_mutex_lock(&shellCB->historyMutex);  // 加锁保护历史记录操作
    if (cmdHistory->count != 0) {  // 检查历史记录是否不为空
        cmdNxt = LOS_DL_LIST_ENTRY(cmdHistory->list.pstPrev, CmdKeyLink, list);  // 获取最后一个历史命令
        if (strcmp(string, cmdNxt->cmdString) == 0) {  // 检查是否与最后一条命令相同
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)cmdkey);  // 释放内存
            (VOID)pthread_mutex_unlock(&shellCB->historyMutex);  // 解锁
            return;  // 直接返回
        }
    }

    if (cmdHistory->count == CMD_HISTORY_LEN) {  // 检查历史记录是否达到最大长度
        cmdNxt = LOS_DL_LIST_ENTRY(cmdHistory->list.pstNext, CmdKeyLink, list);  // 获取第一个历史命令
        LOS_ListDelete(&(cmdNxt->list));  // 删除第一个历史命令
        LOS_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));  // 将新命令插入尾部
        (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)cmdNxt);  // 释放内存
        (VOID)pthread_mutex_unlock(&shellCB->historyMutex);  // 解锁
        return;  // 返回
    }

    LOS_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));  // 将新命令插入历史链表尾部
    cmdHistory->count++;  // 历史记录计数加1

    (VOID)pthread_mutex_unlock(&shellCB->historyMutex);  // 解锁
    return;
}

/**
 * @brief 发送Shell事件通知
 * @param shellCB Shell控制块指针
 */
STATIC VOID ShellNotify(ShellCB *shellCB)
{
    (VOID)LOS_EventWrite(&shellCB->shellEvent, SHELL_CMD_PARSE_EVENT);  // 写入命令解析事件
}

/**
 * @brief 按键状态枚举
 * @details 定义Shell支持的按键状态类型
 */
enum {
    STAT_NORMAL_KEY,  // 普通按键状态
    STAT_ESC_KEY,     // ESC按键状态
    STAT_MULTI_KEY    // 组合按键状态
};

/** https://www.cnblogs.com/Spiro-K/p/6592518.html
 * #!/bin/bash
 * #字符颜色显示
 * #-e:允许echo使用转义
 * #\033[:开始位
 * #\033[0m:结束位
 * #\033等同于\e
 * echo -e "\033[30m黑色字\033[0m"  
 * echo -e "\033[31m红色字\033[0m"  
 * echo -e "\033[32m绿色字\033[0m"  
 * echo -e "\033[33m黄色字\033[0m"  
 * echo -e "\033[34m蓝色字\033[0m"  
 * echo -e "\033[35m紫色字\033[0m"  
 * echo -e "\033[36m天蓝字\033[0m"  
 * echo -e "\033[37m白色字\033[0m" 
*/
/**
 * @brief 检查上下左右方向键
 * @param ch 输入字符
 * @param shellCB Shell控制块指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 ShellCmdLineCheckUDRL(const CHAR ch, ShellCB *shellCB)
{
    INT32 ret = LOS_OK;  // 返回值
    if (ch == 0x1b) { /* 0x1b: ESC键 */
        shellCB->shellKeyType = STAT_ESC_KEY;  // 设置为ESC按键状态
        return ret;  // 返回成功
    } else if (ch == 0x5b) { /* 0x5b: 组合键前缀 */
        if (shellCB->shellKeyType == STAT_ESC_KEY) {  // 检查是否为ESC后跟随的组合键
            shellCB->shellKeyType = STAT_MULTI_KEY;  // 设置为组合按键状态
            return ret;  // 返回成功
        }
    } else if (ch == 0x41) { /* 上方向键 */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {  // 检查是否为组合键
            OsShellHistoryShow(CMD_KEY_UP, shellCB);  // 显示上一条历史命令
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复普通按键状态
            return ret;  // 返回成功
        }
    } else if (ch == 0x42) { /* 下方向键 */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {  // 检查是否为组合键
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复普通按键状态
            OsShellHistoryShow(CMD_KEY_DOWN, shellCB);  // 显示下一条历史命令
            return ret;  // 返回成功
        }
    } else if (ch == 0x43) { /* 右方向键 */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {  // 检查是否为组合键
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复普通按键状态
            return ret;  // 返回成功
        }
    } else if (ch == 0x44) { /* 左方向键 */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {  // 检查是否为组合键
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复普通按键状态
            return ret;  // 返回成功
        }
    }
    return LOS_NOK;  // 返回失败
}

/**
 * @brief Shell命令行解析函数
 * @param c 输入字符
 * @param outputFunc 输出函数指针
 * @param shellCB Shell控制块指针
 */
LITE_OS_SEC_TEXT_MINOR VOID ShellCmdLineParse(CHAR c, pf_OUTPUT outputFunc, ShellCB *shellCB)
{
    const CHAR ch = c;  // 输入字符
    INT32 ret;  // 返回值

    if ((shellCB->shellBufOffset == 0) && (ch != '\n') && (ch != '\0')) {
        (VOID)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);  // 清空缓冲区
    }

    if ((ch == '\r') || (ch == '\n')) {  // 回车或换行
        if (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1)) {  // 检查缓冲区是否未满
            shellCB->shellBuf[shellCB->shellBufOffset] = '\0';  // 添加字符串结束符
        }
        shellCB->shellBufOffset = 0;  // 重置缓冲区偏移
        (VOID)pthread_mutex_lock(&shellCB->keyMutex);  // 加锁
        OsShellCmdPush(shellCB->shellBuf, shellCB->cmdKeyLink);  // 将命令压入链表
        (VOID)pthread_mutex_unlock(&shellCB->keyMutex);  // 解锁
        ShellNotify(shellCB);  // 发送通知
        return;  // 返回
    } else if ((ch == '\b') || (ch == 0x7F)) { /* 退格键或删除键(0x7F) */
        if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {  // 检查缓冲区偏移是否有效
            shellCB->shellBuf[shellCB->shellBufOffset - 1] = '\0';  // 删除前一个字符
            shellCB->shellBufOffset--;  // 偏移减1
            outputFunc("\b \b");  // 输出退格控制字符
        }
        return;  // 返回
    } else if (ch == 0x09) { /* 0x09: Tab键 */
        if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {  // 检查缓冲区偏移是否有效
            ret = OsTabCompletion(shellCB->shellBuf, &shellCB->shellBufOffset);  // 命令补全
            if (ret > 1) {  // 补全多个结果
                outputFunc("OHOS # %s", shellCB->shellBuf);  // 输出补全后的命令
            }
        }
        return;  // 返回
    }
    /* 解析上下左右方向键 */
    ret = ShellCmdLineCheckUDRL(ch, shellCB);  // 检查方向键
    if (ret == LOS_OK) {  // 方向键处理成功
        return;  // 返回
    }

    if ((ch != '\n') && (ch != '\0')) {  // 非换行和空字符
        if (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1)) {  // 缓冲区未满
            shellCB->shellBuf[shellCB->shellBufOffset] = ch;  // 保存字符到缓冲区
        } else {
            shellCB->shellBuf[SHOW_MAX_LEN - 1] = '\0';  // 缓冲区已满，添加结束符
        }
        shellCB->shellBufOffset++;  // 偏移加1
        outputFunc("%c", ch);  // 输出字符
    }

    shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复普通按键状态
}

/**
 * @brief 获取Shell消息类型
 * @param cmdParsed 命令解析结构体指针
 * @param cmdType 命令类型字符串
 * @return 成功返回LOS_OK，失败返回OS_INVALID
 */
LITE_OS_SEC_TEXT_MINOR UINT32 ShellMsgTypeGet(CmdParsed *cmdParsed, const CHAR *cmdType)
{
    CmdItemNode *curCmdItem = (CmdItemNode *)NULL;  // 当前命令项节点
    UINT32 len;  // 命令类型长度
    UINT32 minLen;  // 最小长度
    CmdModInfo *cmdInfo = OsCmdInfoGet();  // 获取命令信息

    if ((cmdParsed == NULL) || (cmdType == NULL)) {  // 参数检查
        return OS_INVALID;  // 返回无效
    }

    len = strlen(cmdType);  // 获取命令类型长度
    LOS_DL_LIST_FOR_EACH_ENTRY(curCmdItem, &(cmdInfo->cmdList.list), CmdItemNode, list) {  // 遍历命令列表
        if ((len == strlen(curCmdItem->cmd->cmdKey)) &&  // 长度匹配
            (strncmp((CHAR *)(curCmdItem->cmd->cmdKey), cmdType, len) == 0)) {  // 内容匹配
            minLen = (len < CMD_KEY_LEN) ? len : CMD_KEY_LEN;  // 取较小长度
            (VOID)memcpy_s((CHAR *)(cmdParsed->cmdKeyword), CMD_KEY_LEN, cmdType, minLen);  // 复制命令关键字
            cmdParsed->cmdType = curCmdItem->cmd->cmdType;  // 设置命令类型
            return LOS_OK;  // 返回成功
        }
    }

    return OS_INVALID;  // 返回无效
}

/**
 * @brief 获取消息名称并执行命令
 * @param cmdParsed 命令解析结构体指针
 * @param output 输出字符串
 * @param len 字符串长度
 * @return 成功返回LOS_OK，失败返回OS_INVALID
 */
STATIC UINT32 ShellMsgNameGetAndExec(CmdParsed *cmdParsed, const CHAR *output, UINT32 len)
{
    UINT32 loop;  // 循环变量
    UINT32 ret;  // 返回值
    const CHAR *tmpStr = NULL;  // 临时字符串指针
    BOOL quotes = FALSE;  // 引号匹配标志
    CHAR *msgName = (CHAR *)LOS_MemAlloc(m_aucSysMem0, len + 1);  // 分配内存
    if (msgName == NULL) {  // 内存分配检查
        PRINTK("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);  // 打印错误信息
        return OS_INVALID;  // 返回无效
    }
    /* 扫描'output'字符串获取命令 */
    /* 注意: 命令字符串不能包含任何特殊名称 */
    for (tmpStr = output, loop = 0; (*tmpStr != '\0') && (loop < len);) {  // 遍历字符串
        /* 如果遇到双引号，切换引号匹配状态 */
        if (*tmpStr == '\"') {  // 双引号
            SWITCH_QUOTES_STATUS(quotes);  // 切换引号状态
            /* 忽略双引号字符本身 */
            tmpStr++;  // 指针后移
            continue;  // 继续循环
        }
        /* 如果检测到空格且引号匹配状态为关闭 */
        /* 表示已检测到第一个分隔空格，结束扫描 */
        if ((*tmpStr == ' ') && (QUOTES_STATUS_CLOSE(quotes))) {  // 空格且未在引号内
            break;  // 跳出循环
        }
        msgName[loop] = *tmpStr++;  // 复制字符
        loop++;  // 计数器加1
    }
    msgName[loop] = '\0';  // 添加结束符
    /* 扫描命令列表检查是否能找到命令 */
    ret = ShellMsgTypeGet(cmdParsed, msgName);  // 获取命令类型
    PRINTK("\n");  // 打印换行
    if (ret != LOS_OK) {  // 命令未找到
        PRINTK("%s:command not found", msgName);  // 打印错误信息
    } else {
        (VOID)OsCmdExec(cmdParsed, (CHAR *)output);  // 执行命令
    }
    (VOID)LOS_MemFree(m_aucSysMem0, msgName);  // 释放内存
    return ret;  // 返回结果
}

/**
 * @brief Shell消息解析函数
 * @param msg 消息指针
 * @return 成功返回LOS_OK，失败返回OS_INVALID
 */
LITE_OS_SEC_TEXT_MINOR UINT32 ShellMsgParse(const VOID *msg)
{
    CHAR *output = NULL;  // 输出缓冲区
    UINT32 len, cmdLen, newLen;  // 长度变量
    CmdParsed cmdParsed;  // 命令解析结构体
    UINT32 ret = OS_INVALID;  // 返回值
    CHAR *buf = (CHAR *)msg;  // 消息缓冲区
    CHAR *newMsg = NULL;  // 新消息缓冲区
    CHAR *cmd = "exec";  // 执行命令

    if (msg == NULL) {  // 参数检查
        goto END;  // 跳转到结束
    }

    len = strlen(msg);  // 获取消息长度
    /* 2: strlen("./") */
    if ((len > 2) && (buf[0] == '.') && (buf[1] == '/')) {  // 检查是否为./开头的路径
        cmdLen = strlen(cmd);  // 获取命令长度
        newLen = len + 1 + cmdLen + 1;  // 计算新消息长度
        newMsg = (CHAR *)LOS_MemAlloc(m_aucSysMem0, newLen);  // 分配内存
        if (newMsg == NULL) {  // 内存分配检查
            PRINTK("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);  // 打印错误信息
            goto END;  // 跳转到结束
        }
        (VOID)memcpy_s(newMsg, newLen, cmd, cmdLen);  // 复制命令
        newMsg[cmdLen] = ' ';  // 添加空格
        (VOID)memcpy_s(newMsg + cmdLen + 1, newLen - cmdLen - 1,  (CHAR *)msg + 1, len);  // 复制消息内容
        msg = newMsg;  // 更新消息指针
        len = newLen - 1;  // 更新长度
    }
    output = (CHAR *)LOS_MemAlloc(m_aucSysMem0, len + 1);  // 分配输出缓冲区
    if (output == NULL) {  // 内存分配检查
        PRINTK("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);  // 打印错误信息
        goto END;  // 跳转到结束
    }
    /* 调用函数'OtCmdKeyShift'压缩并清除字符串缓冲区中无用或过多的空格 */
    ret = OsCmdKeyShift((CHAR *)msg, output, len + 1);  // 处理命令字符串
    if ((ret != LOS_OK) || (strlen(output) == 0)) {  // 处理失败或输出为空
        ret = OS_INVALID;  // 设置返回值
        goto END_FREE_OUTPUT;  // 跳转到释放输出
    }

    (VOID)memset_s(&cmdParsed, sizeof(CmdParsed), 0, sizeof(CmdParsed));  // 初始化命令解析结构体

    ret = ShellMsgNameGetAndExec(&cmdParsed, output, len);  // 获取命令名称并执行

END_FREE_OUTPUT:
    (VOID)LOS_MemFree(m_aucSysMem0, output);  // 释放输出缓冲区
END:
    if (newMsg != NULL) {  // 检查新消息缓冲区
        (VOID)LOS_MemFree(m_aucSysMem0, newMsg);  // 释放新消息缓冲区
    }
    return ret;  // 返回结果
}

#ifdef LOSCFG_FS_VFS
/**
 * @brief Shell入口函数
 * @param param 参数指针
 * @return 成功返回0，失败返回1
 */
LITE_OS_SEC_TEXT_MINOR UINT32 ShellEntry(UINTPTR param)
{
    CHAR ch;  // 输入字符
    INT32 n;  // 读取字节数
    ShellCB *shellCB = (ShellCB *)param;  // Shell控制块指针

    CONSOLE_CB *consoleCB = OsGetConsoleByID((INT32)shellCB->consoleID);  // 获取控制台CB
    if (consoleCB == NULL) {  // 控制台CB检查
        PRINT_ERR("Shell task init error!\n");  // 打印错误信息
        return 1;  // 返回失败
    }

    (VOID)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);  // 清空Shell缓冲区

    while (1) {  // 无限循环
#ifdef LOSCFG_PLATFORM_CONSOLE
        if (!IsConsoleOccupied(consoleCB)) {  // 检查控制台是否被占用
#endif
            /* 控制台是否准备好接收Shell输入？ */
            n = read(consoleCB->fd, &ch, 1);  // 读取一个字符
            if (n == 1) {  // 读取成功
                ShellCmdLineParse(ch, (pf_OUTPUT)dprintf, shellCB);  // 解析命令行
            }
            if (is_nonblock(consoleCB)) {  // 检查是否为非阻塞模式
                LOS_Msleep(50); /* 50: 休眠50毫秒 */
            }
#ifdef LOSCFG_PLATFORM_CONSOLE
        }
#endif
    }
}
#endif

/**
 * @brief Shell命令处理函数
 * @param shellCB Shell控制块指针
 */
STATIC VOID ShellCmdProcess(ShellCB *shellCB)
{
    CHAR *buf = NULL;  // 命令缓冲区
    while (1) {  // 循环处理命令
        buf = ShellGetInputBuf(shellCB);  // 获取输入缓冲区
        if (buf == NULL) {  // 缓冲区为空
            break;  // 跳出循环
        }
        (VOID)ShellMsgParse(buf);  // 解析消息
        ShellSaveHistoryCmd(buf, shellCB);  // 保存历史命令
        shellCB->cmdMaskKeyLink = shellCB->cmdHistoryKeyLink;  // 更新命令掩码链表
    }
}

/**
 * @brief Shell任务函数
 * @param param1 参数1
 * @param param2 参数2
 * @param param3 参数3
 * @param param4 参数4
 * @return 0
 */
LITE_OS_SEC_TEXT_MINOR UINT32 ShellTask(UINTPTR param1,
                                        UINTPTR param2,
                                        UINTPTR param3,
                                        UINTPTR param4)
{
    UINT32 ret;  // 返回值
    ShellCB *shellCB = (ShellCB *)param1;  // Shell控制块指针
    (VOID)param2;  // 未使用参数
    (VOID)param3;  // 未使用参数
    (VOID)param4;  // 未使用参数

    while (1) {  // 无限循环
        PRINTK("\nOHOS # ");  // 打印命令提示符
        ret = LOS_EventRead(&shellCB->shellEvent,  // 读取事件
                            0xFFF, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
        if (ret == SHELL_CMD_PARSE_EVENT) {  // 命令解析事件
            ShellCmdProcess(shellCB);  // 处理命令
        } else if (ret == CONSOLE_SHELL_KEY_EVENT) {  // 控制台Shell按键事件
            break;  // 跳出循环
        }
    }
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdKeyLink);  // 反初始化命令链表
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdHistoryKeyLink);  // 反初始化历史命令链表
    (VOID)LOS_EventDestroy(&shellCB->shellEvent);  // 销毁事件
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB);  // 释放Shell控制块内存
    return 0;  // 返回成功
}

#define SERIAL_SHELL_TASK_NAME "SerialShellTask"  // 串口Shell任务名称
#define SERIAL_ENTRY_TASK_NAME "SerialEntryTask"  // 串口入口任务名称
#define TELNET_SHELL_TASK_NAME "TelnetShellTask"  // Telnet Shell任务名称
#define TELNET_ENTRY_TASK_NAME "TelnetEntryTask"  // Telnet入口任务名称

/**
 * @brief Shell任务初始化
 * @param shellCB Shell控制块指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
LITE_OS_SEC_TEXT_MINOR UINT32 ShellTaskInit(ShellCB *shellCB)
{
    CHAR *name = NULL;  // 任务名称
    TSK_INIT_PARAM_S initParam = {0};  // 任务初始化参数

    if (shellCB->consoleID == CONSOLE_SERIAL) {  // 串口控制台
        name = SERIAL_SHELL_TASK_NAME;  // 设置串口Shell任务名称
    } else if (shellCB->consoleID == CONSOLE_TELNET) {  // Telnet控制台
        name = TELNET_SHELL_TASK_NAME;  // 设置Telnet Shell任务名称
    } else {
        return LOS_NOK;  // 返回失败
    }

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)ShellTask;  // 设置任务入口函数
    initParam.usTaskPrio   = 9; /* 9:Shell任务优先级 */
    initParam.auwArgs[0]   = (UINTPTR)shellCB;  // 设置任务参数
    initParam.uwStackSize  = 0x3000;  // 设置栈大小
    initParam.pcName       = name;  // 设置任务名称
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED;  // 设置任务分离状态

    (VOID)LOS_EventInit(&shellCB->shellEvent);  // 初始化事件

    return LOS_TaskCreate(&shellCB->shellTaskHandle, &initParam);  // 创建任务
}

/**
 * @brief Shell入口初始化
 * @param shellCB Shell控制块指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
LITE_OS_SEC_TEXT_MINOR UINT32 ShellEntryInit(ShellCB *shellCB)
{
    UINT32 ret;  // 返回值
    CHAR *name = NULL;  // 任务名称
    TSK_INIT_PARAM_S initParam = {0};  // 任务初始化参数

    if (shellCB->consoleID == CONSOLE_SERIAL) {  // 串口控制台
        name = SERIAL_ENTRY_TASK_NAME;  // 设置串口入口任务名称
    } else if (shellCB->consoleID == CONSOLE_TELNET) {  // Telnet控制台
        name = TELNET_ENTRY_TASK_NAME;  // 设置Telnet入口任务名称
    } else {
        return LOS_NOK;  // 返回失败
    }

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)ShellEntry;  // 设置任务入口函数
    initParam.usTaskPrio   = 9; /* 9:Shell任务优先级 */
    initParam.auwArgs[0]   = (UINTPTR)shellCB;  // 设置任务参数
    initParam.uwStackSize  = 0x1000;  // 设置栈大小
    initParam.pcName       = name;  // 设置任务名称
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED;  // 设置任务分离状态

    ret = LOS_TaskCreate(&shellCB->shellEntryHandle, &initParam);  // 创建任务
#ifdef LOSCFG_PLATFORM_CONSOLE
    (VOID)ConsoleTaskReg((INT32)shellCB->consoleID, shellCB->shellEntryHandle);  // 注册控制台任务
#endif

    return ret;  // 返回结果
}