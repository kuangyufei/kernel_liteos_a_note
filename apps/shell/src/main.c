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

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "show.h"
#include "shmsg.h"
#include "shcmd.h"
#include "shell_pri.h"
#include "semaphore.h"
#include "securec.h"

ShellCB *g_shellCB = NULL;	///< 全部shell控制块
/// 获取shell控制块
ShellCB *OsGetShellCb(void)
{
    return g_shellCB;
}

static void ShellDeinit(ShellCB *shellCB)
{
    (void)pthread_mutex_destroy(&shellCB->historyMutex);
    (void)pthread_mutex_destroy(&shellCB->keyMutex);
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdKeyLink);
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdHistoryKeyLink);
    (void)free(shellCB);
}

static int OsShellCreateTask(ShellCB *shellCB)
{
    struct sched_param param = { 0 };
    int ret;

    ret = sched_getparam(getpid(), &param);
    if (ret != SH_OK) {
        return ret;
    }

    param.sched_priority = SHELL_PROCESS_PRIORITY_INIT;

    ret = sched_setparam(getpid(), &param);
    if (ret != SH_OK) {
        return ret;
    }

    ret = ShellTaskInit(shellCB);
    if (ret != SH_OK) {
        return ret;
    }

    shellCB->shellEntryHandle = pthread_self();
    return 0;
}

static int DoShellExec(char **argv)
{
    int i, j;
    int len = 0;
    int ret = SH_NOK;
    char *cmdLine = NULL;

    if (strncmp(argv[0], SHELL_EXEC_COMMAND, SHELL_EXEC_COMMAND_BYTES) == 0) {
        ChildExec(argv[1], argv + 1, FALSE);
    }
    for (i = 0; argv[i]; i++) {
        len += strlen(argv[i]);
    }
    len += i + 1;
    cmdLine = (char *)malloc(len);
    if (!cmdLine) {
        return ret;
    }
    errno_t ret1 = memset_s(cmdLine, len, 0, len);
    if (ret1 != EOK) {
        free(cmdLine);
        return ret1;
    }

    for (j = 0; j < i; j++) {
        (void)strcat_s(cmdLine, len,  argv[j]);
        (void)strcat_s(cmdLine, len, " ");
    }

    cmdLine[len - 2] = '\0';
    ret = syscall(__NR_shellexec, argv[0], cmdLine);//执行 系统调用 __NR_shellexec 对应为 SysShellExec
    free(cmdLine);
    return ret;
}
static void ShellSigChildHook(int sig)
{
    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        continue;
    }
}

/// shell进程的入口函数
int main(int argc, char **argv)
{
    int ret = SH_NOK;
    ShellCB *shellCB = NULL;

    (void)signal(SIGCHLD, ShellSigChildHook);
    if (argc > 1) {
        ret = DoShellExec(argv + 1);
        return ret;
    }

    setbuf(stdout, NULL);

    shellCB = (ShellCB *)malloc(sizeof(ShellCB));//申请shell控制块
    if (shellCB == NULL) {
        return SH_NOK;
    }
    ret = memset_s(shellCB, sizeof(ShellCB), 0, sizeof(ShellCB));//初始化控制块
    if (ret != SH_OK) {
        goto ERR_OUT1;
    }

    ret = pthread_mutex_init(&shellCB->keyMutex, NULL);//初始化待处理命令链表互斥量
    if (ret != SH_OK) {
        goto ERR_OUT1;
    }

    ret = pthread_mutex_init(&shellCB->historyMutex, NULL);//初始化命令历史记录链表互斥量
    if (ret != SH_OK) {
        goto ERR_OUT2;
    }

    ret = (int)OsShellKeyInit(shellCB);//初始化待处理命令
    if (ret != SH_OK) {
        goto ERR_OUT3;
    }//设置工作目录
    (void)strncpy_s(shellCB->shellWorkingDirectory, PATH_MAX, "/", 2); /* 2:space for "/" */

    sem_init(&shellCB->shellSem, 0, 0);//信号量初始化

    g_shellCB = shellCB;//全局变量,说明鸿蒙同时只支持一个shell进程
    ret = OsShellCreateTask(shellCB);
    if (ret != SH_OK) {
        ShellDeinit(shellCB);
        g_shellCB = NULL;
        return ret;
    }

    ShellEntry(shellCB);

ERR_OUT3:
    (void)pthread_mutex_destroy(&shellCB->historyMutex);
ERR_OUT2:
    (void)pthread_mutex_destroy(&shellCB->keyMutex);
ERR_OUT1:
    (void)free(shellCB);
    return ret;
}

