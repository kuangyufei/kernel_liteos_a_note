/*!
 * @file    los_cpup.c
 * @brief
 * @link kernel-small-debug-process-cpu http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-debug-process-cpu.html @endlink
   @verbatim
   基本概念
	   CPU（中央处理器，Central Processing Unit）占用率分为系统CPU占用率、进程CPU占用率、任务CPU占用率
	   和中断CPU占用率。用户通过系统级的CPU占用率，判断当前系统负载是否超出设计规格。通过系统中各个
	   进程/任务/中断的CPU占用情况，判断各个进程/任务/中断的CPU占用率是否符合设计的预期。
	   
		   系统CPU占用率（CPU Percent）

		   指周期时间内系统的CPU占用率，用于表示系统一段时间内的闲忙程度，也表示CPU的负载情况。系统CPU占用率
		   的有效表示范围为0～100，其精度（可通过配置调整）为百分比。100表示系统满负荷运转。
	   
		   进程CPU占用率
		   
		   指单个进程的CPU占用率，用于表示单个进程在一段时间内的闲忙程度。进程CPU占用率的有效表示范围为0～100，
		   其精度（可通过配置调整）为百分比。100表示在一段时间内系统一直在运行该进程。
		   
		   任务CPU占用率
		   
		   指单个任务的CPU占用率，用于表示单个任务在一段时间内的闲忙程度。任务CPU占用率的有效表示范围为0～100，
		   其精度（可通过配置调整）为百分比。100表示在一段时间内系统一直在运行该任务。
		   
		   中断CPU占用率
		   
		   指单个中断的CPU占用率，用于表示单个中断在一段时间内的闲忙程度。中断CPU占用率的有效表示范围为0～100，
		   其精度（可通过配置调整）为百分比。100表示在一段时间内系统一直在运行该中断。

   运行机制
	   OpenHarmony LiteOS-A内核CPUP（CPU Percent，CPU占用率）模块采用进程、任务和中断级记录的方式，在进程/任务切换时，
	   记录进程/任务启动时间，进程/任务切出或者退出时，系统会累加整个进程/任务的占用时间; 在执行中断时系统会累加记录
	   每个中断的执行时间。
	   
	   OpenHarmony 提供以下四种CPU占用率的信息查询：
	   
		   系统CPU占用率
		   进程CPU占用率
		   任务CPU占用率
		   中断CPU占用率
		   
	   CPU占用率的计算方法：
	   
	   系统CPU占用率=系统中除idle任务外其他任务运行总时间/系统运行总时间
	   
	   进程CPU占用率=进程运行总时间/系统运行总时间
	   
	   任务CPU占用率=任务运行总时间/系统运行总时间
	   
	   中断CPU占用率=中断运行总时间/系统运行总时间
	   
   开发流程
	   CPU占用率的典型开发流程：
	   
	   调用获取系统历史CPU占用率函数LOS_HistorySysCpuUsage。
	   
	   调用获取指定进程历史CPU占用率函数LOS_HistoryProcessCpuUsage。
	   
	   若进程已创建，则关中断，根据不同模式正常获取，恢复中断；
	   若进程未创建，则返回错误码；
	   调用获取所有进程CPU占用率函数LOS_GetAllProcessCpuUsage。
	   
	   若CPUP已初始化，则关中断，根据不同模式正常获取，恢复中断；
	   若CPUP未初始化或有非法入参，则返回错误码；
	   调用获取指定任务历史CPU占用率函数LOS_HistoryTaskCpuUsage。
	   
	   若任务已创建，则关中断，根据不同模式正常获取，恢复中断；
	   若任务未创建，则返回错误码；
	   调用获取所有中断CPU占用率函数LOS_GetAllIrqCpuUsage。
	   
	   若CPUP已初始化，则关中断，根据不同模式正常获取，恢复中断；
	   若CPUP未初始化或有非法入参，则返回错误码；
   
   @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/27/mux.png
 * @attention   
 * @version 
 * @author  weharmonyos.com
 * @date    2021-11-21
 */

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

#include "los_cpup_pri.h"
#include "los_base.h"
#include "los_init.h"
#include "los_process_pri.h"
#include "los_swtmr.h"


#ifdef LOSCFG_KERNEL_CPUP

