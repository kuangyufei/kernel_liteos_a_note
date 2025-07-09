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

#include "asm.h"
/*********************************************
BEQ    相等
BNE    不等
BPL    非负
BMI    负
BCC    无进位
BCS    有进位
BLO    小于（无符号数）
BHS    大于等于（无符号数）
BHI    大于（无符号数）
BLS    小于等于（无符号数）
BVC    无溢出（有符号数）
BVS    有溢出（有符号数）
BGT    大于（有符号数）
BGE    大于等于（有符号数）
BLT    小于（有符号数）
BLE    小于等于（有符号数）
*********************************************/

.syntax unified                     @ 使用ARM统一汇编语法，兼容ARM和Thumb指令集
.arm                               @ 指定当前代码段使用ARM指令集（32位）

// errno_t _arm_get_user(void *dst, const void *src, size_t dstTypeLen, size_t srcTypeLen)
@ 功能：从用户空间安全读取数据到内核空间，支持多类型长度检查
@ 参数：
@   r0 - dst: 内核空间目标缓冲区地址
@   r1 - src: 用户空间源数据地址
@   r2 - dstTypeLen: 目标数据类型长度（字节）
@   r3 - srcTypeLen: 源数据类型长度（字节）
@ 返回：
@   0表示成功，-14(EFAULT)表示地址错误或类型不匹配
FUNCTION(_arm_get_user)
    stmdb   sp!, {r0, r1, r2, r3, lr} @ 函数栈帧构建：保存输入参数寄存器和返回地址lr
    cmp     r2, #0                    @ 检查目标类型长度是否为0（无效类型）
    beq     .Lget_user_return         @ 若长度为0，直接返回成功
    cmp     r2, r3                    @ 比较目标与源类型长度是否匹配
    bne     .Lget_user_err            @ 类型长度不匹配则跳转至错误处理
    cmp     r2, #1                    @ 判断数据类型长度是否大于1字节
    bhi     .Lget_user_half           @ 大于1字节则跳转至半字（2字节）处理
.Lget_user_byte:                     @ 字节（8位）数据读取处理
0:  ldrbt   r3, [r1], #0              @ 带用户权限检查的字节读取：
                                     @   ldrbt = Load Byte with User permission check
                                     @   自动处理用户空间地址验证，失败会触发异常
1:  strb    r3, [r0], #0              @ 将读取的字节存储到内核缓冲区
    b       .Lget_user_return         @ 跳转至返回流程
.Lget_user_half:                     @ 半字（16位）数据读取处理
    cmp     r2, #2                    @ 判断数据类型长度是否大于2字节
    bhi     .Lget_user_word           @ 大于2字节则跳转至字（4字节）处理
2:  ldrht   r3, [r1], #0              @ 带用户权限检查的半字读取：
                                     @   ldrht = Load Halfword with User permission check
3:  strh    r3, [r0], #0              @ 将读取的半字存储到内核缓冲区
    b       .Lget_user_return         @ 跳转至返回流程
.Lget_user_word:                     @ 字（32位）数据读取处理
    cmp     r2, #4                    @ 判断数据类型长度是否大于4字节
    bhi     .Lget_user_err            @ ARM32架构不支持>4字节的原生类型，跳转至错误
4:  ldrt   r3, [r1], #0               @ 带用户权限检查的字读取：
                                     @   ldrt = Load Word with User permission check
5:  str    r3, [r0], #0               @ 将读取的字存储到内核缓冲区
.Lget_user_return:                   @ 函数正常返回流程
    ldmia   sp!, {r0, r1, r2, r3, lr} @ 恢复寄存器现场
    mov     r0, 0                     @ 设置返回值为0（成功）
    bx      lr                        @ 返回到调用者
.Lget_user_err:                      @ 错误处理流程
    ldmia   sp!, {r0, r1, r2, r3, lr} @ 恢复寄存器现场
    mov     r0, #-14                  @ 设置返回值为-14（EFAULT：无效地址错误）
    bx      lr                        @ 返回到调用者

@ 异常处理表：将数据访问异常映射到错误处理流程
@ 当标记为0b-5b的指令发生内存访问异常时，自动跳转至.Lget_user_err
.pushsection __exc_table, "a"        @ 将以下内容放入__exc_table段（异常处理表）
    .long   0b,  .Lget_user_err       @ 0b: ldrbt指令异常处理
    .long   1b,  .Lget_user_err       @ 1b: strb指令异常处理
    .long   2b,  .Lget_user_err       @ 2b: ldrht指令异常处理
    .long   3b,  .Lget_user_err       @ 3b: strh指令异常处理
    .long   4b,  .Lget_user_err       @ 4b: ldrt指令异常处理
    .long   5b,  .Lget_user_err       @ 5b: str指令异常处理
.popsection                          @ 结束__exc_table段