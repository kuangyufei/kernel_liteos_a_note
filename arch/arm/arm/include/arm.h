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


#ifndef __LOS_ARM_H__
#define __LOS_ARM_H__
/** 
 * @brief CPSR模式定义
 * @details 定义ARM处理器CPSR寄存器中的用户模式和模式掩码
 */
#define CPSR_MODE_USR  0x10  // 用户模式 (User Mode)，二进制00010000，十进制16
#define CPSR_MODE_MASK 0x1f  // 模式位掩码，二进制00011111，十进制31，用于提取CPSR中的模式字段

/** 
 * @brief 读取系统控制寄存器(SCTLR)
 * @details 读取ARM处理器的系统控制寄存器(SCTLR)，该寄存器控制系统级功能，如MMU使能、数据/指令缓存控制等
 * @return UINT32 - SCTLR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadSctlr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c1,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c1,c0,0寄存器到val
    return val;  // 返回读取到的SCTLR寄存器值
}

/** 
 * @brief 写入系统控制寄存器(SCTLR)
 * @details 写入ARM处理器的系统控制寄存器(SCTLR)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入SCTLR寄存器的值
 */
STATIC INLINE VOID OsArmWriteSctlr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c1,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c1,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取辅助控制寄存器(ACTLR)
 * @details 读取ARM处理器的辅助控制寄存器(ACTLR)，该寄存器控制处理器的一些辅助功能
 * @return UINT32 - ACTLR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadActlr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c1,c0,1" : "=r"(val));  // 汇编指令：读取CP15协处理器c1,c0,1寄存器到val
    return val;  // 返回读取到的ACTLR寄存器值
}

/** 
 * @brief 写入辅助控制寄存器(ACTLR)
 * @details 写入ARM处理器的辅助控制寄存器(ACTLR)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入ACTLR寄存器的值
 */
STATIC INLINE VOID OsArmWriteActlr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c1,c0,1" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c1,c0,1寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取协处理器访问控制寄存器(CPACR)
 * @details 读取ARM处理器的协处理器访问控制寄存器(CPACR)，控制对协处理器(如NEON、VFP)的访问权限
 * @return UINT32 - CPACR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadCpacr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c1,c0,2" : "=r"(val));  // 汇编指令：读取CP15协处理器c1,c0,2寄存器到val
    return val;  // 返回读取到的CPACR寄存器值
}

/** 
 * @brief 写入协处理器访问控制寄存器(CPACR)
 * @details 写入ARM处理器的协处理器访问控制寄存器(CPACR)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入CPACR寄存器的值
 */
STATIC INLINE VOID OsArmWriteCpacr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c1,c0,2" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c1,c0,2寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取 translation table base register(TTBR)
 * @details 读取ARM处理器的页表基地址寄存器(TTBR)，存储一级页表的基地址
 * @return UINT32 - TTBR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTtbr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c2,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c2,c0,0寄存器到val
    return val;  // 返回读取到的TTBR寄存器值
}

/** 
 * @brief 写入 translation table base register(TTBR)
 * @details 写入ARM处理器的页表基地址寄存器(TTBR)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入TTBR寄存器的值
 */
STATIC INLINE VOID OsArmWriteTtbr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c2,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c2,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取 translation table base register 0(TTBR0)
 * @details 读取ARM处理器的页表基地址寄存器0(TTBR0)，用于存储低地址空间的页表基地址
 * @return UINT32 - TTBR0寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTtbr0(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c2,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c2,c0,0寄存器到val
    return val;  // 返回读取到的TTBR0寄存器值
}

/** 
 * @brief 写入 translation table base register 0(TTBR0)
 * @details 写入ARM处理器的页表基地址寄存器0(TTBR0)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入TTBR0寄存器的值
 */
STATIC INLINE VOID OsArmWriteTtbr0(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c2,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c2,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取 translation table base register 1(TTBR1)
 * @details 读取ARM处理器的页表基地址寄存器1(TTBR1)，用于存储高地址空间的页表基地址
 * @return UINT32 - TTBR1寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTtbr1(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c2,c0,1" : "=r"(val));  // 汇编指令：读取CP15协处理器c2,c0,1寄存器到val
    return val;  // 返回读取到的TTBR1寄存器值
}

/** 
 * @brief 写入 translation table base register 1(TTBR1)
 * @details 写入ARM处理器的页表基地址寄存器1(TTBR1)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入TTBR1寄存器的值
 */
STATIC INLINE VOID OsArmWriteTtbr1(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c2,c0,1" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c2,c0,1寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取 translation table base control register(TTBAR)
 * @details 读取ARM处理器的页表基地址控制寄存器(TTBAR)，控制TTBR0和TTBR1的划分和使用
 * @return UINT32 - TTBAR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTtbcr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c2,c0,2" : "=r"(val));  // 汇编指令：读取CP15协处理器c2,c0,2寄存器到val
    return val;  // 返回读取到的TTBAR寄存器值
}

/** 
 * @brief 写入 translation table base control register(TTBAR)
 * @details 写入ARM处理器的页表基地址控制寄存器(TTBAR)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入TTBAR寄存器的值
 */
STATIC INLINE VOID OsArmWriteTtbcr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c2,c0,2" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c2,c0,2寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取域访问控制寄存器(DACR)
 * @details 读取ARM处理器的域访问控制寄存器(DACR)，控制内存域的访问权限
 * @return UINT32 - DACR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadDacr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c3,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c3,c0,0寄存器到val
    return val;  // 返回读取到的DACR寄存器值
}

/** 
 * @brief 写入域访问控制寄存器(DACR)
 * @details 写入ARM处理器的域访问控制寄存器(DACR)，并执行ISB指令确保修改立即生效
 * @param val [IN] 要写入DACR寄存器的值
 */
STATIC INLINE VOID OsArmWriteDacr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c3,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c3,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取数据故障状态寄存器(DFSR)
 * @details 读取ARM处理器的数据故障状态寄存器(DFSR)，记录数据访问异常的状态信息
 * @return UINT32 - DFSR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadDfsr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c5,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c5,c0,0寄存器到val
    return val;  // 返回读取到的DFSR寄存器值
}

/** 
 * @brief 写入数据故障状态寄存器(DFSR)
 * @details 写入ARM处理器的数据故障状态寄存器(DFSR)，通常用于清除故障状态
 * @param val [IN] 要写入DFSR寄存器的值
 */
STATIC INLINE VOID OsArmWriteDfsr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c5,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c5,c0,0寄存器    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取指令故障状态寄存器(Iris)
 * @details 读取ARM处理器的指令故障状态寄存器(IFSR)，记录指令访问异常的状态信息
 * @return UINT32 - IFSR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadIfsr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c5,c0,1" : "=r"(val));  // 汇编指令：读取CP15协处理器c5,c0,1寄存器到val
    return val;  // 返回读取到的IFSR寄存器值
}

/** 
 * @brief 写入指令故障状态寄存器(IFSR)
 * @details 写入ARM处理器的指令故障状态寄存器(IFSR)，通常用于清除故障状态
 * @param val [IN] 要写入IFSR寄存器的值
 */
STATIC INLINE VOID OsArmWriteIfsr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c5,c0,1" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c5,c0,1寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取数据故障地址寄存器(DFAR)
 * @details 读取ARM处理器的数据故障地址寄存器(DFAR)，存储发生数据访问异常的地址
 * @return UINT32 - DFAR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadDfar(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c6,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c6,c0,0寄存器到val
    return val;  // 返回读取到的DFAR寄存器值
}

