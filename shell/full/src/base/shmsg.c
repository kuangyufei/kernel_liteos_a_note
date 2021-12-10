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

//获取输入命令buf
CHAR *ShellGetInputBuf(ShellCB *shellCB)
{
    CmdKeyLink *cmdkey = shellCB->cmdKeyLink;//待处理的shell命令链表
    CmdKeyLink *cmdNode = NULL;

    (VOID)pthread_mutex_lock(&shellCB->keyMutex);
    if ((cmdkey == NULL) || LOS_ListEmpty(&cmdkey->list)) {//链表为空的处理
        (VOID)pthread_mutex_unlock(&shellCB->keyMutex);
        return NULL;
    }

    cmdNode = LOS_DL_LIST_ENTRY(cmdkey->list.pstNext, CmdKeyLink, list);//获取当前命令项
    LOS_ListDelete(&(cmdNode->list)); /* 'cmdNode' freed in history save process *///将自己摘出去,但在历史记录中还存在
    (VOID)pthread_mutex_unlock(&shellCB->keyMutex);

    return cmdNode->cmdString;//返回命令内容
}
///保存命令历史记录,这个函数写的不太好
STATIC VOID ShellSaveHistoryCmd(const CHAR *string, ShellCB *shellCB)
{
    CmdKeyLink *cmdHistory = shellCB->cmdHistoryKeyLink;//获取历史记录的源头
    CmdKeyLink *cmdkey = LOS_DL_LIST_ENTRY(string, CmdKeyLink, cmdString);// @note_good ,获取CmdKeyLink,这里挺秒的,通过局部字符串找到整体
    CmdKeyLink *cmdNxt = NULL;

    if ((string == NULL) || (strlen(string) == 0)) {
        return;
    }

    (VOID)pthread_mutex_lock(&shellCB->historyMutex);//对链表的操作都要拿互斥锁
    if (cmdHistory->count != 0) { //有历史记录的情况
        cmdNxt = LOS_DL_LIST_ENTRY(cmdHistory->list.pstPrev, CmdKeyLink, list);//获取最老的历史记录
        if (strcmp(string, cmdNxt->cmdString) == 0) {//比较是否一样,这个地方感觉很怪,只比较一个吗?
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)cmdkey);
            (VOID)pthread_mutex_unlock(&shellCB->historyMutex);
            return;
        }
    }

    if (cmdHistory->count == CMD_HISTORY_LEN) {//历史记录已满,一删一添导致历史记录永远是满的,所以一旦跑进来了
        cmdNxt = LOS_DL_LIST_ENTRY(cmdHistory->list.pstNext, CmdKeyLink, list);//后续 ShellSaveHistoryCmd都会跑进来执行
        LOS_ListDelete(&(cmdNxt->list));//先删除一个最早插入的
        LOS_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));//再从尾部挂入新的节点,变成最新的记录
        (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)cmdNxt);//释放已经删除的节点, @note_thinking     , 建议和上一句换个位置,保证逻辑上的完整性
        (VOID)pthread_mutex_unlock(&shellCB->historyMutex);//释放互斥锁
        return;
    }
	//未满的情况下执行到此处
    LOS_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));//从尾部插入
    cmdHistory->count++;//历史记录增加

    (VOID)pthread_mutex_unlock(&shellCB->historyMutex);//释放互斥锁
    return;
}
///发送解析事件
STATIC VOID ShellNotify(ShellCB *shellCB)
{
    (VOID)LOS_EventWrite(&shellCB->shellEvent, SHELL_CMD_PARSE_EVENT);
}

