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

#ifndef _LOS_LD_ELH_PRI_H
#define _LOS_LD_ELH_PRI_H

#include "los_base.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* 举例: shell elf 
root@5e3abe332c5a:/home/harmony/out/hispark_aries/ipcamera_hispark_aries/bin# readelf -h shell
ELF Header:
  Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF32
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              DYN (Shared object file)
  Machine:                           ARM
  Version:                           0x1
  Entry point address:               0x1000
  Start of program headers:          52 (bytes into file)
  Start of section headers:          25268 (bytes into file)
  Flags:                             0x5000200, Version5 EABI, soft-float ABI
  Size of this header:               52 (bytes)
  Size of program headers:           32 (bytes)
  Number of program headers:         11
  Size of section headers:           40 (bytes)
  Number of section headers:         27
  Section header string table index: 26
*/

/* Elf header */
#define LD_EI_NIDENT           16  // ELF标识字段的长度，固定为16字节

typedef struct {
    UINT8       elfIdent[LD_EI_NIDENT]; // ELF文件标识数组，包含魔术数字和格式信息
    UINT16      elfType;                // ELF文件类型（可执行文件/共享库等）
    UINT16      elfMachine;             // 目标架构类型
    UINT32      elfVersion;             // ELF文件版本
    UINT32      elfEntry;               // 程序入口点虚拟地址
    UINT32      elfPhoff;               // 程序头表在文件中的偏移量
    UINT32      elfShoff;               // 节头表在文件中的偏移量
    UINT32      elfFlags;               // 处理器特定标志位
    UINT16      elfHeadSize;            // ELF头部大小（字节）
    UINT16      elfPhEntSize;           // 程序头表条目大小
    UINT16      elfPhNum;               // 程序头表条目数量
    UINT16      elfShEntSize;           // 节头表条目大小
    UINT16      elfShNum;               // 节头表条目数量
    UINT16      elfShStrIndex;          // 节名字符串表在节头表中的索引
} LDElf32Ehdr;  // 32位ELF文件头部结构

typedef struct {
    UINT8       elfIdent[LD_EI_NIDENT]; // ELF文件标识数组
    UINT16      elfType;                // ELF文件类型
    UINT16      elfMachine;             // 目标架构类型
    UINT32      elfVersion;             // ELF文件版本
    UINT64      elfEntry;               // 程序入口点虚拟地址（64位）
    UINT64      elfPhoff;               // 程序头表偏移量（64位）
    UINT64      elfShoff;               // 节头表偏移量（64位）
    UINT32      elfFlags;               // 处理器特定标志位
    UINT16      elfHeadSize;            // ELF头部大小
    UINT16      elfPhEntSize;           // 程序头表条目大小
    UINT16      elfPhNum;               // 程序头表条目数量
    UINT16      elfShEntSize;           // 节头表条目大小
    UINT16      elfShNum;               // 节头表条目数量
    UINT16      elfShStrIndex;          // 节名字符串表索引
} LDElf64Ehdr;  // 64位ELF文件头部结构

#ifdef LOSCFG_AARCH64
#define LD_ELF_EHDR            LDElf64Ehdr  // AArch64架构使用64位头部
#else
#define LD_ELF_EHDR            LDElf32Ehdr  // 32位架构使用32位头部
#endif

/* e_ident[]数组索引定义 */
#define LD_EI_MAG0             0  // 魔术数字字节0（0x7F）
#define LD_EI_MAG1             1  // 魔术数字字节1（'E'）
#define LD_EI_MAG2             2  // 魔术数字字节2（'L'）
#define LD_EI_MAG3             3  // 魔术数字字节3（'F'）
#define LD_EI_CLASS            4  // 架构类别（32/64位）
#define LD_EI_DATA             5  // 数据存储格式（大端/小端）
#define LD_EI_VERSION          6  // ELF版本号
#define LD_EI_PAD              7  // 填充字节起始位置

/* ELF魔术数字定义 */
#define LD_ELFMAG0             0x7f  // ELF魔术数字第一个字节
#define LD_ELFMAG1             'E'   // ELF魔术数字第二个字节
#define LD_ELFMAG2             'L'   // ELF魔术数字第三个字节
#define LD_ELFMAG3             'F'   // ELF魔术数字第四个字节
#define LD_ELFMAG              "\177ELF"  // ELF魔术数字字符串
#define LD_SELFMAG             4  // 魔术数字长度（字节）

