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

#ifndef _LOS_MP_H
#define _LOS_MP_H

#include "los_config.h"
#include "los_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_MP_CPU_ALL       LOSCFG_KERNEL_CPU_MASK

#define OS_MP_GC_PERIOD     100 /* ticks */

/// 核间中断
typedef enum {
    LOS_MP_IPI_WAKEUP,            ///!< 唤醒CPU
    LOS_MP_IPI_SCHEDULE,          ///!< 调度CPU
    LOS_MP_IPI_HALT,              ///!< 停止CPU
#ifdef LOSCFG_KERNEL_SMP_CALL
    LOS_MP_IPI_FUNC_CALL,         ///!< 触发CPU的回调函数,这些回调函数都挂到了cpu的链表上
#endif
} MP_IPI_TYPE;

typedef VOID (*SMP_FUNC_CALL)(VOID *args);

#ifdef LOSCFG_KERNEL_SMP
extern VOID LOS_MpSchedule(UINT32 target);
extern VOID OsMpWakeHandler(VOID);
extern VOID OsMpScheduleHandler(VOID);
extern VOID OsMpHaltHandler(VOID);
extern UINT32 OsMpInit(VOID);
#else
STATIC INLINE VOID LOS_MpSchedule(UINT32 target)
{
    (VOID)target;
}
#endif

#ifdef LOSCFG_KERNEL_SMP_CALL //多核下的回调开关
typedef struct {
    LOS_DL_LIST node;	///< 链表节点,将挂到 g_percpu[cpuid]上
    SMP_FUNC_CALL func;	///< 回调函数地址
    VOID *args;			///< 回调函数的参数
} MpCallFunc;

/**
 * It is used to call function on target cpus by sending ipi, and the first param is target cpu mask value.
 */
extern VOID OsMpFuncCall(UINT32 target, SMP_FUNC_CALL func, VOID *args);
extern VOID OsMpFuncCallHandler(VOID);
#else
INLINE VOID OsMpFuncCall(UINT32 target, SMP_FUNC_CALL func, VOID *args)
{
    (VOID)target;
    if (func != NULL) {
        func(args);
    }
}
#endif /* LOSCFG_KERNEL_SMP_CALL */

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_MP_H_ */
