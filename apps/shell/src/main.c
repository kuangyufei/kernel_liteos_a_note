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

// 全局shell控制块指针，用于管理shell的运行时状态
ShellCB *g_shellCB = NULL;

/**
 * @brief 获取全局shell控制块
 * @return ShellCB* 指向当前shell控制块的指针
 */
ShellCB *OsGetShellCb(void)
{
    return g_shellCB;
}

/**
 * @brief 释放shell控制块资源
 * @param shellCB 需要释放的shell控制块指针
 * @note 依次销毁互斥锁、释放命令按键链表和历史命令链表资源，最后释放控制块内存
 */
static void ShellDeinit(ShellCB *shellCB)
{
    (void)pthread_mutex_destroy(&shellCB->historyMutex);  // 销毁历史记录互斥锁
    (void)pthread_mutex_destroy(&shellCB->keyMutex);      // 销毁按键互斥锁
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdKeyLink);  // 释放命令按键链表资源
    OsShellKeyDeInit((CmdKeyLink *)shellCB->cmdHistoryKeyLink);  // 释放历史命令按键链表资源
    (void)free(shellCB);  // 释放shell控制块内存
}

/**
 * @brief 创建并初始化shell任务
 * @param shellCB shell控制块指针
 * @return int 成功返回0，失败返回错误码
 * @note 设置进程优先级并初始化shell任务入口
 */
static int OsShellCreateTask(ShellCB *shellCB)
{
    struct sched_param param = { 0 };  // 调度参数结构体
    int ret;

    // 获取当前进程调度参数
    ret = sched_getparam(getpid(), &param);
    if (ret != SH_OK) {
        return ret;
    }

    param.sched_priority = SHELL_PROCESS_PRIORITY_INIT;  // 设置shell进程初始优先级

    // 设置进程调度参数
    ret = sched_setparam(getpid(), &param);
    if (ret != SH_OK) {
        return ret;
    }

    // 初始化shell任务
    ret = ShellTaskInit(shellCB);
    if (ret != SH_OK) {
        return ret;
    }

    shellCB->shellEntryHandle = pthread_self();  // 保存当前线程句柄
    return 0;
}
/**
 * @brief 执行shell命令的核心处理函数
 * @param argv 命令参数数组（argv[0]为命令名称）
 * @return 执行结果状态码
 * @details 功能流程：
 * 1. 处理特殊执行命令（SHELL_EXEC_COMMAND）
 * 2. 计算命令行总长度并分配内存
 * 3. 拼接完整命令行字符串
 * 4. 通过系统调用执行命令
 */
static int DoShellExec(char **argv)
{
    int i, j;          // 循环控制变量：i参数计数，j字符串拼接
    int len = 0;       // 命令行总长度（含终止符）
    int ret = SH_NOK;  // 默认返回失败状态
    char *cmdLine = NULL;  // 命令行缓冲区指针

    // 处理SHELL_EXEC_COMMAND特殊指令
    if (strncmp(argv[0], SHELL_EXEC_COMMAND, SHELL_EXEC_COMMAND_BYTES) == 0) {
        ChildExec(argv[1], argv + 1, FALSE);  // 直接执行子进程
    }

    // 计算命令行总长度（参数长度+空格数+终止符）
    for (i = 0; argv[i]; i++) {
        len += strlen(argv[i]);
    }
    len += i + 1;  // i个空格分隔符，1个终止符

    cmdLine = (char *)malloc(len);  // 分配命令行缓冲区
    if (!cmdLine) {  // 内存分配失败处理
        return ret;
    }

    // 初始化缓冲区内存
    errno_t ret1 = memset_s(cmdLine, len, 0, len);
    if (ret1 != EOK) {  // 内存初始化失败
        free(cmdLine);
        return ret1;
    }

    // 拼接命令行参数
    for (j = 0; j < i; j++) {
        (void)strcat_s(cmdLine, len, argv[j]);  // 追加参数
        (void)strcat_s(cmdLine, len, " ");      // 添加空格分隔符
    }

    cmdLine[len - 2] = '\0';  // 修正终止符位置（保留最后两位）
    ret = syscall(__NR_shellexec, argv[0], cmdLine);  // 系统调用执行
    free(cmdLine);  // 释放命令行缓冲区
    return ret;     // 返回执行结果
}

