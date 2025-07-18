
/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-21
 */

#include "los_cpup_pri.h"
#include "los_base.h"
#include "los_init.h"
#include "los_process_pri.h"
#include "los_info_pri.h"
#include "los_swtmr.h"


#ifdef LOSCFG_KERNEL_CPUP
// CPU使用率统计定时器ID                                                    // 静态全局变量：CPU使用率统计定时器ID
LITE_OS_SEC_BSS STATIC UINT16 cpupSwtmrID;
// CPU使用率统计初始化标志位（0：未初始化，1：已初始化）                       // 静态全局变量：初始化状态标志
LITE_OS_SEC_BSS STATIC UINT16 cpupInitFlg = 0;
// 中断CPU使用率统计控制块指针                                               // 静态全局变量：中断CPU使用率统计结构体指针
LITE_OS_SEC_BSS OsIrqCpupCB *g_irqCpup = NULL;
// 最大中断CPU使用率统计项数量                                               // 静态全局变量：中断统计项最大数量
LITE_OS_SEC_BSS STATIC UINT32 cpupMaxNum;
// 当前历史时间采样点位置                                                   // 静态全局变量：历史记录数组当前索引
LITE_OS_SEC_BSS STATIC UINT16 cpupHisPos = 0; /* current Sampling point of historyTime */
// CPU历史时间记录数组（存储采样时刻的CPU周期数）                            // 静态全局变量：CPU周期历史记录数组
LITE_OS_SEC_BSS STATIC UINT64 cpuHistoryTime[OS_CPUP_HISTORY_RECORD_NUM + 1];
// 各CPU核心当前运行任务控制块指针数组                                       // 静态全局变量：核心运行任务记录数组
LITE_OS_SEC_BSS STATIC LosTaskCB *runningTasks[LOSCFG_KERNEL_CORE_NUM];
// CPU使用率统计起始周期数                                                   // 静态全局变量：统计起始CPU周期数
LITE_OS_SEC_BSS STATIC UINT64 cpupStartCycles = 0;
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
// 各CPU核心中断切换时间累计值                                               // 全局变量：中断耗时统计数组（按核心）
LITE_OS_SEC_BSS UINT64 timeInIrqSwitch[LOSCFG_KERNEL_CORE_NUM];
// 各CPU核心中断开始时间                                                    // 静态全局变量：中断开始时间数组（按核心）
LITE_OS_SEC_BSS STATIC UINT64 cpupIntTimeStart[LOSCFG_KERNEL_CORE_NUM];
#endif

// 无效ID定义（32位无符号整数的最大值）                                      // 宏定义：表示无效的ID值
#define INVALID_ID ((UINT32)-1)

// CPU使用率统计项未使用状态                                                // 宏定义：统计项状态常量-未使用
#define OS_CPUP_UNUSED 0x0U
// CPU使用率统计项已使用状态                                                // 宏定义：统计项状态常量-已使用
#define OS_CPUP_USED   0x1U
// 高位偏移位数（32位）                                                      // 宏定义：CPU周期高位数值偏移位数
#define HIGH_BITS 32

// 获取前一个历史记录位置（循环数组）                                        // 宏定义：计算历史记录数组的前一个索引
#define CPUP_PRE_POS(pos) (((pos) == 0) ? (OS_CPUP_HISTORY_RECORD_NUM - 1) : ((pos) - 1))
// 获取后一个历史记录位置（循环数组）                                        // 宏定义：计算历史记录数组的后一个索引
#define CPUP_POST_POS(pos) (((pos) == (OS_CPUP_HISTORY_RECORD_NUM - 1)) ? 0 : ((pos) + 1))

/**
 * @brief 获取CPU自统计开始后的周期数
 * @return CPU周期数（64位），相对于统计起始点的偏移值
 */
STATIC UINT64 OsGetCpuCycle(VOID)
{
    UINT32 high;                                                              // CPU周期计数值高位
    UINT32 low;                                                               // CPU周期计数值低位
    UINT64 cycles;                                                            // 组合后的64位CPU周期数

    LOS_GetCpuCycle(&high, &low);                                             // 获取当前CPU周期计数值
    cycles = ((UINT64)high << HIGH_BITS) + low;                              // 组合高低位得到64位周期数
    if (cpupStartCycles == 0) {                                               // 如果是首次调用（起始周期未设置）
        cpupStartCycles = cycles;                                             // 初始化起始周期数
    }

    /*
     * 周期数应持续增长，如果检查失败，说明LOS_GetCpuCycle存在问题需要修复
     */
    LOS_ASSERT(cycles >= cpupStartCycles);                                    // 断言：当前周期数必须大于等于起始周期数

    return (cycles - cpupStartCycles);                                        // 返回相对于起始点的周期偏移值
}

