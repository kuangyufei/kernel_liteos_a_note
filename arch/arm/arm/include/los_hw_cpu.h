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

/*!
 * @brief 内存屏障（英语：Memory barrier），也称内存栅栏，内存栅障，屏障指令等，是一类同步屏障指令，
	 \n 它使得 CPU 或编译器在对内存进行操作的时候, 严格按照一定的顺序来执行, 也就是说在memory barrier 之前的指令
	 \n 和memory barrier之后的指令不会由于系统优化等原因而导致乱序。
	 \n 大多数现代计算机为了提高性能而采取乱序执行，这使得内存屏障成为必须。

	 \n 语义上，内存屏障之前的所有写操作都要写入内存；内存屏障之后的读操作都可以获得同步屏障之前的写操作的结果。
	 \n 因此，对于敏感的程序块，写操作之后、读操作之前可以插入内存屏障。	
 *
 * @param index	
 * @return	
 *
 * @see
 */
// https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/memory-access-ordering---an-introduction

/* ARM System Registers */
/*
 * 内存屏障与系统指令宏定义
 * 这些宏封装了ARM架构特有的内存屏障和系统控制指令
 * 用于确保内存操作顺序和处理器状态控制
 */
#define DSB     __asm__ volatile("dsb" ::: "memory")  /* 数据同步屏障 - 确保所有先前的内存访问完成 */
#define DMB     __asm__ volatile("dmb" ::: "memory")  /* 数据内存屏障 - 确保内存访问顺序 */
#define ISB     __asm__ volatile("isb" ::: "memory")  /* 指令同步屏障 - 刷新流水线，确保新指令取指 */
#define WFI     __asm__ volatile("wfi" ::: "memory")  /* 等待中断 - 使处理器进入低功耗状态，等待中断唤醒 */
#define BARRIER __asm__ volatile("":::"memory")       /* 编译器屏障 - 阻止编译器重排内存操作，无硬件效果 */
#define WFE     __asm__ volatile("wfe" ::: "memory")  /* 等待事件 - 使处理器进入低功耗状态，等待事件唤醒 */
#define SEV     __asm__ volatile("sev" ::: "memory")  /* 发送事件 - 唤醒处于WFE状态的处理器 */

/*
 * ARM系统寄存器访问宏
 * 封装了ARM架构特有的系统寄存器读写操作
 * 使用GCC内联汇编实现对CP14/CP15等协处理器寄存器的访问
 */
#define ARM_SYSREG_READ(REG)                    \
({                                              \
    UINT32 _val;                                \
    __asm__ volatile("mrc " REG : "=r" (_val)); \
    _val;                                       \
})                                              /* 读取32位系统寄存器，使用MRC指令 */

#define ARM_SYSREG_WRITE(REG, val)              \
({                                              \
    __asm__ volatile("mcr " REG :: "r" (val));  \
    ISB;                                        \
})                                              /* 写入32位系统寄存器，使用MCR指令并紧跟ISB确保生效 */

#define ARM_SYSREG64_READ(REG)                   \
({                                               \
    UINT64 _val;                                 \
    __asm__ volatile("mrrc " REG : "=r" (_val)); \
    _val;                                        \
})                                               /* 读取64位系统寄存器，使用MRRC指令 */

#define ARM_SYSREG64_WRITE(REG, val)             \
({                                               \
    __asm__ volatile("mcrr " REG :: "r" (val));  \
    ISB;                                         \
})                                               /* 写入64位系统寄存器，使用MCRR指令并紧跟ISB确保生效 */

/*
 * 协处理器寄存器操作宏
 * 定义CP14和CP15协处理器寄存器的访问格式
 * 符合ARM架构协处理器指令编码规范
 */
#define CP14_REG(CRn, Op1, CRm, Op2)    "p14, "#Op1", %0, "#CRn","#CRm","#Op2 /* CP14协处理器寄存器访问格式，参数依次为CRn(主寄存器), Op1(操作码1), CRm(次寄存器), Op2(操作码2) */
#define CP15_REG(CRn, Op1, CRm, Op2)    "p15, "#Op1", %0, "#CRn","#CRm","#Op2 /* CP15协处理器寄存器访问格式，参数同上 */
#define CP15_REG64(CRn, Op1)            "p15, "#Op1", %0,    %H0,"#CRn         /* 64位CP15寄存器访问格式 */