/* EI_CLASS取值定义 */
#define LD_ELF_CLASS_NONE      0  // 无效类别
#define LD_ELF_CLASS32         1  // 32位架构
#define LD_ELF_CLASS64         2  // 64位架构

#ifdef LOSCFG_AARCH64
#define LD_ELF_CLASS LD_ELF_CLASS64  // AArch64架构默认64位
#else
#define LD_ELF_CLASS LD_ELF_CLASS32  // 其他架构默认32位
#endif

/* EI_DATA数据格式定义 */
#define LD_ELF_DATA_NONE       0  // 无效格式
#define LD_ELF_DATA2LSB        1  // 小端字节序
#define LD_ELF_DATA2MSB        2  // 大端字节序

/* e_type文件类型定义 */
#define LD_ET_NONE             0  // 未知类型
#define LD_ET_REL              1  // 可重定位文件
#define LD_ET_EXEC             2  // 可执行文件
#define LD_ET_DYN              3  // 动态链接库
#define LD_ET_CORE             4  // 核心转储文件
#define LD_ET_LOPROC           0xff00  // 处理器专用（低范围）
#define LD_ET_HIPROC           0xffff  // 处理器专用（高范围）

/* e_machine架构类型定义 */
#define LD_EM_NONE             0     // 未知架构
#define LD_EM_M32              1     // AT&T WE 32100
#define LD_EM_SPARC            2     // SPARC架构
#define LD_EM_386              3     // Intel 80386
#define LD_EM_68K              4     // Motorola 68000
#define LD_EM_88K              5     // Motorola 88000
#define LD_EM_486              6     // Intel 80486
#define LD_EM_860              7     // Intel 80860
#define LD_EM_MIPS             8     // MIPS RS3000（大端）
#define LD_EM_MIPS_RS4_BE      10    // MIPS RS4000（大端）
#define LD_EM_PPC_OLD          17    // PowerPC（旧版本）
#define LD_EM_PPC              20    // PowerPC
#define LD_EM_RCE_OLD          25    // RCE（旧版本）
#define LD_EM_RCE              39    // RCE
#define LD_EM_MCORE            39    // MCORE
#define LD_EM_SH               42    // SH架构
#define LD_EM_M32R             36929 // M32R架构
#define LD_EM_NEC              36992 // NEC 850系列
#define LD_EM_NEC_830          36    // NEC 830系列
#define LD_EM_SC               58    // SC架构
#define LD_EM_ARM              40    // ARM架构
#define LD_EM_XTENSA           0x5E  // XTENSA架构
#define LD_EM_AARCH64          183   // ARM AARCH64架构

#ifdef LOSCFG_AARCH64
#define  LD_EM_TYPE  LD_EM_AARCH64  // AArch64架构类型
#else
#define  LD_EM_TYPE  LD_EM_ARM      // ARM32架构类型
#endif

/* e_flags处理器标志位 */
#define LD_EF_PPC_EMB          0x80000000  // PowerPC嵌入式标志

#define LD_EF_MIPS_NOREORDER   0x00000001  // MIPS不允许重排指令
#define LD_EF_MIPS_PIC         0x00000002  // MIPS位置无关代码
#define LD_EF_MIPS_CPIC        0x00000004  // MIPS调用者位置无关
#define LD_EF_MIPS_ARCH        0xf0000000  // MIPS架构版本掩码
#define LD_EF_MIPS_ARCH_MIPS_2 0x10000000  // MIPS II架构
#define LD_EF_MIPS_ARCH_MIPS_3 0x20000000  // MIPS III架构

/* 程序头表类型 */
#define LD_PT_NULL             0  // 无效段
#define LD_PT_LOAD             1  // 可加载段
#define LD_PT_DYNAMIC          2  // 动态链接信息段
#define LD_PT_INTERP           3  // 解释器路径段
#define LD_PT_NOTE             4  // 辅助信息段
#define LD_PT_SHLIB            5  // 保留（未使用）
#define LD_PT_PHDR             6  // 程序头表自身段
#define LD_PT_GNU_STACK        0x6474e551  // GNU栈属性段

