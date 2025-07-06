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

#include "shell_lk.h"
#include "securec.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "shcmd.h"
#ifdef LOSCFG_SHELL_DMESG
#include "dmesg_pri.h"
#endif
#include "los_init.h"
#include "los_printf_pri.h"
#include "los_process_pri.h"

#ifdef LOSCFG_SHELL_LK
/**
 * @brief 模块标志枚举定义
 * @details 定义了系统支持的模块标识，用于日志模块级别的控制
 */
typedef enum {
    MODULE0 = 0,  // 模块0
    MODULE1 = 1,  // 模块1
    MODULE2 = 2,  // 模块2
    MODULE3 = 3,  // 模块3
    MODULE4 = 4,  // 模块4
} MODULE_FLAG;

/**
 * @brief 日志器结构体定义
 * @details 包含日志相关的配置信息，包括模块级别、跟踪级别和日志文件指针
 */
typedef struct {
    INT32 module_level;  // 模块日志级别
    INT32 trace_level;   // 跟踪日志级别
    FILE *fp;            // 日志文件指针
} Logger;

STATIC INT32 g_tracelevel;   // 全局跟踪日志级别变量
STATIC INT32 g_modulelevel;  // 全局模块日志级别变量

STATIC Logger g_logger = { 0 };  // 日志器实例初始化

/**
 * @brief 默认日志处理函数声明
 * @param level 日志级别
 * @param func 函数名
 * @param line 行号
 * @param fmt 格式化字符串
 * @param ap 可变参数列表
 */
VOID OsLkDefaultFunc(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, va_list ap);

LK_FUNC g_osLkHook = (LK_FUNC)OsLkDefaultFunc;  // 日志钩子函数初始化为默认函数

/**
 * @brief 获取当前跟踪日志级别
 * @return 当前跟踪日志级别
 */
STATIC INLINE INT32 OsLkTraceLvGet(VOID)
{
    return g_tracelevel;  // 返回全局跟踪日志级别变量值
}

/**
 * @brief 获取当前日志级别字符串
 * @return 当前日志级别对应的字符串
 */
const CHAR *OsLkCurLogLvGet(VOID)
{
    return OsLogLvGet(g_tracelevel);  // 调用OsLogLvGet获取日志级别字符串
}

/**
 * @brief 设置跟踪日志级别
 * @param level 要设置的跟踪日志级别
 */
VOID OsLkTraceLvSet(INT32 level)
{
    g_tracelevel = level;          // 更新全局跟踪日志级别变量
    g_logger.trace_level = level;  // 更新日志器中的跟踪级别
    return;
}

/**
 * @brief 设置模块日志级别
 * @param level 要设置的模块日志级别
 */
VOID OsLkModuleLvSet(INT32 level)
{
    g_modulelevel = level;          // 更新全局模块日志级别变量
    g_logger.module_level = level;  // 更新日志器中的模块级别
    return;
}

/**
 * @brief 获取当前模块日志级别
 * @return 当前模块日志级别
 */
INT32 OsLkModuleLvGet(VOID)
{
    return g_modulelevel;  // 返回全局模块日志级别变量值
}

/**
 * @brief 设置日志输出文件
 * @param str 日志文件路径
 */
VOID OsLkLogFileSet(const CHAR *str)
{
    FILE *fp = NULL;
    FILE *oldfp = g_logger.fp;  // 保存旧的文件指针

    if (str == NULL) {
        return;  // 路径为空则直接返回
    }
    fp = fopen(str, "w+");  // 以读写方式打开文件，若不存在则创建
    if (fp == NULL) {
        printf("Error can't open the %s file\n", str);  // 打开失败打印错误信息
        return;
    }

    g_logger.fp = fp;  // 更新日志器文件指针
    if (oldfp != NULL) {
        fclose(oldfp);  // 关闭旧的文件指针
    }
}

/**
 * @brief 获取当前日志文件指针
 * @return 当前日志文件指针
 */
FILE *OsLogFpGet(VOID)
{
    return g_logger.fp;  // 返回日志器中的文件指针
}

/**
 * @brief 日志命令处理函数
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 0表示成功，-1表示失败
 */
