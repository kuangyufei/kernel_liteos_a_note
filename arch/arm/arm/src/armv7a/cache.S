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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES@ LOSS OF USE, DATA, OR PROFITS@
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// 包含汇编相关的头文件
#include "asm.h"

// 指定使用 VFPv4 浮点运算单元
    .fpu vfpv4
// 指定使用 ARMv7-A 架构
    .arch armv7a

// 定义一个宏，用于获取数据缓存行的大小
// @param reg 用于存储缓存行大小的寄存器
// @param tmp 临时寄存器，用于中间计算
.macro  DCACHE_LINE_SIZE, reg, tmp
    mrc     p15, 0, \tmp, c0, c0, 1  @ 从协处理器 15 的寄存器 c0, c0, 1 读取处理器特征信息到临时寄存器 \tmp
    lsr     \tmp, \tmp, #16          @ 将 \tmp 逻辑右移 16 位，提取出缓存相关的信息
    and     \tmp, \tmp, #0xf         @ 将 \tmp 与 0xf 进行按位与操作，得到缓存行大小的指数部分
    mov     \reg, #4                 @ 初始将 \reg 设置为 4
    mov     \reg, \reg, lsl \tmp     @ 将 \reg 逻辑左移 \tmp 位，计算出实际的缓存行大小
.endm

// 定义函数 arm_inv_cache_range，用于使指定范围的缓存无效
FUNCTION(arm_inv_cache_range)
    push    {r2, r3}                 @ 将 r2 和 r3 寄存器的值压入栈中保存
    DCACHE_LINE_SIZE r2, r3          @ 调用 DCACHE_LINE_SIZE 宏，获取数据缓存行的大小，结果存于 r2
    sub    r3, r2, #1                @ 计算 r3 = r2 - 1，用于后续的地址对齐操作
    tst    r0, r3                    @ 测试 r0 和 r3 的按位与结果，判断 r0 是否对齐到缓存行边界
    bic    r0, r0, r3                @ 清除 r0 中与 r3 对应位为 1 的部分，使 r0 对齐到缓存行边界
    mcrne  p15, 0, r0, c7, c14, 1    @ 如果 r0 之前未对齐（NE 标志置位），则将 r0 地址的缓存无效化
    tst    r1, r3                    @ 测试 r1 和 r3 的按位与结果，判断 r1 是否对齐到缓存行边界
    bic    r1, r1, r3                @ 清除 r1 中与 r3 对应位为 1 的部分，使 r1 对齐到缓存行边界
    mcrne  p15, 0, r1, c7, c14, 1    @ 如果 r1 之前未对齐（NE 标志置位），则将 r1 地址的缓存无效化
1:
    mcr    p15, 0,  r0, c7, c6, 1    @ 将 r0 地址的缓存无效化
    add    r0,  r0, r2               @ r0 = r0 + r2，指向下一个缓存行
    cmp    r0,  r1                   @ 比较 r0 和 r1 的大小
    blo    1b                        @ 如果 r0 < r1，跳转到标号 1 继续执行
    dsb                              @ 数据同步屏障，确保缓存操作完成
    pop    {r2, r3}                  @ 从栈中弹出之前保存的 r2 和 r3 寄存器的值
    mov    pc, lr                    @ 函数返回，将链接寄存器 lr 的值赋给程序计数器 pc

// 定义函数 arm_clean_cache_range，用于清理指定范围的缓存
FUNCTION(arm_clean_cache_range)
    push   {r2, r3}                  @ 将 r2 和 r3 寄存器的值压入栈中保存
    DCACHE_LINE_SIZE r2, r3          @ 调用 DCACHE_LINE_SIZE 宏，获取数据缓存行的大小，结果存于 r2
    sub    r3, r2, #1                @ 计算 r3 = r2 - 1，用于后续的地址对齐操作
    bic    r0, r0, r3                @ 清除 r0 中与 r3 对应位为 1 的部分，使 r0 对齐到缓存行边界
1:
    mcr    p15, 0,  r0, c7, c10, 1   @ 将 r0 地址的缓存数据写回内存
    add    r0,  r0, r2               @ r0 = r0 + r2，指向下一个缓存行
    cmp    r0,  r1                   @ 比较 r0 和 r1 的大小
    blo    1b                        @ 如果 r0 < r1，跳转到标号 1 继续执行
    dsb                              @ 数据同步屏障，确保缓存操作完成
    pop    {r2, r3}                  @ 从栈中弹出之前保存的 r2 和 r3 寄存器的值
    mov    pc, lr                    @ 函数返回，将链接寄存器 lr 的值赋给程序计数器 pc