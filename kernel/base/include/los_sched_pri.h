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

#define OS_SCHED_MINI_PERIOD       (OS_SYS_CLOCK / LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI) ///< 1毫秒的时钟周期
#define OS_TICK_RESPONSE_PRECISION (UINT32)((OS_SCHED_MINI_PERIOD * 75) / 100) ///< 不明白为啥是 * 75 就精确了???  @note_thinking
#define OS_SCHED_MAX_RESPONSE_TIME (UINT64)(((UINT64)-1) - 1U)

extern UINT32 g_taskScheduled;
typedef BOOL (*SchedScan)(VOID);

extern UINT64 g_sysSchedStartTime;
//获取当前调度经历了多少个时间周期
STATIC INLINE UINT64 OsGetCurrSchedTimeCycle(VOID)
{
    if (g_sysSchedStartTime != OS_64BIT_MAX) {
        return (HalClockGetCycles() - g_sysSchedStartTime);
    }

    return 0;
}
/// 更新中断使用时间
STATIC INLINE VOID OsSchedIrqUpdateUsedTime(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqUsedTime = OsGetCurrSchedTimeCycle() - runTask->irqStartTime;//获取时间差
}
/// 获取中断开始时间
STATIC INLINE VOID OsSchedIrqStartTime(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqStartTime = OsGetCurrSchedTimeCycle(); //获取当前时间
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

#define OS_SCHEDULER_ACTIVE (g_taskScheduled & (1U << ArchCurrCpuid()))///< 用于判断当前cpu是否在调度中

typedef enum {
    INT_NO_RESCH = 0x0,   /**< no needs to schedule | 不需要调度*/
    INT_PEND_RESCH = 0x1, /**< pending schedule flag | 因定时器/任务优先级调整等导致的调度,可以理解为内因触发的调度*/
    INT_PEND_TICK = 0x2,  /**< pending tick | 因tick导致的调度,可以理解为外因触发的调度*/
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
        OsPercpuGet()->schedFlag |= INT_PEND_RESCH;
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
     * will increase by 1 in that case. | 对于 smp ,调度必须持有任务自旋锁，在这种情况下，此计数器将增加 1。
     */
    preemptable = (OsPercpuGet()->taskLockCnt == 1);//SMP时 taskLockCnt=1 才能执行调度任务

#else
    preemptable = (OsPercpuGet()->taskLockCnt == 0);
#endif
    if (!preemptable) {
        /* Set schedule flag if preemption is disabled | 如果禁用抢占，则设置调度标志*/
        OsPercpuGet()->schedFlag |= INT_PEND_RESCH;
    }

    return preemptable;
}
/// 申请CPU调度锁
STATIC INLINE VOID OsCpuSchedLock(Percpu *cpu)
{
    cpu->taskLockCnt++;
}
/// 释放CPU调度锁
STATIC INLINE VOID OsCpuSchedUnlock(Percpu *cpu, UINT32 intSave)
{
    if (cpu->taskLockCnt > 0) {
        cpu->taskLockCnt--;
        if ((cpu->taskLockCnt == 0) && (cpu->schedFlag & INT_PEND_RESCH) && OS_SCHEDULER_ACTIVE) {
            cpu->schedFlag &= ~INT_PEND_RESCH;
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
