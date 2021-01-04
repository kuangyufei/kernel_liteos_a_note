/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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
 * @defgroup los_hw Hardware
 * @ingroup kernel
 */

#ifndef _LOS_HW_CPU_H
#define _LOS_HW_CPU_H

#include "los_typedef.h"
#include "los_toolchain.h"
#include "los_hw_arch.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* ARM System Registers */
#define DSB     __asm__ volatile("dsb" ::: "memory")
#define DMB     __asm__ volatile("dmb" ::: "memory")
#define ISB     __asm__ volatile("isb" ::: "memory")
#define BARRIER __asm__ volatile("":::"memory")

#define ARM_SYSREG_READ(REG)                    \
({                                              \
    UINT32 _val;                                \
    __asm__ volatile("mrc " REG : "=r" (_val)); \
    _val;                                       \
})

#define ARM_SYSREG_WRITE(REG, val)              \
({                                              \
    __asm__ volatile("mcr " REG :: "r" (val));  \
    ISB;                                        \
})

#define ARM_SYSREG64_READ(REG)                   \
({                                               \
    UINT64 _val;                                 \
    __asm__ volatile("mrrc " REG : "=r" (_val)); \
    _val;                                        \
})

#define ARM_SYSREG64_WRITE(REG, val)             \
({                                               \
    __asm__ volatile("mcrr " REG :: "r" (val));  \
    ISB;                                         \
})

#define CP14_REG(CRn, Op1, CRm, Op2)    "p14, "#Op1", %0, "#CRn","#CRm","#Op2
#define CP15_REG(CRn, Op1, CRm, Op2)    "p15, "#Op1", %0, "#CRn","#CRm","#Op2
#define CP15_REG64(CRn, Op1)            "p15, "#Op1", %0,    %H0,"#CRn
//CP15 协处理器各寄存器信息
/*
 * Identification registers (c0)	//c0 - 身份寄存器
 */
#define MIDR                CP15_REG(c0, 0, c0, 0)    /* Main ID Register */	//主ID寄存器
#define MPIDR               CP15_REG(c0, 0, c0, 5)    /* Multiprocessor Affinity Register *///多处理器关联寄存器给每个CPU制定一个逻辑地址
#define CCSIDR              CP15_REG(c0, 1, c0, 0)    /* Cache Size ID Registers */	//缓存大小ID寄存器
#define CLIDR               CP15_REG(c0, 1, c0, 1)    /* Cache Level ID Register */	//缓存登记ID寄存器
#define VPIDR               CP15_REG(c0, 4, c0, 0)    /* Virtualization Processor ID Register */	//虚拟化处理器ID寄存器
#define VMPIDR              CP15_REG(c0, 4, c0, 5)    /* Virtualization Multiprocessor ID Register */	//虚拟化多处理器ID寄存器

/*
 * System control registers (c1)	//c1 - 系统控制寄存器 各种控制位（可读写）
 */
#define SCTLR               CP15_REG(c1, 0, c0, 0)    /* System Control Register */	//系统控制寄存器
#define ACTLR               CP15_REG(c1, 0, c0, 1)    /* Auxiliary Control Register */	//辅助控制寄存器
#define CPACR               CP15_REG(c1, 0, c0, 2)    /* Coprocessor Access Control Register */	//协处理器访问控制寄存器

/*
 * Memory protection and control registers (c2 & c3) //c2 - 传说中的TTB寄存器，主要是给MMU使用 c3 - 域访问控制位
 */
#define TTBR0               CP15_REG(c2, 0, c0, 0)    /* Translation Table Base Register 0 */	//转换表基地址寄存器0
#define TTBR1               CP15_REG(c2, 0, c0, 1)    /* Translation Table Base Register 1 */	//转换表基地址寄存器1
#define TTBCR               CP15_REG(c2, 0, c0, 2)    /* Translation Table Base Control Register */	////转换表基地址控制寄存器
#define DACR                CP15_REG(c3, 0, c0, 0)    /* Domain Access Control Register */	//域访问控制寄存器

/*
 * Memory system fault registers (c5 & c6)	//c5 - 内存失效状态 c6 - 内存失效地址
 */
#define DFSR                CP15_REG(c5, 0, c0, 0)    /* Data Fault Status Register */			//数据故障状态寄存器
#define IFSR                CP15_REG(c5, 0, c0, 1)    /* Instruction Fault Status Register */	//指令故障状态寄存器
#define DFAR                CP15_REG(c6, 0, c0, 0)    /* Data Fault Address Register */			//数据故障地址寄存器
#define IFAR                CP15_REG(c6, 0, c0, 2)    /* Instruction Fault Address Register */	//指令错误地址寄存器