/*
 * 标识寄存器 (c0)
 * 用于获取处理器和系统的标识信息
 */
#define MIDR                CP15_REG(c0, 0, c0, 0)    /* 主ID寄存器 - 包含处理器架构、实现和版本信息 */
#define MPIDR               CP15_REG(c0, 0, c0, 5)    /* 多核亲和性寄存器 - 包含处理器核ID和集群ID */
#define CCSIDR              CP15_REG(c0, 1, c0, 0)    /* 缓存大小ID寄存器 - 描述当前缓存级别详细信息 */
#define CLIDR               CP15_REG(c0, 1, c0, 1)    /* 缓存级别ID寄存器 - 描述缓存层次结构组织 */
#define VPIDR               CP15_REG(c0, 4, c0, 0)    /* 虚拟化处理器ID寄存器 - 虚拟化环境下的处理器ID */
#define VMPIDR              CP15_REG(c0, 4, c0, 5)    /* 虚拟化多核ID寄存器 - 虚拟化环境下的多核亲和性信息 */

/*
 * 系统控制寄存器 (c1)
 * 用于配置和控制处理器核心功能
 */
#define SCTLR               CP15_REG(c1, 0, c0, 0)    /* 系统控制寄存器 - 控制MMU、缓存、对齐检查等核心功能 */
#define ACTLR               CP15_REG(c1, 0, c0, 1)    /* 辅助控制寄存器 - 控制处理器特定功能和优化选项 */
#define CPACR               CP15_REG(c1, 0, c0, 2)    /* 协处理器访问控制寄存器 - 控制对CP10/CP11等协处理器的访问权限 */

/*
 * 内存保护和控制寄存器 (c2 & c3)
 * 用于配置内存管理单元(MMU)和内存访问权限
 */
#define TTBR0               CP15_REG(c2, 0, c0, 0)    /* 转换表基址寄存器0 - 存储用户空间页表基地址 */
#define TTBR1               CP15_REG(c2, 0, c0, 1)    /* 转换表基址寄存器1 - 存储内核空间页表基地址 */
#define TTBCR               CP15_REG(c2, 0, c0, 2)    /* 转换表基址控制寄存器 - 控制TTBR0/TTBR1的划分和使用 */
#define DACR                CP15_REG(c3, 0, c0, 0)    /* 域访问控制寄存器 - 控制内存域的访问权限 */

/*
 * 内存系统故障寄存器 (c5 & c6)
 * 用于捕获和报告内存访问故障信息
 */
#define DFSR                CP15_REG(c5, 0, c0, 0)    /* 数据故障状态寄存器 - 包含数据访问故障的原因和状态 */
#define IFSR                CP15_REG(c5, 0, c0, 1)    /* 指令故障状态寄存器 - 包含指令访问故障的原因和状态 */
#define DFAR                CP15_REG(c6, 0, c0, 0)    /* 数据故障地址寄存器 - 存储导致数据故障的虚拟地址 */
#define IFAR                CP15_REG(c6, 0, c0, 2)    /* 指令故障地址寄存器 - 存储导致指令故障的虚拟地址 */

/*
 * 进程、上下文和线程ID寄存器 (c13)
 * 用于标识和隔离不同进程和线程的执行上下文
 */
#define FCSEIDR             CP15_REG(c13, 0, c0, 0)    /* FCSE进程ID寄存器 - 快速上下文切换扩展的进程ID */
#define CONTEXTIDR          CP15_REG(c13, 0, c0, 1)    /* 上下文ID寄存器 - 存储进程上下文标识符，用于TLB隔离 */
#define TPIDRURW            CP15_REG(c13, 0, c0, 2)    /* 用户读写线程ID寄存器 - 用户模式可读写的线程私有数据指针 */
#define TPIDRURO            CP15_REG(c13, 0, c0, 3)    /* 用户只读线程ID寄存器 - 用户模式只读的线程私有数据指针 */
#define TPIDRPRW            CP15_REG(c13, 0, c0, 4)    /* PL1级线程ID寄存器 - 特权模式线程私有数据指针，内核线程ID存储于此 */
/*
 * CPU ID掩码宏定义
 * 用于从MPIDR寄存器值中提取CPU ID字段
 * 0xffU表示取低8位，支持最多256个CPU核心的系统
 */
#define MPIDR_CPUID_MASK    (0xffU)

