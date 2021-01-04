/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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
#include "los_sortlink_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#if (LOSCFG_KERNEL_SMP == YES)
typedef enum {
    CPU_RUNNING = 0,   /* cpu is running */ 	//CPU正在运行状态
    CPU_HALT,          /* cpu in the halt */	//CPU处于暂停状态
    CPU_EXC            /* cpu in the exc */		//CPU处于异常状态
} ExcFlag;
#endif

typedef struct {
    SortLinkAttribute taskSortLink;             /* task sort link */	//每个CPU core 都有一个task排序链表
    SortLinkAttribute swtmrSortLink;            /* swtmr sort link */	//每个CPU core 都有一个定时器排序链表

    UINT32 idleTaskID;                          /* idle task id */		//空闲任务ID 见于 OsIdleTaskCreate
    UINT32 taskLockCnt;                         /* task lock flag */	//任务锁的数量,当 > 0 的时候,需要重新调度了
    UINT32 swtmrHandlerQueue;                   /* software timer timeout queue id */	//软时钟超时队列句柄
    UINT32 swtmrTaskID;                         /* software timer task id */	//软时钟任务ID

    UINT32 schedFlag;                           /* pending scheduler flag */	//调度标识 INT_NO_RESCH INT_PEND_RESCH
#if (LOSCFG_KERNEL_SMP == YES)
    UINT32 excFlag;                             /* cpu halt or exc flag */	//CPU处于停止或运行的标识
#endif
} Percpu;

/* the kernel per-cpu structure */
extern Percpu g_percpu[LOSCFG_KERNEL_CORE_NUM];//每个cpu的内核描述符
//获得当前运行CPU的信息
STATIC INLINE Percpu *OsPercpuGet(VOID)
{
    return &g_percpu[ArchCurrCpuid()];	
}
//获得参数CPU的信息
STATIC INLINE Percpu *OsPercpuGetByID(UINT32 cpuid)
{
    return &g_percpu[cpuid];
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_PERCPU_PRI_H */
