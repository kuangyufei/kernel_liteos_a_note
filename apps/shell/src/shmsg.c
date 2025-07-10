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

#define _GNU_SOURCE

#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "sys/prctl.h"
#include "sys/ioctl.h"
#include "syscall.h"
#include "sys/wait.h"
#include "pthread.h"
#include "securec.h"
#include "shmsg.h"
#include "shell_pri.h"
#include "shcmd.h"
// 控制字符定义：Ctrl+C（中断命令）
#define CHAR_CTRL_C   '\x03'
// 控制字符定义：删除键（ASCII码0x7F）
#define CHAR_CTRL_DEL '\x7F'

// 判断字符是否为可见字符（ASCII码0x20-0x7E）
#define VISIABLE_CHAR(ch) ((ch) > 0x1F && (ch) < 0x7F)

/**
 * @brief 从命令队列获取命令行字符串
 * @param shellCB Shell控制块指针
 * @return 成功返回命令字符串指针，队列为空返回NULL
 */
char *GetCmdline(ShellCB *shellCB)
{
    CmdKeyLink *cmdkey = shellCB->cmdKeyLink;  // 命令链表头指针
    CmdKeyLink *cmdNode = NULL;                // 当前命令节点指针

    (void)pthread_mutex_lock(&shellCB->keyMutex);  // 加锁保护命令链表操作
    // 检查链表是否为空
    if ((cmdkey == NULL) || SH_ListEmpty(&cmdkey->list)) {
        (void)pthread_mutex_unlock(&shellCB->keyMutex);  // 解锁
        return NULL;
    }

    // 获取链表第一个节点
    cmdNode = SH_LIST_ENTRY(cmdkey->list.pstNext, CmdKeyLink, list);
    if (cmdNode == NULL) {
        (void)pthread_mutex_unlock(&shellCB->keyMutex);  // 解锁
        return NULL;
    }

    SH_ListDelete(&(cmdNode->list));  // 从链表删除当前节点
    (void)pthread_mutex_unlock(&shellCB->keyMutex);      // 解锁

    // 检查命令字符串是否为空
    if (strlen(cmdNode->cmdString) == 0) {
        free(cmdNode);  // 释放空节点内存
        return NULL;
    }

    return cmdNode->cmdString;  // 返回命令字符串
}

/**
 * @brief 保存命令到历史记录
 * @param string 命令字符串
 * @param shellCB Shell控制块指针
 * @note 内部函数，自动处理重复命令和历史记录长度限制
 */
static void ShellSaveHistoryCmd(char *string, ShellCB *shellCB)
{
    CmdKeyLink *cmdHistory = shellCB->cmdHistoryKeyLink;  // 历史命令链表头
    CmdKeyLink *cmdkey = SH_LIST_ENTRY(string, CmdKeyLink, cmdString);  // 当前命令节点
    CmdKeyLink *cmdNxt = NULL;  // 临时节点指针

    if (*string == '\n') {  // 忽略空命令（仅换行符）
        free(cmdkey);
        return;
    }

    (void)pthread_mutex_lock(&shellCB->historyMutex);  // 加锁保护历史记录操作
    // 检查是否与上一条命令重复
    if (cmdHistory->count != 0) {
        cmdNxt = SH_LIST_ENTRY(cmdHistory->list.pstPrev, CmdKeyLink, list);
        if (strcmp(string, cmdNxt->cmdString) == 0) {
            free((void *)cmdkey);  // 释放重复命令节点
            (void)pthread_mutex_unlock(&shellCB->historyMutex);  // 解锁
            return;
        }
    }

    // 历史记录达到最大长度，移除最早的记录
    if (cmdHistory->count >= CMD_HISTORY_LEN) {
        cmdNxt = SH_LIST_ENTRY(cmdHistory->list.pstNext, CmdKeyLink, list);
        SH_ListDelete(&(cmdNxt->list));  // 删除头节点
        SH_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));  // 添加新节点到尾部
        free((void *)cmdNxt);  // 释放旧节点内存
        (void)pthread_mutex_unlock(&shellCB->historyMutex);  // 解锁
        return;
    }

    // 添加新命令到历史记录尾部
    SH_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));
    cmdHistory->count++;  // 历史记录计数加1

    (void)pthread_mutex_unlock(&shellCB->historyMutex);  // 解锁
    return;
}