enum {
    STAT_NORMAL_KEY,	///< 普通的按键
    STAT_ESC_KEY,	//<ESC>键在VT控制规范中时控制的起始键
    STAT_MULTI_KEY	//组合键
};
//解析上下左右键
/* https://www.cnblogs.com/Spiro-K/p/6592518.html
#!/bin/bash
#字符颜色显示
#-e:允许echo使用转义
#\033[:开始位
#\033[0m:结束位
#\033等同于\e
echo -e "\033[30m黑色字\033[0m"  
echo -e "\033[31m红色字\033[0m"  
echo -e "\033[32m绿色字\033[0m"  
echo -e "\033[33m黄色字\033[0m"  
echo -e "\033[34m蓝色字\033[0m"  
echo -e "\033[35m紫色字\033[0m"  
echo -e "\033[36m天蓝字\033[0m"  
echo -e "\033[37m白色字\033[0m" 
*/
STATIC INT32 ShellCmdLineCheckUDRL(const CHAR ch, ShellCB *shellCB)
{
    INT32 ret = LOS_OK;
    if (ch == 0x1b) { /* 0x1b: ESC *///按下<ESC>键(逃逸键)
        shellCB->shellKeyType = STAT_ESC_KEY;//代表控制开始
        return ret;
    } else if (ch == 0x5b) { /* 0x5b: first Key combination */ //为[键 ,遵循 vt100 规则
        if (shellCB->shellKeyType == STAT_ESC_KEY) {
            shellCB->shellKeyType = STAT_MULTI_KEY;
            return ret;
        }
    } else if (ch == 0x41) { /* up */	//上方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            OsShellHistoryShow(CMD_KEY_UP, shellCB);
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x42) { /* down *///下方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            OsShellHistoryShow(CMD_KEY_DOWN, shellCB);
            return ret;
        }
    } else if (ch == 0x43) { /* right *///右方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x44) { /* left *///左方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            return ret;
        }
    }
    return LOS_NOK;
}
///对命令行内容解析
LITE_OS_SEC_TEXT_MINOR VOID ShellCmdLineParse(CHAR c, pf_OUTPUT outputFunc, ShellCB *shellCB)
{
    const CHAR ch = c;
    INT32 ret;
	//不是回车键和字符串结束,且偏移量为0
    if ((shellCB->shellBufOffset == 0) && (ch != '\n') && (ch != '\0')) {
        (VOID)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);//重置buf
    }
	//遇到回车或换行
    if ((ch == '\r') || (ch == '\n')) {
        if (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1)) {
            shellCB->shellBuf[shellCB->shellBufOffset] = '\0';//字符串结束
        }
        shellCB->shellBufOffset = 0;
        (VOID)pthread_mutex_lock(&shellCB->keyMutex);
        OsShellCmdPush(shellCB->shellBuf, shellCB->cmdKeyLink);//解析回车或换行
        (VOID)pthread_mutex_unlock(&shellCB->keyMutex);
        ShellNotify(shellCB);//通知任务解析shell命令
        return;
    } else if ((ch == '\b') || (ch == 0x7F)) { /* backspace or delete(0x7F) */ //遇到删除键
        if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {
            shellCB->shellBuf[shellCB->shellBufOffset - 1] = '\0';//填充`\0`
            shellCB->shellBufOffset--;//buf减少
            outputFunc("\b \b");//回调入参函数
        }
        return;
    } else if (ch == 0x09) { /* 0x09: tab *///遇到tab键
        if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {
            ret = OsTabCompletion(shellCB->shellBuf, &shellCB->shellBufOffset);//解析tab键
            if (ret > 1) {
                outputFunc("OHOS # %s", shellCB->shellBuf);//回调入参函数
            }
        }
        return;
    }
    /* parse the up/down/right/left key */
    ret = ShellCmdLineCheckUDRL(ch, shellCB);//解析上下左右键
    if (ret == LOS_OK) {
        return;
    }
	
    if ((ch != '\n') && (ch != '\0')) {//普通的字符的处理
        if (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1)) {//buf范围
            shellCB->shellBuf[shellCB->shellBufOffset] = ch;//直接加入
        } else {
            shellCB->shellBuf[SHOW_MAX_LEN - 1] = '\0';//加入字符串结束符
        }
        shellCB->shellBufOffset++;//偏移量增加
        outputFunc("%c", ch);//向终端输出字符
    }

    shellCB->shellKeyType = STAT_NORMAL_KEY;//普通字符
}
///获取shell消息类型
LITE_OS_SEC_TEXT_MINOR UINT32 ShellMsgTypeGet(CmdParsed *cmdParsed, const CHAR *cmdType)
{
    CmdItemNode *curCmdItem = (CmdItemNode *)NULL;
    UINT32 len;
    UINT32 minLen;
    CmdModInfo *cmdInfo = OsCmdInfoGet();//获取全局变量

    if ((cmdParsed == NULL) || (cmdType == NULL)) {
        return OS_INVALID;
    }

    len = strlen(cmdType);
    LOS_DL_LIST_FOR_EACH_ENTRY(curCmdItem, &(cmdInfo->cmdList.list), CmdItemNode, list) {
        if ((len == strlen(curCmdItem->cmd->cmdKey)) &&
            (strncmp((CHAR *)(curCmdItem->cmd->cmdKey), cmdType, len) == 0)) {
            minLen = (len < CMD_KEY_LEN) ? len : CMD_KEY_LEN;
            (VOID)memcpy_s((CHAR *)(cmdParsed->cmdKeyword), CMD_KEY_LEN, cmdType, minLen);
            cmdParsed->cmdType = curCmdItem->cmd->cmdType;
            return LOS_OK;
        }
    }

    return OS_INVALID;
}
///获取命令名称和参数,并执行
STATIC UINT32 ShellMsgNameGetAndExec(CmdParsed *cmdParsed, const CHAR *output, UINT32 len)
{
    UINT32 loop;
    UINT32 ret;
    const CHAR *tmpStr = NULL;
    BOOL quotes = FALSE;
    CHAR *msgName = (CHAR *)LOS_MemAlloc(m_aucSysMem0, len + 1);
    if (msgName == NULL) {
        PRINTK("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return OS_INVALID;
    }
    /* Scan the 'output' string for command */
    /* Notice: Command string must not have any special name */
    for (tmpStr = output, loop = 0; (*tmpStr != '\0') && (loop < len);) {
        /* If reach a double quotes, switch the quotes matching status */
        if (*tmpStr == '\"') {
            SWITCH_QUOTES_STATUS(quotes);
            /* Ignore the double quote CHARactor itself */
            tmpStr++;
            continue;
        }
        /* If detected a space which the quotes matching status is false */
        /* which said has detected the first space for seperator, finish this scan operation */
        if ((*tmpStr == ' ') && (QUOTES_STATUS_CLOSE(quotes))) {
            break;
        }
        msgName[loop] = *tmpStr++;
        loop++;
    }
    msgName[loop] = '\0';
    /* Scan the command list to check whether the command can be found */
    ret = ShellMsgTypeGet(cmdParsed, msgName);
    PRINTK("\n");
    if (ret != LOS_OK) {
        PRINTK("%s:command not found", msgName);
    } else {
        (VOID)OsCmdExec(cmdParsed, (CHAR *)output);//真正的执行命令 output为输出设备
    }
    (VOID)LOS_MemFree(m_aucSysMem0, msgName);
    return ret;
}
///命令内容解析
LITE_OS_SEC_TEXT_MINOR UINT32 ShellMsgParse(const VOID *msg)
{
    CHAR *output = NULL;
    UINT32 len, cmdLen, newLen;
    CmdParsed cmdParsed;
    UINT32 ret = OS_INVALID;
    CHAR *buf = (CHAR *)msg;
    CHAR *newMsg = NULL;
    CHAR *cmd = "exec";

    if (msg == NULL) {
        goto END;
    }

    len = strlen(msg);
    /* 2: strlen("./") */
    if ((len > 2) && (buf[0] == '.') && (buf[1] == '/')) {
        cmdLen = strlen(cmd);
        newLen = len + 1 + cmdLen + 1;
        newMsg = (CHAR *)LOS_MemAlloc(m_aucSysMem0, newLen);
        if (newMsg == NULL) {
            PRINTK("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
            goto END;
        }
        (VOID)memcpy_s(newMsg, newLen, cmd, cmdLen);
        newMsg[cmdLen] = ' ';
        (VOID)memcpy_s(newMsg + cmdLen + 1, newLen - cmdLen - 1,  (CHAR *)msg + 1, len);
        msg = newMsg;
        len = newLen - 1;
    }
    output = (CHAR *)LOS_MemAlloc(m_aucSysMem0, len + 1);
    if (output == NULL) {
        PRINTK("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        goto END;
    }//对字符串缓冲区,调用函数“OsCmdKeyShift”来挤压和清除无用或过多的空间
    /* Call function 'OsCmdKeyShift' to squeeze and clear useless or overmuch space if string buffer */
    ret = OsCmdKeyShift((CHAR *)msg, output, len + 1);
    if ((ret != LOS_OK) || (strlen(output) == 0)) {
        ret = OS_INVALID;
        goto END_FREE_OUTPUT;
    }

    (VOID)memset_s(&cmdParsed, sizeof(CmdParsed), 0, sizeof(CmdParsed));

    ret = ShellMsgNameGetAndExec(&cmdParsed, output, len);////获取命令名称和参数,并执行

END_FREE_OUTPUT:
    (VOID)LOS_MemFree(m_aucSysMem0, output);
END:
    if (newMsg != NULL) {
        (VOID)LOS_MemFree(m_aucSysMem0, newMsg);
    }
    return ret;
}
///读取命令行内容
#ifdef LOSCFG_FS_VFS
LITE_OS_SEC_TEXT_MINOR UINT32 ShellEntry(UINTPTR param)
{
    CHAR ch;
    INT32 n = 0;
    ShellCB *shellCB = (ShellCB *)param;

    CONSOLE_CB *consoleCB = OsGetConsoleByID((INT32)shellCB->consoleID);//获取控制台
    if (consoleCB == NULL) {
        PRINT_ERR("Shell task init error!\n");
        return 1;
    }

    (VOID)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);//重置shell命令buf

    while (1) {
#ifdef LOSCFG_PLATFORM_CONSOLE
        if (!IsConsoleOccupied(consoleCB)) {//控制台是否被占用
#endif
            /* is console ready for shell ? */
            n = read(consoleCB->fd, &ch, 1);//从控制台读取一个字符内容,字符一个个处理
            if (n == 1) {//如果能读到一个字符
                ShellCmdLineParse(ch, (pf_OUTPUT)dprintf, shellCB);
            }
            if (is_nonblock(consoleCB)) {//在非阻塞模式下暂停 50ms
                LOS_Msleep(50); /* 50: 50MS for sleep */
            }
#ifdef LOSCFG_PLATFORM_CONSOLE
        }
#endif
    }
}
#endif
//处理shell 命令
STATIC VOID ShellCmdProcess(ShellCB *shellCB)
{
    CHAR *buf = NULL;
    while (1) {
        buf = ShellGetInputBuf(shellCB);//获取命令buf
        if (buf == NULL) {
            break;
        }
        (VOID)ShellMsgParse(buf);//解析buf
        ShellSaveHistoryCmd(buf, shellCB);//保存到历史记录中
        shellCB->cmdMaskKeyLink = shellCB->cmdHistoryKeyLink;
    }
}
///shell 任务,处理解析,执行命令
LITE_OS_SEC_TEXT_MINOR UINT32 ShellTask(UINTPTR param1,
                                        UINTPTR param2,
                                        UINTPTR param3,
                                        UINTPTR param4)
{
    UINT32 ret;
    ShellCB *shellCB = (ShellCB *)param1;
    (VOID)param2;
    (VOID)param3;
    (VOID)param4;

    while (1) {
        PRINTK("\nOHOS # ");//在没有事件的时候,会一直停留在此, 读取shell 输入事件 例如: cat weharmony.net 命令
        ret = LOS_EventRead(&shellCB->shellEvent,
                            0xFFF, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
        if (ret == SHELL_CMD_PARSE_EVENT) {//获得解析命令事件
            ShellCmdProcess(shellCB);//处理命令 
        } else if (ret == CONSOLE_SHELL_KEY_EVENT) {//退出shell事件
            break;
        }
    }
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdKeyLink);//
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdHistoryKeyLink);
    (VOID)LOS_EventDestroy(&shellCB->shellEvent);//注销事件
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, shellCB);//释放shell控制块
    return 0;
}

