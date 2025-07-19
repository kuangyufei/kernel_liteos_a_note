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
#include "los_sched_pri.h"
/**
 * @def SIZEBUF
 * @brief 默认输出缓冲区大小定义
 * @details 栈上分配的初始缓冲区大小，单位字节，当输出内容超过此大小时将自动扩容
 * @value 256
 * @attention 必须为2的幂次方，以便通过位运算实现高效扩容
 * @ingroup kernel_printf
 */
#define SIZEBUF  256

/**
 * @var g_logString
 * @brief 日志级别字符串映射表
 * @details 定义日志级别枚举值与字符串的对应关系，索引需与日志级别宏定义严格一致
 * @attention 新增日志级别时需同步扩展此数组，确保索引匹配
 * @ingroup kernel_printf
 */
const CHAR *g_logString[] = {
    "EMG",    // 0: 紧急错误级别
    "COMMON", // 1: 通用信息级别
    "ERR",    // 2: 错误级别
    "WARN",   // 3: 警告级别
    "INFO",   // 4: 信息级别
    "DEBUG",  // 5: 调试级别
    "TRACE"   // 6: 跟踪级别
};

/**
 * @brief 获取日志级别对应的字符串
 * @param[in] level 日志级别，范围需在[0, LOS_TRACE_LEVEL]
 * @return const CHAR* 日志级别字符串指针，非法级别返回NULL
 * @note 此函数无参数校验，调用前需确保level有效性
 * @ingroup kernel_printf
 */
const CHAR *OsLogLvGet(INT32 level)
{
    return g_logString[level];
}

/**
 * @brief 输出格式错误提示信息
 * @details 当vsnprintf_s调用失败时输出标准错误信息到UART
 * @note 始终使用UART_WITH_LOCK确保输出原子性
 * @ingroup kernel_printf
 * @internal
 */
STATIC VOID ErrorMsg(VOID)
{
    const CHAR *p = "Output illegal string! vsnprintf_s failed!\n";  // 错误提示字符串
    UartPuts(p, (UINT32)strlen(p), UART_WITH_LOCK);  // 带锁输出到UART
}

/**
 * @brief UART输出控制函数
 * @param[in] str 待输出字符串指针
 * @param[in] len 输出长度，单位字节
 * @param[in] isLock 是否加锁，取值UART_WITH_LOCK/UART_WITHOUT_LOCK
 * @details 根据编译配置决定是否记录日志到dmesg缓冲区
 * @note 中断上下文调用时必须使用UART_WITHOUT_LOCK
 * @ingroup kernel_printf
 * @internal
 */
STATIC VOID UartOutput(const CHAR *str, UINT32 len, BOOL isLock)
{
#ifdef LOSCFG_SHELL_DMESG
    if (!OsCheckUartLock()) {  // 检查UART锁状态
        UartPuts(str, len, isLock);  // 输出到UART
    }
    if (isLock != UART_WITHOUT_LOCK) {  // 非无锁模式下记录日志
        (VOID)OsLogMemcpyRecord(str, len);  // 拷贝日志到dmesg缓冲区
    }
#else
    UartPuts(str, len, isLock);  // 直接输出到UART
#endif
}

#ifdef LOSCFG_PLATFORM_CONSOLE
/**
 * @brief 控制台输出实现
 * @param[in] str 待输出字符串指针
 * @param[in] len 输出长度，单位字节
 * @details 通过write系统调用输出到标准输出，支持中断环境下的部分写入处理
 * @note 仅当定义LOSCFG_PLATFORM_CONSOLE时编译此函数
 * @ingroup kernel_printf
 * @internal
 */
STATIC VOID ConsoleOutput(const CHAR *str, UINT32 len)
{
    ssize_t written = 0;       // 已写入字节数
    ssize_t cnt;               // 单次写入字节数
    ssize_t toWrite = len;     // 剩余待写入字节数

    for (;;) {  // 循环直到全部写入或出错
        cnt = write(STDOUT_FILENO, str + written, (size_t)toWrite);
        // 退出条件：写入出错/中断环境下写入0字节/全部写入完成
        if ((cnt < 0) || ((cnt == 0) && ((!OsPreemptable()) || (OS_INT_ACTIVE))) || (toWrite == cnt)) {
            break;
        }
        written += cnt;  // 更新已写入计数
        toWrite -= cnt;  // 更新剩余计数
    }
}
#endif