INT32 CmdLog(INT32 argc, const CHAR **argv)
{
    size_t level;
    size_t module;
    CHAR *p = NULL;

    if ((argc != 2) || (argv == NULL)) { /* 2:参数个数检查 */
        PRINTK("Usage: log level <num>\n");
        PRINTK("Usage: log module <num>\n");
        PRINTK("Usage: log path <PATH>\n");
        return -1;
    }

    if (!strncmp(argv[0], "level", strlen(argv[0]) + 1)) {
        level = strtoul(argv[1], &p, 0);  // 将字符串转换为无符号长整数
        if ((*p != 0) || (level > LOS_TRACE_LEVEL) || (level < LOS_EMG_LEVEL)) {
            PRINTK("current log level %s\n", OsLkCurLogLvGet());
            PRINTK("log %s [num] can access as 0:EMG 1:COMMON 2:ERROR 3:WARN 4:INFO 5:DEBUG\n", argv[0]);
        } else {
            OsLkTraceLvSet(level);  // 设置跟踪日志级别
            PRINTK("Set current log level %s\n", OsLkCurLogLvGet());
        }
    } else if (!strncmp(argv[0], "module", strlen(argv[0]) + 1)) {
        module = strtoul(argv[1], &p, 0);  // 将字符串转换为无符号长整数
        if ((*p != 0) || (module > MODULE4)) {
            PRINTK("log %s can't access %s\n", argv[0], argv[1]);
            PRINTK("not support yet\n");
            return -1;
        } else {
            OsLkModuleLvSet(module);  // 设置模块日志级别
            PRINTK("not support yet\n");
        }
    } else if (!strncmp(argv[0], "path", strlen(argv[0]) + 1)) {
        PRINTK("not support yet\n");
    } else {
        PRINTK("Usage: log level <num>\n");
        PRINTK("Usage: log module <num>\n");
        PRINTK("Usage: log path <PATH>\n");
        return -1;
    }

    return 0;
}

#ifdef LOSCFG_SHELL_DMESG
/**
 * @brief 日志循环记录函数
 * @param level 日志级别
 */
STATIC INLINE VOID OsLogCycleRecord(INT32 level)
{
    UINT32 tmpLen;
    if (level != LOS_COMMON_LEVEL && (level > LOS_EMG_LEVEL && level <= LOS_TRACE_LEVEL)) {
        tmpLen = strlen(OsLogLvGet(level));  // 获取日志级别字符串长度
        const CHAR* tmpPtr = OsLogLvGet(level);  // 获取日志级别字符串
        (VOID)OsLogRecordStr(tmpPtr, tmpLen);  // 记录日志字符串
    }
}
#endif

/**
 * @brief 默认日志处理函数实现
 * @param level 日志级别
 * @param func 函数名
 * @param line 行号
 * @param fmt 格式化字符串
 * @param ap 可变参数列表
 */
VOID OsLkDefaultFunc(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, va_list ap)
{
    if (level > OsLkTraceLvGet()) {
#ifdef LOSCFG_SHELL_DMESG
        if ((UINT32)level <= OsDmesgLvGet()) {
            OsLogCycleRecord(level);  // 记录循环日志
            DmesgPrintf(fmt, ap);     // 使用DmesgPrintf输出
        }
#endif
        return;
    }
    if ((level != LOS_COMMON_LEVEL) && ((level > LOS_EMG_LEVEL) && (level <= LOS_TRACE_LEVEL))) {
        dprintf("[%s][%s:%s]", OsLogLvGet(level),
                ((OsCurrProcessGet() == NULL) ? "NULL" : OsCurrProcessGet()->processName),
                ((OsCurrTaskGet() == NULL) ? "NULL" : OsCurrTaskGet()->taskName));  // 输出日志级别、进程名和任务名
    }
    LkDprintf(fmt, ap);  // 使用LkDprintf输出日志内容
}

/**
 * @brief 日志打印函数
 * @param level 日志级别
 * @param func 函数名
 * @param line 行号
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
VOID LOS_LkPrint(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, ...)
{
    va_list ap;
    if (g_osLkHook != NULL) {
        va_start(ap, fmt);
        g_osLkHook(level, func, line, fmt, ap);  // 调用日志钩子函数
        va_end(ap);
    }
}

/**
 * @brief 注册日志钩子函数
 * @param hook 要注册的钩子函数
 */
VOID LOS_LkRegHook(LK_FUNC hook)
{
    g_osLkHook = hook;  // 更新日志钩子函数
}

/**
 * @brief 日志器初始化函数
 * @return LOS_OK表示成功
 */
UINT32 OsLkLoggerInit(VOID)
{
    (VOID)memset_s(&g_logger, sizeof(Logger), 0, sizeof(Logger));  // 初始化日志器结构体
    OsLkTraceLvSet(TRACE_DEFAULT);  // 设置默认跟踪日志级别
    LOS_LkRegHook(OsLkDefaultFunc);  // 注册默认日志钩子函数
#ifdef LOSCFG_SHELL_DMESG
    (VOID)LOS_DmesgLvSet(TRACE_DEFAULT);  // 设置默认dmesg日志级别
#endif
    return LOS_OK;
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(log_shellcmd, CMD_TYPE_EX, "log", 1, (CmdCallBackFunc)CmdLog);
#endif

LOS_MODULE_INIT(OsLkLoggerInit, LOS_INIT_LEVEL_EARLIEST);

#endif