/**
 * @brief CPU使用率统计保护函数（定时采样）
 * @details 定期更新CPU、任务、进程和中断的历史时间记录，用于后续计算CPU使用率
 */
LITE_OS_SEC_TEXT_INIT VOID OsCpupGuard(VOID)
{
    UINT16 prevPos;                                                           // 上一个采样位置
    UINT32 loop;                                                              // 循环变量
    UINT32 intSave;                                                           // 中断保存标志
    UINT64 cycle, cycleIncrement;                                             // 当前周期数和周期增量
    LosProcessCB *processCB = NULL;                                           // 进程控制块指针

    SCHEDULER_LOCK(intSave);                                                  // 锁定调度器，防止任务切换影响统计

    cycle = OsGetCpuCycle();                                                  // 获取当前CPU周期数
    prevPos = cpupHisPos;                                                     // 保存当前采样位置
    cpupHisPos = CPUP_POST_POS(cpupHisPos);                                   // 更新采样位置到下一个
    cpuHistoryTime[prevPos] = cycle;                                          // 记录当前周期数到历史数组

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    // 更新所有活跃中断的历史时间记录
    for (loop = 0; loop < cpupMaxNum; loop++) {
        if (g_irqCpup[loop].status == OS_CPUP_UNUSED) {                       // 跳过未使用的中断统计项
            continue;
        }
        // 记录当前中断的累计时间到历史数组
        g_irqCpup[loop].cpup.historyTime[prevPos] = g_irqCpup[loop].cpup.allTime;
    }
#endif

    // 更新所有进程的历史时间记录
    for (loop = 0; loop < g_processMaxNum; loop++) {
        processCB = OS_PCB_FROM_RPID(loop);                                   // 通过进程ID获取进程控制块
        if (processCB->processCpup == NULL) {                                 // 跳过没有CPU统计信息的进程
            continue;
        }
        // 记录当前进程的累计CPU时间到历史数组
        processCB->processCpup->historyTime[prevPos] = processCB->processCpup->allTime;
    }

    // 更新所有任务的历史时间记录
    for (loop = 0; loop < g_taskMaxNum; loop++) {
        LosTaskCB *taskCB = OS_TCB_FROM_RTID(loop);                           // 通过任务ID获取任务控制块
        // 记录当前任务的累计CPU时间到历史数组
        taskCB->taskCpup.historyTime[prevPos] = taskCB->taskCpup.allTime;
    }

    // 更新各CPU核心当前运行任务的历史时间记录
    for (loop = 0; loop < LOSCFG_KERNEL_CORE_NUM; loop++) {
        LosTaskCB *runTask = runningTasks[loop];                              // 获取核心当前运行任务
        if (runTask == NULL) {                                                // 跳过空闲核心
            continue;
        }

        /* 重新获取周期数以防止翻转 */
        cycle = OsGetCpuCycle();                                              // 再次获取当前周期数（避免长时间循环导致的偏差）
        cycleIncrement = cycle - runTask->taskCpup.startTime;                 // 计算自上次记录以来的周期增量
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
        cycleIncrement -= timeInIrqSwitch[loop];                              // 减去中断占用的周期数
#endif
        // 更新任务的历史CPU时间（累加周期增量）
        runTask->taskCpup.historyTime[prevPos] += cycleIncrement;
        processCB = OS_PCB_FROM_TCB(runTask);                                 // 获取任务所属进程
        if (processCB->processCpup != NULL) {                                 // 如果进程有CPU统计信息
            // 更新进程的历史CPU时间（累加周期增量）
            processCB->processCpup->historyTime[prevPos] += cycleIncrement;
        }
    }

    SCHEDULER_UNLOCK(intSave);                                                // 解锁调度器
}

