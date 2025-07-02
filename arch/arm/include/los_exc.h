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
 * @defgroup los_exc Exception handling
 * @ingroup kernel
 */
#ifndef _LOS_EXC_H
#define _LOS_EXC_H

#include "los_typedef.h"
#include "arch_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_exc
 * Register information structure
 *
 * Description: register information stored when an exception occurs on an LPC2458 platform.
 *
 * Note: The following register names without uw are the register names used in the chip manual.
 */ //在LPC2458平台上发生异常时存储的寄存器信息 以下不带uw的寄存器名是芯片手册中使用的寄存器名
#ifdef LOSCFG_ARCH_ARM_AARCH64
#define EXC_GEN_REGS_NUM     30
typedef struct {
    UINT64 X[EXC_GEN_REGS_NUM]; /**< Register X0-X29 */
    UINT64 LR;                  /**< Program returning address. X30 */
    UINT64 SP;
    UINT64 regELR;
    UINT64 SPSR;
} ExcContext;
#else
/* It has the same structure as TaskContext */
typedef struct { //异常上下文,任务被中断需切换上下文,就是一种异常
    UINT32 R4;      /**< Register R4 */
    UINT32 R5;      /**< Register R5 */
    UINT32 R6;      /**< Register R6 */
    UINT32 R7;      /**< Register R7 */
    UINT32 R8;      /**< Register R8 */
    UINT32 R9;      /**< Register R9 */
    UINT32 R10;     /**< Register R10 */
    UINT32 R11;     /**< Register R11 */

    UINT32 SP;      /**< svc sp */	//内核态栈指针
    UINT32 reserved; /**< Reserved, multiplexing register */
    UINT32 USP;
    UINT32 ULR;
    UINT32 R0;      /**< Register R0 */
    UINT32 R1;      /**< Register R1 */
    UINT32 R2;      /**< Register R2 */
    UINT32 R3;      /**< Register R3 */
    UINT32 R12;     /**< Register R12 */
    UINT32 LR;      /**< Program returning address. */	//用户态下程序返回地址
    UINT32 PC;      /**< PC pointer of the exceptional function */ //异常函数的程序计数器PC位置
    UINT32 regCPSR;
} ExcContext;
#endif

/**
 * @ingroup los_exc
 * Exception information structure
 *
 * Description: exception information stored when an exception occurs on an LPC2458 platform.
 *
 */
typedef struct {//异常信息结构体
    UINT16 phase;        /**< Phase in which an exception occurs *///异常发生的阶段
    UINT16 type;         /**< Exception type *///异常类型
    UINT16 nestCnt;      /**< Count of nested exception *///嵌套异常计数
    UINT16 reserved;     /**< Reserved for alignment */ //为对齐而保留
    ExcContext *context; /**< Hardware context when an exception occurs *///异常发生时的硬件上下文
} ExcInfo;

/**
 * @ingroup los_exc
 * @brief Kernel FP Register address obtain function.
 *
 * @par Description:
 * The API is used to obtain the FP Register address.
 * @attention None.
 *
 * @param  None.
 *
 * @retval #UINTPTR The FP Register address.
 *
 * @par Dependency:
 * los_exc.h: the header file that contains the API declaration.
 * @see None.
 */
STATIC INLINE UINTPTR Get_Fp(VOID)//获取内核FP寄存器地址
{
    UINTPTR regFp;

#ifdef LOSCFG_ARCH_ARM_AARCH64
    __asm__ __volatile__("mov %0, X29" : "=r"(regFp));
#else
    __asm__ __volatile__("mov %0, fp" : "=r"(regFp));
#endif

    return regFp;
}

/**
 * @ingroup los_exc
 * @brief Define an exception handling function hook.
 *
 * @par Description:
 * This API is used to define the exception handling function hook based on the type of
 * the exception handling function and record exceptions.
 * @attention None.
 *
 * @param None.
 *
 * @retval None.
 *
 * @par Dependency:
 * los_exc.h: the header file that contains the API declaration.
 * @see None.
 */
typedef VOID (*EXC_PROC_FUNC)(UINT32, ExcContext *, UINT32, UINT32);//定义异常处理函数钩子
//此API用于根据异常处理函数的类型定义异常处理函数钩子并记录异常
/**
 * @ingroup los_exc
 * @brief Register an exception handling hook.
 *
 * @par Description:
 * This API is used to register an exception handling hook.
 * @attention If the hook is registered for multiple times, the hook registered at the last time is effective.
 * @attention The hook can be registered as NULL, indicating that the hook registration is canceled.
 * @param excHook [IN] Type #EXC_PROC_FUNC: hook function.
 *
 * @retval #LOS_OK                      The exception handling hook is successfully registered.
 *
 * @par Dependency:
 * los_exc.h: the header file that contains the API declaration.
 * @see None.
 */
extern UINT32 LOS_ExcRegHook(EXC_PROC_FUNC excHook);//注册异常处理钩子

/**
 * @ingroup los_exc
 * @brief Kernel panic function.
 *
 * @par Description:
 * Stack function that prints kernel panics.
 * @attention After this function is called and stack information is printed, the system will fail to respond.
 * @attention The input parameter can be NULL.
 * @param fmt [IN] Type #CHAR* : variadic argument.
 *
 * @retval #None.
 *
 * @par Dependency:
 * los_exc.h: the header file that contains the API declaration.
 * @see None.
 */
NORETURN VOID LOS_Panic(const CHAR *fmt, ...);

/**
 * @ingroup los_exc
 * @brief record LR function.
 *
 * @par Description:
 * @attention
 * @param LR            [IN] Type #UINTPTR * LR buffer.
 * @param recordCount   [IN] Type UINT32 record LR lay number.
 * @param jumpCount     [IN] Type UINT32 ignore LR lay number.
 *
 * @retval #None.
 *
 * @par Dependency:
 * los_exc.h: the header file that contains the API declaration.
 * @see None.
 */
VOID LOS_RecordLR(UINTPTR *LR, UINT32 LRSize, UINT32 recordCount, UINT32 jumpCount);

/**
 * @ingroup los_exc
 * @brief Kernel backtrace function.
 *
 * @par Description:
 * Backtrace function that prints task call stack information traced from the running task.
 * @attention None.
 *
 * @param None.
 *
 * @retval #None.
 *
 * @par Dependency:
 * los_exc.h: the header file that contains the API declaration.
 * @see None.
 */
extern VOID OsBackTrace(VOID);

/**
 * @ingroup los_exc
 * @brief Kernel task backtrace function.
 *
 * @par Description:
 * Backtrace function that prints task call stack information traced from the input task.
 * @attention
 * <ul>
 * <li>The input taskID should be valid.</li>
 * </ul>
 *
 * @param  taskID [IN] Type #UINT32 Task ID.
 *
 * @retval #None.
 *
 * @par Dependency:
 * los_exc.h: the header file that contains the API declaration.
 * @see None.
 */
extern VOID OsTaskBackTrace(UINT32 taskID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_EXC_H */