/**
 * @brief 等待Shell信号量（阻塞操作）
 * @param shellCB Shell控制块指针
 * @return 成功返回0，失败返回错误码
 */
int ShellPend(ShellCB *shellCB)
{
    if (shellCB == NULL) {
        return SH_NOK;  // 控制块为空返回错误
    }

    return sem_wait(&shellCB->shellSem);  // 等待信号量
}

/**
 * @brief 发送Shell信号量（唤醒操作）
 * @param shellCB Shell控制块指针
 * @return 成功返回0，失败返回错误码
 */
int ShellNotify(ShellCB *shellCB)
{
    if (shellCB == NULL) {
        return SH_NOK;  // 控制块为空返回错误
    }

    return sem_post(&shellCB->shellSem);  // 发送信号量
}

// 键盘输入状态枚举
enum {
    STAT_NORMAL_KEY,  // 正常字符状态
    STAT_ESC_KEY,     // ESC键按下状态
    STAT_MULTI_KEY    // 组合键状态
};

/**
 * @brief 处理方向键（上/下/左/右）输入
 * @param ch 输入字符
 * @param shellCB Shell控制块指针
 * @return 成功返回SH_OK，非方向键返回SH_NOK
 */
static int ShellCmdLineCheckUDRL(const char ch, ShellCB *shellCB)
{
    int ret = SH_OK;
    if (ch == 0x1b) {  // 0x1b: ESC键，进入ESC状态
        shellCB->shellKeyType = STAT_ESC_KEY;
        return ret;
    } else if (ch == 0x5b) {  // 0x5b: 组合键前缀，进入组合键状态
        if (shellCB->shellKeyType == STAT_ESC_KEY) {
            shellCB->shellKeyType = STAT_MULTI_KEY;
            return ret;
        }
    } else if (ch == 0x41) {  // 0x41: 'A'，上方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            OsShellHistoryShow(CMD_KEY_UP, shellCB);  // 显示上一条历史命令
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复正常状态
            return ret;
        }
    } else if (ch == 0x42) {  // 0x42: 'B'，下方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复正常状态
            OsShellHistoryShow(CMD_KEY_DOWN, shellCB);  // 显示下一条历史命令
            return ret;
        }
    } else if (ch == 0x43) {  // 0x43: 'C'，右方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复正常状态
            return ret;
        }
    } else if (ch == 0x44) {  // 0x44: 'D'，左方向键
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复正常状态
            return ret;
        }
    }
    return SH_NOK;  // 非方向键
}

/**
 * @brief 通知Shell任务执行命令
 * @param shellCB Shell控制块指针
 */
void ShellTaskNotify(ShellCB *shellCB)
{
    int ret;

    (void)pthread_mutex_lock(&shellCB->keyMutex);  // 加锁保护命令队列
    OsShellCmdPush(shellCB->shellBuf, shellCB->cmdKeyLink);  // 将命令加入队列
    (void)pthread_mutex_unlock(&shellCB->keyMutex);  // 解锁

    ret = ShellNotify(shellCB);  // 通知Shell任务
    if (ret != SH_OK) {
        printf("command execute failed, \"%s\"", shellCB->shellBuf);  // 打印执行失败信息
    }
}

/**
 * @brief 处理回车键输入
 * @param outputFunc 输出函数指针
 * @param shellCB Shell控制块指针
 */
void ParseEnterKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;  // 参数合法性检查
    }

    if (shellCB->shellBufOffset == 0) {  // 空命令（仅回车）
        shellCB->shellBuf[shellCB->shellBufOffset] = '\n';
        shellCB->shellBuf[shellCB->shellBufOffset + 1] = '\0';
        goto NOTIFY;  // 跳转到通知执行
    }

    // 正常命令，添加字符串结束符
    if (shellCB->shellBufOffset <= (SHOW_MAX_LEN - 1)) {
        shellCB->shellBuf[shellCB->shellBufOffset] = '\0';
    }
