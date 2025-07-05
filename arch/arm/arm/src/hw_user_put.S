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

.syntax unified
.arm
//将内核空间数据拷贝用户空间汇编实现
// errno_t _arm_put_user(void *dst, const void *src, size_t dstTypeLen, size_t srcTypeLen)
FUNCTION(_arm_put_user)
    stmdb   sp!, {r0, r1, r2, r3, lr}      @ 保存现场，入栈r0-r3和lr
    cmp     r2, #0                         @ 检查长度是否为0
    beq     .Lget_user_return              @ 长度为0，直接返回成功
    cmp     r2, r3                         @ 检查目标长度和源长度是否一致
    bne     .Lget_user_err                 @ 不一致，返回错误
    cmp     r2, #1                         @ 判断长度类型
    bhi     .Lget_user_half                @ 长度大于1，跳转处理半字（2字节）

.Lget_user_byte:
0:  ldrb    r3, [r1], #0                   @ 按字节从src读取到r3
1:  strbt   r3, [r0], #0                   @ 按字节写入到dst
    b       .Lget_user_return              @ 完成后返回

.Lget_user_half:
    cmp     r2, #2                         @ 判断是否为2字节
    bhi     .Lget_user_word                @ 大于2字节，跳转处理字（4字节）
2:  ldrh    r3, [r1], #0                   @ 按半字从src读取到r3
3:  strht   r3, [r0], #0                   @ 按半字写入到dst
    b       .Lget_user_return              @ 完成后返回

.Lget_user_word:
    cmp     r2, #4                         @ 判断是否为4字节
    bhi     .Lget_user_err                 @ 大于4字节，不支持，返回错误
4:  ldr    r3, [r1], #0                    @ 按字从src读取到r3
5:  strt   r3, [r0], #0                    @ 按字写入到dst

.Lget_user_return:
    ldmia   sp!, {r0, r1, r2, r3, lr}      @ 恢复现场
    mov     r0, 0                          @ 返回0表示成功
    bx      lr                             @ 返回

.Lget_user_err:
    ldmia   sp!, {r0, r1, r2, r3, lr}      @ 恢复现场
    mov     r0, #-14                       @ 返回-14（EFAULT，内存访问错误）
    bx      lr                             @ 返回

.pushsection __exc_table, "a"
    .long   0b,  .Lget_user_err            @ ldrb异常处理
    .long   1b,  .Lget_user_err            @ strbt异常处理
    .long   2b,  .Lget_user_err            @ ldrh异常处理
    .long   3b,  .Lget_user_err            @ strht异常处理
    .long   4b,  .Lget_user_err            @ ldr异常处理
    .long   5b,  .Lget_user_err            @ strt异常处理
.popsection