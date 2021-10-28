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

#ifndef _LOS_LOAD_ELF_H
#define _LOS_LOAD_ELF_H

#include "los_ld_elf_pri.h"
#include "los_elf_auxvec_pri.h"
#include "los_process_pri.h"
#include "los_memory.h"
#include "los_strncpy_from_user.h"
#include "los_strnlen_user.h"
#include "los_user_put.h"
#include "los_user_get.h"
#include "user_copy.h"
#include "sys/stat.h"
#ifdef LOSCFG_DRIVERS_TZDRIVER
#include "fs/file.h"
#endif
#include "fs/file.h"
#include "unistd.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define INTERP_FULL_PATH                    "/lib/libc.so" //解析器路径
#define INVALID_FD                          (-1)//无效文件描述符,用于初始值.
#define STRINGS_COUNT_MAX                   256	//argv[], envp[]最大数量 
#define ELF_PHDR_NUM_MAX                    128	//ELF最大段数量
#define FILE_LENGTH_MAX                     0x1000000	//段占用的文件大小 最大1M
#define MEM_SIZE_MAX                        0x1000000	//运行时占用进程空间内存大小最大1M

#ifndef FILE_PATH_MAX
#define FILE_PATH_MAX                       PATH_MAX
#endif
#ifndef FILE_PATH_MIN
#define FILE_PATH_MIN                       2
#endif

#define USER_STACK_SIZE                     0x100000	//用户空间栈大小 1M
#define USER_PARAM_BYTE_MAX                 0x1000		//4K
#define USER_STACK_TOP_MAX                  USER_ASPACE_TOP_MAX	//用户空间栈顶位置

#define EXEC_MMAP_BASE                      0x02000000	//可执行文件分配基地址

#ifdef LOSCFG_ASLR
#define RANDOM_MASK                         ((((USER_ASPACE_TOP_MAX + GB - 1) & (-GB)) >> 3) - 1)
#endif

#define STACK_ALIGN_SIZE                    0x10 	//栈对齐
#define RANDOM_VECTOR_SIZE                  1

/* The permissions on sections in the program header. */ //对段的操作权限
#define PF_R                                0x4		//只读
#define PF_W                                0x2		//只写
#define PF_X                                0x1		//可执行

/**@struct
 * @brief ELF信息结构体
 */
typedef struct {
    LD_ELF_EHDR  elfEhdr;	///< ELF头信息
    LD_ELF_PHDR  *elfPhdr;	///< ELF程序头信息,也称段头信息
    UINT32       fileLen;	///< 文件长度
    INT32        procfd;	///< 文件描述符
} ELFInfo;

/**@struct
 * @brief ELF加载信息
 */
typedef struct {
    ELFInfo      execInfo;	///< 可执行文件信息
    ELFInfo      interpInfo;///< 解析器文件信息 lib/libc.so
    const CHAR   *fileName;///< 文件名称
    CHAR         *execName;///< 程序名称
    INT32        argc;	///< 参数个数
    INT32        envc;	///< 环境变量个数
    CHAR *const  *argv;	///< 参数数组
    CHAR *const  *envp;	///< 环境变量数组
    UINTPTR      stackTop;///< 栈底位置,递减满栈下,stackTop是高地址位
    UINTPTR      stackTopMax;///< 栈最大上限
    UINTPTR      stackBase;///< 栈顶位置
    UINTPTR      stackParamBase;///< 栈参数空间,放置启动ELF时的外部参数,大小为 USER_PARAM_BYTE_MAX 4K
    UINT32       stackSize;///< 栈大小
    INT32        stackProt;///< LD_PT_GNU_STACK栈的权限 ,例如(RW)
    UINTPTR      argStart;
    UINTPTR      loadAddr;	///< 加载地址
    UINTPTR      elfEntry;	///< 装载点地址 即: _start 函数地址
    UINTPTR      topOfMem;	///< 虚拟空间顶部位置,loadInfo->topOfMem = loadInfo->stackTopMax - sizeof(UINTPTR);
    UINTPTR      oldFiles;	///< 旧空间的文件映像
    LosVmSpace   *newSpace;///< 新虚拟空间
    LosVmSpace   *oldSpace;///< 旧虚拟空间
    INT32        randomDevFD;
} ELFLoadInfo;
//不超过用户空间顶部位置
STATIC INLINE BOOL OsIsBadUserAddress(VADDR_T vaddr)
{
    return (vaddr >= USER_STACK_TOP_MAX);
}

extern INT32 OsLoadELFFile(ELFLoadInfo *loadInfo);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_ELF_LIB_H */
