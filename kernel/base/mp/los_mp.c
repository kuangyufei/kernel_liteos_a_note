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

#include "los_mp.h"
#include "los_init.h"
#include "los_percpu_pri.h"
#include "los_sched_pri.h"
#include "los_swtmr.h"
#include "los_task_pri.h"
/*******************************************************	
多CPU核的操作系统3种处理模式(SMP+AMP+BMP) 鸿蒙实现的是 SMP 的方式
	非对称多处理（Asymmetric multiprocessing，AMP）每个CPU内核
	运行一个独立的操作系统或同一操作系统的独立实例（instantiation）。
	
	对称多处理（Symmetric multiprocessing，SMP）一个操作系统的实例
	可以同时管理所有CPU内核，且应用并不绑定某一个内核。
	
	混合多处理（Bound multiprocessing，BMP）一个操作系统的实例可以
	同时管理所有CPU内核，但每个应用被锁定于某个指定的核心。

多核多线程处理器的中断
	由 PIC(Programmable Interrupt Controller）统一控制。PIC 允许一个
	硬件线程中断其他的硬件线程，这种方式被称为核间中断(Inter-Processor Interrupts,IPI）
	
SGI:软件触发中断(Software Generated Interrupt)。在arm处理器中，
	SGI共有16个,硬件中断号分别为ID0~ID15。它通常用于多核间通讯。
********************************************************/

#ifdef LOSCFG_KERNEL_SMP
//给参数CPU发送调度信号
#ifdef LOSCFG_KERNEL_SMP_CALL
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_mpCallSpin);
#define MP_CALL_LOCK(state)       LOS_SpinLockSave(&g_mpCallSpin, &(state))
#define MP_CALL_UNLOCK(state)     LOS_SpinUnlockRestore(&g_mpCallSpin, (state))
#endif
VOID LOS_MpSchedule(UINT32 target)//target每位对应CPU core 
{
    UINT32 cpuid = ArchCurrCpuid();
    target &= ~(1U << cpuid);//获取除了自身之外的其他CPU
    HalIrqSendIpi(target, LOS_MP_IPI_SCHEDULE);//向目标CPU发送调度信号,核间中断(Inter-Processor Interrupts),IPI
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
    OsPercpuGet()->schedFlag |= INT_PEND_RESCH;//贴上调度标签
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
    for (; taskID <= g_taskMaxNum; taskID++) {	//递归检查所有可用任务
        taskCB = &g_taskCBArray[taskID];

        if (OsTaskIsUnused(taskCB) || OsTaskIsRunning(taskCB)) {
            continue;
        }

        /* 虽然任务状态不是原子的，但此检查可能成功，但无法完成删除,此删除将在下次运行之前处理
         * though task status is not atomic, this check may success but not accomplish
         * the deletion; this deletion will be handled until the next run.
         */
        if (taskCB->signal & SIGNAL_KILL) {//任务收到被干掉信号
            ret = LOS_TaskDelete(taskID);//干掉任务,回归任务池
            if (ret != LOS_OK) {
                PRINT_WARN("GC collect task failed err:0x%x\n", ret);
            }
        }
    }
}

#ifdef LOSCFG_KERNEL_SMP_CALL
VOID OsMpFuncCall(UINT32 target, SMP_FUNC_CALL func, VOID *args)
{
    UINT32 index;
    UINT32 intSave;

    if (func == NULL) {
        return;
    }

    if (!(target & OS_MP_CPU_ALL)) {
        return;
    }

    for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
        if (CPUID_TO_AFFI_MASK(index) & target) {
            MpCallFunc *mpCallFunc = (MpCallFunc *)LOS_MemAlloc(m_aucSysMem0, sizeof(MpCallFunc));
            if (mpCallFunc == NULL) {
                PRINT_ERR("smp func call malloc failed\n");
                return;
            }
            mpCallFunc->func = func;
            mpCallFunc->args = args;

            MP_CALL_LOCK(intSave);
            LOS_ListAdd(&g_percpu[index].funcLink, &(mpCallFunc->node));
            MP_CALL_UNLOCK(intSave);
        }
    }
    HalIrqSendIpi(target, LOS_MP_IPI_FUNC_CALL);
}

VOID OsMpFuncCallHandler(VOID)
{
    UINT32 intSave;
    UINT32 cpuid = ArchCurrCpuid();
    LOS_DL_LIST *list = NULL;
    MpCallFunc *mpCallFunc = NULL;

    MP_CALL_LOCK(intSave);
    while (!LOS_ListEmpty(&g_percpu[cpuid].funcLink)) {
        list = LOS_DL_LIST_FIRST(&g_percpu[cpuid].funcLink);
        LOS_ListDelete(list);
        MP_CALL_UNLOCK(intSave);

        mpCallFunc = LOS_DL_LIST_ENTRY(list, MpCallFunc, node);
        mpCallFunc->func(mpCallFunc->args);
        (VOID)LOS_MemFree(m_aucSysMem0, mpCallFunc);

        MP_CALL_LOCK(intSave);
    }
    MP_CALL_UNLOCK(intSave);
}

VOID OsMpFuncCallInit(VOID)
{
    UINT32 index;
    /* init funclink for each core */
    for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
        LOS_ListInit(&g_percpu[index].funcLink);
    }
}
#endif /* LOSCFG_KERNEL_SMP_CALL */
//MP(multiprocessing) 多核处理器初始化
UINT32 OsMpInit(VOID)
{
    UINT16 swtmrId;

    (VOID)LOS_SwtmrCreate(OS_MP_GC_PERIOD, LOS_SWTMR_MODE_PERIOD, //创建一个周期性,持续时间为 100个tick的定时器
                          (SWTMR_PROC_FUNC)OsMpCollectTasks, &swtmrId, 0);//OsMpCollectTasks为超时回调函数
    (VOID)LOS_SwtmrStart(swtmrId);//开始定时任务
#ifdef LOSCFG_KERNEL_SMP_CALL
    OsMpFuncCallInit();
#endif
    return LOS_OK;
}

LOS_MODULE_INIT(OsMpInit, LOS_INIT_LEVEL_KMOD_TASK);//多处理器模块初始化

#endif