LITE_OS_SEC_BSS STATIC UINT16 cpupSwtmrID;	///< 监测CPU使用情况定时器s
LITE_OS_SEC_BSS STATIC UINT16 cpupInitFlg = 0;
LITE_OS_SEC_BSS OsIrqCpupCB *g_irqCpup = NULL;
LITE_OS_SEC_BSS STATIC UINT16 cpupMaxNum;
LITE_OS_SEC_BSS STATIC UINT16 cpupHisPos = 0; /* current Sampling point of historyTime */
LITE_OS_SEC_BSS STATIC UINT64 cpuHistoryTime[OS_CPUP_HISTORY_RECORD_NUM + 1];
LITE_OS_SEC_BSS STATIC UINT32 runningTasks[LOSCFG_KERNEL_CORE_NUM]; 
LITE_OS_SEC_BSS STATIC UINT64 cpupStartCycles = 0; ///< 记录
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
LITE_OS_SEC_BSS UINT64 timeInIrqSwitch[LOSCFG_KERNEL_CORE_NUM];
LITE_OS_SEC_BSS STATIC UINT64 cpupIntTimeStart[LOSCFG_KERNEL_CORE_NUM];
#endif

#define INVALID_ID ((UINT32)-1)

#define OS_CPUP_UNUSED 0x0U
#define OS_CPUP_USED   0x1U
#define HIGH_BITS 32

#define CPUP_PRE_POS(pos) (((pos) == 0) ? (OS_CPUP_HISTORY_RECORD_NUM - 1) : ((pos) - 1))
#define CPUP_POST_POS(pos) (((pos) == (OS_CPUP_HISTORY_RECORD_NUM - 1)) ? 0 : ((pos) + 1))
///< 获取CPU周期
STATIC UINT64 OsGetCpuCycle(VOID)
{
    UINT32 high;
    UINT32 low;
    UINT64 cycles;//周期数用 64位计算，读取分成高低位

    LOS_GetCpuCycle(&high, &low);//将64位拆成两个32位
    cycles = ((UINT64)high << HIGH_BITS) + low;
    if (cpupStartCycles == 0) {
        cpupStartCycles = cycles;
    }

    /*
     * The cycles should keep growing, if the checking failed,
     * it mean LOS_GetCpuCycle has the problem which should be fixed.
     */
    LOS_ASSERT(cycles >= cpupStartCycles);

    return (cycles - cpupStartCycles);
}

LITE_OS_SEC_TEXT_INIT VOID OsCpupGuard(VOID)
{
    UINT16 prevPos;
    UINT32 loop;
    UINT32 runTaskID;
    UINT32 intSave;
    UINT64 cycle, cycleIncrement;
    LosTaskCB *taskCB = NULL;
    LosProcessCB *processCB = NULL;

    SCHEDULER_LOCK(intSave);

    cycle = OsGetCpuCycle();
    prevPos = cpupHisPos;
    cpupHisPos = CPUP_POST_POS(cpupHisPos);
    cpuHistoryTime[prevPos] = cycle;

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    for (loop = 0; loop < cpupMaxNum; loop++) {
        g_irqCpup[loop].cpup.historyTime[prevPos] = g_irqCpup[loop].cpup.allTime;
    }
#endif

    for (loop = 0; loop < g_processMaxNum; loop++) {
        processCB = OS_PCB_FROM_PID(loop);
        if (processCB->processCpup == NULL) {
            continue;
        }
        processCB->processCpup->historyTime[prevPos] = processCB->processCpup->allTime;
    }

    for (loop = 0; loop < g_taskMaxNum; loop++) {
        taskCB = OS_TCB_FROM_TID(loop);
        taskCB->taskCpup.historyTime[prevPos] = taskCB->taskCpup.allTime;
    }

    for (loop = 0; loop < LOSCFG_KERNEL_CORE_NUM; loop++) {
        runTaskID = runningTasks[loop];
        if (runTaskID == INVALID_ID) {
            continue;
        }
        taskCB = OS_TCB_FROM_TID(runTaskID);

        /* reacquire the cycle to prevent flip */
        cycle = OsGetCpuCycle();
        cycleIncrement = cycle - taskCB->taskCpup.startTime;
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
        cycleIncrement -= timeInIrqSwitch[loop];
#endif
        taskCB->taskCpup.historyTime[prevPos] += cycleIncrement;
        processCB = OS_PCB_FROM_PID(taskCB->processID);
        processCB->processCpup->historyTime[prevPos] += cycleIncrement;
    }

    SCHEDULER_UNLOCK(intSave);
}
///创建cpu使用统计定时器
LITE_OS_SEC_TEXT_INIT UINT32 OsCpupGuardCreator(VOID)
{
    (VOID)LOS_SwtmrCreate(LOSCFG_BASE_CORE_TICK_PER_SECOND, LOS_SWTMR_MODE_PERIOD,
                          (SWTMR_PROC_FUNC)OsCpupGuard, &cpupSwtmrID, 0);

    (VOID)LOS_SwtmrStart(cpupSwtmrID);

    return LOS_OK;
}

