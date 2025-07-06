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

#include "shcmd.h"
#include "shmsg.h"
#include "unistd.h"
#include "stdio.h"
#include "time.h"
#include "los_event.h"
#include "los_tick.h"

#include "securec.h"
/**
 * @file watch_shellcmd.c
 * @brief Shell "watch"命令实现，用于周期性执行指定命令并显示输出
 */

/* 时间相关类型和函数的兼容性宏定义 */
#define  timeval64      timeval          // 64位时间结构体兼容定义
#define  gettimeofday64 gettimeofday     // 64位获取时间函数兼容定义
#define  ctime64        ctime            // 64位时间格式化函数兼容定义

#ifdef LOSCFG_SHELL_CMD_DEBUG
/**
 * @brief Watch命令控制块结构体
 */
typedef struct {
    BOOL title;                          /* 是否显示时间戳标题 */
    UINT32 count;                        /* 命令执行的总次数 */
    UINT32 interval;                     /* 命令执行周期( ticks ) */
    EVENT_CB_S watchEvent;               /* watch结构的事件句柄 */
    CHAR cmdbuf[CMD_MAX_LEN];            /* 要监控的命令缓冲区 */
} WatchCB;

/**
 * @brief 全局watch命令控制块指针
 */
STATIC WatchCB *g_watchCmd;

/**
 * @brief 最大监控次数宏定义
 */
#define WATCH_COUNT_MAX     0xFFFFFF      // 最大命令执行次数
/**
 * @brief 最大监控间隔宏定义
 */
#define WATCH_INTERTVAL_MAX 0xFFFFFF      // 最大执行间隔(秒)

/**
 * @brief 打印当前时间
 */
STATIC VOID PrintTime(VOID)
{
    struct timeval64 stNowTime = {0};    // 当前时间结构体

    if (gettimeofday64(&stNowTime, NULL) == 0) {  // 获取当前时间
        PRINTK("%s", ctime64(&(stNowTime.tv_sec)));  // 格式化并打印时间
    }
}

/**
 * @brief Watch命令执行函数，周期性执行指定命令
 * @param arg1 WatchCB结构体指针
 */
STATIC VOID OsShellCmdDoWatch(UINTPTR arg1)
{
    WatchCB *watchItem = (WatchCB *)arg1;  // 将参数转换为WatchCB指针
    UINT32 ret;                            // 函数返回值
    g_watchCmd = watchItem;                // 设置全局watch控制块

    while (watchItem->count--) {           // 循环执行指定次数
        printf("\033[2J\n");               // 清屏操作
        if (watchItem->title) {            // 如果需要显示标题
            PrintTime();                   // 打印当前时间
        }
        (VOID)ShellMsgParse(watchItem->cmdbuf);  // 解析并执行命令
        // 等待指定间隔或事件触发
        ret = LOS_EventRead(&watchItem->watchEvent, 0x01, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, watchItem->interval);
        if (ret == 0x01) {                 // 如果是事件触发
            break;                         // 退出循环
        }
    }

    (VOID)LOS_EventDestroy(&watchItem->watchEvent);  // 销毁事件
    free(g_watchCmd);                      // 释放watch控制块内存
    g_watchCmd = NULL;                     // 重置全局指针
}

/**
 * @brief 打印watch命令用法
 */
STATIC INLINE VOID OsWatchCmdUsage(VOID)
{
    PRINTK("\nUsage: watch\n");            // 打印命令名称
    PRINTK("watch [options] command\n");   // 打印命令格式
}

/**
 * @brief 结束当前watch任务
 * @return 成功返回LOS_OK，失败返回OS_ERROR
 */
STATIC UINT32 OsWatchOverFunc(VOID)
{
    UINT32 ret;                            // 函数返回值
    if (g_watchCmd != NULL) {              // 如果存在watch任务
        // 写入事件以结束watch循环
        ret = LOS_EventWrite(&g_watchCmd->watchEvent, 0x01);
        if (ret != LOS_OK) {               // 检查事件写入结果
            PRINT_ERR("Write event failed in %s,%d\n", __FUNCTION__, __LINE__);  // 打印错误信息
            return OS_ERROR;               // 返回错误
        }
        return LOS_OK;                     // 成功结束任务
    } else {
        PRINTK("No watch task to turn off.\n");  // 无正在运行的watch任务
        return OS_ERROR;                   // 返回错误
    }
}

