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

/*
实现 longjmp 和 setjmp 的 ARM 汇编代码
longjmp/setjmp 用于保存和恢复程序上下文，实现非局部跳转

longjmp(jmp_buf env, int val)
恢复由 setjmp 保存的寄存器环境，并返回 val（若 val 为 0 则返回 1）
*/
FUNCTION(longjmp)
        ldmfd   r0,{r4-r12}        @ 从 env 恢复 r4-r12
        add     r0,#(4 * 9)        @ r0 指向 env 中保存的 sp
        ldr     r13,[r0]           @ 恢复 sp (r13)
        add     r0,#4              @ r0 指向 env 中保存的 lr
        ldr     r14,[r0]           @ 恢复 lr (r14)
        cmp     r1,#0              @ 检查 val 是否为 0
        moveq   r1,#1              @ 如果 val 为 0，返回 1
        mov     r0,r1              @ 返回值放入 r0
        mov     pc,lr              @ 返回调用点

@ setjmp(jmp_buf env)
@ 保存当前寄存器环境到 env，返回 0
FUNCTION(setjmp)
        stmea   r0,{r4-r12}        @ 保存 r4-r12 到 env
        add     r0,#(4 * 9)        @ r0 指向 env 中保存 sp 的位置
        str     r13,[r0]           @ 保存 sp (r13)
        add     r0,#4              @ r0 指向 env 中保存 lr 的位置
        str     r14,[r0]           @ 保存 lr (r14)
        mov     r0,#0              @ 返回 0
        mov     pc,lr              @ 返回调用点
