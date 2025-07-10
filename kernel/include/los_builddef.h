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

#ifndef _LOS_BUILDEF_H
#define _LOS_BUILDEF_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* 字节序定义 */
#define OS_LITTLE_ENDIAN 0x1234 /* 小端字节序标识，十六进制值0x1234对应十进制4660 */
#define OS_BIG_ENDIAN    0x4321 /* 大端字节序标识，十六进制值0x4321对应十进制17185 */

/* 字节序配置，若未定义则默认使用小端字节序 */
#ifndef OS_BYTE_ORDER
#define OS_BYTE_ORDER OS_LITTLE_ENDIAN
#endif

/* 定义OS代码数据段属性宏 */
/* 函数内联指示符 */
#ifndef LITE_OS_SEC_ALW_INLINE
#define LITE_OS_SEC_ALW_INLINE  /* 强制内联属性，对应编译器属性__attribute__((always_inline)) */
#endif

#ifndef LITE_OS_SEC_TEXT
#define LITE_OS_SEC_TEXT        /* SRAM文本段，代码存放于SRAM中，对应编译器属性__attribute__((section(".text.sram"))) */
#endif

#ifndef LITE_OS_SEC_TEXT_MINOR
#define LITE_OS_SEC_TEXT_MINOR  /* DDR文本段，代码存放于DDR中，对应编译器属性__attribute__((section(".text.ddr"))) */
#endif

#ifndef LITE_OS_SEC_TEXT_INIT
#define LITE_OS_SEC_TEXT_INIT   /* 初始化文本段，启动初始化代码存放区域，对应编译器属性__attribute__((section(".text.init"))) */
#endif

#ifndef LITE_OS_SEC_DATA
#define LITE_OS_SEC_DATA        /* SRAM数据段，已初始化数据存放于SRAM中，对应编译器属性__attribute__((section(".data.sram"))) */
#endif

#ifndef LITE_OS_SEC_DATA_MINOR
#define LITE_OS_SEC_DATA_MINOR  /* DDR数据段，已初始化数据存放于DDR中，对应编译器属性__attribute__((section(".data.ddr"))) */
#endif

#ifndef LITE_OS_SEC_DATA_INIT
#define LITE_OS_SEC_DATA_INIT   /* 初始化数据段，启动初始化数据存放区域，对应编译器属性__attribute__((section(".data.init"))) */
#endif

#ifndef LITE_OS_SEC_BSS
#define LITE_OS_SEC_BSS         /* SRAM BSS段，未初始化数据存放于SRAM中，对应编译器属性__attribute__((section(".bss.sram"))) */
#endif

#ifndef LITE_OS_SEC_BSS_MINOR
#define LITE_OS_SEC_BSS_MINOR   /* DDR BSS段，未初始化数据存放于DDR中，对应编译器属性__attribute__((section(".bss.ddr"))) */
#endif

#ifndef LITE_OS_SEC_BSS_INIT
#define LITE_OS_SEC_BSS_INIT    /* 初始化BSS段，启动初始化未初始化数据存放区域，对应编译器属性__attribute__((section(".bss.init"))) */
#endif

#ifndef LITE_OS_SEC_ITCM
#define LITE_OS_SEC_ITCM        /* ITCM(指令紧密耦合内存)段，对应编译器属性__attribute__((section(".itcm "))) */
#endif
#ifndef LITE_OS_SEC_DTCM
#define LITE_OS_SEC_DTCM        /* DTCM(数据紧密耦合内存)段，对应编译器属性__attribute__((section(".dtcm"))) */
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_BUILDEF_H */