/**
 * @brief 输出控制分发函数
 * @param[in] str 待输出字符串指针
 * @param[in] len 输出长度，单位字节
 * @param[in] type 输出类型，取值OutputType枚举
 * @details 根据输出类型和编译配置选择合适的输出终端
 * @note CONSOLE_OUTPUT类型在未定义LOSCFG_PLATFORM_CONSOLE时自动降级为UART输出
 * @ingroup kernel_printf
 */
VOID OutputControl(const CHAR *str, UINT32 len, OutputType type)
{
    switch (type) {
        case CONSOLE_OUTPUT:
#ifdef LOSCFG_PLATFORM_CONSOLE
            if (ConsoleEnable() == TRUE) {  // 检查控制台是否使能
                ConsoleOutput(str, len);    // 输出到控制台
                break;
            }
#endif
            /* fall-through */  // 控制台未使能时降级到UART输出
        case UART_OUTPUT:
            UartOutput(str, len, UART_WITH_LOCK);  // 带锁输出到UART
            break;
        case EXC_OUTPUT:
            UartPuts(str, len, UART_WITH_LOCK);  // 异常场景下直接UART输出
            break;
        default:
            break;  // 忽略未知输出类型
    }
    return;
}

/**
 * @brief 输出缓冲区释放函数
 * @param[in] buf 缓冲区指针
 * @param[in] bufLen 缓冲区大小，单位字节
 * @details 根据缓冲区大小决定释放策略：栈缓冲区不释放，堆缓冲区需显式释放
 * @note 仅当bufLen不等于SIZEBUF时才释放，避免释放栈内存
 * @ingroup kernel_printf
 * @internal
 */
STATIC VOID OsVprintfFree(CHAR *buf, UINT32 bufLen)
{
    if (bufLen != SIZEBUF) {  // 仅释放动态分配的缓冲区
        (VOID)LOS_MemFree(m_aucSysMem0, buf);
    }
}

/**
 * @brief 格式化输出核心实现
 * @param[in] fmt 格式字符串
 * @param[in] ap 可变参数列表
 * @param[in] type 输出类型，取值OutputControl支持的类型
 * @details 实现安全的格式化输出，包含动态缓冲区管理、错误处理和系统状态适配
 * @attention 严禁在中断上下文直接调用，需使用带UART_WITHOUT_LOCK的接口
 * @ingroup kernel_printf
 */