/**
 * @brief 解析watch命令选项
 * @param argc 参数个数
 * @param argoff 参数偏移量指针
 * @param argv 参数数组
 * @param watchItem WatchCB结构体指针
 * @return 成功返回0，失败返回-1
 */
INT32 OsWatchOptionParsed(UINT32 argc, UINT32 *argoff, const CHAR **argv, WatchCB *watchItem)
{
    long tmpVal;                           // 临时数值变量
    CHAR *strPtr = NULL;                   // 字符串指针
    UINT32 argcount = argc;                // 参数计数

    while (argv[*argoff][0] == '-') {      // 循环解析选项
        if (argcount <= 1) {               // 检查参数个数
            OsWatchCmdUsage();             // 打印用法
            return -1;                     // 返回错误
        }

        // 解析间隔选项
        if ((strcmp(argv[*argoff], "-n") == 0) || (strcmp(argv[*argoff], "--interval") == 0)) {
            if (argcount <= 2) { /* 2:参数个数检查 */
                OsWatchCmdUsage();         // 打印用法
                return -1;                 // 返回错误
            }
            // 转换间隔值
            tmpVal = (long)strtoul(argv[*argoff + 1], &strPtr, 0);
            // 检查间隔值有效性
            if ((*strPtr != 0) || (tmpVal <= 0) || (tmpVal > WATCH_INTERTVAL_MAX)) {
                PRINTK("\ninterval time is invalid\n");  // 打印错误信息
                OsWatchCmdUsage();         // 打印用法
                return -1;                 // 返回错误
            }
            watchItem->interval = g_tickPerSecond * (UINT32)tmpVal;  // 设置间隔(ticks)
            argcount -= 2; /* 2:参数偏移 */
            (*argoff) += 2; /* 2:参数偏移 */
        } 
        // 解析无标题选项
        else if ((strcmp(argv[*argoff], "-t") == 0) || (strcmp(argv[*argoff], "-no-title") == 0)) {
            watchItem->title = FALSE;      // 设置不显示标题
            argcount--;
            (*argoff)++;
        } 
        // 解析次数选项
        else if ((strcmp(argv[*argoff], "-c") == 0) || (strcmp(argv[*argoff], "--count") == 0)) {
            if (argcount <= 2) { /* 2:参数个数检查 */
                OsWatchCmdUsage();         // 打印用法
                return -1;                 // 返回错误
            }
            // 转换次数值
            tmpVal = (long)strtoul(argv[*argoff + 1], &strPtr, 0);
            // 检查次数值有效性
            if ((*strPtr != 0) || (tmpVal <= 0) || (tmpVal > WATCH_COUNT_MAX)) {
                PRINTK("\ncount is invalid\n");  // 打印错误信息
                OsWatchCmdUsage();         // 打印用法
                return -1;                 // 返回错误
            }
            watchItem->count = (UINT32)tmpVal;  // 设置执行次数
            argcount -= 2; /* 2:参数偏移 */
            (*argoff) += 2; /* 2:参数偏移 */
        } 
        // 未知选项
        else {
            PRINTK("Unknown option.\n");  // 打印错误信息
            return -1;                     // 返回错误
        }
    }
    return 0;                              // 解析成功
}

/**
 * @brief 拼接watch命令字符串
 * @param argc 参数个数
 * @param argoff 参数偏移量
 * @param argv 参数数组
 * @param watchItem WatchCB结构体指针
 * @return 成功返回0，失败返回-1
 */
INT32 OsWatchCmdSplice(UINT32 argc, UINT32 argoff, const CHAR **argv, WatchCB *watchItem)
{
    INT32 err = 0;                         // 错误码
    if ((argc - argoff) == 0) {            // 检查是否有命令需要执行
        PRINT_ERR("no watch command!\n");  // 打印错误信息
        return -1;                         // 返回错误
    }
    while (argc - argoff) {                // 循环拼接命令
        // 拼接参数到命令缓冲区
        err = strcat_s(watchItem->cmdbuf, sizeof(watchItem->cmdbuf), argv[argoff]);
        if (err != EOK) {                  // 检查拼接结果
            PRINT_ERR("%s, %d strcat_s failed!\n", __FUNCTION__, __LINE__);  // 打印错误信息
            return -1;                     // 返回错误
        }
        err = strcat_s(watchItem->cmdbuf, sizeof(watchItem->cmdbuf), " ");  // 添加空格分隔
        if (err != EOK) {                  // 检查拼接结果
            PRINT_ERR("%s, %d strcat_s failed!\n", __FUNCTION__, __LINE__);  // 打印错误信息
            return -1;                     // 返回错误
        }
        argoff++;                          // 移动到下一个参数
    }
    return err;                            // 返回结果
}

