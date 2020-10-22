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

#ifndef _LOS_SCHED_PRI_H
#define _LOS_SCHED_PRI_H

#include "los_percpu_pri.h"
#include "los_hwi.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

extern UINT32 g_taskScheduled;
//调度标志,一个位代表一个CPU核,这么看鸿蒙最大支持32核,例如:4个CPU,每个4核 就一共16个核,足够了.该标志用于在调用OSStartToRun之前防止内核调度
/*
 * Schedule flag, one bit represents one core.
 * This flag is used to prevent kernel scheduling before OSStartToRun.
 */
#define OS_SCHEDULER_SET(cpuid) do {     \
    g_taskScheduled |= (1U << (cpuid));  \
} while (0);//对应位设置为1

#define OS_SCHEDULER_CLR(cpuid) do {     \
    g_taskScheduled &= ~(1U << (cpuid)); \
} while (0);//对应位设置为0

#define OS_SCHEDULER_ACTIVE (g_taskScheduled & (1U << ArchCurrCpuid()))//代表位上是否为1

typedef enum {
    INT_NO_RESCH = 0,   /* no needs to schedule */
    INT_PEND_RESCH,     /* pending schedule flag */
} SchedFlag;

/* Check if preemptable with counter flag */
STATIC INLINE BOOL OsPreemptable(VOID)//是否可抢占
{
    /*
     * Unlike OsPreemptableInSched, the int may be not disabled when OsPreemptable
     * is called, needs mannually disable interrupt, to prevent current task from
     * being migrated to another core, and get the wrong preeptable status.
     */
     //与OsPreemptableInSched不同，当OsPreemptable时，中断可能不会被禁用,所以调用时，需要手动禁用中断，以防止当前任务
	 //被迁移到另一个CPU核上，并得到错误的可接受状态。
    UINT32 intSave = LOS_IntLock();//手动禁用中断
    BOOL preemptable = (OsPercpuGet()->taskLockCnt == 0);
    if (!preemptable) {
        /* Set schedule flag if preemption is disabled */
        OsPercpuGet()->schedFlag = INT_PEND_RESCH;//如果禁用抢占，则设置调度标志
    }
    LOS_IntRestore(intSave);//手动恢复中断
    return preemptable;
}

STATIC INLINE BOOL OsPreemptableInSched(VOID)
{
    BOOL preemptable = FALSE;

#if (LOSCFG_KERNEL_SMP == YES)
    /*
     * For smp systems, schedule must hold the task spinlock, and this counter
     * will increase by 1 in that case.
     */
    preemptable = (OsPercpuGet()->taskLockCnt == 1);//SMP时 taskLockCnt=1 才能执行调度任务

#else
    preemptable = (OsPercpuGet()->taskLockCnt == 0);
#endif
    if (!preemptable) {
        /* Set schedule flag if preemption is disabled */
        OsPercpuGet()->schedFlag = INT_PEND_RESCH;//重新调度
    }

    return preemptable;
}

/*
 * This function simply picks the next task and switches to it.
 * Current task needs to already be in the right state or the right
 * queues it needs to be in.
 */
extern VOID OsSchedResched(VOID);

/*
 * This function put the current task back to the ready queue and
 * try to do the schedule. However, the schedule won't be definitely
 * taken place while there're no other higher priority tasks or locked.
 */
extern VOID OsSchedPreempt(VOID);

/*
 * Just like OsSchedPreempt, except this function will do the OS_INT_ACTIVE
 * check, in case the schedule taken place in the middle of an interrupt.
 */
STATIC INLINE VOID LOS_Schedule(VOID)
{
    if (OS_INT_ACTIVE) {//硬件中断是否激活,注意调度是需要切换任务上下文的
        OsPercpuGet()->schedFlag = INT_PEND_RESCH;//
        return;
    }

    /*
     * trigger schedule in task will also do the slice check
     * if neccessary, it will give up the timeslice more in time.
     * otherwhise, there's no other side effects.
     */
    OsSchedPreempt();//抢占式调度
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SCHED_PRI_H */
