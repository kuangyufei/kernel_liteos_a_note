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

#include "los_mp.h"
#include "los_task_pri.h"
#include "los_percpu_pri.h"
#include "los_sched_pri.h"
#include "los_swtmr.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
/*******************************************************	
多CPU核的操作系统3种处理模式(SMP+AMP+BMP) 鸿蒙实现的是 SMP 的方式
非对称多处理（Asymmetric multiprocessing，AMP）每个CPU内核运行一个独立的操作系统或同一操作系统的独立实例（instantiation）。
对称多处理（Symmetric multiprocessing，SMP）一个操作系统的实例可以同时管理所有CPU内核，且应用并不绑定某一个内核。
混合多处理（Bound multiprocessing，BMP）一个操作系统的实例可以同时管理所有CPU内核，但每个应用被锁定于某个指定的核心。
********************************************************/

#if (LOSCFG_KERNEL_SMP == YES)
//给target对应位CPU发送调度信号
VOID LOS_MpSchedule(UINT32 target)//target每位对应CPU core 
{
    UINT32 cpuid = ArchCurrCpuid();
    target &= ~(1U << cpuid);
    HalIrqSendIpi(target, LOS_MP_IPI_SCHEDULE);//处理器间中断（IPI）
}
//硬中断唤醒处理函数
VOID OsMpWakeHandler(VOID)
{
    /* generic wakeup ipi, do nothing */
}
//硬中断调度处理函数
VOID OsMpScheduleHandler(VOID)
{//将调度标志设置为与唤醒功能不同，这样就可以在硬中断结束时触发调度程序。
    /*
     * set schedule flag to differ from wake function,
     * so that the scheduler can be triggered at the end of irq.
     */
    OsPercpuGet()->schedFlag = INT_PEND_RESCH;//贴上调度标签
}
//硬中断暂停处理函数
VOID OsMpHaltHandler(VOID)
{
    (VOID)LOS_IntLock();
    OsPercpuGet()->excFlag = CPU_HALT;//让当前Cpu停止工作

    while (1) {}//陷入空循环,也就是空闲状态
}
//MP定时器处理函数, 递归检查所有可用任务
VOID OsMpCollectTasks(VOID)
{
    LosTaskCB *taskCB = NULL;
    UINT32 taskID = 0;
    UINT32 ret;

    /* recursive checking all the available task */
    for (; taskID <= g_taskMaxNum; taskID++) {
        taskCB = &g_taskCBArray[taskID];

        if (OsTaskIsUnused(taskCB) || OsTaskIsRunning(taskCB)) {
            continue;
        }

        /* 虽然任务状态不是原子的，但此检查可能成功，但无法完成删除,此删除将在下次运行之前处理
         * though task status is not atomic, this check may success but not accomplish
         * the deletion; this deletion will be handled until the next run.
         */
        if (taskCB->signal & SIGNAL_KILL) {
            ret = LOS_TaskDelete(taskID);
            if (ret != LOS_OK) {
                PRINT_WARN("GC collect task failed err:0x%x\n", ret);
            }
        }
    }
}
//MP(multiprocessing) 多核处理器初始化
UINT32 OsMpInit(VOID)
{
    UINT16 swtmrId;

    (VOID)LOS_SwtmrCreate(OS_MP_GC_PERIOD, LOS_SWTMR_MODE_PERIOD, //创建一个周期性,持续时间为 100个tick的定时器
                          (SWTMR_PROC_FUNC)OsMpCollectTasks, &swtmrId, 0);//OsMpCollectTasks为超时回调函数
    (VOID)LOS_SwtmrStart(swtmrId);//开始定时任务

    return LOS_OK;
}

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