/* e_version版本号定义 */
#define LD_EV_NONE             0  // 无效版本
#define LD_EV_CURRENT          1  // 当前版本

/* 程序头结构定义（32位） */
typedef struct {
    UINT32 type;     // 段类型
    UINT32 offset;   // 段在文件中的偏移量
    UINT32 vAddr;    // 段虚拟地址
    UINT32 phyAddr;  // 段物理地址
    UINT32 fileSize; // 段在文件中的大小
    UINT32 memSize;  // 段在内存中的大小
    UINT32 flags;    // 段属性标志
    UINT32 align;    // 段对齐要求
} LDElf32Phdr;

/* 程序头结构定义（64位） */
typedef struct {
    UINT32 type;     // 段类型
    UINT32 flags;    // 段属性标志
    UINT64 offset;   // 段在文件中的偏移量（64位）
    UINT64 vAddr;    // 段虚拟地址（64位）
    UINT64 phyAddr;  // 段物理地址（64位）
    UINT64 fileSize; // 段在文件中的大小（64位）
    UINT64 memSize;  // 段在内存中的大小（64位）
    UINT64 align;    // 段对齐要求（64位）
} LDElf64Phdr;

#ifdef LOSCFG_AARCH64
#define LD_ELF_PHDR            LDElf64Phdr  // AArch64使用64位程序头
#else
#define LD_ELF_PHDR            LDElf32Phdr  // 32位架构使用32位程序头
#endif

/* 特殊节索引 */
#define LD_SHN_UNDEF           0  // 未定义节
#define LD_SHN_LORESERVE       0xff00  // 保留节索引（低范围）
#define LD_SHN_LOPROC          0xff00
#define LD_SHN_HIPROC          0xff1f  // 处理器专用节索引（高范围）
#define LD_SHN_ABS             0xfff1  // 绝对地址节
#define LD_SHN_COMMON          0xfff2  // 公共符号节
#define LD_SHN_HIRESERVE       0xffff  // 保留节索引（高范围）
#define LD_SHN_GHCOMMON        0xff00  // 全局公共节
/* 节头结构定义 */
typedef struct {
    UINT32 shName;      // 节名称（字符串表索引）
    UINT32 shType;      // 节类型
    UINT32 shFlags;     // 节属性标志
    UINT32 shAddr;      // 节加载到内存中的虚拟地址
    UINT32 shOffset;    // 节在文件中的偏移量
    UINT32 shSize;      // 节大小（字节）
    UINT32 shLink;      // 链接到其他节的索引
    UINT32 shInfo;      // 节的附加信息
    UINT32 shAddrAlign; // 节对齐要求
    UINT32 shEntSize;   // 表项大小（若节包含表）
} LDElf32Shdr;  // 32位ELF节头结构

typedef struct {
    UINT32 shName;      // 节名称（字符串表索引）
    UINT32 shType;      // 节类型
    UINT64 shFlags;     // 节属性标志（64位）
    UINT64 shAddr;      // 节虚拟地址（64位）
    UINT64 shOffset;    // 节文件偏移量（64位）
    UINT64 shSize;      // 节大小（64位）
    UINT32 shLink;      // 链接到其他节的索引
    UINT32 shInfo;      // 节的附加信息
    UINT64 shAddrAlign; // 节对齐要求（64位）
    UINT64 shEntSize;   // 表项大小（64位）
} LDElf64Shdr;  // 64位ELF节头结构

#ifdef LOSCFG_AARCH64
#define LD_ELF_SHDR            LDElf64Shdr  // AArch64架构使用64位节头
#else
#define LD_ELF_SHDR            LDElf32Shdr  // 32位架构使用32位节头
#endif