/**
 * @brief 创建CPU使用率统计定时器
 * @return 成功返回LOS_OK，失败返回错误码
 * @details 创建周期性定时器，定时调用OsCpupGuard函数进行CPU使用率采样
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsCpupGuardCreator(VOID)
{
    // 创建周期定时器：1秒周期，周期模式，回调函数OsCpupGuard
    (VOID)LOS_SwtmrCreate(LOSCFG_BASE_CORE_TICK_PER_SECOND, LOS_SWTMR_MODE_PERIOD,
                          (SWTMR_PROC_FUNC)OsCpupGuard, &cpupSwtmrID, 0);

    (VOID)LOS_SwtmrStart(cpupSwtmrID);                                        // 启动定时器

    return LOS_OK;
}

// 将OsCpupGuardCreator注册为内核模块初始化函数，初始化级别为任务模块级别
LOS_MODULE_INIT(OsCpupGuardCreator, LOS_INIT_LEVEL_KMOD_TASK);

/**
 * @brief CPU使用率统计模块初始化
 * @return 成功返回LOS_OK，失败返回错误码
 * @details 初始化CPU使用率统计相关的数据结构和全局变量
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsCpupInit(VOID)
{
    UINT16 loop;                                                              // 循环变量
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    UINT32 size;                                                              // 内存分配大小

    // 计算最大中断统计项数量：中断总数 × CPU核心数
    cpupMaxNum = OS_HWI_MAX_NUM * LOSCFG_KERNEL_CORE_NUM;

    /* 每个进程只有一个记录，且不会被同时操作 */
    size = cpupMaxNum * sizeof(OsIrqCpupCB);                                  // 计算中断统计控制块所需内存大小
    // 从系统内存池分配中断统计控制块内存
    g_irqCpup = (OsIrqCpupCB *)LOS_MemAlloc(m_aucSysMem0, size);
    if (g_irqCpup == NULL) {                                                  // 内存分配失败
        PRINT_ERR("OsCpupInit error\n");                                     // 打印错误信息
        return LOS_ERRNO_CPUP_NO_MEMORY;                                      // 返回内存不足错误码
    }

    (VOID)memset_s(g_irqCpup, size, 0, size);                                 // 初始化中断统计控制块内存为0
#endif

    // 初始化各CPU核心的当前运行任务指针为NULL
    for (loop = 0; loop < LOSCFG_KERNEL_CORE_NUM; loop++) {
        runningTasks[loop] = NULL;
    }
    cpupInitFlg = 1;                                                          // 设置初始化完成标志
    return LOS_OK;
}
// 模块初始化：将OsCpupInit函数注册为扩展内核模块，初始化级别为LOS_INIT_LEVEL_KMOD_EXTENDED
LOS_MODULE_INIT(OsCpupInit, LOS_INIT_LEVEL_KMOD_EXTENDED);

/**
 * @brief 重置CPU性能统计基础信息
 * @param cpup 指向OsCpupBase结构体的指针，包含CPU性能统计基础数据
 * @param cycle 当前CPU周期值
 * @return 无
 */
STATIC VOID OsResetCpup(OsCpupBase *cpup, UINT64 cycle)
{
    UINT16 loop;  /* 循环计数器 */

    cpup->startTime = cycle;  /* 设置起始时间为当前周期 */
    cpup->allTime = cycle;    /* 设置总时间为当前周期 */
    // 初始化历史时间数组，将所有记录项设置为当前周期
    for (loop = 0; loop < (OS_CPUP_HISTORY_RECORD_NUM + 1); loop++) {
        cpup->historyTime[loop] = cycle;  /* 历史时间数组第loop项设置为当前周期 */
    }
}

/**
 * @brief 重置系统CPU性能统计信息
 * @return 无
 */
LITE_OS_SEC_TEXT_INIT VOID LOS_CpupReset(VOID)
{
    LosProcessCB *processCB = NULL;  /* 进程控制块指针 */
    LosTaskCB *taskCB = NULL;        /* 任务控制块指针 */
    UINT32 index;                    /* 循环索引 */
    UINT64 cycle;                    /* CPU周期值 */
    UINT32 intSave;                  /* 中断状态保存变量 */

    cpupInitFlg = 0;  /* 重置CPUP初始化标志 */
    intSave = LOS_IntLock();  /* 关中断并保存中断状态 */
    (VOID)LOS_SwtmrStop(cpupSwtmrID);  /* 停止CPUP软件定时器 */
    cycle = OsGetCpuCycle();  /* 获取当前CPU周期值 */

    // 初始化CPU历史时间数组
    for (index = 0; index < (OS_CPUP_HISTORY_RECORD_NUM + 1); index++) {
        cpuHistoryTime[index] = cycle;  /* CPU历史时间数组第index项设置为当前周期 */
    }

    // 重置所有进程的CPUP信息
    for (index = 0; index < g_processMaxNum; index++) {
        processCB = OS_PCB_FROM_PID(index);  /* 根据进程ID获取进程控制块 */
        if (processCB->processCpup == NULL) {
            continue;  /* 若进程CPUP信息为空则跳过 */
        }
        OsResetCpup(processCB->processCpup, cycle);  /* 重置进程CPUP信息 */
    }

    // 重置所有任务的CPUP信息
    for (index = 0; index < g_taskMaxNum; index++) {
        taskCB = OS_TCB_FROM_TID(index);  /* 根据任务ID获取任务控制块 */
        OsResetCpup(&taskCB->taskCpup, cycle);  /* 重置任务CPUP信息 */
    }

#ifdef LOSCFG_CPUP_INCLUDE_IRQ  /* 若配置了包含IRQ统计 */
    if (g_irqCpup != NULL) {
        // 重置所有IRQ的CPUP信息
        for (index = 0; index < cpupMaxNum; index++) {
            OsResetCpup(&g_irqCpup[index].cpup, cycle);  /* 重置IRQ的CPUP基础信息 */
            g_irqCpup[index].timeMax = 0;  /* 重置IRQ最大时间 */
        }

        // 重置所有CPU核心的IRQ切换时间
        for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
            timeInIrqSwitch[index] = 0;  /* IRQ切换时间清零 */
        }
    }
