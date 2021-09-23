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

//LK 注者的理解是 log kernel(内核日志)
#ifdef LOSCFG_SHELL_LK

typedef enum {//模块等级
    MODULE0 = 0,
    MODULE1 = 1,
    MODULE2 = 2,
    MODULE3 = 3,
    MODULE4 = 4,
} MODULE_FLAG;

typedef struct {
    INT32 module_level;//模块等级
    INT32 trace_level;//跟踪等级
    FILE *fp;		  //文件描述结构体
} Logger;

STATIC INT32 g_tracelevel;//日志等级
STATIC INT32 g_modulelevel;//模块等级

STATIC Logger g_logger = { 0 };

VOID OsLkDefaultFunc(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, va_list ap);

LK_FUNC g_osLkHook = (LK_FUNC)OsLkDefaultFunc;
//获取日志等级
STATIC INLINE INT32 OsLkTraceLvGet(VOID)
{
    return g_tracelevel;
}

const CHAR *OsLkCurLogLvGet(VOID)
{
    return OsLogLvGet(g_tracelevel);
}

VOID OsLkTraceLvSet(INT32 level)
{
    g_tracelevel = level;
    g_logger.trace_level = level;
    return;
}

VOID OsLkModuleLvSet(INT32 level)
{
    g_modulelevel = level;
    g_logger.module_level = level;
    return;
}

INT32 OsLkModuleLvGet(VOID)
{
    return g_modulelevel;
}

VOID OsLkLogFileSet(const CHAR *str)
{
    FILE *fp = NULL;
    FILE *oldfp = g_logger.fp;

    if (str == NULL) {
        return;
    }
    fp = fopen(str, "w+");
    if (fp == NULL) {
        printf("Error can't open the %s file\n",str);
        return;
    }

    g_logger.fp = fp;
    if (oldfp != NULL) {
        fclose(oldfp);
    }
}

FILE *OsLogFpGet(VOID)
{
    return g_logger.fp;
}
/**************************************************************
log命令用于修改&查询日志配置,即选择打印哪种日志?
该命令依赖于LOSCFG_SHELL_LK，使用时通过menuconfig在配置项中开启"Enable Shell lk"：

Debug ---> Enable a Debug Version ---> Enable Shell ---> Enable Shell lK。

log level命令用于配置日志的打印等级，包括6个等级

TRACE_EMG = 0,

TRACE_COMMON = 1,

TRACE_ERROR = 2,

TRACE_WARN = 3,

TRACE_INFO = 4,

TRACE_DEBUG = 5

若level不在有效范围内，会打印提示信息。

若log level命令不加[levelNum]参数，则默认查看当前打印等级，并且提示使用方法。

输入log level 4
***************************************************************/
INT32 CmdLog(INT32 argc, const CHAR **argv)
{
    size_t level;
    size_t module;
    CHAR *p = NULL;

    if ((argc != 2) || (argv == NULL)) { /* 2:count of parameter */
        PRINTK("Usage: log level <num>\n");
        PRINTK("Usage: log module <num>\n");
        PRINTK("Usage: log path <PATH>\n");
        return -1;
    }

    if (!strncmp(argv[0], "level", strlen(argv[0]) + 1)) {
        level = strtoul(argv[1], &p, 0);
        if ((*p != 0) || (level > LOS_TRACE_LEVEL) || (level < LOS_EMG_LEVEL)) {
            PRINTK("current log level %s\n", OsLkCurLogLvGet());
            PRINTK("log %s [num] can access as 0:EMG 1:COMMON 2:ERROR 3:WARN 4:INFO 5:DEBUG\n", argv[0]);
        } else {
            OsLkTraceLvSet(level);
            PRINTK("Set current log level %s\n", OsLkCurLogLvGet());
        }
    } else if (!strncmp(argv[0], "module", strlen(argv[0]) + 1)) {
        module = strtoul(argv[1], &p, 0);
        if ((*p != 0) || (module > MODULE4) || (module < MODULE0)) {
            PRINTK("log %s can't access %s\n", argv[0], argv[1]);
            PRINTK("not support yet\n");
            return -1;
        } else {
            OsLkModuleLvSet(module);
            PRINTK("not support yet\n");
        }
    } else if (!strncmp(argv[0], "path", strlen(argv[0]) + 1)) {
        OsLkLogFileSet(argv[1]);
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
STATIC INLINE VOID OsLogCycleRecord(INT32 level)//日志循环记录
{
    UINT32 tmpLen;
    if (level != LOS_COMMON_LEVEL && (level > LOS_EMG_LEVEL && level <= LOS_TRACE_LEVEL)) {
        tmpLen = strlen(OsLogLvGet(level));
        const CHAR* tmpPtr = OsLogLvGet(level);
        (VOID)OsLogRecordStr(tmpPtr, tmpLen);
    }
}
#endif
//内核打印函数,在LOS_LkPrint中回调
VOID OsLkDefaultFunc(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, va_list ap)
{
    if (level > OsLkTraceLvGet()) {
#ifdef LOSCFG_SHELL_DMESG
        if ((UINT32)level <= OsDmesgLvGet()) {
            OsLogCycleRecord(level);
            DmesgPrintf(fmt, ap);
        }
#endif
        return;
    }
    if ((level != LOS_COMMON_LEVEL) && ((level > LOS_EMG_LEVEL) && (level <= LOS_TRACE_LEVEL))) {
        dprintf("[%s]", OsLogLvGet(level));
    }
    LkDprintf(fmt, ap);
}
//打印
VOID LOS_LkPrint(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, ...)
{
    va_list ap;
    if (g_osLkHook != NULL) {
        va_start(ap, fmt);
        g_osLkHook(level, func, line, fmt, ap);
        va_end(ap);
    }
}
//设置回调函数
VOID LOS_LkRegHook(LK_FUNC hook)
{
    g_osLkHook = hook;
}
//内核日志初始化
UINT32 OsLkLoggerInit(VOID)
{
    (VOID)memset_s(&g_logger, sizeof(Logger), 0, sizeof(Logger));
    OsLkTraceLvSet(TRACE_DEFAULT);
    LOS_LkRegHook(OsLkDefaultFunc);//注册钩子函数,将在 LOS_LkPrint中回调钩子
#ifdef LOSCFG_SHELL_DMESG
    (VOID)LOS_DmesgLvSet(TRACE_DEFAULT);
#endif
    return LOS_OK;
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(log_shellcmd, CMD_TYPE_EX, "log", 1, (CmdCallBackFunc)CmdLog);
#endif

LOS_MODULE_INIT(OsLkLoggerInit, LOS_INIT_LEVEL_EARLIEST);//日志模块初始化

#endif