NOTIFY:
    shellCB->shellBufOffset = 0;  // 重置缓冲区偏移
    ShellTaskNotify(shellCB);  // 通知Shell任务执行命令
}

/**
 * @brief 处理Ctrl+C取消命令
 * @param outputFunc 输出函数指针
 * @param shellCB Shell控制块指针
 */
void ParseCancelKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;  // 参数合法性检查
    }

    // 设置取消命令标志（Ctrl+C）
    if (shellCB->shellBufOffset <= (SHOW_MAX_LEN - 1)) {
        shellCB->shellBuf[0] = CHAR_CTRL_C;
        shellCB->shellBuf[1] = '\0';
    }

    shellCB->shellBufOffset = 0;  // 重置缓冲区偏移
    ShellTaskNotify(shellCB);  // 通知Shell任务
}

/**
 * @brief 处理删除键（退格）
 * @param outputFunc 输出函数指针
 * @param shellCB Shell控制块指针
 */
void ParseDeleteKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;  // 参数合法性检查
    }

    // 缓冲区有字符时才执行删除
    if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset <= (SHOW_MAX_LEN - 1))) {
        shellCB->shellBuf[shellCB->shellBufOffset - 1] = '\0';  // 清除当前字符
        shellCB->shellBufOffset--;  // 偏移减1
        outputFunc("\b \b");  // 输出退格控制序列（清除屏幕字符）
    }
}

/**
 * @brief 处理Tab键自动补全
 * @param outputFunc 输出函数指针
 * @param shellCB Shell控制块指针
 */
void ParseTabKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    int ret;

    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;  // 参数合法性检查
    }

    // 缓冲区非空且未达最大长度时处理补全
    if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {
        ret = OsTabCompletion(shellCB->shellBuf, &shellCB->shellBufOffset);  // 调用补全函数
        if (ret > 1) {  // 补全成功（返回匹配数量）
            outputFunc(SHELL_PROMPT"%s", shellCB->shellBuf);  // 重新输出补全后的命令行
        }
    }
}

/**
 * @brief 处理普通可见字符输入
 * @param ch 输入字符
 * @param outputFunc 输出函数指针
 * @param shellCB Shell控制块指针
 */
void ParseNormalChar(char ch, OutputFunc outputFunc, ShellCB *shellCB)
{
    // 检查参数合法性和字符可见性
    if ((shellCB == NULL) || (outputFunc == NULL) || !VISIABLE_CHAR(ch)) {
        return;
    }

    // 字符非空且缓冲区未满时添加字符
    if ((ch != '\0') && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {
        shellCB->shellBuf[shellCB->shellBufOffset] = ch;  // 保存字符到缓冲区
        shellCB->shellBufOffset++;  // 偏移加1
        outputFunc("%c", ch);  // 回显字符
    }

    shellCB->shellKeyType = STAT_NORMAL_KEY;  // 恢复正常状态
}

/**
 * @brief Shell命令行解析主函数
 * @param c 输入字符
 * @param outputFunc 输出函数指针
 * @param shellCB Shell控制块指针
 * @note 根据输入字符类型分发到不同处理函数
 */
void ShellCmdLineParse(char c, OutputFunc outputFunc, ShellCB *shellCB)
{
    const char ch = c;  // 输入字符
    int ret;

    // 首次输入非控制字符时清空缓冲区
    if ((shellCB->shellBufOffset == 0) && (ch != '\n') && (ch != CHAR_CTRL_C) && (ch != '\0')) {
        (void)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);
    }

    switch (ch) {
        case '\r':
        case '\n':  // 回车键
            ParseEnterKey(outputFunc, shellCB);
            break;
        case CHAR_CTRL_C:  // Ctrl+C取消键
            ParseCancelKey(outputFunc, shellCB);
            break;
        case '\b':  // 退格键
        case CHAR_CTRL_DEL:  // 删除键(0x7F)
            ParseDeleteKey(outputFunc, shellCB);
            break;
        case '\t':  // Tab补全键
            ParseTabKey(outputFunc, shellCB);
            break;
        default:
            // 先尝试解析方向键
            ret = ShellCmdLineCheckUDRL(ch, shellCB);
            if (ret == SH_OK) {
                return;
            }
            // 普通字符处理
            ParseNormalChar(ch, outputFunc, shellCB);
            break;
    }

    return;
}

