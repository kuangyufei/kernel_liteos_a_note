/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _LOS_HW_PRI_H
#define _LOS_HW_PRI_H

#include "los_base.h"
#include "los_hw.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/* FPU寄存器数量配置宏 */
#if defined(LOSCFG_ARCH_FPU_VFP_D16)                          /* 条件编译：启用VFP D16浮点单元时 */
#define FP_REGS_NUM     16                                  /* FPU数据寄存器数量：16个(D0-D15) */
#elif defined (LOSCFG_ARCH_FPU_VFP_D32)                       /* 条件编译：启用VFP D32浮点单元时 */
#define FP_REGS_NUM     32                                  /* FPU数据寄存器数量：32个(D0-D31) */
#endif
#define GEN_REGS_NUM    13                                  /* 通用寄存器数量：13个(R4-R11,R12,LR,PC,CPSR) */

/**
 * @brief 任务上下文结构体
 * 用于保存和恢复任务切换时的CPU寄存器状态
 * @note 结构体大小必须小于或等于OS_TSK_STACK_ALIGN指定的大小(16字节)
 */
typedef struct {
#if !defined(LOSCFG_ARCH_FPU_DISABLE)                         /* 条件编译：未禁用FPU时 */
    UINT64 D[FP_REGS_NUM]; /* D0-D31 */                      /* FPU数据寄存器组，共FP_REGS_NUM个64位寄存器 */
    UINT32 regFPSCR;       /* FPSCR */                       /* FPU状态控制寄存器 */
    UINT32 regFPEXC;       /* FPEXC */                       /* FPU使能控制寄存器 */
#endif
    UINT32 R4;                                              /* 通用寄存器R4 */
    UINT32 R5;                                              /* 通用寄存器R5 */
    UINT32 R6;                                              /* 通用寄存器R6 */
    UINT32 R7;                                              /* 通用寄存器R7 */
    UINT32 R8;                                              /* 通用寄存器R8 */
    UINT32 R9;                                              /* 通用寄存器R9 */
    UINT32 R10;                                             /* 通用寄存器R10 */
    UINT32 R11;                                             /* 通用寄存器R11 */

    /* It has the same structure as IrqContext */
    UINT32 reserved2; /**< 复用寄存器，在中断和系统调用中使用但含义不同 */
    UINT32 reserved1; /**< 复用寄存器，在中断和系统调用中使用但含义不同 */
    UINT32 USP;       /**< 用户模式栈指针寄存器 */
    UINT32 ULR;       /**< 用户模式链接寄存器 */
    UINT32 R0;                                              /* 通用寄存器R0 */
    UINT32 R1;                                              /* 通用寄存器R1 */
    UINT32 R2;                                              /* 通用寄存器R2 */
    UINT32 R3;                                              /* 通用寄存器R3 */
    UINT32 R12;                                             /* 通用寄存器R12 */
    UINT32 LR;                                              /* 链接寄存器(异常模式) */
    UINT32 PC;                                              /* 程序计数器 */
    UINT32 regCPSR;                                         /* 当前程序状态寄存器 */
} TaskContext;                                               /* 任务上下文结构体类型 */

/**
 * @brief 中断上下文结构体
 * 用于保存和恢复中断发生时的CPU寄存器状态
 */
typedef struct {
    UINT32 reserved2; /**< 复用寄存器，在中断和系统调用中使用但含义不同 */
    UINT32 reserved1; /**< 复用寄存器，在中断和系统调用中使用但含义不同 */
    UINT32 USP;       /**< 用户模式栈指针寄存器 */
    UINT32 ULR;       /**< 用户模式链接寄存器 */
    UINT32 R0;                                              /* 通用寄存器R0 */
    UINT32 R1;                                              /* 通用寄存器R1 */
    UINT32 R2;                                              /* 通用寄存器R2 */
    UINT32 R3;                                              /* 通用寄存器R3 */
    UINT32 R12;                                             /* 通用寄存器R12 */
    UINT32 LR;                                              /* 链接寄存器(异常模式) */
    UINT32 PC;                                              /* 程序计数器 */
    UINT32 regCPSR;                                         /* 当前程序状态寄存器 */
} IrqContext;                                                /* 中断上下文结构体类型 */

/*
 * Description : task stack initialization
 * Input       : taskID    -- task ID
 *               stackSize -- task stack size
 *               topStack  -- stack top of task (low address)
 * Return      : pointer to the task context
 */
extern VOID *OsTaskStackInit(UINT32 taskID, UINT32 stackSize, VOID *topStack, BOOL initFlag);
extern VOID OsUserCloneParentStack(VOID *childStack, UINTPTR sp, UINTPTR parentTopOfStask, UINT32 parentStackSize);
extern VOID OsUserTaskStackInit(TaskContext *context, UINTPTR taskEntry, UINTPTR stack);
extern VOID OsInitSignalContext(const VOID *sp, VOID *signalContext, UINTPTR sigHandler, UINT32 signo, UINT32 param);
extern void arm_clean_cache_range(UINTPTR start, UINTPTR end);
extern void arm_inv_cache_range(UINTPTR start, UINTPTR end);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HW_PRI_H */
