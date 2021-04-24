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

/* Elf header */
#define LD_EI_NIDENT           16

typedef struct {
    UINT8       elfIdent[LD_EI_NIDENT]; /* Magic number and other info *///含前16个字节，又可细分成class、data、version等字段，具体含义不用太关心，只需知道前4个字节点包含”ELF”关键字，这样可以判断当前文件是否是ELF格式；
    UINT16      elfType;                /* Object file type *///表示具体ELF类型，可重定位文件/可执行文件/共享库文件，显然这里是一个可执行文件；e_machine表示执行的机器平台，这里是x86_64；
    UINT16      elfMachine;             /* Architecture *///表示cpu架构
    UINT32      elfVersion;             /* Object file version *///表示文件版本号，这里的1表示初始版本号；
    UINT32      elfEntry;               /* Entry point virtual address *///对应”Entry point address”，程序入口函数地址，通过进程虚拟地址空间地址(0x400440)表达；
    UINT32      elfPhoff;               /* Program header table file offset *///对应“Start of program headers”，表示program header table在文件内的偏移位置，这里是从第64号字节(假设初始为0号字节)开始；
    UINT32      elfShoff;               /* Section header table file offset *///对应”Start of section headers”，表示section header table在文件内的偏移位置，这里是从第4472号字节开始，靠近文件尾部；
    UINT32      elfFlags;               /* Processor-specific flags *///表示与CPU处理器架构相关的信息，这里为零；
    UINT16      elfHeadSize;            /* ELF header size in bytes *///对应”Size of this header”，表示本ELF header自身的长度，这里为64个字节，回看前面的e_phoff为64，说明ELF header后紧跟着program header table；
    UINT16      elfPhEntSize;           /* Program header table entry size *///对应“Size of program headers”，表示program header table中每个元素的大小，这里为56个字节；
    UINT16      elfPhNum;               /* Program header table entry count *///对应”Number of program headers”，表示program header table中元素个数，这里为9个；
    UINT16      elfShEntSize;           /* Section header table entry size *///e_shentsize 对应”Size of section headers”，表示section header table中每个元素的大小，这里为64个字节；
    UINT16      elfShNum;               /* Section header table entry count *///e_shnum 对应”Number of section headers”，表示section header table中元素的个数，这里为30个；
    UINT16      elfShStrIndex;          /* Section header string table index *///e_shstrndx  对应”Section header string table index”，表示描述各section字符名称的string table在section header table中的下标，详见后文对string table的介绍。
} LDElf32Ehdr;


typedef struct {
    UINT8       elfIdent[LD_EI_NIDENT]; /* Magic number and other info */
    UINT16      elfType;                /* Object file type */
    UINT16      elfMachine;             /* Architecture */
    UINT32      elfVersion;             /* Object file version */
    UINT64      elfEntry;               /* Entry point virtual address */
    UINT64      elfPhoff;               /* Program header table file offset */
    UINT64      elfShoff;               /* Section header table file offset */
    UINT32      elfFlags;               /* Processor-specific flags */
    UINT16      elfHeadSize;            /* ELF header size in bytes */
    UINT16      elfPhEntSize;           /* Program header table entry size */
    UINT16      elfPhNum;               /* Program header table entry count */
    UINT16      elfShEntSize;           /* Section header table entry size */
    UINT16      elfShNum;               /* Section header table entry count */
    UINT16      elfShStrIndex;          /* Section header string table index */
} LDElf64Ehdr;

#ifdef LOSCFG_AARCH64
#define LD_ELF_EHDR            LDElf64Ehdr
#else
#define LD_ELF_EHDR            LDElf32Ehdr
#endif

/* e_ident[] values */
#define LD_EI_MAG0             0
#define LD_EI_MAG1             1
#define LD_EI_MAG2             2
#define LD_EI_MAG3             3
#define LD_EI_CLASS            4
#define LD_EI_DATA             5
#define LD_EI_VERSION          6
#define LD_EI_PAD              7

#define LD_ELFMAG0             0x7f
#define LD_ELFMAG1             'E'
#define LD_ELFMAG2             'L'
#define LD_ELFMAG3             'F'
#define LD_ELFMAG              "\177ELF"
#define LD_SELFMAG             4

/* EI_CLASS */
#define LD_ELF_CLASS_NONE      0
#define LD_ELF_CLASS32         1
#define LD_ELF_CLASS64         2