#endif

    (VOID)LOS_SwtmrStart(cpupSwtmrID);  /* 启动CPUP软件定时器 */
    LOS_IntRestore(intSave);  /* 恢复中断状态 */
    cpupInitFlg = 1;  /* 设置CPUP初始化完成标志 */

    return;
}

/**
 * @brief 任务切换时更新CPU性能统计信息
 * @param runTask 指向当前运行任务控制块的指针
 * @param newTask 指向新调度任务控制块的指针
 * @return 无
 */
VOID OsCpupCycleEndStart(LosTaskCB *runTask, LosTaskCB *newTask)
{
    /* 不允许调用OsCurrTaskGet和OsCurrProcessGet函数 */
    OsCpupBase *runTaskCpup = &runTask->taskCpup;  /* 当前任务的CPUP基础信息指针 */
    OsCpupBase *newTaskCpup = &newTask->taskCpup;  /* 新任务的CPUP基础信息指针 */
    OsCpupBase *processCpup = OS_PCB_FROM_TCB(runTask)->processCpup;  /* 当前进程的CPUP基础信息指针 */
    UINT64 cpuCycle, cycleIncrement;  /* CPU周期值和周期增量 */
    UINT16 cpuid = ArchCurrCpuid();  /* 当前CPU核心ID */

    if (cpupInitFlg == 0) {
        return;  /* 若CPUP未初始化则直接返回 */
    }

    cpuCycle = OsGetCpuCycle();  /* 获取当前CPU周期值 */
    if (runTaskCpup->startTime != 0) {  /* 若当前任务的起始时间有效 */
        cycleIncrement = cpuCycle - runTaskCpup->startTime;  /* 计算周期增量（任务运行时间） */
#ifdef LOSCFG_CPUP_INCLUDE_IRQ  /* 若配置了包含IRQ统计 */
        cycleIncrement -= timeInIrqSwitch[cpuid];  /* 减去IRQ切换时间 */
        timeInIrqSwitch[cpuid] = 0;  /* IRQ切换时间清零 */
#endif
        runTaskCpup->allTime += cycleIncrement;  /* 更新当前任务总运行时间 */
        if (processCpup != NULL) {
            processCpup->allTime += cycleIncrement;  /* 更新当前进程总运行时间 */
        }
        runTaskCpup->startTime = 0;  /* 重置当前任务起始时间 */
    }

    newTaskCpup->startTime = cpuCycle;  /* 设置新任务起始时间为当前周期 */
    runningTasks[cpuid] = newTask;  /* 更新当前CPU核心运行任务 */
}

/**
 * @brief 获取CPU性能统计历史记录位置
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @param curPosPointer 指向当前位置的指针
 * @param prePosPointer 指向前一位置的指针
 * @return 无
 */
LITE_OS_SEC_TEXT_MINOR STATIC VOID OsCpupGetPos(UINT16 mode, UINT16 *curPosPointer, UINT16 *prePosPointer)
{
    UINT16 curPos;   /* 当前位置 */
    UINT16 tmpPos;   /* 临时位置 */
    UINT16 prePos;   /* 前一位置 */

    tmpPos = cpupHisPos;  /* 获取当前历史记录位置 */
    curPos = CPUP_PRE_POS(tmpPos);  /* 计算当前位置 */

    /* 当前位置与CPUP模式无关，但前一位置因模式而异 */
    switch (mode) {
        case CPUP_LAST_ONE_SECONDS:  /* 最近1秒模式 */
            prePos = CPUP_PRE_POS(curPos);  /* 前一位置为当前位置的前一个 */
            break;
        case CPUP_LAST_TEN_SECONDS:  /* 最近10秒模式 */
            prePos = tmpPos;  /* 前一位置为临时位置 */
            break;
        case CPUP_ALL_TIME:  /* 所有时间模式 */
            /* fall-through */
        default:
            prePos = OS_CPUP_HISTORY_RECORD_NUM;  /* 前一位置为历史记录总数 */
            break;
    }

    *curPosPointer = curPos;  /* 输出当前位置 */
    *prePosPointer = prePos;  /* 输出前一位置 */

    return;
}

/**
 * @brief 内联函数：计算CPU使用率
 * @param cpup 指向OsCpupBase结构体的指针
 * @param pos 当前位置
 * @param prePos 前一位置
 * @param allCycle 总周期数
 * @return CPU使用率（按LOS_CPUP_SINGLE_CORE_PRECISION精度）
 */
