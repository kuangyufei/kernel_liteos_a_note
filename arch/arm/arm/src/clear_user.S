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
    push    {r0-r6, lr}
    cmp     r1, #0
    beq     .Lclear_user_return
    tst     r0, #7
    beq     .Lclear_user_aligned

.Lclear_user_byte:
    mov     r2, #0
0:  strb    r2, [r0], #1
    subs    r1, r1, #1
    beq     .Lclear_user_return
    tst     r0, #7
    bne     0b

.Lclear_user_aligned:
    eor     r3, r3
    eor     r4, r4
    bics    r2, r1, #15
    bne     .Lclear_user_16bytes
    bics    r2, r1, #7
    bne     .Lclear_user_8bytes
    b       .Lclear_user_remaining

.Lclear_user_16bytes:
    eor     r5, r5
    eor     r6, r6
1:  stmia   r0!, {r3, r4, r5, r6}
    subs    r2, r2, #16
    bne     1b
    ands    r1, r1, #15
    beq     .Lclear_user_return
    bics    r2, r1, #7
    beq     .Lclear_user_remaining

.Lclear_user_8bytes:
2:  stmia   r0!, {r3, r4}
    ands    r1, r1, #7
    beq     .Lclear_user_return

.Lclear_user_remaining:
    mov     r2, #0
3:  strb    r2, [r0], #1
    subs    r1, r1, #1
    bne     3b

.Lclear_user_return:
    pop     {r0-r6, lr}
    mov     r0, #0
    bx      lr

.Lclear_user_err:
    pop     {r0, r1}
    sub     r0, r2, r0
    sub     r0, r1, r0
    pop     {r2-r6, lr}
    bx      lr

.pushsection __exc_table, "a"
    .long   0b,  .Lclear_user_err
    .long   1b,  .Lclear_user_err
    .long   2b,  .Lclear_user_err
    .long   3b,  .Lclear_user_err
.popsection