#define SERIAL_SHELL_TASK_NAME "SerialShellTask"
#define SERIAL_ENTRY_TASK_NAME "SerialEntryTask"
#define TELNET_SHELL_TASK_NAME "TelnetShellTask"
#define TELNET_ENTRY_TASK_NAME "TelnetEntryTask"
//shell 服务端任务初始化,这个任务负责解析和执行命令
LITE_OS_SEC_TEXT_MINOR UINT32 ShellTaskInit(ShellCB *shellCB)
{
    CHAR *name = NULL;
    TSK_INIT_PARAM_S initParam = {0};
	//输入Shell命令的两种方式
    if (shellCB->consoleID == CONSOLE_SERIAL) {	//通过串口工具
        name = SERIAL_SHELL_TASK_NAME;
    } else if (shellCB->consoleID == CONSOLE_TELNET) {//通过远程工具
        name = TELNET_SHELL_TASK_NAME;
    } else {
        return LOS_NOK;
    }

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)ShellTask;//任务入口函数,主要是解析shell命令
    initParam.usTaskPrio   = 9; /* 9:shell task priority */
    initParam.auwArgs[0]   = (UINTPTR)shellCB;
    initParam.uwStackSize  = 0x3000;
    initParam.pcName       = name;
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED;

    (VOID)LOS_EventInit(&shellCB->shellEvent);//初始化事件,以事件方式通知任务解析命令

    return LOS_TaskCreate(&shellCB->shellTaskHandle, &initParam);//创建任务
}
///进入shell客户端任务初始化,这个任务负责编辑命令,处理命令产生的过程,例如如何处理方向键,退格键,回车键等
LITE_OS_SEC_TEXT_MINOR UINT32 ShellEntryInit(ShellCB *shellCB)
{
    UINT32 ret;
    CHAR *name = NULL;
    TSK_INIT_PARAM_S initParam = {0};

    if (shellCB->consoleID == CONSOLE_SERIAL) {
        name = SERIAL_ENTRY_TASK_NAME;
    } else if (shellCB->consoleID == CONSOLE_TELNET) {
        name = TELNET_ENTRY_TASK_NAME;
    } else {
        return LOS_NOK;
    }

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)ShellEntry;//任务入口函数
    initParam.usTaskPrio   = 9; /* 9:shell task priority */
    initParam.auwArgs[0]   = (UINTPTR)shellCB;
    initParam.uwStackSize  = 0x1000;
    initParam.pcName       = name;	//任务名称
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED;

    ret = LOS_TaskCreate(&shellCB->shellEntryHandle, &initParam);//创建shell任务
#ifdef LOSCFG_PLATFORM_CONSOLE
    (VOID)ConsoleTaskReg((INT32)shellCB->consoleID, shellCB->shellEntryHandle);//将shell注册到控制台
#endif

    return ret;
}