VOID OsVprintf(const CHAR *fmt, va_list ap, OutputType type)
{
    INT32 len;                                 // vsnprintf_s返回值
    const CHAR *errMsgMalloc = "OsVprintf, malloc failed!\n";  // 内存分配失败提示
    const CHAR *errMsgLen = "OsVprintf, length overflow!\n";   // 缓冲区溢出提示
    CHAR aBuf[SIZEBUF] = {0};                  // 栈上初始缓冲区
    CHAR *bBuf = NULL;                         // 动态缓冲区指针
    UINT32 bufLen = SIZEBUF;                   // 当前缓冲区大小
    UINT32 systemStatus;                       // 系统状态

    bBuf = aBuf;  // 初始使用栈缓冲区
    // 安全格式化输出，保留1字节空间给\0
    len = vsnprintf_s(bBuf, bufLen, bufLen - 1, fmt, ap);
    // 检查参数合法性（格式化失败且缓冲区为空）
    if ((len == -1) && (*bBuf == '\0')) {
        /* parameter is illegal or some features in fmt dont support */
        ErrorMsg();
        return;
    }

    // 缓冲区不足时动态扩容（指数级增长）
    while (len == -1) {
        /* bBuf is not enough */
        OsVprintfFree(bBuf, bufLen);  // 释放旧缓冲区

        bufLen = bufLen << 1;  // 缓冲区大小翻倍（256->512->1024...）
        if ((INT32)bufLen <= 0) {  // 检查溢出（32位系统最大2^31-1）
            UartPuts(errMsgLen, (UINT32)strlen(errMsgLen), UART_WITH_LOCK);
            return;
        }
        // 从系统内存池分配新缓冲区
        bBuf = (CHAR *)LOS_MemAlloc(m_aucSysMem0, bufLen);
        if (bBuf == NULL) {  // 内存分配失败处理
            UartPuts(errMsgMalloc, (UINT32)strlen(errMsgMalloc), UART_WITH_LOCK);
            return;
        }
        // 重新尝试格式化输出
        len = vsnprintf_s(bBuf, bufLen, bufLen - 1, fmt, ap);
        if (*bBuf == '\0') {  // 参数非法导致格式化失败
            /* parameter is illegal or some features in fmt dont support */
            (VOID)LOS_MemFree(m_aucSysMem0, bBuf);
            ErrorMsg();
            return;
        }
    }
    *(bBuf + len) = '\0';  // 确保字符串以\0结尾

    systemStatus = OsGetSystemStatus();  // 获取当前系统状态
    // 根据系统状态选择合适的输出策略
    if ((systemStatus == OS_SYSTEM_NORMAL) || (systemStatus == OS_SYSTEM_EXC_OTHER_CPU)) {
        OutputControl(bBuf, len, type);  // 正常状态使用指定输出类型
    } else if (systemStatus == OS_SYSTEM_EXC_CURR_CPU) {
        OutputControl(bBuf, len, EXC_OUTPUT);  // 异常状态强制使用异常输出
    }
    OsVprintfFree(bBuf, bufLen);  // 释放缓冲区
}

/**
 * @brief UART格式化输出（可变参数版）
 * @param[in] fmt 格式字符串
 * @param[in] ap 可变参数列表
 * @details UART输出的封装接口，使用UART_OUTPUT类型
 * @see OsVprintf
 * @ingroup kernel_printf
 */
VOID UartVprintf(const CHAR *fmt, va_list ap)
{
    OsVprintf(fmt, ap, UART_OUTPUT);
}

/**
 * @brief UART格式化输出
 * @param[in] fmt 格式字符串
 * @param[in] ... 可变参数
 * @details 提供标准printf风格的UART输出接口
 * @note 使用noinline属性避免编译器优化导致的栈问题
 * @ingroup kernel_printf
 */
__attribute__((noinline)) VOID UartPrintf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, UART_OUTPUT);
    va_end(ap);
}

#ifndef LOSCFG_LIBC_NEWLIB
/**
 * @brief 控制台格式化输出
 * @param[in] fmt 格式字符串
 * @param[in] ... 可变参数
 * @details 当未使用newlib时提供dprintf实现，输出到控制台
 * @note 异常状态下会将输出同时记录到异常缓冲区
 * @ingroup kernel_printf
 */
__attribute__((noinline)) VOID dprintf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
#ifdef LOSCFG_SAVE_EXCINFO
    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) {
        WriteExcBufVa(fmt, ap);  // 异常时保存日志到异常缓冲区
    }
#endif
    va_end(ap);
}
#endif

/**
 * @brief LK控制台格式化输出（可变参数版）
 * @param[in] fmt 格式字符串
 * @param[in] ap 可变参数列表
 * @details LK子系统专用输出接口，支持异常日志记录
 * @ingroup kernel_printf
 */
VOID LkDprintf(const CHAR *fmt, va_list ap)
{
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
#ifdef LOSCFG_SAVE_EXCINFO
    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) {
        WriteExcBufVa(fmt, ap);  // 异常状态下记录日志
    }
#endif
}

#ifdef LOSCFG_SHELL_DMESG
/**
 * @brief dmesg日志格式化输出（可变参数版）
 * @param[in] fmt 格式字符串
 * @param[in] ap 可变参数列表
 * @details 仅当启用dmesg功能时编译，用于内核日志记录
 * @ingroup kernel_printf
 */
