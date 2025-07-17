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
    /*
     * 声明VDSO文本段起始地址为全局符号，供内核其他模块引用
     * 全局符号使得链接器和内核可以定位VDSO代码的起始位置
     */
    .globl __vdso_text_start
    /*
     * 定义VDSO文本数据节区
     * .data.vdso.text节区用于存储VDSO（虚拟动态共享对象）的可执行代码
     * 该节区通常具有特殊的内存保护属性（如只读/可执行）
     */
    .section .data.vdso.text
/*
 * VDSO文本段起始地址标签
 * 作为VDSO代码的基准地址，内核通过此标签计算VDSO在用户空间的映射位置
 */
__vdso_text_start:
    /*
     * 二进制包含VDSO共享对象文件
     * OHOS_VDSO_SO为预编译的VDSO二进制文件路径（通常由构建系统定义）
     * 此指令将VDSO二进制内容直接嵌入到当前节区，避免运行时动态加载
     */
    .incbin OHOS_VDSO_SO