LOS_MODULE_INIT(OsCpupGuardCreator, LOS_INIT_LEVEL_KMOD_TASK);

/*
 * Description: initialization of CPUP
 * Return     : LOS_OK or Error Information
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsCpupInit(VOID)
{
    UINT16 loop;
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    UINT32 size;

    cpupMaxNum = OS_HWI_MAX_NUM;

    /* every process has only one record, and it won't operated at the same time */
    size = cpupMaxNum * sizeof(OsIrqCpupCB);
    g_irqCpup = (OsIrqCpupCB *)LOS_MemAlloc(m_aucSysMem0, size);
    if (g_irqCpup == NULL) {
        PRINT_ERR("OsCpupInit error\n");
        return LOS_ERRNO_CPUP_NO_MEMORY;
    }

    (VOID)memset_s(g_irqCpup, size, 0, size);
#endif

    for (loop = 0; loop < LOSCFG_KERNEL_CORE_NUM; loop++) {
        runningTasks[loop] = INVALID_ID;
    }
    cpupInitFlg = 1;
    return LOS_OK;
}

LOS_MODULE_INIT(OsCpupInit, LOS_INIT_LEVEL_KMOD_EXTENDED);

STATIC VOID OsResetCpup(OsCpupBase *cpup, UINT64 cycle)
{
    UINT16 loop;

    cpup->startTime = cycle;
    cpup->allTime = cycle;
    for (loop = 0; loop < (OS_CPUP_HISTORY_RECORD_NUM + 1); loop++) {
        cpup->historyTime[loop] = cycle;
    }
}

LITE_OS_SEC_TEXT_INIT VOID LOS_CpupReset(VOID)
{
    LosProcessCB *processCB = NULL;
    LosTaskCB *taskCB = NULL;
    UINT32 index;
    UINT64 cycle;
    UINT32 intSave;

    cpupInitFlg = 0;
    intSave = LOS_IntLock();
    (VOID)LOS_SwtmrStop(cpupSwtmrID);
    cycle = OsGetCpuCycle();

    for (index = 0; index < (OS_CPUP_HISTORY_RECORD_NUM + 1); index++) {
        cpuHistoryTime[index] = cycle;
    }

    for (index = 0; index < g_processMaxNum; index++) {
        processCB = OS_PCB_FROM_PID(index);
        if (processCB->processCpup == NULL) {
            continue;
        }
        OsResetCpup(processCB->processCpup, cycle);
    }

    for (index = 0; index < g_taskMaxNum; index++) {
        taskCB = OS_TCB_FROM_TID(index);
        OsResetCpup(&taskCB->taskCpup, cycle);
    }

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    if (g_irqCpup != NULL) {
        for (index = 0; index < cpupMaxNum; index++) {
            OsResetCpup(&g_irqCpup[index].cpup, cycle);
        }

        for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
            timeInIrqSwitch[index] = 0;
        }
    }
#endif

    (VOID)LOS_SwtmrStart(cpupSwtmrID);
    LOS_IntRestore(intSave);
    cpupInitFlg = 1;

    return;
}

