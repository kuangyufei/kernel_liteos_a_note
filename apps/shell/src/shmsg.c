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

#define CHAR_CTRL_C   '\x03'
#define CHAR_CTRL_DEL '\x7F'

#define VISIABLE_CHAR(ch) ((ch) > 0x1F && (ch) < 0x7F)
//获取命令行
char *GetCmdline(ShellCB *shellCB)
{
    CmdKeyLink *cmdkey = shellCB->cmdKeyLink;
    CmdKeyLink *cmdNode = NULL;

    (void)pthread_mutex_lock(&shellCB->keyMutex);
    if ((cmdkey == NULL) || SH_ListEmpty(&cmdkey->list)) {
        (void)pthread_mutex_unlock(&shellCB->keyMutex);
        return NULL;
    }

    cmdNode = SH_LIST_ENTRY(cmdkey->list.pstNext, CmdKeyLink, list);
    if (cmdNode == NULL) {
        (void)pthread_mutex_unlock(&shellCB->keyMutex);
        return NULL;
    }

    SH_ListDelete(&(cmdNode->list));
    (void)pthread_mutex_unlock(&shellCB->keyMutex);

    if (strlen(cmdNode->cmdString) == 0) {
        free(cmdNode);
        return NULL;
    }

    return cmdNode->cmdString;
}
///保存shell命令历史
static void ShellSaveHistoryCmd(char *string, ShellCB *shellCB)
{
    CmdKeyLink *cmdHistory = shellCB->cmdHistoryKeyLink;
    CmdKeyLink *cmdkey = SH_LIST_ENTRY(string, CmdKeyLink, cmdString);
    CmdKeyLink *cmdNxt = NULL;

    if (*string == '\n') {
        free(cmdkey);
        return;
    }

    (void)pthread_mutex_lock(&shellCB->historyMutex);
    if (cmdHistory->count != 0) {//这个分支的目的是去重,又不遍历整个链表,感觉没多大意义.
        cmdNxt = SH_LIST_ENTRY(cmdHistory->list.pstPrev, CmdKeyLink, list);
        if (strcmp(string, cmdNxt->cmdString) == 0) {
            free((void *)cmdkey);
            (void)pthread_mutex_unlock(&shellCB->historyMutex);
            return;
        }
    }

    if (cmdHistory->count >= CMD_HISTORY_LEN) {//已经满了的处理,则需要删除一条再插入新的,数量不变,所以不存在 count的 +-
        cmdNxt = SH_LIST_ENTRY(cmdHistory->list.pstNext, CmdKeyLink, list);//拿到下一条命令行记录
        SH_ListDelete(&(cmdNxt->list));//将自己摘出去
        SH_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));//尾部插入新的命令
        free((void *)cmdNxt);//释放旧数据
        (void)pthread_mutex_unlock(&shellCB->historyMutex);
        return;
    }

    SH_ListTailInsert(&(cmdHistory->list), &(cmdkey->list));//从尾部插入
    cmdHistory->count++;//历史记录的数量增加

    (void)pthread_mutex_unlock(&shellCB->historyMutex);
    return;
}
//// shell挂起
int ShellPend(ShellCB *shellCB)
{
    if (shellCB == NULL) {
        return SH_NOK;
    }

    return sem_wait(&shellCB->shellSem);
}
//// shell通知
int ShellNotify(ShellCB *shellCB)
{
    if (shellCB == NULL) {
        return SH_NOK;
    }

    return sem_post(&shellCB->shellSem);
}