STATIC INLINE UINT32 OsCalculateCpupUsage(const OsCpupBase *cpup, UINT16 pos, UINT16 prePos, UINT64 allCycle)
{
    UINT32 usage = 0;  /* CPU使用率 */
    UINT64 cpuCycle = cpup->historyTime[pos] - cpup->historyTime[prePos];  /* 计算使用周期数 */

    if (allCycle) {  /* 若总周期数不为0 */
        // 计算使用率：(使用周期数 * 精度) / 总周期数
        usage = (UINT32)((LOS_CPUP_SINGLE_CORE_PRECISION * cpuCycle) / allCycle);
    }
    return usage;  /* 返回CPU使用率 */
}

/**
 * @brief 非安全模式下获取系统历史CPU使用率
 * @param mode 统计模式
 * @return 系统CPU使用率，失败返回错误码
 */
STATIC UINT32 OsHistorySysCpuUsageUnsafe(UINT16 mode)
{
    UINT64 cpuAllCycle;  /* CPU总周期数 */
    UINT16 pos;          /* 当前位置 */
    UINT16 prePos;       /* 前一位置 */
    OsCpupBase *processCpup = NULL;  /* 进程CPUP基础信息指针 */

    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;  /* CPUP未初始化，返回错误码 */
    }

    OsCpupGetPos(mode, &pos, &prePos);  /* 获取统计位置 */
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];  /* 计算总周期数 */

    // 获取内核空闲进程的CPUP信息
    processCpup = OS_PCB_FROM_PID(OS_KERNEL_IDLE_PROCESS_ID)->processCpup;
    // 系统使用率 = 总精度 - 空闲进程使用率
    return (LOS_CPUP_PRECISION - OsCalculateCpupUsage(processCpup, pos, prePos, cpuAllCycle));
}

/**
 * @brief 获取系统历史CPU使用率
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @return 系统CPU使用率，失败返回错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_HistorySysCpuUsage(UINT16 mode)
{
    UINT32 cpup;       /* CPU使用率 */
    UINT32 intSave;    /* 中断状态保存变量 */

    /* 获取当前进程的结束时间 */
    SCHEDULER_LOCK(intSave);  /* 关调度器 */
    cpup = OsHistorySysCpuUsageUnsafe(mode);  /* 调用非安全模式下的系统CPU使用率计算函数 */
    SCHEDULER_UNLOCK(intSave);  /* 开调度器 */
    return cpup;  /* 返回系统CPU使用率 */
}

/**
 * @brief 非安全模式下获取进程历史CPU使用率
 * @param pid 进程ID
 * @param mode 统计模式
 * @return 进程CPU使用率，失败返回错误码
 */
STATIC UINT32 OsHistoryProcessCpuUsageUnsafe(UINT32 pid, UINT16 mode)
{
    LosProcessCB *processCB = NULL;  /* 进程控制块指针 */
    UINT64 cpuAllCycle;              /* CPU总周期数 */
    UINT16 pos, prePos;              /* 当前位置和前一位置 */

    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;  /* CPUP未初始化，返回错误码 */
    }

    if (OS_PID_CHECK_INVALID(pid)) {
        return LOS_ERRNO_CPUP_ID_INVALID;  /* 进程ID无效，返回错误码 */
    }

    processCB = OS_PCB_FROM_PID(pid);  /* 根据进程ID获取进程控制块 */
    if (OsProcessIsUnused(processCB)) {
        return LOS_ERRNO_CPUP_NO_CREATED;  /* 进程未创建，返回错误码 */
    }

    if (processCB->processCpup == NULL) {
        return LOS_ERRNO_CPUP_ID_INVALID;  /* 进程CPUP信息无效，返回错误码 */
    }

    OsCpupGetPos(mode, &pos, &prePos);  /* 获取统计位置 */
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];  /* 计算总周期数 */

    // 计算并返回进程CPU使用率
    return OsCalculateCpupUsage(processCB->processCpup, pos, prePos, cpuAllCycle);
}

/**
 * @brief 获取进程历史CPU使用率
 * @param pid 进程ID
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @return 进程CPU使用率，失败返回错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_HistoryProcessCpuUsage(UINT32 pid, UINT16 mode)
{
    UINT32 cpup;       /* CPU使用率 */
    UINT32 intSave;    /* 中断状态保存变量 */

    SCHEDULER_LOCK(intSave);  /* 关调度器 */
    cpup = OsHistoryProcessCpuUsageUnsafe(pid, mode);  /* 调用非安全模式下的进程CPU使用率计算函数 */
    SCHEDULER_UNLOCK(intSave);  /* 开调度器 */
    return cpup;  /* 返回进程CPU使用率 */
}

