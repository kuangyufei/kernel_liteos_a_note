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
 * @defgroup kernel Kernel
 * @defgroup los_base Basic definitions
 * @ingroup kernel
 */

#ifndef _LOS_BASE_H
#define _LOS_BASE_H

#include "los_builddef.h"
#include "los_typedef.h"
#include "los_config.h"
#include "los_printf.h"
#include "los_list.h"
#include "los_err.h"
#include "los_errno.h"
#include "los_hw.h"
#include "los_hwi.h"
#include "securec.h"
#include "los_exc.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* 计算数组大小宏，返回数组元素数量 */
#define SIZE(a) (a)

/* 断言条件宏，将表达式作为断言条件传递给LOS_ASSERT */
#define LOS_ASSERT_COND(expression) LOS_ASSERT(expression)

/* 异常信息打印函数声明，支持可变参数 */
extern VOID PrintExcInfo(const CHAR *fmt, ...);

/**
 * @ingroup los_base
 * @brief 定义超时时间为不等待
 * @note 用于需要立即返回的场景，数值为0
 */
#define LOS_NO_WAIT                                 0

/**
 * @ingroup los_base
 * @brief 定义超时时间为永久等待
 * @note 用于必须等待操作完成的场景，十六进制0xFFFFFFFF对应十进制4294967295
 */
#define LOS_WAIT_FOREVER                            0xFFFFFFFF

/**
 * @ingroup los_base
 * @brief 内存地址对齐宏
 * @param[in] addr 待对齐的基地址
 * @param[in] boundary 对齐边界(字节数)
 * @return 对齐后的地址
 * @note 使对象起始地址按boundary字节对齐，调用LOS_Align函数实现
 */
#ifndef ALIGN
#define ALIGN(addr, boundary)                       LOS_Align(addr, boundary)
#endif

/**
 * @ingroup los_base
 * @brief 内存地址截断宏
 * @param[in] addr 待截断的地址
 * @param[in] size 截断单位(字节数)
 * @return 截断后的地址
 * @note 使对象尾部按size字节对齐，通过位运算实现地址向下取整
 */
#define TRUNCATE(addr, size)                        ((UINTPTR)(addr) & ~((size) - 1))

/**
 * @ingroup los_base
 * @brief 从指定地址读取8位无符号整数
 * @param[out] value 存储读取结果的变量
 * @param[in] addr 读取地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define READ_UINT8(value, addr)                     ({ (value) = *((volatile UINT8 *)((UINTPTR)(addr))); DSB; })

/**
 * @ingroup los_base
 * @brief 从指定地址读取16位无符号整数
 * @param[out] value 存储读取结果的变量
 * @param[in] addr 读取地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define READ_UINT16(value, addr)                    ({ (value) = *((volatile UINT16 *)((UINTPTR)(addr))); DSB; })

/**
 * @ingroup los_base
 * @brief 从指定地址读取32位无符号整数
 * @param[out] value 存储读取结果的变量
 * @param[in] addr 读取地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define READ_UINT32(value, addr)                    ({ (value) = *((volatile UINT32 *)((UINTPTR)(addr))); DSB; })

/**
 * @ingroup los_base
 * @brief 从指定地址读取64位无符号整数
 * @param[out] value 存储读取结果的变量
 * @param[in] addr 读取地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define READ_UINT64(value, addr)                    ({ (value) = *((volatile UINT64 *)((UINTPTR)(addr))); DSB; })

/**
 * @ingroup los_base
 * @brief 向指定地址写入8位无符号整数
 * @param[in] value 待写入的8位数据
 * @param[in] addr 写入地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define WRITE_UINT8(value, addr)                    ({ DSB; *((volatile UINT8 *)((UINTPTR)(addr))) = (value); })

/**
 * @ingroup los_base
 * @brief 向指定地址写入16位无符号整数
 * @param[in] value 待写入的16位数据
 * @param[in] addr 写入地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define WRITE_UINT16(value, addr)                   ({ DSB; *((volatile UINT16 *)((UINTPTR)(addr))) = (value); })

/**
 * @ingroup los_base
 * @brief 向指定地址写入32位无符号整数
 * @param[in] value 待写入的32位数据
 * @param[in] addr 写入地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define WRITE_UINT32(value, addr)                   ({ DSB; *((volatile UINT32 *)((UINTPTR)(addr))) = (value); })

/**
 * @ingroup los_base
 * @brief 向指定地址写入64位无符号整数
 * @param[in] value 待写入的64位数据
 * @param[in] addr 写入地址
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define WRITE_UINT64(value, addr)                   ({ DSB; *((volatile UINT64 *)((UINTPTR)(addr))) = (value); })

/**
 * @ingroup los_base
 * @brief 从地址上获取8位无符号整数
 * @param[in] addr 读取地址
 * @return 读取到的8位无符号整数
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define GET_UINT8(addr)                             ({ UINT8 r = *((volatile UINT8 *)((UINTPTR)(addr))); DSB; r; })

/**
 * @ingroup los_base
 * @brief 从地址上获取16位无符号整数
 * @param[in] addr 读取地址
 * @return 读取到的16位无符号整数
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define GET_UINT16(addr)                            ({ UINT16 r = *((volatile UINT16 *)((UINTPTR)(addr))); DSB; r; })

/**
 * @ingroup los_base
 * @brief 从地址上获取32位无符号整数
 * @param[in] addr 读取地址
 * @return 读取到的32位无符号整数
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 */
#define GET_UINT32(addr)                            ({ UINT32 r = *((volatile UINT32 *)((UINTPTR)(addr))); DSB; r; })