/*
 * 获取当前任务控制块指针
 * 通过读取TPIDRPRW寄存器(PL1级线程ID寄存器)获取内核任务指针
 * TPIDRPRW寄存器在任务切换时由调度器更新
 *
 * @return VOID* 当前运行任务的控制块指针
 */
STATIC INLINE VOID *ArchCurrTaskGet(VOID)
{
    return (VOID *)(UINTPTR)ARM_SYSREG_READ(TPIDRPRW);  /* 读取TPIDRPRW寄存器并转换为任务指针 */
}

/*
 * 设置当前任务控制块指针
 * 通过写入TPIDRPRW寄存器(PL1级线程ID寄存器)保存内核任务指针
 * 供上下文切换时快速访问当前任务信息
 *
 * @param val 要设置的任务控制块指针
 */
STATIC INLINE VOID ArchCurrTaskSet(VOID *val)
{
    ARM_SYSREG_WRITE(TPIDRPRW, (UINT32)(UINTPTR)val);  /* 将任务指针转换为UINT32并写入TPIDRPRW寄存器 */
}

/*
 * 设置用户模式任务指针
 * 通过写入TPIDRURO寄存器(用户只读线程ID寄存器)保存用户任务信息
 * 用户模式下可读取此寄存器获取任务上下文
 *
 * @param val 要设置的用户任务指针(无符号整数形式)
 */
STATIC INLINE VOID ArchCurrUserTaskSet(UINTPTR val)
{
    ARM_SYSREG_WRITE(TPIDRURO, (UINT32)val);  /* 将用户任务指针写入TPIDRURO寄存器 */
}

/*
 * 获取当前CPU核心ID
 * 在SMP配置下从MPIDR寄存器提取CPU ID，否则返回0
 * CPU ID用于多核调度和核间通信
 *
 * @return UINT32 当前CPU核心ID号
 */
STATIC INLINE UINT32 ArchCurrCpuid(VOID)
{
#ifdef LOSCFG_KERNEL_SMP
    return ARM_SYSREG_READ(MPIDR) & MPIDR_CPUID_MASK;  /* 读取MPIDR寄存器并应用CPU ID掩码 */
#else
    return 0;  /* 非SMP模式下始终返回0 */
#endif
}

/*
 * 获取硬件ID信息
 * 读取MPIDR(多核亲和性寄存器)的完整值
 * 包含CPU ID、集群ID等多核架构信息
 *
 * @return UINT64 MPIDR寄存器的64位值
 */
STATIC INLINE UINT64 OsHwIDGet(VOID)
{
    return ARM_SYSREG_READ(MPIDR);  /* 读取MPIDR寄存器的完整值 */
}

/*
 * 获取主ID寄存器值
 * 读取MIDR(主ID寄存器)获取处理器实现和版本信息
 * 包含架构版本、供应商代码和处理器型号
 *
 * @return UINT32 MIDR寄存器值
 */
STATIC INLINE UINT32 OsMainIDGet(VOID)
{
    return ARM_SYSREG_READ(MIDR);  /* 读取MIDR寄存器值 */
}

/*
 * CPU中断屏蔽处理实现
 * 根据ARM架构版本提供不同的中断开关实现
 * ARMv6及以上架构使用cps指令，低版本架构直接操作CPSR寄存器
 */
#if LOSCFG_ARM_ARCH >= 6  /* ARMv6及以上架构中断控制实现 */

/*
 * 关闭中断并保存状态
 * 使用cpsid指令关闭IRQ和FIQ中断，并返回关中断前的CPSR值
 * 用于临界区保护，需与ArchIntRestore配合使用
 *
 * @return UINT32 关中断前的CPSR寄存器值(用于恢复)
 */
STATIC INLINE UINT32 ArchIntLock(VOID)
{
    UINT32 intSave;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"  /* 读取当前CPSR状态 */
        "cpsid  if              "  /* 关闭IRQ和FIQ中断 */
        : "=r"(intSave)
        :
        : "memory");  /* 内存屏障，防止编译器重排指令 */
    return intSave;
}

/*
 * 打开中断并保存状态
 * 使用cpsie指令打开IRQ和FIQ中断，并返回开中断前的CPSR值
 *
 * @return UINT32 开中断前的CPSR寄存器值
 */
STATIC INLINE UINT32 ArchIntUnlock(VOID)
{
    UINT32 intSave;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"  /* 读取当前CPSR状态 */
        "cpsie  if              "  /* 打开IRQ和FIQ中断 */
        : "=r"(intSave)
        :
        : "memory");  /* 内存屏障 */
    return intSave;
}

