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
#include "unistd.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define INTERP_FULL_PATH                    "/lib/libc.so"  // 解释器完整路径
#define INVALID_FD                          (-1)            // 无效文件描述符
#define STRINGS_COUNT_MAX                   256             // 字符串计数最大值
#define ELF_PHDR_NUM_MAX                    128             // ELF程序头数量最大值
#define FILE_LENGTH_MAX                     0x1000000       // 文件长度最大值
#define MEM_SIZE_MAX                        0x1000000       // 内存大小最大值

#ifndef FILE_PATH_MAX
#define FILE_PATH_MAX                       PATH_MAX        // 文件路径最大长度
#endif
#ifndef FILE_PATH_MIN
#define FILE_PATH_MIN                       2               // 文件路径最小长度
#endif

#define USER_STACK_SIZE                     0x100000        // 用户栈大小
#define USER_PARAM_BYTE_MAX                 0x1000          // 用户参数字节最大值
#define USER_STACK_TOP_MAX                  USER_ASPACE_TOP_MAX  // 用户栈顶最大值

#define EXEC_MMAP_BASE                      0x02000000      // 可执行文件内存映射基地址

#ifdef LOSCFG_ASLR
#define RANDOM_MASK                         ((((USER_ASPACE_TOP_MAX + GB - 1) & (-GB)) >> 3) - 1)  // 随机掩码，用于地址空间布局随机化
#endif

#define STACK_ALIGN_SIZE                    0x10            // 栈对齐大小
#define RANDOM_VECTOR_SIZE                  1               // 随机向量大小

/* The permissions on sections in the program header. */
#define PF_R                                0x4             // 段可读权限
#define PF_W                                0x2             // 段可写权限
#define PF_X                                0x1             // 段可执行权限

/**
 * @brief ELF文件信息结构体
 */
typedef struct {
    LD_ELF_EHDR  elfEhdr;                // ELF文件头部
    LD_ELF_PHDR  *elfPhdr;               // ELF程序头指针
    UINT32       fileLen;                // 文件长度
    INT32        procfd;                 // 进程文件描述符
} ELFInfo;

/**
 * @brief ELF加载信息结构体
 */
typedef struct {
    ELFInfo      execInfo;               // 可执行文件信息
    ELFInfo      interpInfo;             // 解释器信息
    const CHAR   *fileName;              // 文件名
    CHAR         *execName;              // 可执行文件名
    INT32        argc;                   // 参数计数
    INT32        envc;                   // 环境变量计数
    CHAR * const  *argv;                 // 参数数组
    CHAR * const  *envp;                 // 环境变量数组
    UINTPTR      stackTop;               // 栈顶地址
    UINTPTR      stackTopMax;            // 栈顶最大地址
    UINTPTR      stackBase;              // 栈基地址
    UINTPTR      stackParamBase;         // 栈参数基地址
    UINT32       stackSize;              // 栈大小
    INT32        stackProt;              // 栈保护属性
    UINTPTR      argStart;               // 参数起始地址
    UINTPTR      loadAddr;               // 加载地址
    UINTPTR      elfEntry;               // ELF入口地址
    UINTPTR      topOfMem;               // 内存顶部地址
    UINTPTR      oldFiles;               // 旧文件
    LosVmSpace   *newSpace;              // 新虚拟地址空间
    LosVmSpace   *oldSpace;              // 旧虚拟地址空间
    INT32        randomDevFD;            // 随机设备文件描述符
} ELFLoadInfo;

/**
 * @brief 检查用户地址是否有效
 * @param vaddr 虚拟地址
 * @return 地址无效返回TRUE，有效返回FALSE
 */
STATIC INLINE BOOL OsIsBadUserAddress(VADDR_T vaddr)
{
    return (vaddr >= USER_STACK_TOP_MAX);  // 比较虚拟地址是否超过栈顶最大值
}

extern UINT32 OsGetRndOffset(INT32 randomDevFD);
extern INT32 OsLoadELFFile(ELFLoadInfo *loadInfo);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_ELF_LIB_H */