/* sh_type节类型定义 */
#define LD_SHT_NULL            0U  // 无效节
#define LD_SHT_PROGBITS        1U  // 程序数据节
#define LD_SHT_SYMTAB          2U  // 符号表节
#define LD_SHT_STRTAB          3U  // 字符串表节
#define LD_SHT_RELA            4U  // 带加数的重定位表节
#define LD_SHT_HASH            5U  // 符号哈希表节
#define LD_SHT_DYNAMIC         6U  // 动态链接信息节
#define LD_SHT_NOTE            7U  // 辅助信息节
#define LD_SHT_NOBITS          8U  // 无数据节（如BSS）
#define LD_SHT_REL             9U  // 无加数的重定位表节
#define LD_SHT_SHLIB           10U // 保留（未使用）
#define LD_SHT_DYNSYM          11U // 动态链接符号表节
#define LD_SHT_COMDAT          12U // 公共数据节
#define LD_SHT_LOPROC          0x70000000U  // 处理器专用（低范围）
#define LD_SHT_HIPROC          0x7fffffffU  // 处理器专用（高范围）
#define LD_SHT_LOUSER          0x80000000U  // 用户自定义（低范围）
#define LD_SHT_HIUSER          0xffffffffU  // 用户自定义（高范围）

/* sh_flags节属性标志 */
#define LD_SHF_WRITE           0x1U  // 可写属性
#define LD_SHF_ALLOC           0x2U  // 需分配内存
#define LD_SHF_EXECINSTR       0x4U  // 可执行指令
#define LD_SHF_MASKPROC        0xf0000000U  // 处理器专用标志掩码

/* 符号表结构定义 */
typedef struct {
    UINT32 stName;  // 符号名称（字符串表索引）
    UINT32 stValue; // 符号值（地址或偏移量）
    UINT32 stSize;  // 符号大小（字节）
    UINT8 stInfo;   // 符号类型和绑定属性
    UINT8 stOther;  // 符号可见性
    UINT16 stShndx; // 符号所在节索引
} LDElf32Sym;  // 32位符号表项结构

typedef struct {
    UINT32 stName;  // 符号名称（字符串表索引）
    UINT8 stInfo;   // 符号类型和绑定属性
    UINT8 stOther;  // 符号可见性
    UINT16 stShndx; // 符号所在节索引
    UINT64 stValue; // 符号值（64位）
    UINT64 stSize;  // 符号大小（64位）
} LDElf64Sym;  // 64位符号表项结构

#ifdef LOSCFG_AARCH64
#define LD_ELF_SYM             LDElf64Sym  // AArch64架构使用64位符号表
#else
#define LD_ELF_SYM             LDElf32Sym  // 32位架构使用32位符号表
#endif

#define LD_STN_UNDEF           0U  // 未定义符号索引

/* 符号绑定类型 */
#define LD_STB_LOCAL           0U  // 局部符号（仅当前文件可见）
#define LD_STB_GLOBAL          1U  // 全局符号（可跨文件引用）
#define LD_STB_WEAK            2U  // 弱符号（可被覆盖）
#define LD_STB_LOPROC          13U // 处理器专用绑定（低范围）
#define LD_STB_HIPROC          15U // 处理器专用绑定（高范围）

/* 符号类型 */
#define LD_STT_NOTYPE          0U  // 未指定类型
#define LD_STT_OBJECT          1U  // 数据对象（变量）
#define LD_STT_FUNC            2U  // 函数
#define LD_STT_SECTION         3U  // 节符号
#define LD_STT_FILE            4U  // 文件符号
#define LD_STT_LOPROC          13U // 处理器专用类型（低范围）
#define LD_STT_HIPROC          15U // 处理器专用类型（高范围）
#define LD_STT_THUMB           0x80U  // Thumb指令标志

// 从stInfo获取符号绑定属性
#define LD_ELF_ST_BIND(info) ((info) >> 4)  // 右移4位获取绑定信息
// 从stInfo获取符号类型
#define LD_ELF_ST_TYPE(info) ((info) & 0xFU)  // 低4位获取类型信息
// 组合绑定和类型为stInfo值
#define LD_ELF_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xFU))  // 绑定左移4位+类型

/* 动态链接结构定义 */
typedef struct {
    UINT32 dynTag;  // 动态项类型
    union {
        UINT32 val; // 整数值
        UINT32 ptr; // 地址值
    } dyn;
} LDElf32Dyn;  // 32位动态链接项结构

typedef struct {
    UINT64 dynTag;  // 动态项类型
    union {
        UINT64 val; // 整数值（64位）
        UINT64 ptr; // 地址值（64位）
    } dyn;
} LDElf64Dyn;  // 64位动态链接项结构

