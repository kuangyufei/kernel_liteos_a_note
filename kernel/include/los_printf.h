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

/**
 * @defgroup los_printf Printf
 * @ingroup kernel
 */

#ifndef _LOS_PRINTF_H
#define _LOS_PRINTF_H

#ifdef LOSCFG_LIB_LIBC
#include "stdarg.h"
#endif
#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

//见于\third_party\musl\kernel\include\*.h
extern VOID LOS_LkPrint(INT32 level, const CHAR *func, INT32 line, const CHAR *fmt, ...);

#define LOS_EMG_LEVEL    0						//用于紧急事件消息，它们一般是系统崩溃之前提示的消息

#define LOS_COMMON_LEVEL (LOS_EMG_LEVEL + 1)	//日志等级:普通
	
#define LOS_ERR_LEVEL    (LOS_COMMON_LEVEL + 1)	//日志等级:错误类，用于报告错误状态，设备驱动程序会经常使用它来报告来自硬件的问题

#define LOS_WARN_LEVEL   (LOS_ERR_LEVEL + 1)	//日志等级:警告类，对可能出现问题的情况进行警告，但这类错误通常不会对系统造成严重的问题

#define LOS_INFO_LEVEL   (LOS_WARN_LEVEL + 1)	//日志等级:信息类，提示性信息，很多驱动程序在启动时用这个级别来打印硬件信息

#define LOS_DEBUG_LEVEL  (LOS_INFO_LEVEL + 1)	//日志等级:调试类，用于调试信息

#define LOS_TRACE_LEVEL  (LOS_DEBUG_LEVEL + 1)	//日志等级:跟踪类

#define PRINT_LEVEL      LOS_ERR_LEVEL			//默认打印错误类

typedef VOID (*pf_OUTPUT)(const CHAR *fmt, ...);

/**
 * @ingroup los_printf
 * @brief Format and print data.
 *
 * @par Description:
 * Print argument(s) according to fmt.
 *
 * @attention
 * <ul>
 * <li>None</li>
 * </ul>
 *
 * @param fmt [IN] Type char* controls the ouput as in C printf.
 *
 * @retval None
 * @par Dependency:
 * <ul><li>los_printf.h: the header file that contains the API declaration.</li></ul>
 * @see printf
 */
#ifndef LOSCFG_LIBC_NEWLIB
extern void dprintf(const char *fmt, ...);
#endif

#define PRINT_DEBUG(fmt, args...)    LOS_LkPrint(LOS_DEBUG_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)
#define PRINT_INFO(fmt, args...)     LOS_LkPrint(LOS_INFO_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)
#define PRINT_WARN(fmt, args...)     LOS_LkPrint(LOS_WARN_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)
#define PRINT_ERR(fmt, args...)      LOS_LkPrint(LOS_ERR_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)
#define PRINTK(fmt, args...)         LOS_LkPrint(LOS_COMMON_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)
#define PRINT_EMG(fmt, args...)      LOS_LkPrint(LOS_EMG_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)
#define PRINT_RELEASE(fmt, args...)  LOS_LkPrint(LOS_COMMON_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)
#define PRINT_TRACE(fmt, args...)    LOS_LkPrint(LOS_TRACE_LEVEL, __FUNCTION__, __LINE__, fmt, ##args)

typedef enum { //输出类型
    NO_OUTPUT = 0,		
    UART_OUTPUT = 1,	//串口输出
    CONSOLE_OUTPUT = 2,	//控制台输出
    EXC_OUTPUT = 3		//出现异常时的输出
} OutputType;

extern VOID OsVprintf(const CHAR *fmt, va_list ap, OutputType type);

#define UART_WITHOUT_LOCK 0
#define UART_WITH_LOCK    1
extern VOID UartPuts(const CHAR *s, UINT32 len, BOOL isLock);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_PRINTF_H */