/*
 * 禁用IRQ中断
 * 使用cpsid指令仅关闭IRQ中断，FIQ中断不受影响
 */
STATIC INLINE VOID ArchIrqDisable(VOID)
{
    __asm__ __volatile__(
        "cpsid  i      "  /* 关闭IRQ中断 */
        :
        :
        : "memory", "cc");  /* 内存屏障和条件码寄存器 */
}

/*
 * 启用IRQ中断
 * 使用cpsie指令仅打开IRQ中断
 */
STATIC INLINE VOID ArchIrqEnable(VOID)
{
    __asm__ __volatile__(
        "cpsie  i      "  /* 打开IRQ中断 */
        :
        :
        : "memory", "cc");  /* 内存屏障和条件码寄存器 */
}

#else  /* ARMv6以下架构中断控制实现 */

/*
 * 关闭中断并保存状态(ARMv6以下)
 * 直接操作CPSR寄存器，设置I位(0x80)和F位(0x40)关闭IRQ和FIQ
 *
 * @return UINT32 关中断前的CPSR寄存器值
 */
STATIC INLINE UINT32 ArchIntLock(VOID)
{
    UINT32 intSave, temp;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"  /* 读取当前CPSR状态 */
        "orr    %1, %0, #0xc0 \n"  /* 设置I位(0x80)和F位(0x40) */
        "msr    cpsr_c, %1      "  /* 写入CPSR控制位 */
        :"=r"(intSave),  "=r"(temp)
        : :"memory");  /* 内存屏障 */
    return intSave;
}

/*
 * 打开中断并保存状态(ARMv6以下)
 * 直接操作CPSR寄存器，清除I位(0x80)和F位(0x40)打开中断
 *
 * @return UINT32 开中断前的CPSR寄存器值
 */
STATIC INLINE UINT32 ArchIntUnlock(VOID)
{
    UINT32 intSave;
    __asm__ __volatile__(
        "mrs    %0, cpsr      \n"  /* 读取当前CPSR状态 */
        "bic    %0, %0, #0xc0 \n"  /* 清除I位和F位 */
        "msr    cpsr_c, %0      "  /* 写入CPSR控制位 */
        : "=r"(intSave)
        : : "memory");  /* 内存屏障 */
    return intSave;
}

#endif  /* LOSCFG_ARM_ARCH >= 6 */

/*
 * 恢复中断状态
 * 将CPSR寄存器恢复为指定值，用于临界区结束后恢复中断
 * 必须与ArchIntLock配合使用，参数为ArchIntLock的返回值
 *
 * @param intSave 要恢复的CPSR状态值(通常是ArchIntLock的返回值)
 */
STATIC INLINE VOID ArchIntRestore(UINT32 intSave)
{
    __asm__ __volatile__(
        "msr    cpsr_c, %0      "  /* 恢复CPSR控制位 */
        :
        : "r"(intSave)
        : "memory");  /* 内存屏障 */
}

/*
 * CPSR寄存器中断标志位定义
 * PSR_I_BIT表示CPSR中的IRQ中断禁止位(第7位)
 * 当此位为1时IRQ中断被禁止，为0时允许IRQ中断
 */
#define PSR_I_BIT   0x00000080U

/*
 * 检查当前中断是否被锁定
 * 读取CPSR寄存器并检查I位状态
 *
 * @return UINT32 非0表示中断已锁定(禁止)，0表示中断未锁定(允许)
 */
STATIC INLINE UINT32 OsIntLocked(VOID)
{
    UINT32 intSave;

    asm volatile(
        "mrs    %0, cpsr        "  /* 读取CPSR寄存器 */
        : "=r" (intSave)
        :
        : "memory", "cc");  /* 内存屏障和条件码寄存器 */

    return intSave & PSR_I_BIT;  /* 返回I位状态 */
}

/*
 * 获取当前栈指针(SP)
 * 读取当前模式下的栈指针寄存器
 * 用于栈回溯、栈检查等调试和内存管理功能
 *
 * @return UINT32 当前栈指针值
 */
STATIC INLINE UINT32 ArchSPGet(VOID)
{
    UINT32 val;
    __asm__ __volatile__("mov %0, sp" : "=r"(val));  /* 将SP寄存器值移动到通用寄存器 */
    return val;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HW_CPU_H */
