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

#include "asm.h"

.syntax unified
.arm

// size_t _arm_clear_user(void *addr, size_t bytes)
FUNCTION(_arm_clear_user)
    push    {r0-r6, lr}          @ 保存寄存器r0-r6和链接寄存器lr
    cmp     r1, #0               @ 比较字节数是否为0
    beq     .Lclear_user_return  @ 如果为0，跳转返回
    tst     r0, #7               @ 测试地址是否8字节对齐
    beq     .Lclear_user_aligned @ 如果对齐，跳转对齐处理

.Lclear_user_byte:
    mov     r2, #0               @ 将0加载到r2，用于清零
0:  strb    r2, [r0], #1         @ 存储字节0到地址，地址自增1
    subs    r1, r1, #1           @ 字节数减1
    beq     .Lclear_user_return  @ 如果字节数为0，返回
    tst     r0, #7               @ 测试地址是否对齐
    bne     0b                   @ 如果未对齐，继续字节清零

.Lclear_user_aligned:
    eor     r3, r3               @ 清零r3
    eor     r4, r4               @ 清零r4
    bics    r2, r1, #15          @ 计算16字节块数
    bne     .Lclear_user_16bytes @ 如果有16字节块，跳转处理
    bics    r2, r1, #7           @ 计算8字节块数
    bne     .Lclear_user_8bytes  @ 如果有8字节块，跳转处理
    b       .Lclear_user_remaining @ 跳转处理剩余字节

.Lclear_user_16bytes:
    eor     r5, r5               @ 清零r5
    eor     r6, r6               @ 清零r6
1:  stmia   r0!, {r3, r4, r5, r6} @ 存储16字节(4个寄存器)并地址自增
    subs    r2, r2, #16          @ 减少16字节计数
    bne     1b                   @ 如果未完成，继续循环
    ands    r1, r1, #15          @ 计算剩余字节数
    beq     .Lclear_user_return  @ 如果无剩余，返回
    bics    r2, r1, #7           @ 计算8字节块数
    beq     .Lclear_user_remaining @ 如果无8字节块，跳转剩余处理

.Lclear_user_8bytes:
2:  stmia   r0!, {r3, r4}        @ 存储8字节(2个寄存器)并地址自增
    ands    r1, r1, #7           @ 计算剩余字节数
    beq     .Lclear_user_return  @ 如果无剩余，返回

.Lclear_user_remaining:
    mov     r2, #0               @ 清零r2
3:  strb    r2, [r0], #1        @ 存储字节0并地址自增
    subs    r1, r1, #1           @ 剩余字节数减1
    bne     3b                   @ 如果未完成，继续循环

.Lclear_user_return:
    pop     {r0-r6, lr}          @ 恢复寄存器r0-r6和lr
    mov     r0, #0               @ 返回值0
    bx      lr                   @ 返回调用者

.Lclear_user_err:
    pop     {r0, r1}             @ 异常处理，恢复r0和r1
    sub     r0, r2, r0           @ 计算错误偏移
    sub     r0, r1, r0           @ 计算错误偏移
    pop     {r2-r6, lr}          @ 恢复其他寄存器和lr
    bx      lr                   @ 返回调用者

.pushsection __exc_table, "a"
    .long   0b,  .Lclear_user_err @ 异常表，指向错误处理
    .long   1b,  .Lclear_user_err
    .long   2b,  .Lclear_user_err
    .long   3b,  .Lclear_user_err
.popsection