/**
 * @brief 非安全模式下获取任务历史CPU使用率
 * @param tid 任务ID
 * @param mode 统计模式
 * @return 任务CPU使用率，失败返回错误码
 */
STATIC UINT32 OsHistoryTaskCpuUsageUnsafe(UINT32 tid, UINT16 mode)
{
    LosTaskCB *taskCB = NULL;  /* 任务控制块指针 */
    UINT64 cpuAllCycle;        /* CPU总周期数 */
    UINT16 pos, prePos;        /* 当前位置和前一位置 */

    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;  /* CPUP未初始化，返回错误码 */
    }

    if (OS_TID_CHECK_INVALID(tid)) {
        return LOS_ERRNO_CPUP_ID_INVALID;  /* 任务ID无效，返回错误码 */
    }

    taskCB = OS_TCB_FROM_TID(tid);  /* 根据任务ID获取任务控制块 */
    if (OsTaskIsUnused(taskCB)) {
        return LOS_ERRNO_CPUP_NO_CREATED;  /* 任务未创建，返回错误码 */
    }

    OsCpupGetPos(mode, &pos, &prePos);  /* 获取统计位置 */
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];  /* 计算总周期数 */

    // 计算并返回任务CPU使用率
    return OsCalculateCpupUsage(&taskCB->taskCpup, pos, prePos, cpuAllCycle);
}

/**
 * @brief 获取任务历史CPU使用率
 * @param tid 任务ID
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @return 任务CPU使用率，失败返回错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_HistoryTaskCpuUsage(UINT32 tid, UINT16 mode)
{
    UINT32 intSave;  /* 中断状态保存变量 */
    UINT32 cpup;     /* CPU使用率 */

    SCHEDULER_LOCK(intSave);  /* 关调度器 */
    cpup = OsHistoryTaskCpuUsageUnsafe(tid, mode);  /* 调用非安全模式下的任务CPU使用率计算函数 */
    SCHEDULER_UNLOCK(intSave);  /* 开调度器 */
    return cpup;  /* 返回任务CPU使用率 */
}

/**
 * @brief CPU使用率参数检查与重置
 * @param cpupInfo 指向CPUP_INFO_S结构体的指针
 * @param len 缓冲区长度
 * @param number 数据个数
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsCpupUsageParamCheckAndReset(CPUP_INFO_S *cpupInfo, UINT32 len, UINT32 number)
{
    if (cpupInitFlg == 0) {
        return LOS_ERRNO_CPUP_NO_INIT;  /* CPUP未初始化，返回错误码 */
    }

    if ((cpupInfo == NULL) || (len < (sizeof(CPUP_INFO_S) * number))) {
        return LOS_ERRNO_CPUP_PTR_ERR;  /* 参数无效（指针为空或缓冲区不足），返回错误码 */
    }

    (VOID)memset_s(cpupInfo, len, 0, len);  /* 将输出缓冲区清零 */
    return LOS_OK;  /* 成功返回 */
}
/**
 * @brief 非安全模式下获取所有进程CPU使用率
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @param cpupInfo 指向CPUP_INFO_S结构体数组的指针，用于存储进程CPU使用率信息
 * @param len 缓冲区长度
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 GetAllProcessCpuUsageUnsafe(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    LosProcessCB *processCB = NULL;  /* 进程控制块指针 */
    UINT64 cpuAllCycle;              /* CPU总周期数 */
    UINT16 pos, prePos;              /* 当前位置和前一位置 */
    UINT32 processID;                /* 进程ID */
    UINT32 ret;                      /* 返回值 */

    // 参数检查与缓冲区重置
    ret = OsCpupUsageParamCheckAndReset(cpupInfo, len, g_processMaxNum);
    if (ret != LOS_OK) {
        return ret;  /* 参数无效，返回错误码 */
    }

    OsCpupGetPos(mode, &pos, &prePos);  /* 获取统计位置 */
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];  /* 计算总周期数 */

    // 遍历所有进程，计算CPU使用率
    for (processID = 0; processID < g_processMaxNum; processID++) {
        processCB = OS_PCB_FROM_PID(processID);  /* 根据进程ID获取进程控制块 */
        // 跳过未使用或无CPUP信息的进程
        if (OsProcessIsUnused(processCB) || (processCB->processCpup == NULL)) {
            continue;
        }

        // 计算并设置进程CPU使用率
        cpupInfo[processID].usage = OsCalculateCpupUsage(processCB->processCpup, pos, prePos, cpuAllCycle);
        cpupInfo[processID].status = OS_CPUP_USED;  /* 标记进程为已使用状态 */
    }

    return LOS_OK;  /* 成功返回 */
}