enum {
    STAT_NORMAL_KEY,	///< 普通按键
    STAT_ESC_KEY,	///< <ESC>键,支持VT规范
    STAT_MULTI_KEY	///< 组合键 例如: <ESC>[30m
};
///检查上下左右键
static int ShellCmdLineCheckUDRL(const char ch, ShellCB *shellCB)
{
    int ret = SH_OK;
    if (ch == 0x1b) { /* 0x1b: ESC */
        shellCB->shellKeyType = STAT_ESC_KEY;
        return ret;
    } else if (ch == 0x5b) { /* 0x5b: first Key combination */
        if (shellCB->shellKeyType == STAT_ESC_KEY) {
            shellCB->shellKeyType = STAT_MULTI_KEY;
            return ret;
        }
    } else if (ch == 0x41) { /* up */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            OsShellHistoryShow(CMD_KEY_UP, shellCB);
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x42) { /* down */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            OsShellHistoryShow(CMD_KEY_DOWN, shellCB);
            return ret;
        }
    } else if (ch == 0x43) { /* right */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x44) { /* left */
        if (shellCB->shellKeyType == STAT_MULTI_KEY) {
            shellCB->shellKeyType = STAT_NORMAL_KEY;
            return ret;
        }
    }
    return SH_NOK;
}
//// 通知任务解析shell命令
void ShellTaskNotify(ShellCB *shellCB)
{
    int ret;

    (void)pthread_mutex_lock(&shellCB->keyMutex);
    OsShellCmdPush(shellCB->shellBuf, shellCB->cmdKeyLink);
    (void)pthread_mutex_unlock(&shellCB->keyMutex);

    ret = ShellNotify(shellCB);//通知解析shell命令的任务
    if (ret != SH_OK) {
        printf("command execute failed, \"%s\"", shellCB->shellBuf);
    }
}
//// 解析回车键 , 按回车后的处理
void ParseEnterKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;
    }

    if (shellCB->shellBufOffset == 0) {
        shellCB->shellBuf[shellCB->shellBufOffset] = '\n';
        shellCB->shellBuf[shellCB->shellBufOffset + 1] = '\0';
        goto NOTIFY;
    }

    if (shellCB->shellBufOffset <= (SHOW_MAX_LEN - 1)) {
        shellCB->shellBuf[shellCB->shellBufOffset] = '\0';
    }
NOTIFY:
    shellCB->shellBufOffset = 0;
    ShellTaskNotify(shellCB);
}

void ParseCancelKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;
    }

    if (shellCB->shellBufOffset <= (SHOW_MAX_LEN - 1)) {
        shellCB->shellBuf[0] = CHAR_CTRL_C;
        shellCB->shellBuf[1] = '\0';
    }

    shellCB->shellBufOffset = 0;
    ShellTaskNotify(shellCB);
}
///解析删除键 
void ParseDeleteKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;
    }

    if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset <= (SHOW_MAX_LEN - 1))) {
        shellCB->shellBuf[shellCB->shellBufOffset - 1] = '\0';
        shellCB->shellBufOffset--;
        outputFunc("\b \b");
    }
}
//// 解析tab键
void ParseTabKey(OutputFunc outputFunc, ShellCB *shellCB)
{
    int ret;

    if ((shellCB == NULL) || (outputFunc == NULL)) {
        return;
    }

    if ((shellCB->shellBufOffset > 0) && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {
        ret = OsTabCompletion(shellCB->shellBuf, &shellCB->shellBufOffset);
        if (ret > 1) {
            outputFunc(SHELL_PROMPT"%s", shellCB->shellBuf);
        }
    }
}
//// 解析普通字符
void ParseNormalChar(char ch, OutputFunc outputFunc, ShellCB *shellCB)
{
    if ((shellCB == NULL) || (outputFunc == NULL) || !VISIABLE_CHAR(ch)) {
        return;
    }

    if ((ch != '\0') && (shellCB->shellBufOffset < (SHOW_MAX_LEN - 1))) {
        shellCB->shellBuf[shellCB->shellBufOffset] = ch;
        shellCB->shellBufOffset++;
        outputFunc("%c", ch);
    }

    shellCB->shellKeyType = STAT_NORMAL_KEY;
}

void ShellCmdLineParse(char c, OutputFunc outputFunc, ShellCB *shellCB)
{
    const char ch = c;
    int ret;

    if ((shellCB->shellBufOffset == 0) && (ch != '\n') && (ch != CHAR_CTRL_C) && (ch != '\0')) {
        (void)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);
    }

    switch (ch) {
        case '\r':
        case '\n': /* enter */
            ParseEnterKey(outputFunc, shellCB);
            break;
        case CHAR_CTRL_C: /* ctrl + c */
            ParseCancelKey(outputFunc, shellCB);
            break;
        case '\b': /* backspace */
        case CHAR_CTRL_DEL: /* delete(0x7F) */
            ParseDeleteKey(outputFunc, shellCB);
            break;
        case '\t': /* tab */
            ParseTabKey(outputFunc, shellCB);
            break;
        default:
            /* parse the up/down/right/left key */
            ret = ShellCmdLineCheckUDRL(ch, shellCB);
            if (ret == SH_OK) {
                return;
            }
            ParseNormalChar(ch, outputFunc, shellCB);
            break;
    }

    return;
}