#ifdef LOSCFG_AARCH64
#define LD_ELF_DYN             LDElf64Dyn  // AArch64架构使用64位动态结构
#else
#define LD_ELF_DYN             LDElf32Dyn  // 32位架构使用32位动态结构
#endif

/* 动态链接项类型定义 */
#define LD_DT_NULL             0  // 动态项列表结束
#define LD_DT_NEEDED           1  // 依赖的共享库名称
#define LD_DT_PLTRELSZ         2  // PLT重定位项总大小
#define LD_DT_PLTGOT           3  // PLT/GOT表地址
#define LD_DT_HASH             4  // 符号哈希表地址
#define LD_DT_STRTAB           5  // 字符串表地址
#define LD_DT_SYMTAB           6  // 符号表地址
#define LD_DT_RELA             7  // RELA重定位表地址
#define LD_DT_RELASZ           8  // RELA表大小
#define LD_DT_RELAENT          9  // 单个RELA项大小
#define LD_DT_STRSZ            10  // 字符串表大小
#define LD_DT_SYMENT           11  // 单个符号表项大小
#define LD_DT_INIT             12  // 初始化函数地址
#define LD_DT_FINI             13  // 终止函数地址
#define LD_DT_SONAME           14  // 共享库名称
#define LD_DT_RPATH            15  // 运行时库搜索路径
#define LD_DT_SYMBOLIC         16  // 符号优先解析本库
#define LD_DT_REL              17  // REL重定位表地址
#define LD_DT_RELSZ            18  // REL表大小
#define LD_DT_RELENT           19  // 单个REL项大小
#define LD_DT_PLTREL           20  // PLT使用的重定位类型
#define LD_DT_DEBUG            21  // 调试信息
#define LD_DT_TEXTREL          22  // 文本段存在重定位
#define LD_DT_JMPREL           23  // PLT重定位项地址
#define LD_DT_ENCODING         32  // 动态项编码方式

/* 重定位结构定义 */
typedef struct {
    UINT32 offset; // 重定位地址
    UINT32 info;   // 重定位类型和符号索引
} LDElf32Rel;  // 32位REL重定位结构（无加数）

typedef struct {
    UINT32 offset; // 重定位地址
    UINT32 info;   // 重定位类型和符号索引
    INT32 addend;  // 重定位加数
} LDElf32Rela;  // 32位RELA重定位结构（带加数）

typedef struct {
    UINT64 offset; // 重定位地址（64位）
    UINT64 info;   // 重定位类型和符号索引（64位）
} LDElf64Rel;  // AArch64架构REL重定位结构

typedef struct {
    UINT64 offset; // 重定位地址（64位）
    UINT64 info;   // A重定位类型和符号索引（64位）
    INT64 addend;  // 重定位加数（64位）
} LDElf64Rela;  // AArch64架构RELA重定位结构

#ifdef LOSCFG_AARCH64
#define LD_ELF_REL             LDElf64Rel  // AArch64使用64位REL结构
#define LD_ELF_RELA            LDElf64Rela  // AArch64使用64位RELA结构
#else
#define LD_ELF_REL             LDElf32Rel  // 32位架构使用32位REL结构
#define LD_ELF_RELA            LDElf32Rela  // 32位架构使用32位RELA结构
#endif

#ifdef LOSCFG_AARCH64
// 从info获取符号索引（64位）
#define LD_ELF_R_SYM(info) ((info) >> 32)  // 高32位为符号索引
// 从info获取重定位类型（64位）
#define LD_ELF_R_TYPE(info) ((info) & 0xFFFFFFFFUL)  // 低32位为重定位类型
// 组合符号索引和类型为info值（64位）
#define LD_ELF_R_INFO(sym, type) ((((UINT64)(sym)) << 32) + (type))  // 符号索引左移32位+类型
#else
// 从info获取符号索引（32位）
#define LD_ELF_R_SYM(info) ((info) >> 8)  // 高8位为符号索引
// 从info获取重定位类型（32位）
#define LD_ELF_R_TYPE(info) ((info) & 0xFFU)  // 低8位为重定位类型
// 组合符号索引和类型为info值（32位）
#define LD_ELF_R_INFO(sym, type) (((sym) << 8) + (UINT8)(type))  // 符号索引左移8位+类型
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_LD_ELH_PRI_H */
