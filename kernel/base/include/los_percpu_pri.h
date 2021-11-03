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
#include "los_spinlock.h"
#include "los_sortlink_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_KERNEL_SMP
/*! \enum

*/
typedef enum {
    CPU_RUNNING = 0,   ///< cpu is running | CPU正在运行状态
    CPU_HALT,          ///< cpu in the halt | CPU处于暂停状态
    CPU_EXC            ///< cpu in the exc | CPU处于异常状态
} ExcFlag;
#endif

/*! \struct  内核对cpu的描述, per-CPU变量是linux系统一个非常有趣的特性，它为系统中的每个处理器都分配了该变量的副本。
 * 这样做的好处是，在多处理器系统中，当处理器操作属于它的变量副本时，不需要考虑与其他处理器的竞争的问题，
 */
typedef struct {
    SortLinkAttribute taskSortLink;             /*! task sort link | 挂等待和延时的任务 */
    SPIN_LOCK_S       taskSortLinkSpin;      ///<  task sort link spin lock | 操作taskSortLink链表的自旋锁
    SortLinkAttribute swtmrSortLink;         ///<  swtmr sort link | 挂还没到时间的定时器	
    SPIN_LOCK_S       swtmrSortLinkSpin;     ///<  swtmr sort link spin lock |* 操作swtmrSortLink链表的自旋锁
    UINT64            responseTime;          ///<  Response time for current nuclear Tick interrupts | 当前CPU核 Tick 中断的响应时间
    UINT64            tickStartTime;         ///<  The time when the tick interrupt starts processing |
    UINT32            responseID;            ///<  The response ID of the current nuclear TICK interrupt | 当前CPU核TICK中断的响应ID
    UINTPTR           runProcess;            ///<  The address of the process control block pointer to which the current kernel is running | 当前进程控制块地址
    UINT32 idleTaskID;                          ///<  idle task id | 每个CPU都有一个空闲任务 见于 OsIdleTaskCreate
    UINT32 taskLockCnt;                         ///<  task lock flag | 任务锁的数量,当 > 0 的时候,需要重新调度了
    UINT32 swtmrHandlerQueue;                   ///<  software timer timeout queue id | 软时钟超时队列句柄
    UINT32 swtmrTaskID;                         ///<  software timer task id | 软时钟任务ID
    UINT32 schedFlag;                           ///<  pending scheduler flag | 调度标识 INT_NO_RESCH INT_PEND_RESCH
#ifdef LOSCFG_KERNEL_SMP
    UINT32            excFlag;               ///<  cpu halt or exc flag | cpu 停止或 异常 标志
#ifdef LOSCFG_KERNEL_SMP_CALL
    LOS_DL_LIST       funcLink;              ///<  mp function call link
#endif
#endif
} Percpu;

/*! the kernel per-cpu structure | 每个cpu的内核描述符 */
extern Percpu g_percpu[LOSCFG_KERNEL_CORE_NUM];
/*! 获得当前运行CPU的信息 */
STATIC INLINE Percpu *OsPercpuGet(VOID)
{
    return &g_percpu[ArchCurrCpuid()];	
}
/*! 获得参数CPU的信息 */
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