VOID OsCpupCycleEndStart(UINT32 runTaskID, UINT32 newTaskID)
{
    /* OsCurrTaskGet and OsCurrProcessGet are not allowed to be called. */
    LosTaskCB *runTask = OS_TCB_FROM_TID(runTaskID);
    OsCpupBase *runTaskCpup = &runTask->taskCpup;
    OsCpupBase *newTaskCpup = (OsCpupBase *)&(OS_TCB_FROM_TID(newTaskID)->taskCpup);
    OsCpupBase *processCpup = OS_PCB_FROM_PID(runTask->processID)->processCpup;
    UINT64 cpuCycle, cycleIncrement;
    UINT16 cpuID = ArchCurrCpuid();

    if (cpupInitFlg == 0) {
        return;
    }

    cpuCycle = OsGetCpuCycle();
    if (runTaskCpup->startTime != 0) {
        cycleIncrement = cpuCycle - runTaskCpup->startTime;
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
        cycleIncrement -= timeInIrqSwitch[cpuID];
        timeInIrqSwitch[cpuID] = 0;
#endif
        runTaskCpup->allTime += cycleIncrement;
        if (processCpup != NULL) {
            processCpup->allTime += cycleIncrement;
        }
        runTaskCpup->startTime = 0;
    }

    newTaskCpup->startTime = cpuCycle;
    runningTasks[cpuID] = newTaskID;
}

LITE_OS_SEC_TEXT_MINOR STATIC VOID OsCpupGetPos(UINT16 mode, UINT16 *curPosPointer, UINT16 *prePosPointer)
{
    UINT16 curPos;
    UINT16 tmpPos;
    UINT16 prePos;

    tmpPos = cpupHisPos;
    curPos = CPUP_PRE_POS(tmpPos);

    /*
     * The current postion has nothing to do with the CPUP modes,
     * however, the previous postion differs.
     */
    switch (mode) {
        case CPUP_LAST_ONE_SECONDS:
            prePos = CPUP_PRE_POS(curPos);
            break;
        case CPUP_LAST_TEN_SECONDS:
            prePos = tmpPos;
            break;
        case CPUP_ALL_TIME:
            /* fall-through */
        default:
            prePos = OS_CPUP_HISTORY_RECORD_NUM;
            break;
    }

    *curPosPointer = curPos;
    *prePosPointer = prePos;

    return;
}

STATIC INLINE UINT32 OsCalculateCpupUsage(const OsCpupBase *cpup, UINT16 pos, UINT16 prePos, UINT64 allCycle)
{
    UINT32 usage = 0;
    UINT64 cpuCycle = cpup->historyTime[pos] - cpup->historyTime[prePos];

    if (allCycle) {
        usage = (UINT32)((LOS_CPUP_SINGLE_CORE_PRECISION * cpuCycle) / allCycle);
    }
    return usage;
}

STATIC UINT32 OsHistorySysCpuUsageUnsafe(UINT16 mode)
{
    UINT64 cpuAllCycle;
    UINT16 pos;
    UINT16 prePos;
    UINT32 idleProcessID;
    OsCpupBase *processCpup = NULL;

    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;
    }

    OsCpupGetPos(mode, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];

    idleProcessID = OsGetIdleProcessID();
    processCpup = OS_PCB_FROM_PID(idleProcessID)->processCpup;
    return (LOS_CPUP_PRECISION - OsCalculateCpupUsage(processCpup, pos, prePos, cpuAllCycle));
}

/**
  * \brief 获取系统历史CPU占用率
  * 指周期时间内系统的CPU占用率，用于表示系统一段时间内的闲忙程度，也表示CPU的负载情况。系统CPU
  * 占用率的有效表示范围为0～100，其精度（可通过配置调整）为百分比。100表示系统满负荷运转。
  * \param mode
  *
  * \return 
  */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_HistorySysCpuUsage(UINT16 mode)
{
    UINT32 cpup;
    UINT32 intSave;

    /* get end time of current process */
    SCHEDULER_LOCK(intSave);
    cpup = OsHistorySysCpuUsageUnsafe(mode);
    SCHEDULER_UNLOCK(intSave);
    return cpup;
}

STATIC UINT32 OsHistoryProcessCpuUsageUnsafe(UINT32 pid, UINT16 mode)
{
    LosProcessCB *processCB = NULL;
    UINT64 cpuAllCycle;
    UINT16 pos, prePos;

    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;
    }

    if (OS_PID_CHECK_INVALID(pid)) {
        return LOS_ERRNO_CPUP_ID_INVALID;
    }

    processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        return LOS_ERRNO_CPUP_NO_CREATED;
    }

    if (processCB->processCpup == NULL) {
        return LOS_ERRNO_CPUP_ID_INVALID;
    }

    OsCpupGetPos(mode, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];

    return OsCalculateCpupUsage(processCB->processCpup, pos, prePos, cpuAllCycle);
}