/**
 * @brief 获取所有进程CPU使用率
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @param cpupInfo 指向CPUP_INFO_S结构体数组的指针，用于存储进程CPU使用率信息
 * @param len 缓冲区长度
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_GetAllProcessCpuUsage(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    UINT32 intSave;  /* 中断状态保存变量 */
    UINT32 ret;      /* 返回值 */

    SCHEDULER_LOCK(intSave);  /* 关调度器 */
    ret = GetAllProcessCpuUsageUnsafe(mode, cpupInfo, len);  /* 调用非安全模式下的获取所有进程CPU使用率函数 */
    SCHEDULER_UNLOCK(intSave);  /* 开调度器 */
    return ret;  /* 返回结果 */
}

/**
 * @brief 非安全模式下获取进程所有时间粒度的CPU使用率
 * @param processCpup 指向进程CPUP基础信息的指针
 * @param processInfo 指向ProcessInfo结构体的指针，用于存储进程CPU使用率信息
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsGetProcessAllCpuUsageUnsafe(OsCpupBase *processCpup, ProcessInfo *processInfo)
{
    UINT64 cpuAllCycle;  /* CPU总周期数 */
    UINT16 pos, prePos;  /* 当前位置和前一位置 */
    if ((processCpup == NULL) || (processInfo == NULL)) {
        return LOS_ERRNO_CPUP_PTR_ERR;  /* 参数指针为空，返回错误码 */
    }

    // 获取最近1秒CPU使用率
    OsCpupGetPos(CPUP_LAST_ONE_SECONDS, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];
    processInfo->cpup1sUsage = OsCalculateCpupUsage(processCpup, pos, prePos, cpuAllCycle);

    // 获取最近10秒CPU使用率
    OsCpupGetPos(CPUP_LAST_TEN_SECONDS, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];
    processInfo->cpup10sUsage = OsCalculateCpupUsage(processCpup, pos, prePos, cpuAllCycle);

    // 获取所有时间CPU使用率
    OsCpupGetPos(CPUP_ALL_TIME, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];
    processInfo->cpupAllsUsage = OsCalculateCpupUsage(processCpup, pos, prePos, cpuAllCycle);
    return LOS_OK;  /* 成功返回 */
}

/**
 * @brief 非安全模式下获取任务所有时间粒度的CPU使用率
 * @param taskCpup 指向任务CPUP基础信息的指针
 * @param taskInfo 指向TaskInfo结构体的指针，用于存储任务CPU使用率信息
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsGetTaskAllCpuUsageUnsafe(OsCpupBase *taskCpup, TaskInfo *taskInfo)
{
    UINT64 cpuAllCycle;  /* CPU总周期数 */
    UINT16 pos, prePos;  /* 当前位置和前一位置 */
    if ((taskCpup == NULL) || (taskInfo == NULL)) {
        return LOS_ERRNO_CPUP_PTR_ERR;  /* 参数指针为空，返回错误码 */
    }

    // 获取最近1秒CPU使用率
    OsCpupGetPos(CPUP_LAST_ONE_SECONDS, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];
    taskInfo->cpup1sUsage = OsCalculateCpupUsage(taskCpup, pos, prePos, cpuAllCycle);

    // 获取最近10秒CPU使用率
    OsCpupGetPos(CPUP_LAST_TEN_SECONDS, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];
    taskInfo->cpup10sUsage = OsCalculateCpupUsage(taskCpup, pos, prePos, cpuAllCycle);

    // 获取所有时间CPU使用率
    OsCpupGetPos(CPUP_ALL_TIME, &pos, &prePos);
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];
    taskInfo->cpupAllsUsage = OsCalculateCpupUsage(taskCpup, pos, prePos, cpuAllCycle);
    return LOS_OK;  /* 成功返回 */
}

#ifdef LOSCFG_CPUP_INCLUDE_IRQ  /* 若配置了包含IRQ统计 */
/**
 * @brief 记录IRQ开始时的CPU周期
 * @param cpuid CPU核心ID
 * @return 无
 */
LITE_OS_SEC_TEXT_MINOR VOID OsCpupIrqStart(UINT16 cpuid)
{
    UINT32 high;  /* CPU周期高位 */
    UINT32 low;   /* CPU周期低位 */

    LOS_GetCpuCycle(&high, &low);  /* 获取当前CPU周期 */
    // 计算并保存IRQ开始时间（合并高低位）
    cpupIntTimeStart[cpuid] = ((UINT64)high << HIGH_BITS) + low;
    return;
}

/**
 * @brief 计算IRQ结束时的CPU使用率
 * @param cpuid CPU核心ID
 * @param intNum 中断号
 * @return 无
 */