/**
 * @brief 获取消息名称（未实现）
 * @param cmdParsed 解析后的命令结构
 * @param cmdType 命令类型
 * @return 固定返回SH_ERROR
 */
unsigned int ShellMsgNameGet(CmdParsed *cmdParsed, const char *cmdType)
{
    (void)cmdParsed;  // 未使用参数
    (void)cmdType;    // 未使用参数
    return SH_ERROR;  // 功能未实现
}

/**
 * @brief 从命令行字符串提取命令名
 * @param cmdline 完整命令行字符串
 * @param len 最大提取长度
 * @return 成功返回命令名字符串指针，失败返回NULL
 * @note 支持带引号的命令名解析
 */
char *GetCmdName(const char *cmdline, unsigned int len)
{
    unsigned int loop;          // 循环计数器
    const char *tmpStr = NULL;  // 字符串遍历指针
    bool quotes = FALSE;        // 引号匹配状态
    char *cmdName = NULL;       // 命令名字符串
    if (cmdline == NULL) {
        return NULL;  // 参数合法性检查
    }

    cmdName = (char *)malloc(len + 1);  // 分配命令名内存（+1保存终止符）
    if (cmdName == NULL) {
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);  // 打印分配失败信息
        return NULL;
    }

    // 扫描命令行字符串提取命令名
    /* 注意：命令字符串不能包含特殊名称 */
    for (tmpStr = cmdline, loop = 0; (*tmpStr != '\0') && (loop < len); ) {
        // 遇到双引号时切换引号匹配状态
        if (*tmpStr == '\"') {
            SWITCH_QUOTES_STATUS(quotes);
            /* 忽略双引号字符本身 */
            tmpStr++;
            continue;
        }
        // 遇到空格且不在引号内时，结束命令名提取
        if ((*tmpStr == ' ') && (QUOTES_STATUS_CLOSE(quotes))) {
            break;
        }
        cmdName[loop] = *tmpStr++;  // 复制字符到命令名
        loop++;
    }
    cmdName[loop] = '\0';  // 添加字符串终止符

    return cmdName;  // 返回命令名
}
/**
 * @brief 子进程执行命令函数
 * @param cmdName 命令名称
 * @param paramArray 命令参数数组
 * @param foreground 是否前台运行标识（TRUE表示前台，FALSE表示后台）
 * @details 该函数在子进程中执行，设置进程组、控制终端，并通过execve系统调用执行命令
 */
void ChildExec(const char *cmdName, char *const paramArray[], bool foreground)
{
    int ret;                  // 系统调用返回值
    pid_t gid;                // 进程组ID

    // 设置进程组，将当前进程设置为新进程组的组长
    ret = setpgrp();
    if (ret == -1) {
        exit(1);              // 设置进程组失败，退出子进程
    }

    // 获取当前进程组ID
    gid = getpgrp();
    if (gid < 0) {
        printf("get group id failed, pgrpid %d, errno %d\n", gid, errno);
        exit(1);              // 获取进程组ID失败，退出子进程
    }

    // 如果是后台进程，将控制终端分配给新进程组
    if (!foreground) {
        ret = tcsetpgrp(STDIN_FILENO, gid);  // STDIN_FILENO为标准输入文件描述符(0)
        if (ret != 0) {
            printf("tcsetpgrp failed, errno %d\n", errno);
            exit(1);          // 设置控制终端失败，退出子进程
        }
    }

    // 执行命令，替换当前进程映像
    ret = execve(cmdName, paramArray, NULL);  // 环境变量为空
    if (ret == -1) {
        perror("execve");       // 打印execve错误信息
        exit(-1);             // 执行命令失败，退出子进程
    }
}

/**
 * @brief 检查是否为退出命令
 * @param cmdName 命令名称
 * @param cmdParsed 解析后的命令结构
 * @return 0-非退出命令，-1-参数错误，执行exit()表示正常退出
 * @details 验证命令是否为"exit"，并检查参数合法性，支持带一个数字退出码
 */