#ifdef LOSCFG_AARCH64
#define LD_ELF_CLASS LD_ELF_CLASS64
#else
#define LD_ELF_CLASS LD_ELF_CLASS32
#endif

/* EI_DATA */
#define LD_ELF_DATA_NONE       0
#define LD_ELF_DATA2LSB        1
#define LD_ELF_DATA2MSB        2

/* e_type */
#define LD_ET_NONE             0
#define LD_ET_REL              1
#define LD_ET_EXEC             2
#define LD_ET_DYN              3
#define LD_ET_CORE             4
#define LD_ET_LOPROC           0xff00
#define LD_ET_HIPROC           0xffff

/* e_machine */
#define LD_EM_NONE             0     /* No machine */
#define LD_EM_M32              1     /* AT&T WE 32100 */
#define LD_EM_SPARC            2     /* SPARC */
#define LD_EM_386              3     /* Intel 80386 */
#define LD_EM_68K              4     /* Motorola 68000 */
#define LD_EM_88K              5     /* Motorola 88000 */
#define LD_EM_486              6     /* Intel 80486 */
#define LD_EM_860              7     /* Intel 80860 */
#define LD_EM_MIPS             8     /* MIPS RS3000 Big-Endian */
#define LD_EM_MIPS_RS4_BE      10    /* MIPS RS4000 Big-Endian */
#define LD_EM_PPC_OLD          17    /* PowerPC - old */
#define LD_EM_PPC              20    /* PowerPC */
#define LD_EM_RCE_OLD          25    /* RCE - old */
#define LD_EM_RCE              39    /* RCE */
#define LD_EM_MCORE            39    /* MCORE */
#define LD_EM_SH               42    /* SH */
#define LD_EM_M32R             36929 /* M32R */
#define LD_EM_NEC              36992 /* NEC 850 series */
#define LD_EM_NEC_830          36    /* NEC 830 series */
#define LD_EM_SC               58    /* SC */
#define LD_EM_ARM              40    /* ARM  */
#define LD_EM_XTENSA           0x5E  /* XTENSA */
#define LD_EM_AARCH64          183   /* ARM AARCH64 */

#ifdef LOSCFG_AARCH64
#define  LD_EM_TYPE  LD_EM_AARCH64
#else
#define  LD_EM_TYPE  LD_EM_ARM
#endif

/* e_flags */
#define LD_EF_PPC_EMB          0x80000000

#define LD_EF_MIPS_NOREORDER   0x00000001
#define LD_EF_MIPS_PIC         0x00000002
#define LD_EF_MIPS_CPIC        0x00000004
#define LD_EF_MIPS_ARCH        0xf0000000
#define LD_EF_MIPS_ARCH_MIPS_2 0x10000000
#define LD_EF_MIPS_ARCH_MIPS_3 0x20000000

#define LD_PT_NULL             0
#define LD_PT_LOAD             1
#define LD_PT_DYNAMIC          2
#define LD_PT_INTERP           3
#define LD_PT_NOTE             4
#define LD_PT_SHLIB            5
#define LD_PT_PHDR             6
#define LD_PT_GNU_STACK        0x6474e551

/* e_version and EI_VERSION */
#define LD_EV_NONE             0
#define LD_EV_CURRENT          1

/* Program Header */
typedef struct {
    UINT32 type;     /* Segment type */
    UINT32 offset;   /* Segment file offset */
    UINT32 vAddr;    /* Segment virtual address */
    UINT32 phyAddr;  /* Segment physical address */
    UINT32 fileSize; /* Segment size in file */
    UINT32 memSize;  /* Segment size in memory */
    UINT32 flags;    /* Segment flags */
    UINT32 align;    /* Segment alignment */
} LDElf32Phdr;

typedef struct {
    UINT32 type;     /* Segment type */
    UINT32 flags;    /* Segment flags */
    UINT64 offset;   /* Segment file offset */
    UINT64 vAddr;    /* Segment virtual address */
    UINT64 phyAddr;  /* Segment physical address */
    UINT64 fileSize; /* Segment size in file */
    UINT64 memSize;  /* Segment size in memory */
    UINT64 align;    /* Segment alignment */
} LDElf64Phdr;

#ifdef LOSCFG_AARCH64
#define LD_ELF_PHDR            LDElf64Phdr
#else
#define LD_ELF_PHDR            LDElf32Phdr
#endif

