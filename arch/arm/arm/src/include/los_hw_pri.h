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

#ifndef _LOS_HW_PRI_H
#define _LOS_HW_PRI_H

#include "los_base.h"
#include "los_hw.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#if defined(LOSCFG_ARCH_FPU_VFP_D16)
#define FP_REGS_NUM     16
#elif defined (LOSCFG_ARCH_FPU_VFP_D32)
#define FP_REGS_NUM     32
#endif
#define GEN_REGS_NUM    13

/* The size of this structure must be smaller than or equal to the size specified by OS_TSK_STACK_ALIGN (16 bytes). */
typedef struct { //参考OsTaskSchedule来理解
#if !defined(LOSCFG_ARCH_FPU_DISABLE) //支持浮点运算
    UINT64 D[FP_REGS_NUM]; /* D0-D31 */
    UINT32 regFPSCR;       /* FPSCR */
    UINT32 regFPEXC;       /* FPEXC */
#endif
    UINT32 R4;
    UINT32 R5;
    UINT32 R6;
    UINT32 R7;
    UINT32 R8;
    UINT32 R9;
    UINT32 R10;
    UINT32 R11;

    /* It has the same structure as IrqContext */
    UINT32 reserved2; /**< Multiplexing registers, used in interrupts and system calls but with different meanings */
    UINT32 reserved1; /**< Multiplexing registers, used in interrupts and system calls but with different meanings */
    UINT32 USP;       /**< User mode sp register */
    UINT32 ULR;       /**< User mode lr register */
    UINT32 R0;
    UINT32 R1;
    UINT32 R2;
    UINT32 R3;
    UINT32 R12;
    UINT32 LR;
    UINT32 PC;
    UINT32 regCPSR;
} TaskContext;

typedef struct {//任务中断上下文
    UINT32 reserved2; /**< Multiplexing registers, used in interrupts and system calls but with different meanings */
    UINT32 reserved1; /**< Multiplexing registers, used in interrupts and system calls but with different meanings */
    UINT32 USP;       /**< User mode sp register */
    UINT32 ULR;       /**< User mode lr register */
    UINT32 R0;
    UINT32 R1;
    UINT32 R2;
    UINT32 R3;
    UINT32 R12;
    UINT32 LR;
    UINT32 PC;
    UINT32 regCPSR;
} IrqContext;

/*
 * Description : task stack initialization
 * Input       : taskID    -- task ID
 *               stackSize -- task stack size
 *               topStack  -- stack top of task (low address)
 * Return      : pointer to the task context
 */
extern VOID *OsTaskStackInit(UINT32 taskID, UINT32 stackSize, VOID *topStack, BOOL initFlag);
extern VOID OsUserCloneParentStack(VOID *childStack, UINTPTR parentTopOfStask, UINT32 parentStackSize);
extern VOID OsUserTaskStackInit(TaskContext *context, UINTPTR taskEntry, UINTPTR stack);
extern VOID OsInitSignalContext(VOID *sp, VOID *signalContext, UINTPTR sigHandler, UINT32 signo, UINT32 param);
extern void arm_clean_cache_range(UINTPTR start, UINTPTR end);
extern void arm_inv_cache_range(UINTPTR start, UINTPTR end);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HW_PRI_H */