int CheckExit(const char *cmdName, const CmdParsed *cmdParsed)
{
    int ret = 0;               // 退出码，默认为0

    // 检查命令名称是否为"exit"（CMD_EXIT_COMMAND定义为"exit"，长度为4字节）
    if (strlen(cmdName) != CMD_EXIT_COMMAND_BYTES || strncmp(cmdName, CMD_EXIT_COMMAND, CMD_EXIT_COMMAND_BYTES) != 0) {
        return 0;              // 非exit命令，返回0
    }

    // exit命令不允许超过1个参数
    if (cmdParsed->paramCnt > 1) {
        printf("exit: too many arguments\n");
        return -1;             // 参数过多，返回错误
    }
    // 处理exit命令的退出码参数
    if (cmdParsed->paramCnt == 1) {
        char *p = NULL;        // 字符串转换结束指针
        // 将参数转换为十进制整数（CMD_EXIT_CODE_BASE_DEC定义为10）
        ret = strtol(cmdParsed->paramArray[0], &p, CMD_EXIT_CODE_BASE_DEC);
        if (*p != '\0') {     // 检查是否完全转换
            printf("exit: bad number: %s\n", cmdParsed->paramArray[0]);
            return -1;         // 参数非有效数字，返回错误
        }
    }

    exit(ret);                 // 执行退出，返回退出码ret
}

/**
 * @brief 执行解析后的命令
 * @param cmdName 命令名称
 * @param cmdline 原始命令行字符串
 * @param len 命令行长度
 * @param cmdParsed 解析后的命令结构
 * @details 根据命令类型选择执行方式：带"exec"前缀的命令通过fork+execve执行；普通命令通过系统调用执行
 */
static void DoCmdExec(const char *cmdName, const char *cmdline, unsigned int len, CmdParsed *cmdParsed)
{
    bool foreground = FALSE;   // 是否前台运行标识
    int ret;                   // 系统调用返回值
    pid_t forkPid;             // fork返回的进程ID

    // 检查命令行是否以"exec"前缀开头（CMD_EXEC_COMMAND定义为"exec"）
    if (strncmp(cmdline, CMD_EXEC_COMMAND, CMD_EXEC_COMMAND_BYTES) == 0) {
        // 检查是否以"&"结尾，表示后台运行
        if ((cmdParsed->paramCnt > 1) && (strcmp(cmdParsed->paramArray[cmdParsed->paramCnt - 1], "&") == 0)) {
            free(cmdParsed->paramArray[cmdParsed->paramCnt - 1]);  // 释放"&"参数内存
            cmdParsed->paramArray[cmdParsed->paramCnt - 1] = NULL;
            cmdParsed->paramCnt--;                                 // 参数计数减1
            foreground = TRUE;                                     // 设置为前台运行（此处逻辑需结合上下文理解）
        }

        // 创建子进程
        forkPid = fork();
        if (forkPid < 0) {
            printf("Failed to fork from shell\n");
            return;             // fork失败，返回
        } else if (forkPid == 0) {
            // 子进程：执行命令
            ChildExec(cmdParsed->paramArray[0], cmdParsed->paramArray, foreground);
        } else {
            // 父进程：根据前台/后台模式决定是否等待子进程
            if (!foreground) {
                (void)waitpid(forkPid, 0, 0);  // 阻塞等待子进程结束
            }
            // 将控制终端恢复给shell进程
            ret = tcsetpgrp(STDIN_FILENO, getpid());
            if (ret != 0) {
                printf("tcsetpgrp failed, errno %d\n", errno);
            }
        }
    } else {
        // 非exec命令：检查是否为exit命令
        if (CheckExit(cmdName, cmdParsed) < 0) {
            return;             // exit命令参数错误，返回
        }
        // 通过系统调用执行命令（__NR_shellexec为系统调用号）
        (void)syscall(__NR_shellexec, cmdName, cmdline);
    }
}