/* Special section indexes */
#define LD_SHN_UNDEF           0
#define LD_SHN_LORESERVE       0xff00
#define LD_SHN_LOPROC          0xff00
#define LD_SHN_HIPROC          0xff1f
#define LD_SHN_ABS             0xfff1
#define LD_SHN_COMMON          0xfff2
#define LD_SHN_HIRESERVE       0xffff
#define LD_SHN_GHCOMMON        0xff00

/* Section header */
typedef struct {
    UINT32 shName;      /* Section name (string tbl index) *///表示每个区的名字
    UINT32 shType;      /* Section type *///表示每个区的功能
    UINT32 shFlags;     /* Section flags *///表示每个区的属性
    UINT32 shAddr;      /* Section virtual addr at execution *///表示每个区的进程映射地址
    UINT32 shOffset;    /* Section file offset *///表示文件内偏移
    UINT32 shSize;      /* Section size in bytes *///表示区的大小
    UINT32 shLink;      /* Link to another section *///Link和Info记录不同类型区的相关信息
    UINT32 shInfo;      /* Additional section information *///Link和Info记录不同类型区的相关信息
    UINT32 shAddrAlign; /* Section alignment *///表示区的对齐单位
    UINT32 shEntSize;   /* Entry size if section holds table *///表示区中每个元素的大小(如果该区为一个数组的话，否则该值为0)
} LDElf32Shdr;


typedef struct {
    UINT32 shName;      /* Section name (string tbl index) */
    UINT32 shType;      /* Section type */
    UINT64 shFlags;     /* Section flags */
    UINT64 shAddr;      /* Section virtual addr at execution */
    UINT64 shOffset;    /* Section file offset */
    UINT64 shSize;      /* Section size in bytes */
    UINT32 shLink;      /* Link to another section */
    UINT32 shInfo;      /* Additional section information */
    UINT64 shAddrAlign; /* Section alignment */
    UINT64 shEntSize;   /* Entry size if section holds table */
} LDElf64Shdr;

#ifdef LOSCFG_AARCH64
#define LD_ELF_SHDR            LDElf64Shdr
#else
#define LD_ELF_SHDR            LDElf32Shdr
#endif

/* sh_type */
#define LD_SHT_NULL            0U
#define LD_SHT_PROGBITS        1U
#define LD_SHT_SYMTAB          2U
#define LD_SHT_STRTAB          3U
#define LD_SHT_RELA            4U
#define LD_SHT_HASH            5U
#define LD_SHT_DYNAMIC         6U
#define LD_SHT_NOTE            7U
#define LD_SHT_NOBITS          8U
#define LD_SHT_REL             9U
#define LD_SHT_SHLIB           10U
#define LD_SHT_DYNSYM          11U
#define LD_SHT_COMDAT          12U
#define LD_SHT_LOPROC          0x70000000U
#define LD_SHT_HIPROC          0x7fffffffU
#define LD_SHT_LOUSER          0x80000000U
#define LD_SHT_HIUSER          0xffffffffU

/* sh_flags  */
#define LD_SHF_WRITE           0x1U
#define LD_SHF_ALLOC           0x2U
#define LD_SHF_EXECINSTR       0x4U
#define LD_SHF_MASKPROC        0xf0000000U

/* Symbol table */
typedef struct {
    UINT32 stName;  /* Symbol table name (string tbl index) *///表示符号对应的源码字符串，为对应String Table中的索引
    UINT32 stValue; /* Symbol table value *///表示符号对应的数值
    UINT32 stSize;  /* Symbol table size *///表示符号对应数值的空间占用大小
    UINT8 stInfo;   /* Symbol table type and binding *///表示符号的相关信息 如符号类型(变量符号、函数符号)
    UINT8 stOther;  /* Symbol table visibility */
    UINT16 stShndx; /* Section table index *///表示与该符号相关的区的索引,例如函数符号与对应的代码区相关
} LDElf32Sym;


typedef struct {
    UINT32 stName;  /* Symbol table name (string tbl index) */
    UINT8 stInfo;   /* Symbol table type and binding */
    UINT8 stOther;  /* Symbol table visibility */
    UINT16 stShndx; /* Section table index */
    UINT64 stValue; /* Symbol table value */
    UINT64 stSize;  /* Symbol table size */
} LDElf64Sym;

