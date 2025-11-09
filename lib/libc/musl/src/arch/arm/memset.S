/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

    .syntax     unified          @ 使用统一汇编语法
    .arch       armv7-a          @ 目标架构为ARMv7-A
    .fpu        neon             @ 启用NEON浮点指令集，支持SIMD操作

@ 宏定义：声明函数符号并设置对齐
#define FUNCTION(x) \
.globl x;           \  // 导出函数x为全局可见
.p2align 2;         \  // 函数对齐到4字节边界
.type x,%function;  \  // 定义x为函数类型
x:                 // 函数入口标签

@ 根据LOSCFG_KERNEL_LMS配置选择函数名
#if defined(LOSCFG_KERNEL_LMS)
FUNCTION(__memset)   // LMS模式下使用__memset符号
#else
FUNCTION(memset)     // 标准模式下使用memset符号
#endif
    @ 函数参数说明：
    @ r0 = 目标内存地址 (dest)
    @ r1 = 填充字节值 (char)
    @ r2 = 填充字节数 (count)
    @ 返回值：r0 = 原始目标地址
    .fnstart                    @ 函数开始标记

    push    {r4}                 @ 保存非volatile寄存器r4（调用者需要保存）
    cmp     r2, #0               @ 检查填充字节数是否为0
    beq     Lreturn              @ 若count=0，直接返回
    vdup.8  q0, r1               @ 将r1的8位字节值复制到q0寄存器的所有8位元素（16字节）
    mov     r4, r0               @ r4 = 保存原始目标地址（用于返回）

@ 处理地址未按8字节对齐的情况
L64_byte_alignment:
    ands    r3, r0, #7           @ r3 = 目标地址的低3位（检查8字节对齐）
    beq     L64_byte_aligned     @ 若地址已对齐，跳转到64字节填充逻辑
    rsb     r3, r3, #8           @ r3 = 8 - (地址%8)，计算对齐所需字节数（1-7字节）
    cmp     r2, r3               @ 比较剩余字节数与对齐所需字节数
    movlo   r3, r2               @ 若剩余字节不足，仅填充剩余部分
    sub     r2, r2, r3           @ 更新剩余字节数（count -= 对齐字节数）

@ 逐个字节填充以实现地址对齐
Lloop1:
    strb    r1, [r4], #1         @ 存储一个字节到目标地址，并递增指针
    subs    r3, r3, #1           @ 对齐所需字节数递减
    bgt     Lloop1               @ 未完成则继续填充

/**
 * 64字节块填充主循环（使用NEON指令并行处理）
 * 每次填充64字节，利用8个64位NEON寄存器(d0-d7)实现高效批量写入
 */
L64_byte_aligned:
    vmov    q1, q0               @ 复制q0到q1（q0-q1共32字节）
    vmov    q2, q0               @ 复制q0到q2（q0-q2共48字节）
    cmp     r2, #64              @ 剩余字节数是否 >=64
    blo     L32_byte_aligned     @ 小于64字节，跳转到32字节填充
    vmov    q3, q0               @ 复制q0到q3（q0-q3共64字节）
    sub     r2, r2, #64          @ 剩余字节数 -=64
Lloop2:
    vstmia  r4!, {d0 - d7}       @ 批量存储d0-d7寄存器（共8*8=64字节）到目标地址
    subs    r2, r2, #64          @ 剩余字节数 -=64
    bgt     Lloop2               @ 剩余字节>64则继续循环

/**
 * 二分法处理剩余字节（小于64字节）
 * 前面多减了64字节，此处需要补回
 */
    add     r2, r2, #64          @ 恢复剩余字节数（修正前面的多减操作）
L32_byte_aligned:
    cmp     r2, #0               @ 检查剩余字节数
    beq     Lreturn              @ 若为0，跳转到返回
    cmp     r2, #32              @ 剩余字节数是否 >=32
    blo     L16_byte_aligned     @ 小于32字节，跳转到16字节填充
    sub     r2, r2, #32          @ 剩余字节数 -=32
    vstmia  r4!, {d0 - d3}       @ 存储d0-d3（4*8=32字节）

L16_byte_aligned:
    cmp     r2, #0               @ 检查剩余字节数
    beq     Lreturn              @ 若为0，跳转到返回
    cmp     r2, #16              @ 剩余字节数是否 >=16
    blo     L8_byte_aligned      @ 小于16字节，跳转到8字节填充
    sub     r2, r2, #16          @ 剩余字节数 -=16
    vstmia  r4!, {d0 - d1}       @ 存储d0-d1（2*8=16字节）

L8_byte_aligned:
    cmp     r2, #0               @ 检查剩余字节数
    beq     Lreturn              @ 若为0，跳转到返回
    cmp     r2, #8               @ 剩余字节数是否 >=8
    blo     L4_byte_aligned      @ 小于8字节，跳转到4字节填充
    sub     r2, r2, #8           @ 剩余字节数 -=8
    vstmia  r4!, {d0}            @ 存储d0（8字节）

L4_byte_aligned:
    cmp     r2, #0               @ 检查剩余字节数
    beq     Lreturn              @ 若为0，跳转到返回
    cmp     r2, #4               @ 剩余字节数是否 >=4
    blo     Lless_4_byte         @ 小于4字节，跳转到单字节填充
    sub     r2, r2, #4           @ 剩余字节数 -=4
    vst1.32 {d0[0]}, [r4]!       @ 存储d0的低32位（4字节）到目标地址

@ 处理剩余1-3字节
Lless_4_byte:
    cmp     r2, #0               @ 检查剩余字节数
    beq     Lreturn              @ 若为0，跳转到返回
    strb    r1, [r4], #1         @ 存储一个字节并递增指针
    sub     r2, r2, #1           @ 剩余字节数 -=1
    b       Lless_4_byte         @ 继续处理直到剩余字节为0

@ 函数返回
Lreturn:
    pop     {r4}                 @ 恢复r4寄存器
    bx      lr                   @ 返回调用者（r0仍为原始目标地址）
Lfunc_end:
@ 根据配置定义函数大小
#if defined(LOSCFG_KERNEL_LMS)
    .size __memset, Lfunc_end - __memset
#else
    .size memset, Lfunc_end - memset
#endif
    .cantunwind                  @ 禁止异常展开（优化编译选项）
    .fnend                       @ -- End function