/** 
 * @brief 写入数据故障地址寄存器(DFAR)
 * @details 写入ARM处理器的数据故障地址寄存器(DFAR)，通常用于调试目的
 * @param val [IN] 要写入DFAR寄存器的值
 */
STATIC INLINE VOID OsArmWriteDfar(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c6,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c6,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取写故障地址寄存器(WFAR)
 * @details 读取ARM处理器的写故障地址寄存器(WFAR)，存储发生写访问异常的地址
 * @return UINT32 - WFAR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadWfar(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c6,c0,1" : "=r"(val));  // 汇编指令：读取CP15协处理器c6,c0,1寄存器到val
    return val;  // 返回读取到的WFAR寄存器值
}

/** 
 * @brief 写入写故障地址寄存器(WFAR)
 * @details 写入ARM处理器的写故障地址寄存器(WFAR)，通常用于调试目的
 * @param val [IN] 要写入WFAR寄存器的值
 */
STATIC INLINE VOID OsArmWriteWfar(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c6,c0,1" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c6,c0,1寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取指令故障地址寄存器(IFAR)
 * @details 读取ARM处理器的指令故障地址寄存器(IFAR)，存储发生指令访问异常的地址
 * @return UINT32 - IFAR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadIfar(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c6,c0,2" : "=r"(val));  // 汇编指令：读取CP15协处理器c6,c0,2寄存器到val
    return val;  // 返回读取到的IFAR寄存器值
}

/** 
 * @brief 写入指令故障地址寄存器(IFAR)
 * @details 写入ARM处理器的指令故障地址寄存器(IFAR)，通常用于调试目的
 * @param val [IN] 要写入IFAR寄存器的值
 */
STATIC INLINE VOID OsArmWriteIfar(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c6,c0,2" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c6,c0,2寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取FCSE标识寄存器(FCSEIDR)
 * @details 读取ARM处理器的FCSE(快速上下文切换扩展)标识寄存器，用于上下文切换管理
 * @return UINT32 - FCSEIDR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadFcseidr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c13,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c13,c0,0寄存器到val
    return val;  // 返回读取到的FCSEIDR寄存器值
}

/** 
 * @brief 写入FCSE标识寄存器(FCSEIDR)
 * @details 写入ARM处理器的FCSE标识寄存器，用于配置快速上下文切换功能
 * @param val [IN] 要写入FCSEIDR寄存器的值
 */
STATIC INLINE VOID OsArmWriteFcseidr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c13,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c13,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取上下文标识寄存器(ContextIDR)
 * @details 读取ARM处理器的上下文标识寄存器，用于存储进程ID等上下文信息
 * @return UINT32 - ContextIDR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadContextidr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c13,c0,1" : "=r"(val));  // 汇编指令：读取CP15协处理器c13,c0,1寄存器到val
    return val;  // 返回读取到的ContextIDR寄存器值
}

/** 
 * @brief 写入上下文标识寄存器(ContextIDR)
 * @details 写入ARM处理器的上下文标识寄存器，用于切换进程上下文
 * @param val [IN] 要写入ContextIDR寄存器的值
 */
STATIC INLINE VOID OsArmWriteContextidr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c13,c0,1" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c13,c0,1寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取用户读写线程ID寄存器(TPIDRURW)
 * @details 读取ARM处理器的用户模式读写线程ID寄存器，用于存储线程私有数据指针
 * @return UINT32 - TPIDRURW寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTpidrurw(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c13,c0,2" : "=r"(val));  // 汇编指令：读取CP15协处理器c13,c0,2寄存器到val
    return val;  // 返回读取到的TPIDRURW寄存器值
}

/** 
 * @brief 写入用户读写线程ID寄存器(TPIDRURW)
 * @details 写入ARM处理器的用户模式读写线程ID寄存器，用于设置线程私有数据指针
 * @param val [IN] 要写入TPIDRURW寄存器的值
 */
STATIC INLINE VOID OsArmWriteTpidrurw(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c13,c0,2" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c13,c0,2寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取用户只读线程ID寄存器(TPIDRURO)
 * @details 读取ARM处理器的用户模式只读线程ID寄存器，用于存储只读线程私有数据
 * @return UINT32 - TPIDRURO寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTpidruro(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c13,c0,3" : "=r"(val));  // 汇编指令：读取CP15协处理器c13,c0,3寄存器到val
    return val;  // 返回读取到的TPIDRURO寄存器值
}

/** 
 * @brief 写入用户只读线程ID寄存器(TPIDRURO)
 * @details 写入ARM处理器的用户模式只读线程ID寄存器，用于设置只读线程私有数据
 * @param val [IN] 要写入TPIDRURO寄存器的值
 */
STATIC INLINE VOID OsArmWriteTpidruro(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c13,c0,3" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c13,c0,3寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取特权读写线程ID寄存器(TPIDRPRW)
 * @details 读取ARM处理器的特权模式读写线程ID寄存器，用于存储特权线程私有数据
 * @return UINT32 - TPIDRPRW寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTpidrprw(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c13,c0,4" : "=r"(val));  // 汇编指令：读取CP15协处理器c13,c0,4寄存器到val
    return val;  // 返回读取到的TPIDRPRW寄存器值
}

/** 
 * @brief 写入特权读写线程ID寄存器(TPIDRPRW)
 * @details 写入ARM处理器的特权模式读写线程ID寄存器，用于设置特权线程私有数据
 * @param val [IN] 要写入TPIDRPRW寄存器的值
 */
STATIC INLINE VOID OsArmWriteTpidrprw(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c13,c0,4" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c13,c0,4寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取主ID寄存器(MIDR)
 * @details 读取ARM处理器的主ID寄存器，包含处理器型号、架构版本等信息
 * @return UINT32 - MIDR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadMidr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c0,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c0,c0,0寄存器到val
    return val;  // 返回读取到的MIDR寄存器值
}

/** 
 * @brief 写入主ID寄存器(MIDR)
 * @details 写入ARM处理器的主ID寄存器，通常用于仿真或调试目的
 * @param val [IN] 要写入MIDR寄存器的值
 */
STATIC INLINE VOID OsArmWriteMidr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c0,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c0,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}
/** 
 * @brief 读取多核处理器ID寄存器(MPIDR)
 * @details 读取ARM处理器的多核处理器ID寄存器，获取处理器核心的亲和性信息
 * @return UINT32 - MPIDR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadMpidr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c0,c0,5" : "=r"(val));  // 汇编指令：读取CP15协处理器c0,c0,5寄存器到val
    return val;  // 返回读取到的MPIDR寄存器值
}

/** 
 * @brief 写入多核处理器ID寄存器(MPIDR)
 * @details 写入ARM处理器的多核处理器ID寄存器，设置处理器核心的亲和性信息
 * @param val [IN] 要写入MPIDR寄存器的值
 */
STATIC INLINE VOID OsArmWriteMpidr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c0,c0,5" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c0,c0,5寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取向量基地址寄存器(VBAR)
 * @details 读取ARM处理器的向量基地址寄存器，获取异常向量表的基地址
 * @return UINT32 - VBAR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadVbar(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c12,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c12,c0,0寄存器到val
    return val;  // 返回读取到的VBAR寄存器值
}

/** 
 * @brief 写入向量基地址寄存器(VBAR)
 * @details 写入ARM处理器的向量基地址寄存器，设置异常向量表的基地址
 * @param val [IN] 要写入VBAR寄存器的值
 */
STATIC INLINE VOID OsArmWriteVbar(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c12,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c12,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取配置基地址寄存器(CBAR)
 * @details 读取ARM处理器的配置基地址寄存器，获取系统外设的基地址
 * @return UINT32 - CBAR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadCbar(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 4, %0, c15,c0,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c15,c0,0寄存器到val
    return val;  // 返回读取到的CBAR寄存器值
}

/** 
 * @brief 写入配置基地址寄存器(CBAR)
 * @details 写入ARM处理器的配置基地址寄存器，设置系统外设的基地址
 * @param val [IN] 要写入CBAR寄存器的值
 */
STATIC INLINE VOID OsArmWriteCbar(UINT32 val)
{
    __asm__ volatile("mcr p15, 4, %0, c15,c0,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c15,c0,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats1cpr)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制地址转换功能
 * @return UINT32 - Ats1cpr寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts1cpr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,0寄存器到val
    return val;  // 返回读取到的Ats1cpr寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats1cpr)
 * @details 写入ARM处理器的地址转换系统寄存器，配置地址转换功能
 * @param val [IN] 要写入Ats1cpr寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts1cpr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats1cpw)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制地址转换功能
 * @return UINT32 - Ats1cpw寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts1cpw(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,1" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,1寄存器到val
    return val;  // 返回读取到的Ats1cpw寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats1cpw)
 * @details 写入ARM处理器的地址转换系统寄存器，配置地址转换功能
 * @param val [IN] 要写入Ats1cpw寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts1cpw(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,1" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,1寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats1cur)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制地址转换功能
 * @return UINT32 - Ats1cur寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts1cur(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,2" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,2寄存器到val
    return val;  // 返回读取到的Ats1cur寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats1cur)
 * @details 写入ARM处理器的地址转换系统寄存器，配置地址转换功能
 * @param val [IN] 要写入Ats1cur寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts1cur(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,2" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,2寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats1cuw)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制地址转换功能
 * @return UINT32 - Ats1cuw寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts1cuw(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,3" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,3寄存器到val
    return val;  // 返回读取到的Ats1cuw寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats1cuw)
 * @details 写入ARM处理器的地址转换系统寄存器，配置地址转换功能
 * @param val [IN] 要写入Ats1cuw寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts1cuw(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,3" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,3寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats12nsopr)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制非安全模式下的地址转换
 * @return UINT32 - Ats12nsopr寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts12nsopr(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,4" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,4寄存器到val
    return val;  // 返回读取到的Ats12nsopr寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats12nsopr)
 * @details 写入ARM处理器的地址转换系统寄存器，配置非安全模式下的地址转换
 * @param val [IN] 要写入Ats12nsopr寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts12nsopr(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,4" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,4寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats12nsopw)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制非安全模式下的地址转换
 * @return UINT32 - Ats12nsopw寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts12nsopw(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,5" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,5寄存器到val
    return val;  // 返回读取到的Ats12nsopw寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats12nsopw)
 * @details 写入ARM处理器的地址转换系统寄存器，配置非安全模式下的地址转换
 * @param val [IN] 要写入Ats12nsopw寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts12nsopw(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,5" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,5寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats12nsour)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制非安全模式下的地址转换
 * @return UINT32 - Ats12nsour寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts12nsour(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,6" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,6寄存器到val
    return val;  // 返回读取到的Ats12nsour寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats12nsour)
 * @details 写入ARM处理器的地址转换系统寄存器，配置非安全模式下的地址转换
 * @param val [IN] 要写入Ats12nsour寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts12nsour(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,6" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,6寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取地址转换系统寄存器(Ats12nsouw)
 * @details 读取ARM处理器的地址转换系统寄存器，用于控制非安全模式下的地址转换
 * @return UINT32 - Ats12nsouw寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadAts12nsouw(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c8,7" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c8,7寄存器到val
    return val;  // 返回读取到的Ats12nsouw寄存器值
}

/** 
 * @brief 写入地址转换系统寄存器(Ats12nsouw)
 * @details 写入ARM处理器的地址转换系统寄存器，配置非安全模式下的地址转换
 * @param val [IN] 要写入Ats12nsouw寄存器的值
 */
STATIC INLINE VOID OsArmWriteAts12nsouw(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c8,7" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c8,7寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取物理地址寄存器(PAR)
 * @details 读取ARM处理器的物理地址寄存器，获取最近地址转换的物理地址结果
 * @return UINT32 - PAR寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadPar(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c4,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c4,0寄存器到val
    return val;  // 返回读取到的PAR寄存器值
}

/** 
 * @brief 写入物理地址寄存器(PAR)
 * @details 写入ARM处理器的物理地址寄存器，通常用于调试或测试地址转换
 * @param val [IN] 要写入PAR寄存器的值
 */
STATIC INLINE VOID OsArmWritePar(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c4,0" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c4,0寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取分支预测失效所有寄存器(BPIALL)
 * @details 读取ARM处理器的分支预测失效所有寄存器，控制分支预测功能
 * @return UINT32 - BPIALL寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadBpiall(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c5,6" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c5,6寄存器到val
    return val;  // 返回读取到的BPIALL寄存器值
}

/** 
 * @brief 写入分支预测失效所有寄存器(BPIALL)
 * @details 写入ARM处理器的分支预测失效所有寄存器，失效所有分支预测条目
 * @param val [IN] 要写入BPIALL寄存器的值
 */
STATIC INLINE VOID OsArmWriteBpiall(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c5,6" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c5,6寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取分支预测失效MVA寄存器(BPIMVA)
 * @details 读取ARM处理器的分支预测失效MVA寄存器，按虚拟地址失效分支预测条目
 * @return UINT32 - BPIMVA寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadBpimva(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c5,7" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c5,7寄存器到val
    return val;  // 返回读取到的BPIMVA寄存器值
}

/** 
 * @brief 写入分支预测失效MVA寄存器(BPIMVA)
 * @details 写入ARM处理器的分支预测失效MVA寄存器，按虚拟地址失效指定分支预测条目
 * @param val [IN] 要写入BPIMVA寄存器的值
 */
STATIC INLINE VOID OsArmWriteBpimva(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c5,7" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c5,7寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取分支预测失效所有安全寄存器(BPIALLIS)
 * @details 读取ARM处理器的分支预测失效所有安全寄存器，控制安全模式下的分支预测
 * @return UINT32 - BPIALLIS寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadBpiallis(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c7,c1,6" : "=r"(val));  // 汇编指令：读取CP15协处理器c7,c1,6寄存器到val
    return val;  // 返回读取到的BPIALLIS寄存器值
}

/** 
 * @brief 写入分支预测失效所有安全寄存器(BPIALLIS)
 * @details 写入ARM处理器的分支预测失效所有安全寄存器，失效安全模式下的所有分支预测条目
 * @param val [IN] 要写入BPIALLIS寄存器的值
 */
STATIC INLINE VOID OsArmWriteBpiallis(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c7,c1,6" ::"r"(val));  // 汇编指令：将val写入CP15协处理器c7,c1,6寄存器
    __asm__ volatile("isb" ::: "memory");  // 执行指令同步屏障，确保前面的写操作完成并刷新流水线
}

/** 
 * @brief 读取TLB失效所有指令共享寄存器(TLBIALLIS)
 * @details 读取ARM处理器的TLB失效所有指令共享寄存器，控制指令TLB的失效操作
 * @return UINT32 - TLBIALLIS寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTlbiallis(VOID)
{
    UINT32 val;  // 用于存储读取的寄存器值
    __asm__ volatile("mrc p15, 0, %0, c8,c3,0" : "=r"(val));  // 汇编指令：读取CP15协处理器c8,c3,0寄存器到val
    return val;  // 返回读取到的TLBIALLIS寄存器值
}

/**
 * @brief   写入TLB失效指令共享寄存器（TLBIALLIS）
 * @details 用于在共享域中使所有TLB项失效，使用ISB指令确保操作立即生效
 * @param   val [IN] 写入的值（通常为0，具体取决于处理器实现）
 */
STATIC INLINE VOID OsArmWriteTlbiallis(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c3,0" ::"r"(val)); // 协处理器p15操作，c8=c3=0组合表示TLBIALLIS寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障，确保TLB失效操作完成
}

/**
 * @brief   读取TLB失效MVA共享寄存器（TLBIMVAIS）
 * @details 获取最近一次基于虚拟地址的TLB失效操作状态
 * @return  寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTlbimvais(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c3,1" : "=r"(val)); // 协处理器p15读取，c8=c3=1组合表示TLBIMVAIS寄存器
    return val;
}

/**
 * @brief   写入TLB失效MVA共享寄存器（TLBIMVAIS）
 * @details 使指定虚拟地址的TLB项在共享域中失效
 * @param   val [IN] 包含虚拟地址的操作数
 */
STATIC INLINE VOID OsArmWriteTlbimvais(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c3,1" ::"r"(val)); // 协处理器p15操作，c8=c3=1组合表示TLBIMVAIS寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障
}

/**
 * @brief   读取TLB失效ASID共享寄存器（TLBIASIDIS）
 * @details 获取基于ASID（地址空间标识符）的TLB失效状态
 * @return  寄存器当前值
 */
STATIC INLINE UINT32 OsArmReadTlbiasidis(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c3,2" : "=r"(val)); // 协处理器p15读取，c8=c3=2组合表示TLBIASIDIS寄存器
    return val;
}

/// 记录由协处理器记录当前是哪个进程在跑
/**
 * @brief   写入TLB失效ASID共享寄存器（TLBIASIDIS）
 * @details 使指定ASID对应的所有TLB项在共享域中失效
 * @param   val [IN] 包含ASID的操作数（低8位有效）
 */
STATIC INLINE VOID OsArmWriteTlbiasidis(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c3,2" ::"r"(val)); // 协处理器p15操作，c8=c3=2组合表示TLBIASIDIS寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障
}


/**
 * @brief   读取TLB失效按虚拟地址所有安全指令寄存器
 * @details 读取CP15协处理器中用于按虚拟地址使所有安全指令TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c3,3
 */
STATIC INLINE UINT32 OsArmReadTlbimvaais(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c3,3" : "=r"(val)); // 读取TLB失效按虚拟地址所有安全指令寄存器到val
    return val;
}

/**
 * @brief   写入TLB失效按虚拟地址所有安全指令寄存器
 * @details 写入CP15协处理器中用于按虚拟地址使所有安全指令TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c3,3
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteTlbimvaais(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c3,3" ::"r"(val)); // 将val写入TLB失效按虚拟地址所有安全指令寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取指令TLB全部失效寄存器
 * @details 读取CP15协处理器中用于使所有指令TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c5,0
 */
STATIC INLINE UINT32 OsArmReadItlbiall(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c5,0" : "=r"(val)); // 读取指令TLB全部失效寄存器到val
    return val;
}

/**
 * @brief   写入指令TLB全部失效寄存器
 * @details 写入CP15协处理器中用于使所有指令TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c5,0
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteItlbiall(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c5,0" ::"r"(val)); // 将val写入指令TLB全部失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取指令TLB按虚拟地址失效寄存器
 * @details 读取CP15协处理器中用于按虚拟地址使指令TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c5,1
 */
STATIC INLINE UINT32 OsArmReadItlbimva(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c5,1" : "=r"(val)); // 读取指令TLB按虚拟地址失效寄存器到val
    return val;
}

/**
 * @brief   写入指令TLB按虚拟地址失效寄存器
 * @details 写入CP15协处理器中用于按虚拟地址使指令TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c5,1
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteItlbimva(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c5,1" ::"r"(val)); // 将val写入指令TLB按虚拟地址失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取指令TLB按ASID失效寄存器
 * @details 读取CP15协处理器中用于按ASID(地址空间标识符)使指令TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c5,2
 */
STATIC INLINE UINT32 OsArmReadItlbiasid(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c5,2" : "=r"(val)); // 读取指令TLB按ASID失效寄存器到val
    return val;
}

/**
 * @brief   写入指令TLB按ASID失效寄存器
 * @details 写入CP15协处理器中用于按ASID(地址空间标识符)使指令TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c5,2
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteItlbiasid(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c5,2" ::"r"(val)); // 将val写入指令TLB按ASID失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取数据TLB全部失效寄存器
 * @details 读取CP15协处理器中用于使所有数据TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c6,0
 */
STATIC INLINE UINT32 OsArmReadDtlbiall(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c6,0" : "=r"(val)); // 读取数据TLB全部失效寄存器到val
    return val;
}

/**
 * @brief   写入数据TLB全部失效寄存器
 * @details 写入CP15协处理器中用于使所有数据TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c6,0
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDtlbiall(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c6,0" ::"r"(val)); // 将val写入数据TLB全部失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取数据TLB按虚拟地址失效寄存器
 * @details 读取CP15协处理器中用于按虚拟地址使数据TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c6,1
 */
STATIC INLINE UINT32 OsArmReadDtlbimva(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c6,1" : "=r"(val)); // 读取数据TLB按虚拟地址失效寄存器到val
    return val;
}

/**
 * @brief   写入数据TLB按虚拟地址失效寄存器
 * @details 写入CP15协处理器中用于按虚拟地址使数据TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c6,1
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDtlbimva(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c6,1" ::"r"(val)); // 将val写入数据TLB按虚拟地址失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取数据TLB按ASID失效寄存器
 * @details 读取CP15协处理器中用于按ASID(地址空间标识符)使数据TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c6,2
 */
STATIC INLINE UINT32 OsArmReadDtlbiasid(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c6,2" : "=r"(val)); // 读取数据TLB按ASID失效寄存器到val
    return val;
}

/**
 * @brief   写入数据TLB按ASID失效寄存器
 * @details 写入CP15协处理器中用于按ASID(地址空间标识符)使数据TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c6,2
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDtlbiasid(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c6,2" ::"r"(val)); // 将val写入数据TLB按ASID失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取TLB全部失效寄存器
 * @details 读取CP15协处理器中用于使所有指令和数据TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c7,0
 */
STATIC INLINE UINT32 OsArmReadTlbiall(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c7,0" : "=r"(val)); // 读取TLB全部失效寄存器到val
    return val;
}

/**
 * @brief   写入TLB全部失效寄存器
 * @details 写入CP15协处理器中用于使所有指令和数据TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c7,0
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteTlbiall(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c7,0" ::"r"(val)); // 将val写入TLB全部失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取TLB按虚拟地址失效寄存器
 * @details 读取CP15协处理器中用于按虚拟地址使指令和数据TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c7,1
 */
STATIC INLINE UINT32 OsArmReadTlbimva(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c7,1" : "=r"(val)); // 读取TLB按虚拟地址失效寄存器到val
    return val;
}

/**
 * @brief   写入TLB按虚拟地址失效寄存器
 * @details 写入CP15协处理器中用于按虚拟地址使指令和数据TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c7,1
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteTlbimva(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c7,1" ::"r"(val)); // 将val写入TLB按虚拟地址失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取TLB按ASID失效寄存器
 * @details 读取CP15协处理器中用于按ASID(地址空间标识符)使指令和数据TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c7,2
 */
STATIC INLINE UINT32 OsArmReadTlbiasid(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c7,2" : "=r"(val)); // 读取TLB按ASID失效寄存器到val
    return val;
}

/**
 * @brief   写入TLB按ASID失效寄存器
 * @details 写入CP15协处理器中用于按ASID(地址空间标识符)使指令和数据TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c7,2
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteTlbiasid(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c7,2" ::"r"(val)); // 将val写入TLB按ASID失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取TLB按虚拟地址所有失效寄存器
 * @details 读取CP15协处理器中用于按虚拟地址使所有TLB项失效的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 0, %0, c8,c7,3
 */
STATIC INLINE UINT32 OsArmReadTlbimvaa(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 0, %0, c8,c7,3" : "=r"(val)); // 读取TLB按虚拟地址所有失效寄存器到val
    return val;
}

/**
 * @brief   写入TLB按虚拟地址所有失效寄存器
 * @details 写入CP15协处理器中用于按虚拟地址使所有TLB项失效的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 0, %0, c8,c7,3
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteTlbimvaa(UINT32 val)
{
    __asm__ volatile("mcr p15, 0, %0, c8,c7,3" ::"r"(val)); // 将val写入TLB按虚拟地址所有失效寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取L2缓存控制寄存器
 * @details 读取CP15协处理器中用于控制L2缓存操作的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 1, %0, c9,c0,2
 *          协处理器操作数1为1，表示访问L2缓存相关寄存器
 */
STATIC INLINE UINT32 OsArmReadL2ctlr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 1, %0, c9,c0,2" : "=r"(val)); // 读取L2缓存控制寄存器到val
    return val;
}

/**
 * @brief   写入L2缓存控制寄存器
 * @details 写入CP15协处理器中用于控制L2缓存操作的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 1, %0, c9,c0,2
 *          协处理器操作数1为1，表示访问L2缓存相关寄存器
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteL2ctlr(UINT32 val)
{
    __asm__ volatile("mcr p15, 1, %0, c9,c0,2" ::"r"(val)); // 将val写入L2缓存控制寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取L2扩展缓存控制寄存器
 * @details 读取CP15协处理器中用于控制L2缓存扩展功能的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p15, 1, %0, c9,c0,3
 *          协处理器操作数1为1，表示访问L2缓存相关寄存器
 */
STATIC INLINE UINT32 OsArmReadL2ectlr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p15, 1, %0, c9,c0,3" : "=r"(val)); // 读取L2扩展缓存控制寄存器到val
    return val;
}

/**
 * @brief   写入L2扩展缓存控制寄存器
 * @details 写入CP15协处理器中用于控制L2缓存扩展功能的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p15, 1, %0, c9,c0,3
 *          协处理器操作数1为1，表示访问L2缓存相关寄存器
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteL2ectlr(UINT32 val)
{
    __asm__ volatile("mcr p15, 1, %0, c9,c0,3" ::"r"(val)); // 将val写入L2扩展缓存控制寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试调试器设备ID寄存器
 * @details 读取CP14协处理器中用于标识调试器设备ID的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c0,0
 *          使用CP14协处理器进行调试寄存器访问
 */
STATIC INLINE UINT32 OsArmReadDbddidr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c0,0" : "=r"(val)); // 读取调试调试器设备ID寄存器到val
    return val;
}

/**
 * @brief   写入调试调试器设备ID寄存器
 * @details 写入CP14协处理器中用于标识调试器设备ID的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c0,0
 *          使用CP14协处理器进行调试寄存器访问
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbddidr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c0,0" ::"r"(val)); // 将val写入调试调试器设备ID寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试调试器地址寄存器
 * @details 读取CP14协处理器中用于存储调试器地址的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c1,c0,0
 *          使用CP14协处理器进行调试寄存器访问
 */
STATIC INLINE UINT32 OsArmReadDbgdrar(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c1,c0,0" : "=r"(val)); // 读取调试调试器地址寄存器到val
    return val;
}

/**
 * @brief   写入调试调试器地址寄存器
 * @details 写入CP14协处理器中用于存储调试器地址的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c1,c0,0
 *          使用CP14协处理器进行调试寄存器访问
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdrar(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c1,c0,0" ::"r"(val)); // 将val写入调试调试器地址寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试调试器状态寄存器
 * @details 读取CP14协处理器中用于存储调试器状态的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c2,c0,0
 *          使用CP14协处理器进行调试寄存器访问
 */
STATIC INLINE UINT32 OsArmReadDbgdsar(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c2,c0,0" : "=r"(val)); // 读取调试调试器状态寄存器到val
    return val;
}

/**
 * @brief   写入调试调试器状态寄存器
 * @details 写入CP14协处理器中用于存储调试器状态的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c2,c0,0
 *          使用CP14协处理器进行调试寄存器访问
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdsar(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c2,c0,0" ::"r"(val)); // 将val写入调试调试器状态寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试调试器控制寄存器
 * @details 读取CP14协处理器中用于控制调试器操作的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c1,0
 *          使用CP14协处理器进行调试寄存器访问
 */
STATIC INLINE UINT32 OsArmReadDbgdscr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c1,0" : "=r"(val)); // 读取调试调试器控制寄存器到val
    return val;
}

/**
 * @brief   写入调试调试器控制寄存器
 * @details 写入CP14协处理器中用于控制调试器操作的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c1,0
 *          使用CP14协处理器进行调试寄存器访问
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdscr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c1,0" ::"r"(val)); // 将val写入调试调试器控制寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试调试器传输发送中断寄存器
 * @details 读取CP14协处理器中用于调试器传输发送中断状态的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c5,0
 *          使用CP14协处理器进行调试寄存器访问
 */
STATIC INLINE UINT32 OsArmReadDbgdtrtxint(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c5,0" : "=r"(val)); // 读取调试调试器传输发送中断寄存器到val
    return val;
}


/**
 * @brief   写入调试UART发送中断状态寄存器
 * @details 控制调试UART发送缓冲区中断
 * @param   val [IN] 写入值，bit0用于清除中断
 */
STATIC INLINE VOID OsArmWriteDbgdtrtxint(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c5,0" ::"r"(val)); // 协处理器p14写入，c0=c5=0组合表示调试UART发送中断状态寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障，确保操作立即生效
}

/**
 * @brief   读取调试UART接收中断状态寄存器
 * @details 获取调试UART接收缓冲区中断状态
 * @return  寄存器当前值，bit0表示中断状态
 */
STATIC INLINE UINT32 OsArmReadDbgdtrrxint(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c5,0" : "=r"(val)); // 协处理器p14读取，c0=c5=0组合表示调试UART接收中断状态寄存器
    return val;
}

/**
 * @brief   写入调试UART接收中断状态寄存器
 * @details 控制调试UART接收缓冲区中断
 * @param   val [IN] 写入值，bit0用于清除中断
 */
STATIC INLINE VOID OsArmWriteDbgdtrrxint(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c5,0" ::"r"(val)); // 协处理器p14写入，c0=c5=0组合表示调试UART接收中断状态寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障
}

/**
 * @brief   读取调试观察点故障地址寄存器
 * @details 获取最后一次观察点故障的虚拟地址
 * @return  故障地址值
 */
STATIC INLINE UINT32 OsArmReadDbgwfar(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c6,0" : "=r"(val)); // 协处理器p14读取，c0=c6=0组合表示观察点故障地址寄存器
    return val;
}

/**
 * @brief   写入调试观察点故障地址寄存器
 * @details 手动设置观察点故障地址（用于调试）
 * @param   val [IN] 故障地址值
 */
STATIC INLINE VOID OsArmWriteDbgwfar(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c6,0" ::"r"(val)); // 协处理器p14写入，c0=c6=0组合表示观察点故障地址寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障
}


/**
 * @brief   读取调试向量捕获寄存器
 * @details 读取CP14协处理器中用于控制向量捕获功能的调试寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c7,0
 *          用于调试异常向量地址捕获配置
 */
STATIC INLINE UINT32 OsArmReadDbgvcr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c7,0" : "=r"(val)); // 读取调试向量捕获寄存器到val
    return val;
}

/**
 * @brief   写入调试向量捕获寄存器
 * @details 写入CP14协处理器中用于控制向量捕获功能的调试寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c7,0
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgvcr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c7,0" ::"r"(val)); // 将val写入调试向量捕获寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试异常控制寄存器
 * @details 读取CP14协处理器中用于控制调试异常行为的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c9,0
 *          用于配置调试异常的生成和处理方式
 */
STATIC INLINE UINT32 OsArmReadDbgecr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c9,0" : "=r"(val)); // 读取调试异常控制寄存器到val
    return val;
}

/**
 * @brief   写入调试异常控制寄存器
 * @details 写入CP14协处理器中用于控制调试异常行为的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c9,0
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgecr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c9,0" ::"r"(val)); // 将val写入调试异常控制寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试DSU配置控制寄存器
 * @details 读取CP14协处理器中用于调试DSU(调试系统单元)配置的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c10,0
 */
STATIC INLINE UINT32 OsArmReadDbgdsccr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c10,0" : "=r"(val)); // 读取调试DSU配置控制寄存器到val
    return val;
}

/**
 * @brief   写入调试DSU配置控制寄存器
 * @details 写入CP14协处理器中用于调试DSU(调试系统单元)配置的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c10,0
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdsccr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c10,0" ::"r"(val)); // 将val写入调试DSU配置控制寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试DSU模式控制寄存器
 * @details 读取CP14协处理器中用于控制DSU(调试系统单元)工作模式的寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c11,0
 */
STATIC INLINE UINT32 OsArmReadDbgdsmcr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c11,0" : "=r"(val)); // 读取调试DSU模式控制寄存器到val
    return val;
}

/**
 * @brief   写入调试DSU模式控制寄存器
 * @details 写入CP14协处理器中用于控制DSU(调试系统单元)工作模式的寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c11,0
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdsmcr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c11,0" ::"r"(val)); // 将val写入调试DSU模式控制寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试DTR接收扩展寄存器
 * @details 读取CP14协处理器中调试DTR(调试传输寄存器)接收扩展功能寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c0,2
 */
STATIC INLINE UINT32 OsArmReadDbgdtrrxext(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c0,2" : "=r"(val)); // 读取调试DTR接收扩展寄存器到val
    return val;
}

/**
 * @brief   写入调试DTR接收扩展寄存器
 * @details 写入CP14协处理器中调试DTR(调试传输寄存器)接收扩展功能寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c0,2
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdtrrxext(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c0,2" ::"r"(val)); // 将val写入调试DTR接收扩展寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试DSCR扩展寄存器
 * @details 读取CP14协处理器中调试DSCR(调试状态控制寄存器)扩展功能寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c2,2
 */
STATIC INLINE UINT32 OsArmReadDbgdscrext(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c2,2" : "=r"(val)); // 读取调试DSCR扩展寄存器到val
    return val;
}

/**
 * @brief   写入调试DSCR扩展寄存器
 * @details 写入CP14协处理器中调试DSCR(调试状态控制寄存器)扩展功能寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c2,2
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdscrext(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c2,2" ::"r"(val)); // 将val写入调试DSCR扩展寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试DTR发送扩展寄存器
 * @details 读取CP14协处理器中调试DTR(调试传输寄存器)发送扩展功能寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c3,2
 */
STATIC INLINE UINT32 OsArmReadDbgdtrtxext(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c3,2" : "=r"(val)); // 读取调试DTR发送扩展寄存器到val
    return val;
}

/**
 * @brief   写入调试DTR发送扩展寄存器
 * @details 写入CP14协处理器中调试DTR(调试传输寄存器)发送扩展功能寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c3,2
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdtrtxext(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c3,2" ::"r"(val)); // 将val写入调试DTR发送扩展寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试DTR控制寄存器
 * @details 读取CP14协处理器中调试DTR(调试传输寄存器)控制功能寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c4,2
 */
STATIC INLINE UINT32 OsArmReadDbgdrcr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c4,2" : "=r"(val)); // 读取调试DTR控制寄存器到val
    return val;
}

/**
 * @brief   写入调试DTR控制寄存器
 * @details 写入CP14协处理器中调试DTR(调试传输寄存器)控制功能寄存器
 * @param   val [IN] 要写入的寄存器值
 * @note    汇编指令: mcr p14, 0, %0, c0,c4,2
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgdrcr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c4,2" ::"r"(val)); // 将val写入调试DTR控制寄存器
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试观察点值寄存器0
 * @details 读取CP14协处理器中第0号调试观察点的值寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c0,4
 *          用于存储观察点0监控的地址值
 */
STATIC INLINE UINT32 OsArmReadDbgvr0(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c0,4" : "=r"(val)); // 读取调试观察点值寄存器0到val
    return val;
}

/**
 * @brief   写入调试观察点值寄存器0
 * @details 写入CP14协处理器中第0号调试观察点的值寄存器
 * @param   val [IN] 要写入的观察点地址值
 * @note    汇编指令: mcr p14, 0, %0, c0,c0,4
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgvr0(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c0,4" ::"r"(val)); // 将val写入调试观察点值寄存器0
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试观察点值寄存器1
 * @details 读取CP14协处理器中第1号调试观察点的值寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c1,4
 *          用于存储观察点1监控的地址值
 */
STATIC INLINE UINT32 OsArmReadDbgvr1(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c1,4" : "=r"(val)); // 读取调试观察点值寄存器1到val
    return val;
}

/**
 * @brief   写入调试观察点值寄存器1
 * @details 写入CP14协处理器中第1号调试观察点的值寄存器
 * @param   val [IN] 要写入的观察点地址值
 * @note    汇编指令: mcr p14, 0, %0, c0,c1,4
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgvr1(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c1,4" ::"r"(val)); // 将val写入调试观察点值寄存器1
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试观察点值寄存器2
 * @details 读取CP14协处理器中第2号调试观察点的值寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c2,4
 *          用于存储观察点2监控的地址值
 */
STATIC INLINE UINT32 OsArmReadDbgvr2(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c2,4" : "=r"(val)); // 读取调试观察点值寄存器2到val
    return val;
}

/**
 * @brief   写入调试观察点值寄存器2
 * @details 写入CP14协处理器中第2号调试观察点的值寄存器
 * @param   val [IN] 要写入的观察点地址值
 * @note    汇编指令: mcr p14, 0, %0, c0,c2,4
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgvr2(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c2,4" ::"r"(val)); // 将val写入调试观察点值寄存器2
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试断点控制寄存器0
 * @details 读取CP14协处理器中第0号调试断点的控制寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c0,5
 *          用于配置断点0的触发条件和行为
 */
STATIC INLINE UINT32 OsArmReadDbgbcr0(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c0,5" : "=r"(val)); // 读取调试断点控制寄存器0到val
    return val;
}

/**
 * @brief   写入调试断点控制寄存器0
 * @details 写入CP14协处理器中第0号调试断点的控制寄存器
 * @param   val [IN] 要写入的断点控制值
 * @note    汇编指令: mcr p14, 0, %0, c0,c0,5
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgbcr0(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c0,5" ::"r"(val)); // 将val写入调试断点控制寄存器0
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试断点控制寄存器1
 * @details 读取CP14协处理器中第1号调试断点的控制寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c1,5
 *          用于配置断点1的触发条件和行为
 */
STATIC INLINE UINT32 OsArmReadDbgbcr1(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c1,5" : "=r"(val)); // 读取调试断点控制寄存器1到val
    return val;
}

/**
 * @brief   写入调试断点控制寄存器1
 * @details 写入CP14协处理器中第1号调试断点的控制寄存器
 * @param   val [IN] 要写入的断点控制值
 * @note    汇编指令: mcr p14, 0, %0, c0,c1,5
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgbcr1(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c1,5" ::"r"(val)); // 将val写入调试断点控制寄存器1
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试断点控制寄存器2
 * @details 读取CP14协处理器中第2号调试断点的控制寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c2,5
 *          用于配置断点2的触发条件和行为
 */
STATIC INLINE UINT32 OsArmReadDbgbcr2(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c2,5" : "=r"(val)); // 读取调试断点控制寄存器2到val
    return val;
}

/**
 * @brief   写入调试断点控制寄存器2
 * @details 写入CP14协处理器中第2号调试断点的控制寄存器
 * @param   val [IN] 要写入的断点控制值
 * @note    汇编指令: mcr p14, 0, %0, c0,c2,5
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgbcr2(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c2,5" ::"r"(val)); // 将val写入调试断点控制寄存器2
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试观察点值寄存器0
 * @details 读取CP14协处理器中第0号调试观察点的值寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c0,6
 *          用于存储观察点0监控的数据值
 */
STATIC INLINE UINT32 OsArmReadDbgwvr0(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c0,6" : "=r"(val)); // 读取调试观察点值寄存器0到val
    return val;
}

/**
 * @brief   写入调试观察点值寄存器0
 * @details 写入CP14协处理器中第0号调试观察点的值寄存器
 * @param   val [IN] 要写入的观察点数据值
 * @note    汇编指令: mcr p14, 0, %0, c0,c0,6
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgwvr0(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c0,6" ::"r"(val)); // 将val写入调试观察点值寄存器0
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试观察点值寄存器1
 * @details 读取CP14协处理器中第1号调试观察点的值寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c1,6
 *          用于存储观察点1监控的数据值
 */
STATIC INLINE UINT32 OsArmReadDbgwvr1(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c1,6" : "=r"(val)); // 读取调试观察点值寄存器1到val
    return val;
}

/**
 * @brief   写入调试观察点值寄存器1
 * @details 写入CP14协处理器中第1号调试观察点的值寄存器
 * @param   val [IN] 要写入的观察点数据值
 * @note    汇编指令: mcr p14, 0, %0, c0,c1,6
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgwvr1(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c1,6" ::"r"(val)); // 将val写入调试观察点值寄存器1
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试观察点控制寄存器0
 * @details 读取CP14协处理器中第0号调试观察点的控制寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c0,7
 *          用于配置观察点0的触发条件(读/写/访问)和数据掩码
 */
STATIC INLINE UINT32 OsArmReadDbgwcr0(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c0,7" : "=r"(val)); // 读取调试观察点控制寄存器0到val
    return val;
}

/**
 * @brief   写入调试观察点控制寄存器0
 * @details 写入CP14协处理器中第0号调试观察点的控制寄存器
 * @param   val [IN] 要写入的观察点控制值
 * @note    汇编指令: mcr p14, 0, %0, c0,c0,7
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgwcr0(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c0,7" ::"r"(val)); // 将val写入调试观察点控制寄存器0
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}

/**
 * @brief   读取调试观察点控制寄存器1
 * @details 读取CP14协处理器中第1号调试观察点的控制寄存器
 * @return  UINT32 寄存器值
 * @note    汇编指令: mrc p14, 0, %0, c0,c1,7
 *          用于配置观察点1的触发条件(读/写/访问)和数据掩码
 */
STATIC INLINE UINT32 OsArmReadDbgwcr1(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c0,c1,7" : "=r"(val)); // 读取调试观察点控制寄存器1到val
    return val;
}

/**
 * @brief   写入调试观察点控制寄存器1
 * @details 写入CP14协处理器中第1号调试观察点的控制寄存器
 * @param   val [IN] 要写入的观察点控制值
 * @note    汇编指令: mcr p14, 0, %0, c0,c1,7
 *          包含ISB指令确保指令同步
 */
STATIC INLINE VOID OsArmWriteDbgwcr1(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c0,c1,7" ::"r"(val)); // 将val写入调试观察点控制寄存器1
    __asm__ volatile("isb" ::: "memory"); // 指令同步屏障，确保之前的指令执行完成
}


/**
 * @brief   读取调试OS锁定访问寄存器
 * @details 获取OS调试锁定状态，用于控制对调试资源的访问权限
 * @return  寄存器当前值，bit0表示锁定状态（1=已锁定，0=未锁定）
 */
STATIC INLINE UINT32 OsArmReadDbgoslar(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c1,c0,4" : "=r"(val)); // 协处理器p14读取，c1=c0=4组合表示OS锁定访问寄存器
    return val;
}

/**
 * @brief   写入调试OS锁定访问寄存器
 * @details 设置OS调试锁定状态，操作系统可通过此寄存器控制调试器访问权限
 * @param   val [IN] 锁定值，写入0x1可获取锁定权
 */
STATIC INLINE VOID OsArmWriteDbgoslar(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c1,c0,4" ::"r"(val)); // 协处理器p14写入，c1=c0=4组合表示OS锁定访问寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障
}

/**
 * @brief   读取调试OS锁定状态寄存器
 * @details 获取当前调试锁定状态和所有权信息
 * @return  寄存器当前值，bit0表示锁定状态，bit1表示调试器是否请求锁定
 */
STATIC INLINE UINT32 OsArmReadDbgoslsr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c1,c1,4" : "=r"(val)); // 协处理器p14读取，c1=c1=4组合表示OS锁定状态寄存器
    return val;
}

/**
 * @brief 写入调试OS锁定状态寄存器(Debug OS Lock Status Register)
 * @param val 待写入的32位值
 * @note 该寄存器用于指示调试OS锁定的当前状态，ISB指令确保后续指令看到更新后的寄存器状态
 */
STATIC INLINE VOID OsArmWriteDbgoslsr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c1,c1,4" ::"r"(val));
    __asm__ volatile("isb" ::: "memory");                     // 指令同步屏障，确保寄存器写入完成
}

/**
 * @brief 读取调试OS安全状态恢复寄存器(Debug OS Secure Status Restore Register)
 * @return 32位寄存器值，包含安全状态恢复相关信息
 * @note 该寄存器用于在调试模式切换时恢复安全状态配置
 */
STATIC INLINE UINT32 OsArmReadDbgossrr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c1,c2,4" : "=r"(val)); // 调试协处理器(p14)操作，c1=c2=4组合表示DbgOSS RR寄存器
    return val;
}

/**
 * @brief 写入调试OS安全状态恢复寄存器(Debug OS Secure Status Restore Register)
 * @param val 待写入的32位值，包含安全状态恢复配置
 * @note ISB指令确保安全状态配置立即生效
 */
STATIC INLINE VOID OsArmWriteDbgossrr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c1,c2,4" ::"r"(val));
    __asm__ volatile("isb" ::: "memory");                     // 指令同步屏障，确保寄存器写入完成
}

/**
 * @brief 读取调试特权控制寄存器(Debug Privilege Control Register)
 * @return 32位寄存器值，包含特权级调试控制配置
 * @note 该寄存器控制调试操作的特权级别访问权限
 */
STATIC INLINE UINT32 OsArmReadDbgprcr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c1,c4,4" : "=r"(val)); // 调试协处理器(p14)操作，c1=c4=4组合表示DbgPRCR寄存器
    return val;
}

/**
 * @brief 写入调试特权控制寄存器(Debug Privilege Control Register)
 * @param val 待写入的32位值，配置特权级调试控制选项
 * @note 特权级调试配置可能影响系统安全性，需谨慎操作
 */
STATIC INLINE VOID OsArmWriteDbgprcr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c1,c4,4" ::"r"(val));
    __asm__ volatile("isb" ::: "memory");                     // 指令同步屏障，确保特权配置立即生效
}

/**
 * @brief 读取调试特权状态寄存器(Debug Privilege Status Register)
 * @return 32位寄存器值，包含当前特权级调试状态信息
 * @note 该寄存器反映调试操作的当前特权状态，只读
 */
STATIC INLINE UINT32 OsArmReadDbgprsr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c1,c5,4" : "=r"(val)); // 调试协处理器(p14)操作，c1=c5=4组合表示DbgPRSR寄存器
    return val;
}

/**
 * @brief 写入调试特权状态寄存器(Debug Privilege Status Register)
 * @param val 待写入的32位值，更新特权级调试状态
 * @note 该寄存器通常用于清除特定状态标志位
 */
STATIC INLINE VOID OsArmWriteDbgprsr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c1,c5,4" ::"r"(val));
    __asm__ volatile("isb" ::: "memory");                     // 指令同步屏障，确保状态更新立即生效
}

/**
 * @brief 读取调试所有权声明寄存器(Debug Claim Set Register)
 * @return 32位寄存器值，包含调试设备所有权声明状态
 * @note 该寄存器用于多核系统中声明调试资源的所有权
 */
STATIC INLINE UINT32 OsArmReadDbgclaimset(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c7,c8,6" : "=r"(val)); // 调试协处理器(p14)操作，c7=c8=6组合表示DbgClaimSet寄存器
    return val;
}

/**
 * @brief 写入调试所有权声明寄存器(Debug Claim Set Register)
 * @param val 待写入的32位值，声明对调试资源的所有权
 * @note 在多核调试场景中，需通过此寄存器协调调试资源访问
 */
STATIC INLINE VOID OsArmWriteDbgclaimset(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c7,c8,6" ::"r"(val));
    __asm__ volatile("isb" ::: "memory");                     // 指令同步屏障，确保所有权声明立即生效
}

/**
 * @brief 读取调试所有权清除寄存器(Debug Claim Clear Register)
 * @return 32位寄存器值，包含调试设备所有权清除状态
 * @note 该寄存器用于释放之前声明的调试资源所有权
 */
STATIC INLINE UINT32 OsArmReadDbgclaimclr(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c7,c9,6" : "=r"(val)); // 调试协处理器(p14)操作，c7=c9=6组合表示DbgClaimClr寄存器
    return val;
}

/**
 * @brief 写入调试所有权清除寄存器(Debug Claim Clear Register)
 * @param val 待写入的32位值，释放调试资源所有权
 * @note 释放调试资源前需确保相关调试操作已完成
 */
STATIC INLINE VOID OsArmWriteDbgclaimclr(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c7,c9,6" ::"r"(val));
    __asm__ volatile("isb" ::: "memory");                     // 指令同步屏障，确保所有权释放立即生效
}

/**
 * @brief 读取调试认证状态寄存器(Debug Authentication Status Register)
 * @return 32位寄存器值，包含调试认证状态信息
 * @note 该寄存器反映调试访问的认证状态，用于安全调试场景
 */
STATIC INLINE UINT32 OsArmReadDbgauthstatus(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c7,c14,6" : "=r"(val)); // 调试协处理器(p14)操作，c7=c14=6组合表示DbgAuthStatus寄存器
    return val;
}

/**
 * @brief 写入调试认证状态寄存器(Debug Authentication Status Register)
 * @param val 待写入的32位值，更新调试认证状态
 * @note 调试认证状态通常由硬件安全机制控制，软件写入可能受限
 */
STATIC INLINE VOID OsArmWriteDbgauthstatus(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c7,c14,6" ::"r"(val));
    __asm__ volatile("isb" ::: "memory");                      // 指令同步屏障，确保认证状态更新立即生效
}

/**
 * @brief 读取调试设备ID寄存器(Debug Device ID Register)
 * @return 32位寄存器值，包含调试设备的ID信息
 * @note 设备ID用于标识调试硬件的型号和版本，只读
 */
STATIC INLINE UINT32 OsArmReadDbgdevid(VOID)
{
    UINT32 val;
    __asm__ volatile("mrc p14, 0, %0, c7,c2,7" : "=r"(val)); // 协处理器p14读取，c7=c2=7组合表示设备ID寄存器
    return val;
}

/**
 * @brief   写入调试设备ID寄存器
 * @details 配置调试硬件的设备标识信息（通常由硬件初始化，软件无需修改）
 * @param   val [IN] 设备标识值
 */
STATIC INLINE VOID OsArmWriteDbgdevid(UINT32 val)
{
    __asm__ volatile("mcr p14, 0, %0, c7,c2,7" ::"r"(val)); // 协处理器p14写入，c7=c2=7组合表示设备ID寄存器
    __asm__ volatile("isb" ::: "memory");                  // 指令同步屏障
}
#endif /* __LOS_ARM_H__ */