/**
 * @brief 解析并执行命令行
 * @param cmdParsed 命令解析结构，用于存储解析结果
 * @param cmdline 命令行字符串
 * @param len 命令行长度
 * @details 完成命令行字符串复制、命令名提取、命令解析和执行，并在执行后清理资源和更新工作目录
 */
static void ParseAndExecCmdline(CmdParsed *cmdParsed, const char *cmdline, unsigned int len)
{
    int i;                          // 循环索引
    unsigned int ret;               // 函数返回值
    char shellWorkingDirectory[PATH_MAX + 1] = { 0 };  // 当前工作目录缓冲区（PATH_MAX定义为文件路径最大长度）
    char *cmdlineOrigin = NULL;     // 原始命令行字符串副本
    char *cmdName = NULL;           // 命令名称

    // 复制命令行字符串（为避免原字符串被修改）
    cmdlineOrigin = strdup(cmdline);
    if (cmdlineOrigin == NULL) {
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return;                     // 内存分配失败，返回
    }

    // 提取命令名称（从命令行字符串中）
    cmdName = GetCmdName(cmdline, len);
    if (cmdName == NULL) {
        free(cmdlineOrigin);        // 释放已分配的内存
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return;                     // 命令名提取失败，返回
    }

    // 解析命令行，填充cmdParsed结构
    ret = OsCmdParse((char *)cmdline, cmdParsed);
    if (ret != SH_OK) {             // SH_OK表示解析成功
        printf("cmd parse failure in %s[%d]\n", __FUNCTION__, __LINE__);
        goto OUT;                   // 解析失败，跳转到清理流程
    }

    // 执行解析后的命令
    DoCmdExec(cmdName, cmdlineOrigin, len, cmdParsed);

    // 获取并更新当前工作目录
    if (getcwd(shellWorkingDirectory, PATH_MAX) != NULL) {
        (void)OsShellSetWorkingDirectory(shellWorkingDirectory, (PATH_MAX + 1));
    }

OUT:
    // 释放参数数组内存
    for (i = 0; i < cmdParsed->paramCnt; i++) {
        if (cmdParsed->paramArray[i] != NULL) {
            free(cmdParsed->paramArray[i]);
            cmdParsed->paramArray[i] = NULL;
        }
    }
    free(cmdName);                  // 释放命令名内存
    free(cmdlineOrigin);            // 释放命令行副本内存
}

/**
 * @brief 命令行预处理函数
 * @param input 原始命令行输入
 * @param output 预处理后的命令行输出
 * @param outputlen 输出命令行长度
 * @return SH_OK-成功，SH_NOK-失败
 * @details 处理命令行中的多余空格，并将以"./"开头的命令转换为带"exec"前缀的格式
 */
