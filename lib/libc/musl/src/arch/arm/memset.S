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

    .syntax     unified
    .arch       armv7-a
    .fpu        neon

#define FUNCTION(x) \
.globl x; \
.p2align 2; \
.type x,%function; \
x:

#if defined(LOSCFG_KERNEL_LMS)
FUNCTION(__memset)
#else
FUNCTION(memset)
#endif
    @ r0 = address
    @ r1 = char
    @ r2 = count
    @ returns original address in r0
    .fnstart

    push    {r4}
    cmp     r2, #0
    beq     Lreturn
    vdup.8  q0, r1
    mov     r4, r0              @ r4 = r0 = address

L64_byte_alignment:
    ands    r3, r0, #7
    beq     L64_byte_aligned
    rsb     r3, r3, #8          @ r3 = unalignedCnt = 8 - (address % 7)
    cmp     r2, r3
    movlo   r3, r2
    sub     r2, r2, r3

Lloop1:
    strb    r1, [r4], #1
    subs    r3, r3, #1
    bgt     Lloop1

/**
 * Set 64 bytes each time, and use floating-point registers to improve efficiency.
 */
L64_byte_aligned:
    vmov    q1, q0
    vmov    q2, q0
    cmp     r2, #64
    blo     L32_byte_aligned
    vmov    q3, q0
    sub     r2, r2, #64
Lloop2:
    vstmia  r4!, {d0 - d7}
    subs    r2, r2, #64
    bgt     Lloop2

/**
 * The dichotomy handles the case of less than 64 bytes,
 * and the front will subtract 64 more, and you need to make it up at this time.
 */
    add     r2, r2, #64
L32_byte_aligned:
    cmp     r2, #0
    beq     Lreturn
    cmp     r2, #32
    blo     L16_byte_aligned
    sub     r2, r2, #32
    vstmia  r4!, {d0 - d3}

L16_byte_aligned:
    cmp     r2, #0
    beq     Lreturn
    cmp     r2, #16
    blo     L8_byte_aligned
    sub     r2, r2, #16
    vstmia  r4!, {d0 - d1}

L8_byte_aligned:
    cmp     r2, #0
    beq     Lreturn
    cmp     r2, #8
    blo     L4_byte_aligned
    sub     r2, r2, #8
    vstmia  r4!, {d0}

L4_byte_aligned:
    cmp     r2, #0
    beq     Lreturn
    cmp     r2, #4
    blo     Lless_4_byte
    sub     r2, r2, #4
    vst1.32 {d0[0]}, [r4]!

Lless_4_byte:
    cmp     r2, #0
    beq     Lreturn
    strb    r1, [r4], #1
    sub     r2, r2, #1
    b       Lless_4_byte

Lreturn:
    pop     {r4}
    bx      lr
Lfunc_end:
    .size memset, Lfunc_end - memset
    .cantunwind
    .fnend                      @ -- End function
