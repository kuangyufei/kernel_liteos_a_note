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
 * @brief 异常发生时的寄存器上下文结构
 *
 * @par 描述
 * 当LPC2458平台发生异常时，用于存储CPU寄存器状态的结构。根据ARM架构位数不同，包含的寄存器组有所差异。
 *
 * @attention
 * - AArch64架构下包含X0-X29通用寄存器及ELR、SPSR等系统寄存器
 * - 32位架构下寄存器组与TaskContext保持一致，便于任务切换时的上下文保存
 * @par 相关模块
 * @li los_exc 异常处理模块
 */
#ifdef LOSCFG_ARCH_ARM_AARCH64
/**
 * @brief AArch64架构通用寄存器数量
 * @note 对应X0-X29共30个通用寄存器
 */
#define EXC_GEN_REGS_NUM     30
/**
 * @struct ExcContext
 * @brief AArch64架构异常上下文结构
 */
typedef struct {
    UINT64 X[EXC_GEN_REGS_NUM]; /**< 通用寄存器X0-X29，数组索引对应寄存器编号 */
    UINT64 LR;                  /**< 链接寄存器X30，存储程序返回地址 */
    UINT64 SP;                  /**< 栈指针寄存器 */
    UINT64 regELR;              /**< 异常链接寄存器，存储异常发生前的PC值 */
    UINT64 SPSR;                /**< 保存程序状态寄存器，记录异常发生时的处理器状态 */
} ExcContext;
#else
/**
 * @struct ExcContext
 * @brief 32位ARM架构异常上下文结构
 * @note 与TaskContext结构布局完全一致，便于任务切换时的上下文复用
 */
typedef struct {
    UINT32 R4;                  /**< 通用寄存器R4 */
    UINT32 R5;                  /**< 通用寄存器R5 */
    UINT32 R6;                  /**< 通用寄存器R6 */
    UINT32 R7;                  /**< 通用寄存器R7 */
    UINT32 R8;                  /**< 通用寄存器R8 */
    UINT32 R9;                  /**< 通用寄存器R9 */
    UINT32 R10;                 /**< 通用寄存器R10 */
    UINT32 R11;                 /**< 通用寄存器R11 (FP) */

    UINT32 SP;                  /**< SVC模式下的栈指针 */
    UINT32 reserved;            /**< 保留字段，用于结构对齐 */
    UINT32 USP;                 /**< 用户模式下的栈指针 */
    UINT32 ULR;                 /**< 用户模式下的链接寄存器 */
    UINT32 R0;                  /**< 通用寄存器R0，函数调用的第一个参数寄存器 */
    UINT32 R1;                  /**< 通用寄存器R1，函数调用的第二个参数寄存器 */
    UINT32 R2;                  /**< 通用寄存器R2，函数调用的第三个参数寄存器 */
    UINT32 R3;                  /**< 通用寄存器R3，函数调用的第四个参数寄存器 */
    UINT32 R12;                 /**< 通用寄存器R12 (IP)， intra-procedural call scratch register */
    UINT32 LR;                  /**< 链接寄存器，存储程序返回地址 */
    UINT32 PC;                  /**< 程序计数器，指向发生异常的指令地址 */
    UINT32 regCPSR;             /**< 当前程序状态寄存器，记录异常发生时的处理器状态 */
} ExcContext;
#endif

/**
 * @ingroup los_exc
 * @brief 异常信息结构
 *
 * @par 描述
 * 存储异常发生时的详细信息，包括异常类型、嵌套计数和硬件上下文指针。
 * 用于异常处理函数获取异常相关信息并进行诊断和恢复。
 *
 * @attention 该结构在异常处理的全过程中保持有效，包含完整的异常现场信息
 */
typedef struct {
    UINT16 phase;               /**< 异常发生的阶段
                                  *   - 0: 系统启动阶段
                                  *   - 1: 任务调度阶段
                                  *   - 2: 中断处理阶段 */
    UINT16 type;                /**< 异常类型编码
                                  *   - 0x00: 未定义指令异常
                                  *   - 0x01: 软件中断异常
                                  *   - 0x02: 预取指令中止异常
                                  *   - 0x03: 数据访问中止异常
                                  *   - 0x04: IRQ中断异常
                                  *   - 0x05: FIQ中断异常 */
    UINT16 nestCnt;             /**< 异常嵌套计数器，最大支持65535层嵌套 */
    UINT16 reserved;            /**< 保留字段，用于16位对齐 */
    ExcContext *context;        /**< 指向异常发生时的硬件上下文结构
                                  *   @see ExcContext */
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
STATIC INLINE UINTPTR Get_Fp(VOID)
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
typedef VOID (*EXC_PROC_FUNC)(UINT32, ExcContext *, UINT32, UINT32);

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
extern UINT32 LOS_ExcRegHook(EXC_PROC_FUNC excHook);

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
