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

#ifndef _LOS_EXC_PRI_H
#define _LOS_EXC_PRI_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/* 系统状态宏定义 */
#define OS_SYSTEM_NORMAL        0                           /* 系统正常运行状态 */
#define OS_SYSTEM_EXC_CURR_CPU  1                           /* 当前CPU发生异常状态 */
#define OS_SYSTEM_EXC_OTHER_CPU 2                           /* 其他CPU发生异常状态 */

/* 路径长度宏定义 */
#define REGION_PATH_MAX 32                                  /* 内存区域路径最大长度，单位：字节 */

/**
 * @brief 指令指针信息结构体
 * 用于存储异常发生时的指令指针及相关路径信息
 */
typedef struct {
#ifdef LOSCFG_KERNEL_VM                                      /* 条件编译：启用内核虚拟内存管理时 */
    UINTPTR ip;                                             /* 指令指针地址 */
    UINT32 len; /* f_path length */                         /* 文件路径长度 */
    CHAR f_path[REGION_PATH_MAX];                           /* 文件路径缓冲区，最大长度REGION_PATH_MAX */
#else                                                       /* 条件编译：未启用内核虚拟内存管理时 */
    UINTPTR ip;                                             /* 指令指针地址 */
#endif
} IpInfo;                                                    /* 指令指针信息结构体类型 */

extern UINT32 OsGetSystemStatus(VOID);
extern VOID BackTraceSub(UINTPTR regFP);
extern VOID OsExcInit(VOID);
extern BOOL OsSystemExcIsReset(VOID);
extern UINT32 BackTraceGet(UINTPTR regFP, IpInfo *callChain, UINT32 maxDepth);
extern BOOL OsGetUsrIpInfo(UINTPTR ip, IpInfo *info);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_EXC_PRI_H */
