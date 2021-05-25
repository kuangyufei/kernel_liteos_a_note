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

#ifndef _LOS_ELF_AUXVEC_H
#define _LOS_ELF_AUXVEC_H

#define AUX_VECTOR_SIZE_ARCH                 1
#define AUX_VECTOR_SIZE_BASE                 20
#define AUX_VECTOR_SIZE                      ((AUX_VECTOR_SIZE_ARCH + AUX_VECTOR_SIZE_BASE + 1) << 1)

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
/* AUX VECTOR *//* Legal values for a_type (entry type).  */
#define AUX_NULL         	0               /* End of vector */
#define AUX_IGNORE       	1               /* Entry should be ignored */
#define AUX_EXECFD       	2               /* File descriptor of program */
#define AUX_PHDR         	3               /* Program headers for program */
#define AUX_PHENT        	4               /* Size of program header entry */
#define AUX_PHNUM        	5               /* Number of program headers */
#define AUX_PAGESZ       	6               /* System page size */
#define AUX_BASE         	7               /* Base address of interpreter */
#define AUX_FLAGS        	8               /* Flags */
#define AUX_ENTRY        	9               /* Entry point of program */
#define AUX_NOTELF       	10              /* Program is not ELF */
#define AUX_UID          	11              /* Real uid */
#define AUX_EUID         	12              /* Effective uid */
#define AUX_GID          	13              /* Real gid */
#define AUX_EGID         	14              /* Effective gid */
#define AUX_PLATFORM         15
#define AUX_HWCAP            16
#define AUX_CLKTCK       	 17              /* Frequency of times() */
#define AUX_SECURE           23
#define AUX_BASE_PLATFORM    24
#define AUX_RANDOM           25
#define AUX_HWCAP2           26
#define AUX_EXECFN           31
#define AUX_SYSINFO_EHDR     33

#define AUX_VEC_ENTRY(vecEnt, vecId, auxId, auxVal) do { \
    (vecEnt)[(vecId)++] = (auxId); \
    (vecEnt)[(vecId)++] = (auxVal); \
} while (0)

#endif /* _LOS_ELF_AUXVEC_H */
