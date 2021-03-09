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
.syntax unified
.arm

// errno_t _arm_get_user(void *dst, const void *src, size_t dstTypeLen, size_t srcTypeLen)
FUNCTION(_arm_get_user)
    stmdb   sp!, {r0, r1, r2, r3, lr} @四个参数入栈,保存LR
    cmp     r2, #0	@r2 和 0比较
    beq     .Lget_user_return @相等 跳到Lget_user_return 直接返回
    cmp     r2, r3	@r2 和 r3比较
    bne     .Lget_user_err	@不等,说明函数要返回错误
    cmp     r2, #1	@r2 和 1比较
    bhi     .Lget_user_half @if(dstTypeLen>1) 跳转到Lget_user_half
.Lget_user_byte:		 @按字节拷贝数据
0:  ldrbt   r3, [r1], #0 @r3=*r1
1:  strb    r3, [r0], #0 @*r0=r3
    b       .Lget_user_return 
.Lget_user_half:
    cmp     r2, #2	@r2 和 2比较
    bhi     .Lget_user_word  @if(dstTypeLen>2) Lget_user_word
2:  ldrht   r3, [r1], #0 	@完成最后一个字节的拷贝
3:  strh    r3, [r0], #0	@完成最后一个字节的拷贝
    b       .Lget_user_return
.Lget_user_word:
    cmp     r2, #4 @r2 和 4比较
    bhi     .Lget_user_err @if(dstTypeLen>4) 跳转到Lget_user_err
4:  ldrt   r3, [r1], #0
5:  str    r3, [r0], #0
.Lget_user_return:	@返回锚点
    ldmia   sp!, {r0, r1, r2, r3, lr} 	@保存的内容出栈,恢复各寄存器值
    mov     r0, 0	@r0保存返回值为0
    bx      lr	@跳回调用函数继续执行,_arm_get_user到此结束!
.Lget_user_err:
    ldmia   sp!, {r0, r1, r2, r3, lr}	@保存的内容出栈,恢复各寄存器值
    mov     r0, #-14	@r0保存返回值为-14	
    bx      lr	@跳回调用函数继续执行,_arm_get_user到此结束!

.pushsection __exc_table, "a"
    .long   0b,  .Lget_user_err
    .long   1b,  .Lget_user_err
    .long   2b,  .Lget_user_err
    .long   3b,  .Lget_user_err
    .long   4b,  .Lget_user_err
    .long   5b,  .Lget_user_err
.popsection
