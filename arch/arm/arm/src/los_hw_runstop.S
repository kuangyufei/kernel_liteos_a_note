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

#include "arch_config.h" // 包含架构配置头文件，获取相关配置信息

    .equ MPIDR_CPUID_MASK, 0xffU @ 定义常量 MPIDR_CPUID_MASK，值为 0xffU，用于从 MPIDR 寄存器提取 CPU ID

    .extern  g_saveAR @ 声明外部变量 g_saveAR，表明该变量在其他文件中定义
    .extern  g_saveSRContext @ 声明外部变量 g_saveSRContext，表明该变量在其他文件中定义

    .global  OsSRSaveRegister @ 声明全局函数 OsSRSaveRegister，其他文件可调用该函数
    .global  OsSRRestoreRegister @ 声明全局函数 OsSRRestoreRegister，其他文件可调用该函数

    .fpu vfpv4 @ 指定使用 VFPv4 浮点运算单元
    @.fpu neon @ 注释掉的指令，若取消注释则指定使用 NEON 浮点运算单元
    .arch armv7a @ 指定使用 ARMv7-A 架构

    .text @ 切换到代码段

OsSRSaveRegister: @ 函数 OsSRSaveRegister 开始，用于保存寄存器状态
    PUSH    {R2} @ 将寄存器 R2 的值压入栈中保存
    LDR     R2, =g_saveAR @ 将全局变量 g_saveAR 的地址加载到寄存器 R2
    STR     R0, [R2] @ 将寄存器 R0 的值存储到 g_saveAR 指向的内存地址
    STR     R1, [R2, #4] @ 将寄存器 R1 的值存储到 g_saveAR 偏移 4 字节的内存地址
    POP     {R2} @ 从栈中弹出之前保存的 R2 的值到寄存器 R2

    MRC     P15, 0, R0, c0, c0, 5 @ 从协处理器 15 的寄存器 c0, c0, 5 读取 MPIDR 寄存器的值到 R0
    AND     R0, R0, #MPIDR_CPUID_MASK @ 将 R0 的值与 MPIDR_CPUID_MASK 进行按位与操作，提取 CPU ID
    MOV     R1, #72 @ 将立即数 72 移动到寄存器 R1，72 是任务上下文寄存器的总字节数（R0~R15, SPSR, CPSR）
    MUL     R1, R1, R0 @ 计算 R1 = R1 * R0，得到偏移量
    LDR     R0, =g_saveSRContext @ 将全局变量 g_saveSRContext 的地址加载到寄存器 R0
    ADD     R0, R0, R1 @ 将 g_saveSRContext 的地址加上偏移量
    ADD     R0, R0, #72 @ 再加上 72 字节的偏移量

    MOV     R1, SP @ 将栈指针 SP 的值移动到寄存器 R1
    STMFD   R0!, {R1} @ 将 R1 的值存储到 R0 指向的内存地址，然后 R0 自增 4 字节
    MRS     R1,  SPSR @ 将程序状态保存寄存器 SPSR 的值读取到寄存器 R1
    STMFD   R0!, {R1} @ 将 R1 的值存储到 R0 指向的内存地址，然后 R0 自增 4 字节
    MOV     R1,  LR @ 将链接寄存器 LR 的值移动到寄存器 R1
    STMFD   R0!, {R1} @ 将 R1 的值存储到 R0 指向的内存地址，然后 R0 自增 4 字节，保存 PC 值
    STMFD   R0!, {R1} @ 将 R1 的值存储到 R0 指向的内存地址，然后 R0 自增 4 字节，保存 LR 值
    STMFD   R0!, {R12} @ 将寄存器 R12 的值存储到 R0 指向的内存地址，然后 R0 自增 4 字节
    MOV     R12,  R0 @ 将 R0 的值移动到寄存器 R12

    LDR     R0, =g_saveAR @ 将全局变量 g_saveAR 的地址加载到寄存器 R0
    LDR     R0, [R0] @ 从 g_saveAR 指向的内存地址加载值到寄存器 R0
    LDR     R1, =g_saveAR @ 将全局变量 g_saveAR 的地址加载到寄存器 R1
    LDR     R1, [R1, #4] @ 从 g_saveAR 偏移 4 字节的内存地址加载值到寄存器 R1
    STMFD   R12!, {R0-R3} @ 将寄存器 R0 - R3 的值存储到 R12 指向的内存地址，然后 R12 自增 16 字节
    STMFD   R12!, {R4-R11} @ 将寄存器 R4 - R11 的值存储到 R12 指向的内存地址，然后 R12 自增 32 字节
    MRS     R0,  CPSR @ 将当前程序状态寄存器 CPSR 的值读取到寄存器 R0
    STMFD   R12!,  {R0} @ 将 R0 的值存储到 R12 指向的内存地址，然后 R12 自增 4 字节
    BX      LR @ 跳转到链接寄存器 LR 指向的地址，函数返回

OsSRRestoreRegister: @ 函数 OsSRRestoreRegister 开始，用于恢复寄存器状态
    MRC     P15, 0, R0, c0, c0, 5 @ 从协处理器 15 的寄存器 c0, c0, 5 读取 MPIDR 寄存器的值到 R0
    AND     R0, R0, #MPIDR_CPUID_MASK @ 将 R0 的值与 MPIDR_CPUID_MASK 进行按位与操作，提取 CPU ID
    MOV     R1, #72 @ 将立即数 72 移动到寄存器 R1，72 是任务上下文寄存器的总字节数（R0~R15, SPSR, CPSR）
    MUL     R1, R1, R0 @ 计算 R1 = R1 * R0，得到偏移量
    LDR     R12, =g_saveSRContext @ 将全局变量 g_saveSRContext 的地址加载到寄存器 R12
    ADD     R12, R12, R1 @ 将 g_saveSRContext 的地址加上偏移量
    LDMFD   R12!, {R0} @ 从 R12 指向的内存地址加载值到寄存器 R0，然后 R12 自增 4 字节
    MSR     CPSR_cxsf, R0 @ 将 R0 的值写入当前程序状态寄存器 CPSR
    LDMFD   R12!, {R4-R11} @ 从 R12 指向的内存地址加载寄存器 R4 - R11 的值，然后 R12 自增 32 字节
    LDMFD   R12!, {R0-R3} @ 从 R12 指向的内存地址加载寄存器 R0 - R3 的值，然后 R12 自增 16 字节
    PUSH    {R2} @ 将寄存器 R2 的值压入栈中保存
    LDR     R2, =g_saveAR @ 将全局变量 g_saveAR 的地址加载到寄存器 R2
    STR     R0, [R2] @ 将寄存器 R0 的值存储到 g_saveAR 指向的内存地址
    STR     R1, [R2, #4] @ 将寄存器 R1 的值存储到 g_saveAR 偏移 4 字节的内存地址
    POP     {R2} @ 从栈中弹出之前保存的 R2 的值到寄存器 R2
    MOV     R0, R12 @ 将 R12 的值移动到寄存器 R0
    LDMFD   R0!, {R12} @ 从 R0 指向的内存地址加载寄存器 R12 的值，然后 R0 自增 4 字节
    LDMFD   R0!, {R1} @ 从 R0 指向的内存地址加载寄存器 R1 的值，然后 R0 自增 4 字节，恢复 LR 值
    LDMFD   R0!, {R1} @ 从 R0 指向的内存地址加载寄存器 R1 的值，然后 R0 自增 4 字节，恢复 PC 值
    MOV     LR, R1 @ 将 R1 的值移动到链接寄存器 LR
    LDMFD   R0!, {R1} @ 从 R0 指向的内存地址加载寄存器 R1 的值，然后 R0 自增 4 字节
    MSR     SPSR_cxsf, R1 @ 将 R1 的值写入程序状态保存寄存器 SPSR
    LDMFD   R0!, {R1} @ 从 R0 指向的内存地址加载寄存器 R1 的值，然后 R0 自增 4 字节
    MOV     SP, R1 @ 将 R1 的值移动到栈指针 SP
    LDR     R0, =g_saveAR @ 将全局变量 g_saveAR 的地址加载到寄存器 R0
    LDR     R0, [R0] @ 从 g_saveAR 指向的内存地址加载值到寄存器 R0
    LDR     R1, =g_saveAR @ 将全局变量 g_saveAR 的地址加载到寄存器 R1
    LDR     R1, [R1, #4] @ 从 g_saveAR 偏移 4 字节的内存地址加载值到寄存器 R1
    BX      LR @ 跳转到链接寄存器 LR 指向的地址，函数返回

    .end @ 汇编文件结束