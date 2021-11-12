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

#include "los_base.h"
#ifdef LOSCFG_LIB_LIBC
#include "stdlib.h"
#include "unistd.h"
#endif
#include "los_hwi.h"
#include "los_memory_pri.h"
#include "los_process_pri.h"
#ifdef LOSCFG_FS_VFS
#include "console.h"
#endif
#ifdef LOSCFG_SHELL_DMESG
#include "dmesg_pri.h"
#endif
#ifdef LOSCFG_SAVE_EXCINFO
#include "los_excinfo_pri.h"
#endif
#include "los_exc_pri.h"


#define SIZEBUF  256

const CHAR *g_logString[] = {//日志等级
    "EMG",		//紧急
    "COMMON",	//普通
    "ERR",		//错误日志
    "WARN",		//警告
    "INFO",		//信息
    "DEBUG",	//调试	
    "TRACE"		//跟踪
};

const CHAR *OsLogLvGet(INT32 level)
{
    return g_logString[level];
}

STATIC VOID ErrorMsg(VOID)
{
    const CHAR *p = "Output illegal string! vsnprintf_s failed!\n";
    UartPuts(p, (UINT32)strlen(p), UART_WITH_LOCK);
}
///串口输出,打印消息的本质就是向串口输出buf
STATIC VOID UartOutput(const CHAR *str, UINT32 len, BOOL isLock)
{
#ifdef LOSCFG_SHELL_DMESG //是否打开了 dmesg开关,打开了会写到文件中 var/log/dmesg 文件中
    if (!OsCheckUartLock()) {//是否被锁
        UartPuts(str, len, isLock);//直接写串口
    }
    if (isLock != UART_WITHOUT_LOCK) {
        (VOID)OsLogMemcpyRecord(str, len);//写入dmesg缓存区
    }
#else
    UartPuts(str, len, isLock);//没有打开dmesg开关时,直接写串口 
#endif
}
///控制台输出
#ifdef LOSCFG_PLATFORM_CONSOLE
STATIC VOID ConsoleOutput(const CHAR *str, UINT32 len)
{
    ssize_t written = 0;
    ssize_t cnt;
    ssize_t toWrite = len;//每次写入的数量
	
    for (;;) {
        cnt = write(STDOUT_FILENO, str + written, (size_t)toWrite);//向控制台写入数据,STDOUT_FILENO = 1
        if ((cnt < 0) || ((cnt == 0) && (OS_INT_ACTIVE)) || (toWrite == cnt)) {
            break;
        }
        written += cnt;	//已写入数量增加
        toWrite -= cnt;	//要写入数量减少
    }
}
#endif

VOID OutputControl(const CHAR *str, UINT32 len, OutputType type)
{
    switch (type) {//输出类型
        case CONSOLE_OUTPUT://控制台输出
#ifdef LOSCFG_PLATFORM_CONSOLE
            if (ConsoleEnable() == TRUE) {//POSIX 定义了 STDIN_FILENO、STDOUT_FILENO 和 STDERR_FILENO 来代替 0、1、2 
                ConsoleOutput(str, len);//输出到控制台
                break;//在三个文件会在 VFS中默认创建
            }
#endif
            /* fall-through */ //落空的情况下,会接着向串口打印数据
        case UART_OUTPUT:	//串口输出
            UartOutput(str, len, UART_WITH_LOCK);//向串口发送数据
            break;
        case EXC_OUTPUT:	//异常输出
            UartPuts(str, len, UART_WITH_LOCK);
            break;
        default:
            break;
    }
    return;
}

