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
    .fpu        neon             @ 启用NEON浮点指令集
    .globl      memcmp           @ 导出memcmp函数
    .p2align    2                @ 函数对齐到4字节边界
    .type       memcmp,%function @ 定义memcmp为函数类型
    .code       32               @ 使用ARM 32位指令集
memcmp:
    @ 函数参数:
    @ r0 = 源字符串1地址 (str1)
    @ r1 = 源字符串2地址 (str2)
    @ r2 = 比较字节数 (count)
    .fnstart                    @ 函数开始标记
    push    {r4, r5, r6, r7, lr} @ 保存非volatile寄存器
    pld     [r0, #0]             @ 预加载str1数据到缓存
    pld     [r1, #0]             @ 预加载str2数据到缓存
    /**
     * 快速路径优化: 如果地址相同或比较长度为0，直接返回0
     */
    cmp     r0, r1               @ 比较两个字符串地址
    cmpne   r2, #0               @ 如果地址不同，检查比较长度
    beq     Lreturn_0            @ 地址相同或长度为0，返回0
    /**
     * 比较第一个字节，快速处理早期不匹配情况
     */
    ldrb    r3, [r0]             @ r3 = str1[0] (加载第一个字节)
    ldrb    r4, [r1]             @ r4 = str2[0] (加载第一个字节)
    pld     [r0, #64]            @ 预加载str1后续64字节数据
    pld     [r1, #64]            @ 预加载str2后续64字节数据
    cmp     r3, r4               @ 比较首字节
    subne   r0, r3, r4           @ 计算差值 (r3 - r4)
    bne     Lreturn              @ 首字节不同，跳转到返回

/**
 * 32字节块比较主循环 (使用NEON指令并行处理)
 * 每次比较32字节，利用NEON 128位寄存器实现SIMD并行比较
 */
L32_byte_cmp:
    cmp     r2, #32              @ 剩余字节数是否 >=32
    blo     L16_byte_cmp         @ 小于32字节，跳转到16字节比较
    sub     r2, r2, #32          @ 剩余字节数 -=32
    vld1.8      {d0 - d3}, [r0]! @ 从str1加载32字节到d0-d3 (每次加载8位)
    vld1.8      {d4 - d7}, [r1]! @ 从str2加载32字节到d4-d7
    vsub.i8     q0, q0, q2       @ q0 = d0-d1 - d4-d5 (计算前16字节差值)
    vsub.i8     q1, q1, q3       @ q1 = d2-d3 - d6-d7 (计算后16字节差值)
    pld     [r0, #64]            @ 预加载下一个缓存行数据
    pld     [r1, #64]            @ 预加载下一个缓存行数据

    vorr    d4, d0, d1           @ d4 = 前16字节差值的或运算结果 (检测是否有非0值)
    vorr    d5, d2, d3           @ d5 = 后16字节差值的或运算结果
    vorr    d6, d4, d5           @ d6 = 32字节整体比较结果 (非0表示存在差异)
    vmov    r3, r4, d6           @ 将d6的高低32位分别存入r3和r4
    orr     r5, r3, r4           @ r5 = 合并结果 (检查是否有非0差异)
    cmp     r5, #0               @ 是否所有字节都匹配
    beq     L32_byte_cmp         @ 全部匹配，继续比较下一个32字节块

/**
 * 32字节块比较发现差异，定位具体差异位置
 * r3用于标记前半部分(16字节)是否有差异，r4标记后半部分
 */
L32_byte_diff:
    vmov    r3, r4, d4           @ 将前16字节比较结果存入r3-r4
    orr     r3, r3, r4           @ 合并前16字节结果
    /**
     * 调整指针回退到当前块起始位置
     */
    sub     r0, #32              @ str1指针回退32字节
    sub     r1, #32              @ str2指针回退32字节
    cmp     r3, #0               @ 前16字节是否有差异
    addeq   r0, #16              @ 无差异，指针前进16字节(检查后半部分)
    addeq   r1, #16
    beq     L16_byte_diff_back   @ 跳转到后半16字节差异处理
    vmov    r3, r4, d0           @ 提取前8字节比较结果到r3-r4
    vmov    r5, r6, d1           @ 提取后8字节比较结果到r5-r6
    b       L16_byte_diff        @ 处理16字节差异

L16_byte_diff_back:
    vmov    r3, r4, d2           @ 提取后半16字节的前8字节结果
    vmov    r5, r6, d3           @ 提取后半16字节的后8字节结果

L16_byte_diff:
    orr     r7, r3, r4           @ 合并前8字节比较结果
    cmp     r7, #0               @ 前8字节是否有差异
    addeq   r0, #8               @ 无差异，指针前进8字节
    addeq   r1, #8
    beq     L8_byte_diff_back    @ 跳转到后8字节差异处理
    b       L8_byte_diff         @ 处理8字节差异

L8_byte_diff_back:
    mov     r3, r5               @ 取后8字节的前4字节结果
    mov     r4, r6               @ 取后8字节的后4字节结果

L8_byte_diff:
    cmp     r3, #0               @ 前4字节是否有差异
    addeq   r0, #4               @ 无差异，指针前进4字节
    addeq   r1, #4
    beq     L4_byte_diff         @ 跳转到4字节差异处理

L4_byte_diff:
    ldrb    r5, [r0], #1         @ 逐个字节比较，找到第一个差异
    ldrb    r6, [r1], #1
    subs    r5, r5, r6           @ 计算字节差值
    beq     L4_byte_diff         @ 相等则继续比较下一字节
    mov     r0, r5               @ 差异值存入r0作为返回值
    b       Lreturn              @ 跳转到返回

/**
 * 二分法处理小于32字节的情况
 */
L16_byte_cmp:
    cmp     r2, #16              @ 剩余字节数是否 >=16
    blo     L8_byte_cmp          @ 小于16字节，跳转到8字节比较
    sub     r2, r2, #16          @ 剩余字节数 -=16
    vld1.8      {d0 - d1}, [r0]! @ 加载16字节到d0-d1
    vld1.8      {d4 - d5}, [r1]! @ 加载16字节到d4-d5
    vsub.i8     q0, q0, q2       @ 计算16字节差值
    pld     [r0, #64]            @ 预加载后续数据
    pld     [r1, #64]

    vorr    d4, d0, d1           @ 合并16字节比较结果
    vmov    r3, r4, d4           @ 结果存入r3-r4
    orr     r3, r3, r4           @ 检查是否有差异
    cmp     r3, #0
    beq     L8_byte_cmp          @ 无差异，继续处理剩余字节
    sub     r0, #16              @ 有差异，指针回退16字节
    sub     r1, #16
    vmov    r3, r4, d0           @ 提取前8字节结果
    vmov    r5, r6, d1           @ 提取后8字节结果
    b       L16_byte_diff        @ 跳转到16字节差异处理

L8_byte_cmp:
    cmp     r2, #8               @ 剩余字节数是否 >=8
    blo     L4_byte_cmp          @ 小于8字节，跳转到4字节比较
    sub     r2, r2, #8           @ 剩余字节数 -=8
    vld1.8      {d0}, [r0]!      @ 加载8字节到d0
    vld1.8      {d4}, [r1]!      @ 加载8字节到d4
    vsub.i8     d0, d0, d4       @ 计算8字节差值

    vmov    r3, r4, d0           @ 结果存入r3-r4
    orr     r7, r3, r4           @ 检查是否有差异
    cmp     r7, #0
    beq     L4_byte_cmp          @ 无差异，继续处理剩余字节
    sub     r0, #8               @ 有差异，指针回退8字节
    sub     r1, #8
    b       L8_byte_diff         @ 跳转到8字节差异处理

L4_byte_cmp:
    cmp     r2, #4               @ 剩余字节数是否 >=4
    blo     Lless_4_byte_cmp     @ 小于4字节，跳转到单字节比较
    sub     r2, r2, #4           @ 剩余字节数 -=4
    ldr     r3, [r0], #4         @ 加载4字节整数
    ldr     r4, [r1], #4
    cmp     r3, r4               @ 比较4字节整数
    beq     Lless_4_byte_cmp     @ 无差异，继续处理剩余字节
    sub     r0, #4               @ 有差异，指针回退4字节
    sub     r1, #4
    b       L4_byte_diff         @ 跳转到4字节差异处理

Lless_4_byte_cmp:
    cmp     r2, #0               @ 剩余字节数是否为0
    beq     Lreturn_0            @ 比较完成且全部匹配，返回0
    sub     r2, r2, #1           @ 剩余字节数 -=1
    ldrb    r3, [r0], #1         @ 加载单字节
    ldrb    r4, [r1], #1
    sub     r5, r3, r4           @ 计算字节差值
    cmp     r5, #0               @ 比较字节
    movne   r0, r5               @ 差异值存入r0
    bne     Lreturn              @ 有差异，跳转到返回
    b       Lless_4_byte_cmp     @ 无差异，继续比较下一字节

Lreturn_0:
    mov     r0, #0               @ 返回值设为0 (全部匹配)
Lreturn:
    pop     {r4, r5, r6, r7, pc} @ 恢复寄存器并返回
Lfunc_end:
    .size   memcmp, Lfunc_end - memcmp @ 函数大小定义
    .cantunwind                  @ 禁止 unwind 展开
    .fnend                       @ -- End function