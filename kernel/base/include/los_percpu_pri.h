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

#ifndef _LOS_PERCPU_PRI_H
#define _LOS_PERCPU_PRI_H

#include "los_base.h"
#include "los_hw_cpu.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_KERNEL_SMP

/**
 * @enum ExcFlag
 * @brief CPU异常状态枚举
 * @details 定义CPU可能处于的运行状态
 */
typedef enum {
    CPU_RUNNING = 0,   ///< CPU正在运行状态
    CPU_HALT,          ///< CPU处于暂停状态
    CPU_EXC            ///< CPU处于异常状态
} ExcFlag;

/**
 * @struct Percpu
 * @brief 每CPU核心私有数据结构
 * @details 存储特定CPU核心的运行状态和相关信息
 */
typedef struct {
    UINT32      excFlag;               /* CPU暂停或异常标志 */
#ifdef LOSCFG_KERNEL_SMP_CALL
    LOS_DL_LIST funcLink;              /* 多处理器函数调用链表 */
#endif
} Percpu;

/*! 内核每CPU结构数组 | 每个CPU核心的内核描述符 */
extern Percpu g_percpu[LOSCFG_KERNEL_CORE_NUM];

/**
 * @brief 获取当前运行CPU的私有数据
 * @return Percpu* 当前CPU的私有数据指针
 */
STATIC INLINE Percpu *OsPercpuGet(VOID)
{
    return &g_percpu[ArchCurrCpuid()];  /* 获取当前CPU ID对应的私有数据 */
}

/**
 * @brief 通过CPU ID获取指定CPU的私有数据
 * @param cpuid CPU核心ID
 * @return Percpu* 指定CPU的私有数据指针
 */
STATIC INLINE Percpu *OsPercpuGetByID(UINT32 cpuid)
{
    return &g_percpu[cpuid];            /* 返回指定CPU ID的私有数据 */
}

/**
 * @brief 检查指定CPU是否处于暂停状态
 * @param cpuid CPU核心ID
 * @return UINT32 1-暂停状态，0-非暂停状态
 */
STATIC INLINE UINT32 OsCpuStatusIsHalt(UINT16 cpuid)
{
    return (OsPercpuGetByID(cpuid)->excFlag == CPU_HALT);  /* 比较CPU状态标志 */
}

/**
 * @brief 设置当前CPU的运行状态
 * @param flag 要设置的CPU状态(ExcFlag枚举值)
 * @return VOID
 */
STATIC INLINE VOID OsCpuStatusSet(ExcFlag flag)
{
    OsPercpuGet()->excFlag = flag;      /* 设置当前CPU的状态标志 */
}


VOID OsAllCpuStatusOutput(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_PERCPU_PRI_H */