#ifdef LOSCFG_AARCH64
#define LD_ELF_SYM             LDElf64Sym
#else
#define LD_ELF_SYM             LDElf32Sym
#endif

#define LD_STN_UNDEF           0U

#define LD_STB_LOCAL           0U
#define LD_STB_GLOBAL          1U
#define LD_STB_WEAK            2U
#define LD_STB_LOPROC          13U
#define LD_STB_HIPROC          15U

#define LD_STT_NOTYPE          0U
#define LD_STT_OBJECT          1U
#define LD_STT_FUNC            2U
#define LD_STT_SECTION         3U
#define LD_STT_FILE            4U
#define LD_STT_LOPROC          13U
#define LD_STT_HIPROC          15U
#define LD_STT_THUMB           0x80U

#define LD_ELF_ST_BIND(info) ((info) >> 4) /* Obtain the binding information of the symbol table */
#define LD_ELF_ST_TYPE(info) ((info) & 0xFU) /* Obtain the type of the symbol table */
#define LD_ELF_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xFU))

/* Dynamic */
typedef struct {
    UINT32 dynTag;  /* Dynamic entry type */
    union {
        UINT32 val; /* Integer value */
        UINT32 ptr; /* Address value */
    } dyn;
} LDElf32Dyn;

typedef struct {
    UINT64 dynTag;  /* Dynamic entry type */
    union {
        UINT64 val; /* Integer value */
        UINT64 ptr; /* Address value */
    } dyn;
} LDElf64Dyn;

#ifdef LOSCFG_AARCH64
#define LD_ELF_DYN             LDElf64Dyn
#else
#define LD_ELF_DYN             LDElf32Dyn
#endif

/* This is the info that is needed to parse the dynamic section of the file */
#define LD_DT_NULL             0
#define LD_DT_NEEDED           1
#define LD_DT_PLTRELSZ         2
#define LD_DT_PLTGOT           3
#define LD_DT_HASH             4
#define LD_DT_STRTAB           5
#define LD_DT_SYMTAB           6
#define LD_DT_RELA             7
#define LD_DT_RELASZ           8
#define LD_DT_RELAENT          9
#define LD_DT_STRSZ            10
#define LD_DT_SYMENT           11
#define LD_DT_INIT             12
#define LD_DT_FINI             13
#define LD_DT_SONAME           14
#define LD_DT_RPATH            15
#define LD_DT_SYMBOLIC         16
#define LD_DT_REL              17
#define LD_DT_RELSZ            18
#define LD_DT_RELENT           19
#define LD_DT_PLTREL           20
#define LD_DT_DEBUG            21
#define LD_DT_TEXTREL          22
#define LD_DT_JMPREL           23
#define LD_DT_ENCODING         32

/* Relocation */
typedef struct {
    UINT32 offset; /* Address */
    UINT32 info;   /* Relocation type and symbol index */
} LDElf32Rel;

typedef struct {
    UINT32 offset; /* Address */
    UINT32 info;   /* Relocation type and symbol index */
    INT32 addend;  /* Addend */
} LDElf32Rela;

typedef struct {
    UINT64 offset; /* Address */
    UINT64 info;   /* Relocation type and symbol index */
} LDElf64Rel;

typedef struct {
    UINT64 offset; /* Address */
    UINT64 info;   /* Relocation type and symbol index */
    INT64 addend;  /* Addend */
} LDElf64Rela;

#ifdef LOSCFG_AARCH64
#define LD_ELF_REL             LDElf64Rel
#define LD_ELF_RELA            LDElf64Rela
#else
#define LD_ELF_REL             LDElf32Rel
#define LD_ELF_RELA            LDElf32Rela
#endif

#ifdef LOSCFG_AARCH64
#define LD_ELF_R_SYM(info) ((info) >> 32)
#define LD_ELF_R_TYPE(info) ((info) & 0xFFFFFFFFUL)
#define LD_ELF_R_INFO(sym, type) ((((UINT64)(sym)) << 32) + (type))
#else
#define LD_ELF_R_SYM(info) ((info) >> 8)
#define LD_ELF_R_TYPE(info) ((info) & 0xFFU)
#define LD_ELF_R_INFO(sym, type) (((sym) << 8) + (UINT8)(type))
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_LD_ELH_PRI_H */
