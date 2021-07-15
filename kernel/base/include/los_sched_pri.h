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
#include "los_task_pri.h"
#include "los_sys_pri.h"
#include "los_process_pri.h"
#include "los_hwi.h"
#include "hal_timer.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

extern UINT32 g_taskScheduled;
typedef BOOL (*SchedScan)(VOID);

extern UINT64 g_sysSchedStartTime;
//获取当前调度经历了多少个时间周期
STATIC INLINE UINT64 OsGerCurrSchedTimeCycle(VOID)
{
    if (g_sysSchedStartTime == 0) {
        return g_sysSchedStartTime;
    }

    return (HalClockGetCycles() - g_sysSchedStartTime);//经历周期数
}

STATIC INLINE VOID OsSchedIrqUpdateUsedTime(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqUsedTime = OsGerCurrSchedTimeCycle() - runTask->irqStartTime;
}

STATIC INLINE VOID OsSchedIrqStartTime(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqStartTime = OsGerCurrSchedTimeCycle();
}

/*
 * Schedule flag, one bit represents one core.
 * This flag is used to prevent kernel scheduling before OSStartToRun.
 */
#define OS_SCHEDULER_SET(cpuid) do {     \
    g_taskScheduled |= (1U << (cpuid));  \
} while (0);
//清楚调度标识位,对应位设置为0
#define OS_SCHEDULER_CLR(cpuid) do {     \
    g_taskScheduled &= ~(1U << (cpuid)); \
} while (0);

#define OS_SCHEDULER_ACTIVE (g_taskScheduled & (1U << ArchCurrCpuid()))//用于判断当前cpu是否可调度

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

#ifdef LOSCFG_KERNEL_SMP
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

STATIC INLINE VOID OsCpuSchedLock(Percpu *cpu)
{
    cpu->taskLockCnt++;
}

STATIC INLINE VOID OsCpuSchedUnlock(Percpu *cpu, UINT32 intSave)
{
    if (cpu->taskLockCnt > 0) {
        cpu->taskLockCnt--;
        if ((cpu->taskLockCnt == 0) && (cpu->schedFlag == INT_PEND_RESCH) && OS_SCHEDULER_ACTIVE) {
            cpu->schedFlag = INT_NO_RESCH;
            LOS_IntRestore(intSave);
            LOS_Schedule();
            return;
        }
    }

    LOS_IntRestore(intSave);
}

extern UINT32 OsSchedSetTickTimerType(UINT32 timerType);

extern VOID OsSchedSetIdleTaskSchedParam(LosTaskCB *idleTask);

extern UINT32 OsSchedSwtmrScanRegister(SchedScan func);

extern VOID OsSchedUpdateExpireTime(UINT64 startTime);

extern VOID OsSchedToUserReleaseLock(VOID);

extern VOID OsSchedTaskDeQueue(LosTaskCB *taskCB);

extern VOID OsSchedTaskEnQueue(LosTaskCB *taskCB);

extern UINT32 OsSchedTaskWait(LOS_DL_LIST *list, UINT32 timeout, BOOL needSched);

extern VOID OsSchedTaskWake(LosTaskCB *resumedTask);

extern BOOL OsSchedModifyTaskSchedParam(LosTaskCB *taskCB, UINT16 policy, UINT16 priority);

extern BOOL OsSchedModifyProcessSchedParam(LosProcessCB *processCB, UINT16 policy, UINT16 priority);

extern VOID OsSchedDelay(LosTaskCB *runTask, UINT32 tick);

extern VOID OsSchedYield(VOID);

extern VOID OsSchedTaskExit(LosTaskCB *taskCB);

extern VOID OsSchedTick(VOID);

extern UINT32 OsSchedInit(VOID);

extern VOID OsSchedStart(VOID);

/*
 * This function simply picks the next task and switches to it.
 * Current task needs to already be in the right state or the right
 * queues it needs to be in.
 */
extern VOID OsSchedResched(VOID);

extern VOID OsSchedIrqEndCheckNeedSched(VOID);

/*
* This function inserts the runTask to the lock pending list based on the
* task priority.
*/
extern LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList);

#ifdef LOSCFG_SCHED_TICK_DEBUG
extern VOID OsSchedDebugRecordData(VOID);
#endif

extern UINT32 OsShellShowTickRespo(VOID);

extern UINT32 OsShellShowSchedParam(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SCHED_PRI_H */