unsigned int PreHandleCmdline(const char *input, char **output, unsigned int *outputlen)
{
    unsigned int shiftLen, execLen, newLen;  // shiftLen:处理后长度, execLen:exec前缀长度, newLen:新命令长度
    unsigned int removeLen = strlen("./");  // 需要移除的前缀长度（"./"为2字节）
    unsigned int ret;                        // 函数返回值
    char *newCmd = NULL;                     // 新命令缓冲区
    char *execCmd = CMD_EXEC_COMMAND;        // exec前缀字符串（定义为"exec"）
    const char *cmdBuf = input;              // 输入命令缓冲区
    unsigned int cmdBufLen = strlen(cmdBuf); // 输入命令长度
    char *shiftStr = (char *)malloc(cmdBufLen + 1);  // 空格处理后的字符串
    errno_t err;                             // 内存操作错误码

    if (shiftStr == NULL) {
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return SH_NOK;                       // 内存分配失败
    }
    (void)memset_s(shiftStr, cmdBufLen + 1, 0, cmdBufLen + 1);  // 初始化缓冲区

    // 调用OsCmdKeyShift压缩空格并清除无效字符
    ret = OsCmdKeyShift(cmdBuf, shiftStr, cmdBufLen + 1);
    shiftLen = strlen(shiftStr);
    if ((ret != SH_OK) || (shiftLen == 0)) {
        ret = SH_NOK;
        goto END_FREE_SHIFTSTR;              // 处理失败，跳转到释放shiftStr
    }
    *output = shiftStr;
    *outputlen = shiftLen;

    // 检查命令是否以"./"开头（需要转换为exec命令）
    if ((shiftLen > removeLen) && (shiftStr[0] == '.') && (shiftStr[1] == '/')) {
        execLen = strlen(execCmd);
        newLen = execLen + shiftLen - removeLen;  // 新命令长度=exec长度 + (原长度 - ./长度)
        newCmd = (char *)malloc(newLen + 1);      // +1用于存储字符串结束符\0
        if (newCmd == NULL) {
            ret = SH_NOK;
            printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
            goto END_FREE_SHIFTSTR;              // 内存分配失败
        }

        // 复制exec前缀到新命令缓冲区
        err = memcpy_s(newCmd, newLen, execCmd, execLen);
        if (err != EOK) {                        // EOK表示内存复制成功
            printf("memcpy_s failure in %s[%d]\n", __FUNCTION__, __LINE__);
            ret = SH_NOK;
            goto END_FREE_NEWCMD;                // 复制失败，释放newCmd
        }

        // 复制剩余部分（移除./后的内容）
        err = memcpy_s(newCmd + execLen, newLen - execLen, shiftStr + removeLen, shiftLen - removeLen);
        if (err != EOK) {
            printf("memcpy_s failure in %s[%d]\n", __FUNCTION__, __LINE__);
            ret = SH_NOK;
            goto END_FREE_NEWCMD;                // 复制失败，释放newCmd
        }
        newCmd[newLen] = '\0';                  // 添加字符串结束符

        *output = newCmd;                        // 更新输出指针
        *outputlen = newLen;                     // 更新输出长度
        ret = SH_OK;
        goto END_FREE_SHIFTSTR;                  // 跳转到释放shiftStr
    } else {
        ret = SH_OK;
        goto END;                                // 无需转换，直接返回
    }
END_FREE_NEWCMD:
    free(newCmd);                                // 释放newCmd内存
END_FREE_SHIFTSTR:
    free(shiftStr);                              // 释放shiftStr内存
END:
    return ret;
}

/**
 * @brief 命令行执行入口函数
 * @param cmdline 命令行字符串
 * @details 对命令行进行预处理、解析和执行，是命令执行的总入口
 */
static void ExecCmdline(const char *cmdline)
{
    unsigned int ret;               // 函数返回值
    char *output = NULL;            // 预处理后的命令行
    unsigned int outputlen;         // 预处理后命令行长度
    CmdParsed cmdParsed;            // 命令解析结构

    if (cmdline == NULL) {
        return;                     // 命令行为空，直接返回
    }

    // 预处理命令行（处理空格和转换./开头的命令）
    ret = PreHandleCmdline(cmdline, &output, &outputlen);
    if (ret == SH_NOK) {
        return;                     // 预处理失败，返回
    }

    // 初始化命令解析结构
    (void)memset_s(&cmdParsed, sizeof(CmdParsed), 0, sizeof(CmdParsed));
    // 解析并执行命令
    ParseAndExecCmdline(&cmdParsed, output, outputlen);
    free(output);                   // 释放预处理命令行内存
}

/**
 * @brief Shell命令处理循环
 * @param shellCB Shell控制块
 * @details 循环获取命令行、执行命令、保存历史命令并打印提示符，是Shell交互的核心循环
 */
static void ShellCmdProcess(ShellCB *shellCB)
{
    while (1) {
        char *buf = GetCmdline(shellCB);  // 获取用户输入的命令行
        if (buf == NULL) {
            break;                       // 获取命令行失败，退出循环
        }
        if (buf[0] == CHAR_CTRL_C) {      // 检查是否为Ctrl+C（中断信号）
            printf("^C");                // 打印^C提示
            buf[0] = '\n';               // 将第一个字符替换为换行符
        }
        printf("\n");                     // 换行
        ExecCmdline(buf);                 // 执行命令行
        ShellSaveHistoryCmd(buf, shellCB); // 保存命令到历史记录
        shellCB->cmdMaskKeyLink = shellCB->cmdHistoryKeyLink; // 更新历史命令链接
        printf(SHELL_PROMPT);             // 打印Shell提示符（如"$ "）
    }
}