/**
 * @brief SIGCHLD信号处理钩子函数
 * @param sig 信号编号（未使用）
 * @details 功能要点：
 * 1. 使用WNOHANG非阻塞模式
 * 2. 循环回收所有僵尸子进程
 */
static void ShellSigChildHook(int sig)
{
    (void)sig;  // 显式忽略未使用参数

    // 非阻塞式回收所有终止的子进程
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        continue;  // 持续回收直到无更多终止进程
    }
}

/**
 * @brief Shell程序主入口
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序执行状态码
 * @details 主要功能：
 * 1. 注册子进程信号处理
 * 2. 初始化Shell控制块
 * 3. 创建互斥锁和信号量
 * 4. 创建Shell主任务
 */
int main(int argc, char **argv)
{
    int ret = SH_NOK;          // 返回值初始化
    ShellCB *shellCB = NULL;   // Shell控制块指针

    (void)signal(SIGCHLD, ShellSigChildHook);  // 注册SIGCHLD信号处理

    // 带参数启动时直接执行命令
    if (argc > 1) {
        ret = DoShellExec(argv + 1);  // 跳过第一个参数（程序名）
        return ret;
    }

    setbuf(stdout, NULL);       // 禁用标准输出缓冲

    // 分配Shell控制块内存
    shellCB = (ShellCB *)malloc(sizeof(ShellCB));
    if (shellCB == NULL) {
        return SH_NOK;          // 内存分配失败直接返回
    }

    // 初始化控制块内存
    ret = memset_s(shellCB, sizeof(ShellCB), 0, sizeof(ShellCB));
    if (ret != SH_OK) {
        goto ERR_OUT1;          // 初始化失败跳转到错误处理
    }

    // 初始化键盘输入互斥锁
    ret = pthread_mutex_init(&shellCB->keyMutex, NULL);
    if (ret != SH_OK) {
        goto ERR_OUT1;          // 互斥锁初始化失败处理
    }

    // 初始化历史命令互斥锁
    ret = pthread_mutex_init(&shellCB->historyMutex, NULL);
    if (ret != SH_OK) {
        goto ERR_OUT2;          // 互斥锁初始化失败处理
    }

    // 执行键盘初始化
    ret = (int)OsShellKeyInit(shellCB);
    if (ret != SH_OK) {
        goto ERR_OUT3;          // 键盘初始化失败处理
    }

    // 设置初始工作目录
    (void)strncpy_s(shellCB->shellWorkingDirectory, PATH_MAX, "/", 2);  // 2: 保留终止符空间

    sem_init(&shellCB->shellSem, 0, 0);  // 初始化信号量（初始值0，进程间共享）

    g_shellCB = shellCB;        // 设置全局控制块指针

    // 创建Shell主任务
    ret = OsShellCreateTask(shellCB);
    if (ret != SH_OK) {
        ShellDeinit(shellCB);   // 任务创建失败时执行资源回收
        g_shellCB = NULL;       // 重置全局指针
        return ret;
    }

    ShellEntry(shellCB);         // 进入Shell主循环

// 错误处理标签（按反向初始化顺序排列）
ERR_OUT3:
    (void)pthread_mutex_destroy(&shellCB->historyMutex);  // 销毁历史命令互斥锁
ERR_OUT2:
    (void)pthread_mutex_destroy(&shellCB->keyMutex);      // 销毁键盘输入互斥锁
ERR_OUT1:
    (void)free(shellCB);         // 释放控制块内存
    return ret;                 // 返回最终状态码
}