unsigned int ShellMsgNameGet(CmdParsed *cmdParsed, const char *cmdType)
{
    (void)cmdParsed;
    (void)cmdType;
    return SH_ERROR;
}

char *GetCmdName(const char *cmdline, unsigned int len)
{
    unsigned int loop;
    const char *tmpStr = NULL;
    bool quotes = FALSE;
    char *cmdName = NULL;
    if (cmdline == NULL) {
        return NULL;
    }

    cmdName = (char *)malloc(len + 1);
    if (cmdName == NULL) {
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    /* Scan the 'cmdline' string for command */
    /* Notice: Command string must not have any special name */
    for (tmpStr = cmdline, loop = 0; (*tmpStr != '\0') && (loop < len); ) {
        /* If reach a double quotes, switch the quotes matching status */
        if (*tmpStr == '\"') {
            SWITCH_QUOTES_STATUS(quotes);
            /* Ignore the double quote charactor itself */
            tmpStr++;
            continue;
        }
        /* If detected a space which the quotes matching status is false */
        /* which said has detected the first space for seperator, finish this scan operation */
        if ((*tmpStr == ' ') && (QUOTES_STATUS_CLOSE(quotes))) {
            break;
        }
        cmdName[loop] = *tmpStr++;
        loop++;
    }
    cmdName[loop] = '\0';

    return cmdName;
}

void ChildExec(const char *cmdName, char *const paramArray[], bool foreground)
{
    int ret;
    pid_t gid;

    ret = setpgrp();
    if (ret == -1) {
        exit(1);
    }

    gid = getpgrp();
    if (gid < 0) {
        printf("get group id failed, pgrpid %d, errno %d\n", gid, errno);
        exit(1);
    }

    if (!foreground) {
        ret = tcsetpgrp(STDIN_FILENO, gid);
        if (ret != 0) {
            printf("tcsetpgrp failed, errno %d\n", errno);
            exit(1);
        }
    }

    ret = execve(cmdName, paramArray, NULL);
    if (ret == -1) {
        perror("execve");
        exit(-1);
    }
}

int CheckExit(const char *cmdName, const CmdParsed *cmdParsed)
{
    int ret = 0;

    if (strlen(cmdName) != CMD_EXIT_COMMAND_BYTES || strncmp(cmdName, CMD_EXIT_COMMAND, CMD_EXIT_COMMAND_BYTES) != 0) {
        return 0;
    }

    if (cmdParsed->paramCnt > 1) {
        printf("exit: too many arguments\n");
        return -1;
    }
    if (cmdParsed->paramCnt == 1) {
        char *p = NULL;
        ret = strtol(cmdParsed->paramArray[0], &p, CMD_EXIT_CODE_BASE_DEC);
        if (*p != '\0') {
            printf("exit: bad number: %s\n", cmdParsed->paramArray[0]);
            return -1;
        }
    }

    exit(ret);
}

static void DoCmdExec(const char *cmdName, const char *cmdline, unsigned int len, CmdParsed *cmdParsed)
{
    bool foreground = FALSE;
    int ret;
    pid_t forkPid;

    if (strncmp(cmdline, CMD_EXEC_COMMAND, CMD_EXEC_COMMAND_BYTES) == 0) {
        if ((cmdParsed->paramCnt > 1) && (strcmp(cmdParsed->paramArray[cmdParsed->paramCnt - 1], "&") == 0)) {
            free(cmdParsed->paramArray[cmdParsed->paramCnt - 1]);
            cmdParsed->paramArray[cmdParsed->paramCnt - 1] = NULL;
            cmdParsed->paramCnt--;
            foreground = TRUE;
        }

        forkPid = fork();
        if (forkPid < 0) {
            printf("Failed to fork from shell\n");
            return;
        } else if (forkPid == 0) {
            ChildExec(cmdParsed->paramArray[0], cmdParsed->paramArray, foreground);
        } else {
            if (!foreground) {
                (void)waitpid(forkPid, 0, 0);
            }
            ret = tcsetpgrp(STDIN_FILENO, getpid());
            if (ret != 0) {
                printf("tcsetpgrp failed, errno %d\n", errno);
            }
        }
    } else {
        if (CheckExit(cmdName, cmdParsed) < 0) {
            return;
        }
        (void)syscall(__NR_shellexec, cmdName, cmdline);
    }
}

static void ParseAndExecCmdline(CmdParsed *cmdParsed, const char *cmdline, unsigned int len)
{
    int i;
    unsigned int ret;
    char shellWorkingDirectory[PATH_MAX + 1] = { 0 };
    char *cmdlineOrigin = NULL;
    char *cmdName = NULL;

    cmdlineOrigin = strdup(cmdline);
    if (cmdlineOrigin == NULL) {
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return;
    }

    cmdName = GetCmdName(cmdline, len);
    if (cmdName == NULL) {
        free(cmdlineOrigin);
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return;
    }

    ret = OsCmdParse((char *)cmdline, cmdParsed);
    if (ret != SH_OK) {
        printf("cmd parse failure in %s[%d]\n", __FUNCTION__, __LINE__);
        goto OUT;
    }

    DoCmdExec(cmdName, cmdlineOrigin, len, cmdParsed);

    if (getcwd(shellWorkingDirectory, PATH_MAX) != NULL) {
        (void)OsShellSetWorkingDirectory(shellWorkingDirectory, (PATH_MAX + 1));
    }

OUT:
    for (i = 0; i < cmdParsed->paramCnt; i++) {
        if (cmdParsed->paramArray[i] != NULL) {
            free(cmdParsed->paramArray[i]);
            cmdParsed->paramArray[i] = NULL;
        }
    }
    free(cmdName);
    free(cmdlineOrigin);
}

unsigned int PreHandleCmdline(const char *input, char **output, unsigned int *outputlen)
{
    unsigned int shiftLen, execLen, newLen;
    unsigned int removeLen = strlen("./"); /* "./" needs to be removed if it exists */
    unsigned int ret;
    char *newCmd = NULL;
    char *execCmd = CMD_EXEC_COMMAND;
    const char *cmdBuf = input;
    unsigned int cmdBufLen = strlen(cmdBuf);
    char *shiftStr = (char *)malloc(cmdBufLen + 1);
    errno_t err;

    if (shiftStr == NULL) {
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return SH_NOK;
    }
    (void)memset_s(shiftStr, cmdBufLen + 1, 0, cmdBufLen + 1);

    /* Call function 'OsCmdKeyShift' to squeeze and clear useless or overmuch space if string buffer */
    ret = OsCmdKeyShift(cmdBuf, shiftStr, cmdBufLen + 1);
    shiftLen = strlen(shiftStr);
    if ((ret != SH_OK) || (shiftLen == 0)) {
        ret = SH_NOK;
        goto END_FREE_SHIFTSTR;
    }
    *output = shiftStr;
    *outputlen = shiftLen;

    /* Check and parse "./", located at the first two charaters of the cmd */
    if ((shiftLen > removeLen) && (shiftStr[0] == '.') && (shiftStr[1] == '/')) {
        execLen = strlen(execCmd);
        newLen = execLen + shiftLen - removeLen; /* i.e., newLen - execLen == shiftLen - removeLen */
        newCmd = (char *)malloc(newLen + 1);
        if (newCmd == NULL) {
            ret = SH_NOK;
            printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
            goto END_FREE_SHIFTSTR;
        }

        err = memcpy_s(newCmd, newLen, execCmd, execLen);
        if (err != EOK) {
            printf("memcpy_s failure in %s[%d]\n", __FUNCTION__, __LINE__);
            ret = SH_NOK;
            goto END_FREE_NEWCMD;
        }

        err = memcpy_s(newCmd + execLen, newLen - execLen, shiftStr + removeLen, shiftLen - removeLen);
        if (err != EOK) {
            printf("memcpy_s failure in %s[%d]\n", __FUNCTION__, __LINE__);
            ret = SH_NOK;
            goto END_FREE_NEWCMD;
        }
        newCmd[newLen] = '\0';

        *output = newCmd;
        *outputlen = newLen;
        ret = SH_OK;
        goto END_FREE_SHIFTSTR;
    } else {
        ret = SH_OK;
        goto END;
    }
END_FREE_NEWCMD:
    free(newCmd);
END_FREE_SHIFTSTR:
    free(shiftStr);
END:
    return ret;
}

static void ExecCmdline(const char *cmdline)
{
    unsigned int ret;
    char *output = NULL;
    unsigned int outputlen;
    CmdParsed cmdParsed;

    if (cmdline == NULL) {
        return;
    }

    /* strip out unnecessary characters */
    ret = PreHandleCmdline(cmdline, &output, &outputlen);
    if (ret == SH_NOK) {
        return;
    }

    (void)memset_s(&cmdParsed, sizeof(CmdParsed), 0, sizeof(CmdParsed));
    ParseAndExecCmdline(&cmdParsed, output, outputlen);
    free(output);
}

static void ShellCmdProcess(ShellCB *shellCB)
{
    while (1) {
        char *buf = GetCmdline(shellCB);
        if (buf == NULL) {
            break;
        }
        if (buf[0] == CHAR_CTRL_C) {
            printf("^C");
            buf[0] = '\n';
        }
        printf("\n");
        ExecCmdline(buf);
        ShellSaveHistoryCmd(buf, shellCB);
        shellCB->cmdMaskKeyLink = shellCB->cmdHistoryKeyLink;
        printf(SHELL_PROMPT);
    }
}

void *ShellTask(void *argv)
{
    int ret;
    ShellCB *shellCB = (ShellCB *)argv;

    if (shellCB == NULL) {
        return NULL;
    }

    ret = prctl(PR_SET_NAME, "ShellTask");
    if (ret != SH_OK) {
        return NULL;
    }

    printf(SHELL_PROMPT);
    while (1) {
        ret = ShellPend(shellCB);
        if (ret == SH_OK) {
            ShellCmdProcess(shellCB);
        } else if (ret != SH_OK) {
            break;
        }
    }

    return NULL;
}

int ShellTaskInit(ShellCB *shellCB)
{
    unsigned int ret;
    size_t stackSize = SHELL_TASK_STACKSIZE;
    void *arg = NULL;
    pthread_attr_t attr;

    if (shellCB == NULL) {
        return SH_NOK;
    }

    ret = pthread_attr_init(&attr);
    if (ret != SH_OK) {
        return SH_NOK;
    }

    pthread_attr_setstacksize(&attr, stackSize);
    arg = (void *)shellCB;
    ret = pthread_create(&shellCB->shellTaskHandle, &attr, &ShellTask, arg);
    if (ret != SH_OK) {
        return SH_NOK;
    }

    return ret;
}
///< 给控制台注册一个shell客户端任务
static int ShellKernelReg(unsigned int shellHandle)
{
    return ioctl(STDIN_FILENO, CONSOLE_CONTROL_REG_USERTASK, shellHandle);//Shell Entry 任务将从标准输入中读取数据
}

void ShellEntry(ShellCB *shellCB)
{
    char ch;
    int ret;
    int n;
    pid_t tid = syscall(__NR_gettid);//获取当前任务/线程ID, 即 "Shell Entry" 任务的ID

    if (shellCB == NULL) {
        return;
    }

    (void)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);

    ret = ShellKernelReg((int)tid);//向内核注册shell,和控制台捆绑在一块
    if (ret != 0) {
        printf("another shell is already running!\n");
        exit(-1);
    }

    while (1) {
        n = read(0, &ch, 1);//读取输入字符
        if (n == 1) {
            ShellCmdLineParse(ch, (OutputFunc)printf, shellCB);//对命令行内容进行解析
        }
    }
    return;
}