/**
 * @brief Shell任务主函数
 * @param argv 参数，指向Shell控制块
 * @return NULL
 * @details Shell任务入口，设置任务名称，循环等待命令并处理，是Shell的主循环
 */
void *ShellTask(void *argv)
{
    int ret;                   // 函数返回值
    ShellCB *shellCB = (ShellCB *)argv;  // Shell控制块指针

    if (shellCB == NULL) {
        return NULL;           // 控制块为空，返回
    }

    // 设置任务名称为"ShellTask"
    ret = prctl(PR_SET_NAME, "ShellTask");
    if (ret != SH_OK) {
        return NULL;           // 设置任务名称失败，返回
    }

    printf(SHELL_PROMPT);       // 打印初始提示符
    while (1) {
        // 等待命令输入（阻塞操作）
        ret = ShellPend(shellCB);
        if (ret == SH_OK) {
            ShellCmdProcess(shellCB);  // 处理命令
        } else if (ret != SH_OK) {
            break;                   // 等待失败，退出循环
        }
    }

    return NULL;
}

/**
 * @brief Shell任务初始化函数
 * @param shellCB Shell控制块
 * @return SH_OK-成功，SH_NOK-失败
 * @details 创建Shell任务，设置任务属性（如栈大小），并启动任务
 */
int ShellTaskInit(ShellCB *shellCB)
{
    unsigned int ret;                   // 函数返回值
    size_t stackSize = SHELL_TASK_STACKSIZE;  // 任务栈大小（宏定义，单位字节）
    void *arg = NULL;                   // 任务参数
    pthread_attr_t attr;                // 线程属性结构

    if (shellCB == NULL) {
        return SH_NOK;                  // 控制块为空，返回失败
    }

    // 初始化线程属性
    ret = pthread_attr_init(&attr);
    if (ret != SH_OK) {
        return SH_NOK;                  // 初始化属性失败
    }

    // 设置线程栈大小
    pthread_attr_setstacksize(&attr, stackSize);
    arg = (void *)shellCB;              // 设置任务参数为Shell控制块
    // 创建Shell任务线程
    ret = pthread_create(&shellCB->shellTaskHandle, &attr, &ShellTask, arg);
    if (ret != SH_OK) {
        return SH_NOK;                  // 创建线程失败
    }

    return ret;
}

/**
 * @brief Shell内核注册函数
 * @param shellHandle Shell任务句柄
 * @return 0-成功，非0-失败
 * @details 通过ioctl系统调用向内核注册Shell用户任务，用于控制台输入事件通知
 */
static int ShellKernelReg(unsigned int shellHandle)
{
    // CONSOLE_CONTROL_REG_USERTASK为控制台控制命令，用于注册用户任务
    return ioctl(STDIN_FILENO, CONSOLE_CONTROL_REG_USERTASK, shellHandle);
}

/**
 * @brief Shell入口函数
 * @param shellCB Shell控制块
 * @details Shell主入口，初始化缓冲区，注册内核任务，循环读取用户输入并解析
 */
void ShellEntry(ShellCB *shellCB)
{
    char ch;                     // 读取的字符
    int ret;                     // 函数返回值
    int n;                       // 读取字符数
    pid_t tid = syscall(__NR_gettid);  // 获取当前线程ID（__NR_gettid为系统调用号）

    if (shellCB == NULL) {
        return;                 // 控制块为空，返回
    }

    // 初始化Shell缓冲区（SHOW_MAX_LEN为缓冲区最大长度）
    (void)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);

    // 向内核注册Shell任务
    ret = ShellKernelReg((int)tid);
    if (ret != 0) {
        printf("another shell is already running!\n");
        exit(-1);              // 注册失败，退出程序
    }

    // 循环读取用户输入字符
    while (1) {
        n = read(0, &ch, 1);    // 从标准输入（文件描述符0）读取1个字符
        if (n == 1) {
            // 解析输入字符（如处理退格、Tab、回车等）
            ShellCmdLineParse(ch, (OutputFunc)printf, shellCB);
        }
    }
    return;
}