/**
  * \brief 获取指定进程历史CPU占用率
  * 指单个进程的CPU占用率，用于表示单个进程在一段时间内的闲忙程度。进程CPU占用率的有效表示范围为0～100，
  * 其精度（可通过配置调整）为百分比。100表示在一段时间内系统一直在运行该进程。
  * \param pid
  * \param mode
  *
  * \return 
  */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_HistoryProcessCpuUsage(UINT32 pid, UINT16 mode)
{
    UINT32 cpup;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    cpup = OsHistoryProcessCpuUsageUnsafe(pid, mode);
    SCHEDULER_UNLOCK(intSave);
    return cpup;
}

STATIC UINT32 OsHistoryTaskCpuUsageUnsafe(UINT32 tid, UINT16 mode)
{
    LosTaskCB *taskCB = NULL;
    UINT64 cpuAllCycle;
    UINT16 pos, prePos;

    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;
    }

    if (OS_TID_CHECK_INVALID(tid)) {
        return LOS_ERRNO_CPUP_ID_INVALID;
    }

    taskCB = OS_TCB_FROM_TID(tid);
    if (OsTaskIsUnused(taskCB)) {
        return LOS_ERRNO_CPUP_NO_CREATED;
    }

    OsCpupGetPos(mode, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];

    return OsCalculateCpupUsage(&taskCB->taskCpup, pos, prePos, cpuAllCycle);
}

/**
  * \brief 获取指定任务历史CPU占用率
  * 指单个任务的CPU占用率，用于表示单个任务在一段时间内的闲忙程度。任务CPU占用率的有效表示范围为0～100，
  * 其精度（可通过配置调整）为百分比。100表示在一段时间内系统一直在运行该任务。
  * \param tid
  * \param mode
  *
  * \return 
  */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_HistoryTaskCpuUsage(UINT32 tid, UINT16 mode)
{
    UINT32 intSave;
    UINT32 cpup;

    SCHEDULER_LOCK(intSave);
    cpup = OsHistoryTaskCpuUsageUnsafe(tid, mode);
    SCHEDULER_UNLOCK(intSave);
    return cpup;
}