VOID DmesgPrintf(const CHAR *fmt, va_list ap)
{
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
}
#endif

#ifdef LOSCFG_PLATFORM_UART_WITHOUT_VFS
/**
 * @brief 标准printf实现（UART版）
 * @param[in] fmt 格式字符串
 * @param[in] ... 可变参数
 * @details 当不使用VFS时提供标准printf接口，直接输出到UART
 * @return INT32 始终返回0（兼容标准接口）
 * @note 仅在LOSCFG_PLATFORM_UART_WITHOUT_VFS定义时可用
 * @ingroup kernel_printf
 */
__attribute__((noinline)) INT32 printf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, UART_OUTPUT);
    va_end(ap);
    return 0;
}
#endif

/**
 * @brief 系统日志接口
 * @param[in] level log级别（当前未使用）
 * @param[in] fmt 格式字符串
 * @param[in] ... 可变参数
 * @details 兼容POSIX的syslog接口，当前忽略level参数
 * @note level参数预留用于未来日志级别控制
 * @ingroup kernel_printf
 */
__attribute__((noinline)) VOID syslog(INT32 level, const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
    va_end(ap);
    (VOID)level;  // 未使用参数，避免编译警告
}

/**
 * @brief 异常场景格式化输出
 * @param[in] fmt 格式字符串
 * @param[in] ... 可变参数
 * @details 用于异常处理场景的输出，不使用打印锁
 * @attention 仅在异常处理路径调用，如中断或panic处理
 * @ingroup kernel_printf
 */
__attribute__((noinline)) VOID ExcPrintf(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    /* uart output without print-spinlock */
    OsVprintf(fmt, ap, EXC_OUTPUT);  // 使用EXC_OUTPUT类型（无锁输出）
    va_end(ap);
}

/**
 * @brief 异常信息记录与输出
 * @param[in] fmt 格式字符串
 * @param[in] ... 可变参数
 * @details 异常时输出信息并记录到异常缓冲区，用于故障诊断
 * @note 同时支持UART输出和异常缓冲区记录
 * @ingroup kernel_printf
 */
VOID PrintExcInfo(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    /* uart output without print-spinlock */
    OsVprintf(fmt, ap, EXC_OUTPUT);
#ifdef LOSCFG_SAVE_EXCINFO
    WriteExcBufVa(fmt, ap);  // 保存日志到异常缓冲区
#endif
    va_end(ap);
}

#ifndef LOSCFG_SHELL_LK
/**
 * @brief LK日志输出接口
 * @param[in] level 日志级别（需<=PRINT_LEVEL）
 * @param[in] func 函数名
 * @param[in] line 行号
 * @param[in] fmt 格式字符串
 * @param[in] ... 可变参数
 * @details 带日志级别过滤和上下文信息的输出接口
 * @note 当level > PRINT_LEVEL时日志被过滤
 * @ingroup kernel_printf
 */
VOID LOS_LkPrint(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, ...)
{
    va_list ap;

    if (level > PRINT_LEVEL) {  // 日志级别过滤
        return;
    }

    // 非普通级别日志添加上下文信息
    if ((level != LOS_COMMON_LEVEL) && ((level > LOS_EMG_LEVEL) && (level <= LOS_TRACE_LEVEL))) {
        PRINTK("[%s][%s:%s]", g_logString[level],
                ((OsCurrProcessGet() == NULL) ? "NULL" : OsCurrProcessGet()->processName),
                ((OsCurrTaskGet() == NULL) ? "NULL" : OsCurrTaskGet()->taskName));
    }

    va_start(ap, fmt);
    OsVprintf(fmt, ap, CONSOLE_OUTPUT);
#ifdef LOSCFG_SAVE_EXCINFO
    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) {
        WriteExcBufVa(fmt, ap);  // 异常状态下记录日志
    }
#endif
    va_end(ap);
}
#endif