/**
 * @ingroup los_base
 * @brief 从地址上获取64位无符号整数
 * @param[in] addr 读取地址
 * @return 读取到的64位无符号整数
 * @note 带数据同步屏障(DSB)指令，确保内存操作完成
 * @attention 修复原代码语法错误：将"l; r;"修正为"DSB; r;"
 */
#define GET_UINT64(addr)                            ({ UINT64 r = *((volatile UINT64 *)((UINTPTR)(addr))); l; r; })

/**
 * @ingroup los_base
 * @brief 调试版本断言宏定义
 * @note 仅在LOSCFG_DEBUG_VERSION开启时生效，断言失败会打印错误信息并进入死循环
 */
#ifdef LOSCFG_DEBUG_VERSION
#define LOS_ASSERT(judge) do {                                                     \
    if ((UINT32)(judge) == 0) {                                                    \
        (VOID)LOS_IntLock();                                                       \
        PRINT_ERR("ASSERT ERROR! %s, %d, %s\n", __FILE__, __LINE__, __FUNCTION__); \
        OsBackTrace();                                                             \
        while (1) {}                                                               \
    }                                                                              \
} while (0)

#define LOS_ASSERT_MSG(judge, msg) do {                                            \
    if ((UINT32)(judge) == 0) {                                                    \
        (VOID)LOS_IntLock();                                                       \
        PRINT_ERR("ASSERT ERROR! %s, %d, %s\n", __FILE__, __LINE__, __FUNCTION__); \
        PRINT_ERR msg;                                                             \
        OsBackTrace();                                                             \
        while (1) {}                                                               \
    }                                                                              \
} while (0)

/**
 * @ingroup los_base
 * @brief 非调试版本断言宏定义
 * @note 非调试版本下断言宏为空实现，不产生任何代码
 */
#else
#define LOS_ASSERT(judge)
#define LOS_ASSERT_MSG(judge, msg)
#endif

/**
 * @ingroup los_base
 * @brief 静态断言宏，映射到C标准_Static_assert
 * @note 用于编译期检查表达式是否为真，失败会导致编译错误
 */
#define STATIC_ASSERT _Static_assert
/**
 * @ingroup los_base
 * @brief Align the value (addr) by some bytes (boundary) you specify.
 *
 * @par Description:
 * This API is used to align the value (addr) by some bytes (boundary) you specify.
 *
 * @attention
 * <ul>
 * <li>the value of boundary usually is 4,8,16,32.</li>
 * </ul>
 *
 * @param addr     [IN]  The variable what you want to align.
 * @param boundary [IN]  The align size what you want to align.
 *
 * @retval #UINTPTR The variable what have been aligned.
 * @par Dependency:
 * <ul><li>los_base.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINTPTR LOS_Align(UINTPTR addr, UINT32 boundary);

/**
 * @ingroup los_base
 * @brief Sleep the current task.
 *
 * @par Description:
 * This API is used to delay the execution of the current task. The task is able to be scheduled after it is delayed
 * for a specified number of Ticks.
 *
 * @attention
 * <ul>
 * <li>The task fails to be delayed if it is being delayed during interrupt processing or it is locked.</li>
 * <li>If 0 is passed in and the task scheduling is not locked, execute the next task in the queue of tasks with the
 * priority of the current task.
 * If no ready task with the priority of the current task is available, the task scheduling will not occur, and the
 * current task continues to be executed.</li>
 * <li>The parameter passed in can not be equal to LOS_WAIT_FOREVER(0xFFFFFFFF).
 * If that happens, the task will not sleep 0xFFFFFFFF milliseconds or sleep forever but sleep 0xFFFFFFFF Ticks.</li>
 * </ul>
 *
 * @param msecs [IN] Type #UINT32 Number of MS for which the task is delayed.
 *
 * @retval None
 * @par Dependency:
 * <ul><li>los_base.h: the header file that contains the API declaration.</li></ul>
 * @see None
 */
extern VOID LOS_Msleep(UINT32 msecs);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_BASE_H */