STATIC UINT32 OsCpupUsageParamCheckAndReset(CPUP_INFO_S *cpupInfo, UINT32 len, UINT32 number)
{
    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;
    }

    if ((cpupInfo == NULL) || (len < (sizeof(CPUP_INFO_S) * number))) {
        return LOS_ERRNO_CPUP_PTR_ERR;
    }

    (VOID)memset_s(cpupInfo, len, 0, len);
    return LOS_OK;
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsGetAllProcessCpuUsageUnsafe(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    LosProcessCB *processCB = NULL;
    UINT64 cpuAllCycle;
    UINT16 pos, prePos;
    UINT32 processID;
    UINT32 ret;

    ret = OsCpupUsageParamCheckAndReset(cpupInfo, len, g_processMaxNum);
    if (ret != LOS_OK) {
        return ret;
    }

    OsCpupGetPos(mode, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];

    for (processID = 0; processID < g_processMaxNum; processID++) {
        processCB = OS_PCB_FROM_PID(processID);
        if (OsProcessIsUnused(processCB) || (processCB->processCpup == NULL)) {
            continue;
        }

        cpupInfo[processID].usage = OsCalculateCpupUsage(processCB->processCpup, pos, prePos, cpuAllCycle);
        cpupInfo[processID].status = OS_CPUP_USED;
    }

    return LOS_OK;
}
/// 获取系统所有进程的历史CPU占用率
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_GetAllProcessCpuUsage(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    UINT32 intSave;
    UINT32 ret;

    SCHEDULER_LOCK(intSave);
    ret = OsGetAllProcessCpuUsageUnsafe(mode, cpupInfo, len);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsGetAllProcessAndTaskCpuUsageUnsafe(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    UINT64 cpuAllCycle;
    UINT16 pos, prePos;
    UINT32 taskID;
    UINT32 ret;
    LosTaskCB *taskCB = NULL;
    OsCpupBase *processCpupBase = NULL;
    CPUP_INFO_S *processCpup = cpupInfo;
    CPUP_INFO_S *taskCpup = (CPUP_INFO_S *)((UINTPTR)cpupInfo + sizeof(CPUP_INFO_S) * g_processMaxNum);

    ret = OsCpupUsageParamCheckAndReset(cpupInfo, len, g_taskMaxNum + g_processMaxNum);
    if (ret != LOS_OK) {
        return ret;
    }

    OsCpupGetPos(mode, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];

    for (taskID = 0; taskID < g_taskMaxNum; taskID++) {
        taskCB = OS_TCB_FROM_TID(taskID);
        if (OsTaskIsUnused(taskCB)) {
            continue;
        }

        taskCpup[taskID].usage = OsCalculateCpupUsage(&taskCB->taskCpup, pos, prePos, cpuAllCycle);
        taskCpup[taskID].status = OS_CPUP_USED;
        if (processCpup[taskCB->processID].status == OS_CPUP_UNUSED) {
            processCpupBase = OS_PCB_FROM_PID(taskCB->processID)->processCpup;
            if (processCpupBase != NULL) {
                processCpup[taskCB->processID].usage = OsCalculateCpupUsage(processCpupBase, pos, prePos, cpuAllCycle);
                processCpup[taskCB->processID].status = OS_CPUP_USED;
            }
        }
    }

    return LOS_OK;
}

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
LITE_OS_SEC_TEXT_MINOR VOID OsCpupIrqStart(VOID)
{
    UINT32 high;
    UINT32 low;

    LOS_GetCpuCycle(&high, &low);
    cpupIntTimeStart[ArchCurrCpuid()] = ((UINT64)high << HIGH_BITS) + low;
    return;
}

LITE_OS_SEC_TEXT_MINOR VOID OsCpupIrqEnd(UINT32 intNum)
{
    UINT32 high;
    UINT32 low;
    UINT64 intTimeEnd;
    UINT32 cpuID = ArchCurrCpuid();

    LOS_GetCpuCycle(&high, &low);
    intTimeEnd = ((UINT64)high << HIGH_BITS) + low;

    g_irqCpup[intNum].id = intNum;
    g_irqCpup[intNum].status = OS_CPUP_USED;
    timeInIrqSwitch[cpuID] += (intTimeEnd - cpupIntTimeStart[cpuID]);
    g_irqCpup[intNum].cpup.allTime += (intTimeEnd - cpupIntTimeStart[cpuID]);

    return;
}

LITE_OS_SEC_TEXT_MINOR OsIrqCpupCB *OsGetIrqCpupArrayBase(VOID)
{
    return g_irqCpup;
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsGetAllIrqCpuUsageUnsafe(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    UINT16 pos, prePos;
    UINT64 cpuAllCycle;
    UINT32 loop;
    UINT32 ret;

    ret = OsCpupUsageParamCheckAndReset(cpupInfo, len, cpupMaxNum);
    if (ret != LOS_OK) {
        return ret;
    }

    OsCpupGetPos(mode, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];

    for (loop = 0; loop < cpupMaxNum; loop++) {
        if (g_irqCpup[loop].status == OS_CPUP_UNUSED) {
            continue;
        }

        cpupInfo[loop].usage = OsCalculateCpupUsage(&g_irqCpup[loop].cpup, pos, prePos, cpuAllCycle);
        cpupInfo[loop].status = g_irqCpup[loop].status;
    }

    return LOS_OK;
}

/**
  * \brief 获取系统所有中断的历史CPU占用率
  * 指单个中断的CPU占用率，用于表示单个中断在一段时间内的闲忙程度。中断CPU占用率的有效表示范围为0～100，
  * 其精度（可通过配置调整）为百分比。100表示在一段时间内系统一直在运行该中断。
  * \param mode
  * \param cpupInfo
  * \param len
  *
  * \return 
  */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_GetAllIrqCpuUsage(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    UINT32 intSave;
    UINT32 ret;

    SCHEDULER_LOCK(intSave);
    ret = OsGetAllIrqCpuUsageUnsafe(mode, cpupInfo, len);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
#endif

#endif /* LOSCFG_KERNEL_CPUP */