/*
 * Process, context and thread ID registers (c13) //c13 - 进程标识符
 */
#define FCSEIDR             CP15_REG(c13, 0, c0, 0)    /* FCSE Process ID Register */	//FCSE（Fast Context Switch Extension，快速上下文切换）进程ID寄存器 位于CPU和MMU之间
#define CONTEXTIDR          CP15_REG(c13, 0, c0, 1)    /* Context ID Register */	//上下文ID寄存器
#define TPIDRURW            CP15_REG(c13, 0, c0, 2)    /* User Read/Write Thread ID Register */	//用户读/写线程ID寄存器
#define TPIDRURO            CP15_REG(c13, 0, c0, 3)    /* User Read-Only Thread ID Register */	//用户只读写线程ID寄存器
#define TPIDRPRW            CP15_REG(c13, 0, c0, 4)    /* PL1 only Thread ID Register */	//仅PL1线程ID寄存器

#define MPIDR_CPUID_MASK    (0xffU)
//获取当前task
STATIC INLINE VOID *ArchCurrTaskGet(VOID)
{
    return (VOID *)(UINTPTR)ARM_SYSREG_READ(TPIDRPRW);//读c13寄存器
}

STATIC INLINE VOID ArchCurrTaskSet(VOID *val)
{
    ARM_SYSREG_WRITE(TPIDRPRW, (UINT32)(UINTPTR)val);
}

STATIC INLINE VOID ArchCurrUserTaskSet(UINTPTR val)
{
    ARM_SYSREG_WRITE(TPIDRURO, (UINT32)val);
}
//https://www.keil.com/pack/doc/cmsis/Core_A/html/group__CMSIS__MPIDR.html
STATIC INLINE UINT32 ArchCurrCpuid(VOID)
{
#if (LOSCFG_KERNEL_SMP == YES)//CPU多核情况
    return ARM_SYSREG_READ(MPIDR) & MPIDR_CPUID_MASK;
#else//ARM架构通过MPIDR(Multiprocessor Affinity Register)寄存器给每个CPU指定一个逻辑地址。
    return 0;
#endif
}
//获取CPU硬件ID,每个CPU都有自己的唯一标识
STATIC INLINE UINT64 OsHwIDGet(VOID)
{
    return ARM_SYSREG_READ(MPIDR);
}
//获取CPU型号,包含CPU各种信息,例如:[15:4]表示 arm 7或arm 9
STATIC INLINE UINT32 OsMainIDGet(VOID)
{
    return ARM_SYSREG_READ(MIDR);
}

/* CPU interrupt mask handle implementation */ //CPU中断掩码句柄实现
#if LOSCFG_ARM_ARCH >= 6

STATIC INLINE UINT32 ArchIntLock(VOID)
{
    UINT32 intSave;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"
        "cpsid  if              "
        : "=r"(intSave)
        :
        : "memory");
    return intSave;
}

STATIC INLINE UINT32 ArchIntUnlock(VOID)
{
    UINT32 intSave;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"
        "cpsie  if              "
        : "=r"(intSave)
        :
        : "memory");
    return intSave;
}

#else

STATIC INLINE UINT32 ArchIntLock(VOID)
{
    UINT32 intSave, temp;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"
        "orr    %1, %0, #0xc0 \n"
        "msr    cpsr_c, %1      "
        :"=r"(intSave),  "=r"(temp)
        : :"memory");
    return intSave;
}

STATIC INLINE UINT32 ArchIntUnlock(VOID)
{
    UINT32 intSave;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"
        "bic    %0, %0, #0xc0 \n"
        "msr    cpsr_c, %0      "
        : "=r"(intSave)
        : : "memory");
    return intSave;
}

#endif

STATIC INLINE VOID ArchIntRestore(UINT32 intSave)
{
    __asm__ __volatile__(
        "msr    cpsr_c, %0      "
        :
        : "r"(intSave)
        : "memory");
}

#define PSR_I_BIT   0x00000080U

STATIC INLINE UINT32 OsIntLocked(VOID)
{
    UINT32 intSave;

    asm volatile(
        "mrs    %0, cpsr        "
        : "=r" (intSave)
        :
        : "memory", "cc");

    return intSave & PSR_I_BIT;
}

STATIC INLINE UINT32 ArchSPGet(VOID)
{
    UINT32 val;
    __asm__ __volatile__("mov %0, sp" : "=r"(val));
    return val;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HW_CPU_H */