LITE_OS_SEC_TEXT_MINOR VOID OsCpupIrqEnd(UINT16 cpuid, UINT32 intNum)
{
    UINT32 high;       /* CPU周期高位 */
    UINT32 low;        /* CPU周期低位 */
    UINT64 intTimeEnd; /* IRQ结束时间 */
    UINT64 usedTime;   /* IRQ使用时间 */

    LOS_GetCpuCycle(&high, &low);  /* 获取当前CPU周期 */
    intTimeEnd = ((UINT64)high << HIGH_BITS) + low;  /* 计算IRQ结束时间 */
    // 获取IRQ对应的CPUP控制块（中断号*核心数+核心ID）
    OsIrqCpupCB *irqCb = &g_irqCpup[(intNum * LOSCFG_KERNEL_CORE_NUM) + cpuid];
    irqCb->id = intNum;  /* 设置中断号 */
    irqCb->status = OS_CPUP_USED;  /* 标记IRQ为已使用状态 */
    usedTime = intTimeEnd - cpupIntTimeStart[cpuid];  /* 计算IRQ使用时间 */
    timeInIrqSwitch[cpuid] += usedTime;  /* 累加CPU核心的IRQ切换时间 */
    irqCb->cpup.allTime += usedTime;  /* 累加IRQ的总CPU使用时间 */
    if (irqCb->count <= 100) { /* 最多采集100个样本 */
        irqCb->allTime += usedTime;  /* 累加样本总时间 */
        irqCb->count++;  /* 样本计数加1 */
    } else {
        irqCb->allTime = 0;  /* 样本数超过100，重置总时间 */
        irqCb->count = 0;   /* 重置样本计数 */
    }
    if (usedTime > irqCb->timeMax) {
        irqCb->timeMax = usedTime;  /* 更新IRQ最大使用时间 */
    }
    return;
}

/**
 * @brief 获取IRQ的CPUP数组基地址
 * @return 指向OsIrqCpupCB数组的指针
 */
LITE_OS_SEC_TEXT_MINOR OsIrqCpupCB *OsGetIrqCpupArrayBase(VOID)
{
    return g_irqCpup;  /* 返回IRQ的CPUP数组基地址 */
}

/**
 * @brief 非安全模式下获取所有IRQ的CPU使用率
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @param cpupInfo 指向CPUP_INFO_S结构体数组的指针，用于存储IRQ CPU使用率信息
 * @param len 缓冲区长度
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsGetAllIrqCpuUsageUnsafe(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    UINT16 pos, prePos;  /* 当前位置和前一位置 */
    UINT64 cpuAllCycle;  /* CPU总周期数 */
    UINT32 loop;         /* 循环变量 */
    UINT32 ret;          /* 返回值 */

    // 参数检查与缓冲区重置
    ret = OsCpupUsageParamCheckAndReset(cpupInfo, len, cpupMaxNum);
    if (ret != LOS_OK) {
        return ret;  /* 参数无效，返回错误码 */
    }

    OsCpupGetPos(mode, &pos, &prePos);  /* 获取统计位置 */
    cpuAllCycle = cpuHistoryTime[pos] - cpuHistoryTime[prePos];  /* 计算总周期数 */

    // 遍历所有IRQ，计算CPU使用率
    for (loop = 0; loop < cpupMaxNum; loop++) {
        if (g_irqCpup[loop].status == OS_CPUP_UNUSED) {
            continue;  /* 跳过未使用的IRQ */
        }

        // 计算并设置IRQ CPU使用率
        cpupInfo[loop].usage = OsCalculateCpupUsage(&g_irqCpup[loop].cpup, pos, prePos, cpuAllCycle);
        cpupInfo[loop].status = g_irqCpup[loop].status;  /* 设置IRQ状态 */
    }

    return LOS_OK;  /* 成功返回 */
}

/**
 * @brief 获取所有IRQ的CPU使用率
 * @param mode 统计模式（CPUP_LAST_ONE_SECONDS/CPUP_LAST_TEN_SECONDS/CPUP_ALL_TIME）
 * @param cpupInfo 指向CPUP_INFO_S结构体数组的指针，用于存储IRQ CPU使用率信息
 * @param len 缓冲区长度
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_GetAllIrqCpuUsage(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len)
{
    UINT32 intSave;  /* 中断状态保存变量 */
    UINT32 ret;      /* 返回值 */

    SCHEDULER_LOCK(intSave);  /* 关调度器 */
    ret = OsGetAllIrqCpuUsageUnsafe(mode, cpupInfo, len);  /* 调用非安全模式下的获取所有IRQ CPU使用率函数 */
    SCHEDULER_UNLOCK(intSave);  /* 开调度器 */
    return ret;  /* 返回结果 */
}
#endif

#endif /* LOSCFG_KERNEL_CPUP */