/**
 * @brief 创建watch任务
 * @param watchItem WatchCB结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsWatchTaskCreate(WatchCB *watchItem)
{
    TSK_INIT_PARAM_S initParam = {0};      // 任务初始化参数
    UINT32 watchTaskId = 0;                // 任务ID
    UINT32 ret;                            // 函数返回值

    ret = LOS_EventInit(&watchItem->watchEvent);  // 初始化事件
    if (ret != 0) {                        // 检查事件初始化结果
        PRINT_ERR("Watch event init failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误信息
        return ret;                        // 返回错误码
    }

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OsShellCmdDoWatch;  // 设置任务入口函数
    initParam.usTaskPrio   = 10; /* 10:shellcmd_watch任务优先级 */
    initParam.auwArgs[0]   = (UINTPTR)watchItem;  // 设置任务参数
    initParam.uwStackSize  = 0x3000; /* 0x3000:shellcmd_watch任务栈大小 */
    initParam.pcName       = "shellcmd_watch";  // 设置任务名称
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED;  // 设置任务属性为分离状态

    ret = LOS_TaskCreate(&watchTaskId, &initParam);  // 创建任务
    if (ret != 0) {                        // 检查任务创建结果
        PRINT_ERR("Watch task init failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误信息
        return ret;                        // 返回错误码
    }
    return ret;                            // 返回成功
}

/**
 * @brief Shell watch命令入口函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回LOS_OK，失败返回OS_ERROR
 */
UINT32 OsShellCmdWatch(UINT32 argc, const CHAR **argv)
{
    WatchCB *watchItem = NULL;             // Watch控制块指针
    UINT32 argoff = 0;                     // 参数偏移量
    UINT32 ret;                            // 函数返回值
    INT32 err;                             // 错误码

    if (argc == 0) {                       // 检查参数个数
        OsWatchCmdUsage();                 // 打印用法
        return OS_ERROR;                   // 返回错误
    }

    if (argv == NULL) {                    // 检查参数有效性
        OsWatchCmdUsage();                 // 打印用法
        return OS_ERROR;                   // 返回错误
    }

    // 检查是否是结束watch命令
    if ((argc == 1) && (strcmp(argv[0], "--over") == 0)) {
        ret = OsWatchOverFunc();           // 调用结束函数
        return ret;                        // 返回结果
    }

    if (g_watchCmd != NULL) {              // 检查是否已有watch任务在运行
        PRINTK("Please turn off previous watch before to start a new watch.\n");  // 打印提示信息
        return OS_ERROR;                   // 返回错误
    }

    watchItem = (WatchCB *)malloc(sizeof(WatchCB));  // 分配watch控制块内存
    if (watchItem == NULL) {               // 检查内存分配结果
        PRINTK("Malloc error!\n");        // 打印错误信息
        return OS_ERROR;                   // 返回错误
    }
    (VOID)memset_s(watchItem, sizeof(WatchCB), 0, sizeof(WatchCB));  // 初始化控制块
    watchItem->title = TRUE;               // 默认显示标题
    watchItem->count = WATCH_COUNT_MAX;    // 设置默认执行次数
    watchItem->interval = g_tickPerSecond; // 设置默认间隔(1秒)

    err = OsWatchOptionParsed(argc, &argoff, argv, watchItem);  // 解析命令选项
    if (err != 0) {                        // 检查解析结果
        goto WATCH_ERROR;                  // 跳转到错误处理
    }

    err = OsWatchCmdSplice(argc, argoff, argv, watchItem);  // 拼接命令字符串
    if (err != 0) {                        // 检查拼接结果
        goto WATCH_ERROR;                  // 跳转到错误处理
    }

    ret = OsWatchTaskCreate(watchItem);    // 创建watch任务
    if (ret != 0) {                        // 检查任务创建结果
        goto WATCH_ERROR;                  // 跳转到错误处理
    }

    return LOS_OK;                         // 返回成功

WATCH_ERROR:                               // 错误处理标签
    free(watchItem);                       // 释放控制块内存
    return OS_ERROR;                       // 返回错误
}
#endif
SHELLCMD_ENTRY(watch_shellcmd, CMD_TYPE_EX, "watch", XARGS, (CmdCallBackFunc)OsShellCmdWatch);
#endif
