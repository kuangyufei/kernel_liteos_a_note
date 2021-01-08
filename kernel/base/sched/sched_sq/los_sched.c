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

#include "los_base.h"
#include "los_task_pri.h"
#include "los_percpu_pri.h"
#include "los_hw_pri.h"
#include "los_arch_mmu.h"
#include "los_process_pri.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//调度算法-进程切换
STATIC VOID OsSchedSwitchProcess(LosProcessCB *runProcess, LosProcessCB *newProcess)
{
    if (runProcess == newProcess) {
        return;
    }

#if (LOSCFG_KERNEL_SMP == YES)
    runProcess->processStatus = OS_PROCESS_RUNTASK_COUNT_DEC(runProcess->processStatus);
    newProcess->processStatus = OS_PROCESS_RUNTASK_COUNT_ADD(newProcess->processStatus);

    LOS_ASSERT(!(OS_PROCESS_GET_RUNTASK_COUNT(newProcess->processStatus) > LOSCFG_KERNEL_CORE_NUM));
    if (OS_PROCESS_GET_RUNTASK_COUNT(runProcess->processStatus) == 0) {//获取当前进程的任务数量
#endif
        runProcess->processStatus &= ~OS_PROCESS_STATUS_RUNNING;
        if ((runProcess->threadNumber > 1) && !(runProcess->processStatus & OS_PROCESS_STATUS_READY)) {
            runProcess->processStatus |= OS_PROCESS_STATUS_PEND;
        }
#if (LOSCFG_KERNEL_SMP == YES)
    }
#endif
    LOS_ASSERT(!(newProcess->processStatus & OS_PROCESS_STATUS_PEND));//断言进程不是阻塞状态
    newProcess->processStatus |= OS_PROCESS_STATUS_RUNNING;//设置进程状态为运行状态

    if (OsProcessIsUserMode(newProcess)) {//用户模式下切换进程mmu上下文
        LOS_ArchMmuContextSwitch(&newProcess->vmSpace->archMmu);//新进程->虚拟空间中的->Mmu部分入参
    }

#ifdef LOSCFG_KERNEL_CPUP
    OsProcessCycleEndStart(newProcess->processID, OS_PROCESS_GET_RUNTASK_COUNT(runProcess->processStatus) + 1);
#endif /* LOSCFG_KERNEL_CPUP */

    OsCurrProcessSet(newProcess);//将进程置为 g_runProcess

    if ((newProcess->timeSlice == 0) && (newProcess->policy == LOS_SCHED_RR)) {//为用完时间片或初始进程分配时间片
        newProcess->timeSlice = OS_PROCESS_SCHED_RR_INTERVAL;//重新分配时间片，默认 20ms
    }
}
//重新调度实现
VOID OsSchedResched(VOID)
{
    LosTaskCB *runTask = NULL;
    LosTaskCB *newTask = NULL;
    LosProcessCB *runProcess = NULL;
    LosProcessCB *newProcess = NULL;

    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));//必须持有任务自旋锁,自旋锁是不是进程层面去抢锁,而是CPU各自核之间去争夺锁

    if (!OsPreemptableInSched()) {//是否置了重新调度标识位
        return;
    }

    runTask = OsCurrTaskGet();//获取当前任务
    newTask = OsGetTopTask();//获取优先级最最最高的任务

    /* always be able to get one task */
    LOS_ASSERT(newTask != NULL);//不能没有需调度的任务

    if (runTask == newTask) {//当前任务就是最高任务,那还调度个啥的,直接退出.
        return;
    }

    runTask->taskStatus &= ~OS_TASK_STATUS_RUNNING;//当前任务状态位置成不在运行状态
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;//最高任务状态位置成在运行状态

    runProcess = OS_PCB_FROM_PID(runTask->processID);//通过进程ID索引拿到进程实体
    newProcess = OS_PCB_FROM_PID(newTask->processID);//同上

    OsSchedSwitchProcess(runProcess, newProcess);//切换进程,里面主要涉及进程空间的切换,也就是MMU的上下文切换.

#if (LOSCFG_KERNEL_SMP == YES)//CPU多核的情况
    /* mask new running task's owner processor */
    runTask->currCpu = OS_TASK_INVALID_CPUID;//当前任务不占用CPU
    newTask->currCpu = ArchCurrCpuid();//让新任务占用CPU
#endif

    (VOID)OsTaskSwitchCheck(runTask, newTask);//切换task的检查

#if (LOSCFG_KERNEL_SCHED_STATISTICS == YES)
    OsSchedStatistics(runTask, newTask);
#endif

    if ((newTask->timeSlice == 0) && (newTask->policy == LOS_SCHED_RR)) {//没有时间片且是抢占式调度的方式,注意 非抢占式都不需要时间片的.
        newTask->timeSlice = LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT;//给新任务时间片 默认 20ms
    }

    OsCurrTaskSet((VOID*)newTask);//设置新的task为CPU核的当前任务

    if (OsProcessIsUserMode(newProcess)) {//用户模式下会怎么样?
        OsCurrUserTaskSet(newTask->userArea);//设置task栈空间
    }

    PRINT_TRACE("cpu%d run process name: (%s) pid: %d status: %x threadMap: %x task name: (%s) tid: %d status: %x ->\n"
                "     new process name: (%s) pid: %d status: %x threadMap: %x task name: (%s) tid: %d status: %x!\n",
                ArchCurrCpuid(),
                runProcess->processName, runProcess->processID, runProcess->processStatus,
                runProcess->threadScheduleMap, runTask->taskName,  runTask->taskID, runTask->taskStatus,
                newProcess->processName, newProcess->processID, newProcess->processStatus,
                newProcess->threadScheduleMap, newTask->taskName, newTask->taskID, newTask->taskStatus);

    /* do the task context switch */
    OsTaskSchedule(newTask, runTask);//切换CPU的上下文,注意OsTaskSchedule是一个汇编函数 见于 los_dispatch.s
}
//抢占式调度,调用极为频繁的函数
VOID OsSchedPreempt(VOID)
{
    LosTaskCB *runTask = NULL;
    UINT32 intSave;

    if (!OsPreemptable()) {//是否允许抢占式调度? 中断发生时是不允许调度的
        return;
    }

    SCHEDULER_LOCK(intSave);

    /* add run task back to ready queue */
    runTask = OsCurrTaskGet();//当前任务要被重新回到就绪队列
    OS_TASK_SCHED_QUEUE_ENQUEUE(runTask, 0);//参数0代表不改变当前任务的状态,因为任务正在运行,所以将从头部插入

    /* reschedule to new thread */
    OsSchedResched();//执行调度算法

    SCHEDULER_UNLOCK(intSave);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