STATIC VOID OsVprintfFree(CHAR *buf, UINT32 bufLen)
{
    if (bufLen != SIZEBUF) {
        (VOID)LOS_MemFree(m_aucSysMem0, buf);
    }
}
///printf由 print 和 format 两个单词构成,格式化输出函数, 一般用于向标准输出设备按规定格式输出信息 
//鸿蒙由OsVprintf 来实现可变参数日志格式的输入
VOID OsVprintf(const CHAR *fmt, va_list ap, OutputType type)
{
    INT32 len;
    const CHAR *errMsgMalloc = "OsVprintf, malloc failed!\n";//内存分配失败的错误日志,注意这是在打印函数里分配内存失败的情况
    const CHAR *errMsgLen = "OsVprintf, length overflow!\n";//所以要直接向串口发送字符串,不能再调用 printK(...)打印日志了.
    CHAR aBuf[SIZEBUF] = {0};
    CHAR *bBuf = NULL;
    UINT32 bufLen = SIZEBUF;
    UINT32 systemStatus;

    bBuf = aBuf;
    len = vsnprintf_s(bBuf, bufLen, bufLen - 1, fmt, ap);//C语言库函数之一，属于可变参数。用于向字符串中打印数据、数据格式用户自定义。
    if ((len == -1) && (*bBuf == '\0')) {//直接碰到字符串结束符或长度异常
        /* parameter is illegal or some features in fmt dont support */ //参数非法或fmt中的某些功能不支持
        ErrorMsg();
        return;
    }

    while (len == -1) {//处理((len == -1) && (*bBuf != '\0'))的情况
        /* bBuf is not enough */
        OsVprintfFree(bBuf, bufLen);

        bufLen = bufLen << 1;
        if ((INT32)bufLen <= 0) {//异常情况下 向串口发送
            UartPuts(errMsgLen, (UINT32)strlen(errMsgLen), UART_WITH_LOCK);
            return;
        }
        bBuf = (CHAR *)LOS_MemAlloc(m_aucSysMem0, bufLen);
        if (bBuf == NULL) {//分配内存失败,直接向串口发送错误信息
            UartPuts(errMsgMalloc, (UINT32)strlen(errMsgMalloc), UART_WITH_LOCK);
            return;
        }
        len = vsnprintf_s(bBuf, bufLen, bufLen - 1, fmt, ap);//将ap按格式输出到buf中
        if (*bBuf == '\0') {//字符串结束符
            /* parameter is illegal or some features in fmt dont support */
            (VOID)LOS_MemFree(m_aucSysMem0, bBuf);
            ErrorMsg();
            return;
        }
    }
    *(bBuf + len) = '\0';

    systemStatus = OsGetSystemStatus();//获取系统的状态
    if ((systemStatus == OS_SYSTEM_NORMAL) || (systemStatus == OS_SYSTEM_EXC_OTHER_CPU)) {//当前CPU空闲或其他CPU运行时
        OutputControl(bBuf, len, type);//输出到终端
    } else if (systemStatus == OS_SYSTEM_EXC_CURR_CPU) {//当前CPU正在执行
        OutputControl(bBuf, len, EXC_OUTPUT);//串口以无锁的方式输出
    }
    OsVprintfFree(bBuf, bufLen);
}
///串口方式输入printf内容
VOID UartVprintf(const CHAR *fmt, va_list ap)
{
    OsVprintf(fmt, ap, UART_OUTPUT);
}
///__attribute__((noinline)) 意思是告诉编译器 这是非内联函数
__attribute__((noinline)) VOID UartPrintf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, UART_OUTPUT);
    va_end(ap);
}
///可变参数,输出到控制台
__attribute__ ((noinline)) VOID dprintf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
#ifdef LOSCFG_SAVE_EXCINFO
    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) {
        WriteExcBufVa(fmt, ap);
    }
#endif
    va_end(ap);
}
///LK 注者的理解是 log kernel(内核日志)
VOID LkDprintf(const CHAR *fmt, va_list ap)
{
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
#ifdef LOSCFG_SAVE_EXCINFO
    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) {
        WriteExcBufVa(fmt, ap);
    }
#endif
}

#ifdef LOSCFG_SHELL_DMESG
VOID DmesgPrintf(const CHAR *fmt, va_list ap)
{
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
}
#endif

#ifdef LOSCFG_PLATFORM_UART_WITHOUT_VFS
__attribute__ ((noinline)) INT32 printf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, UART_OUTPUT);
    va_end(ap);
    return 0;
}
#endif
//系统日志的输出
__attribute__((noinline)) VOID syslog(INT32 level, const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
    va_end(ap);
    (VOID)level;
}
///异常信息的输出
__attribute__((noinline)) VOID ExcPrintf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    /* uart output without print-spinlock */
    OsVprintf(fmt, ap, EXC_OUTPUT);
    va_end(ap);
}
///打印异常信息
VOID PrintExcInfo(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    /* uart output without print-spinlock */
    OsVprintf(fmt, ap, EXC_OUTPUT);//异常信息打印主体函数
#ifdef LOSCFG_SAVE_EXCINFO
    WriteExcBufVa(fmt, ap);
#endif
    va_end(ap);
}

#ifndef LOSCFG_SHELL_LK //log kernel 内核日志
VOID LOS_LkPrint(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, ...)
{
    va_list ap;

    if (level > PRINT_LEVEL) {
        return;
    }

    if ((level != LOS_COMMON_LEVEL) && ((level > LOS_EMG_LEVEL) && (level <= LOS_TRACE_LEVEL))) {
        dprintf("[%s][%s:%s]", g_logString[level],
                ((OsCurrProcessGet() == NULL) ? "NULL" : OsCurrProcessGet()->processName),
                ((OsCurrTaskGet() == NULL) ? "NULL" : OsCurrTaskGet()->taskName));
    }

    va_start(ap, fmt);
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);//控制台打印
#ifdef LOSCFG_SAVE_EXCINFO
    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) {
        WriteExcBufVa(fmt, ap);
    }
#endif
    va_end(ap);
}
#endif

