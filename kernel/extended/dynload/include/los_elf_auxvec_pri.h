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
http://articles.manugarg.com/aboutelfauxiliaryvectors.html

ELF辅助向量是一种将某些内核级别信息传输到用户进程的机制。此类信息的一个示例是指向内存中系统调用
入口点的指针（AT_SYSINFO）。该信息本质上是动态的，只有在内核完成加载后才知道。
信息由二进制加载程序传递给用户进程，二进制加载程序是内核子系统本身的一部分。内置内核或内核模块。
二进制加载程序将二进制文件（程序）转换为系统上的进程。通常，每种二进制格式都有一个不同的加载器；
值得庆幸的是，二进制格式并不多-大多数基于linux的系统现在都使用ELF二进制文件。
ELF二进制加载器在以下文件 /usr/src/linux/fs/binfmt_elf.c中定义。

ELF加载器解析ELF文件，在内存中映射各个程序段，设置入口点并初始化进程堆栈。
它将ELF辅助向量与其他信息（如argc，argv，envp）一起放在进程堆栈中。初始化后，进程的堆栈如下所示：
*/
/**
 * @file los_elf_auxvec_pri.h
 * @brief ELF辅助向量（Auxiliary Vector）私有头文件
 */
#ifndef _LOS_ELF_AUXVEC_H  // 防止头文件重复包含
#define _LOS_ELF_AUXVEC_H

#define AUX_VECTOR_SIZE_ARCH                 1  // 架构相关的辅助向量数量
#define AUX_VECTOR_SIZE_BASE                 20  // 基础辅助向量数量
#define AUX_VECTOR_SIZE                      ((AUX_VECTOR_SIZE_ARCH + AUX_VECTOR_SIZE_BASE + 1) << 1)  // 辅助向量总大小（每个向量包含id和value，因此×2）


/* 辅助向量类型定义 */
#define AUX_NULL             0  // 辅助向量结束标记
#define AUX_IGNORE           1  // 忽略此条目
#define AUX_EXECFD           2  // 可执行文件的文件描述符
#define AUX_PHDR             3  // 程序头表（Program Header Table）地址
#define AUX_PHENT            4  // 程序头表条目大小
#define AUX_PHNUM            5  // 程序头表条目数量
#define AUX_PAGESZ           6  // 系统页面大小
#define AUX_BASE             7  // 程序加载基地址
#define AUX_FLAGS            8  // 标志位
#define AUX_ENTRY            9  // 程序入口地址
#define AUX_NOTELF           10  // 非ELF格式标记
#define AUX_UID              11  // 用户ID
#define AUX_EUID             12  // 有效用户ID
#define AUX_GID              13  // 组ID
#define AUX_EGID             14  // 有效组ID
#define AUX_PLATFORM         15  // 平台名称
#define AUX_HWCAP            16  // 硬件能力标志
#define AUX_CLKTCK           17  // 时钟滴答频率
#define AUX_SECURE           23  // 安全模式标志
#define AUX_BASE_PLATFORM    24  // 基础平台名称
#define AUX_RANDOM           25  // 随机数种子
#define AUX_HWCAP2           26  // 扩展硬件能力标志
#define AUX_EXECFN           31  // 可执行文件名
#define AUX_SYSINFO_EHDR     33  // 系统信息头地址

#define AUX_VEC_ENTRY(vecEnt, vecId, auxId, auxVal) do { \
    (vecEnt)[(vecId)++] = (auxId);    \ // 设置辅助向量ID
    (vecEnt)[(vecId)++] = (auxVal);   \ // 设置辅助向量值
} while (0)  // 循环体为空，用于确保宏定义作为单个语句执行
#endif /* _LOS_ELF_AUXVEC_H */
