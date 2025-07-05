/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_statistics_pri.h"
#include "los_task_pri.h"
#include "los_process_pri.h"

#ifdef LOSCFG_SCHED_DEBUG
#ifdef LOSCFG_SCHED_TICK_DEBUG
typedef struct {
    UINT64 responseTime;       // 总响应时间(周期数)
    UINT64 responseTimeMax;    // 最大响应时间(周期数)
    UINT64 count;              // 统计次数
} SchedTickDebug;              // 调度 tick 调试统计结构体
STATIC SchedTickDebug *g_schedTickDebug = NULL;

UINT32 OsSchedDebugInit(VOID)
{
    UINT32 size = sizeof(SchedTickDebug) * LOSCFG_KERNEL_CORE_NUM;
    g_schedTickDebug = (SchedTickDebug *)LOS_MemAlloc(m_aucSysMem0, size);
    if (g_schedTickDebug == NULL) {
        return LOS_ERRNO_TSK_NO_MEMORY;
    }

    (VOID)memset_s(g_schedTickDebug, size, 0, size);
    return LOS_OK;
}
// 记录调度响应时间数据
VOID OsSchedDebugRecordData(VOID)                                  
{
    SchedRunqueue *rq = OsSchedRunqueue();                         // 获取当前CPU运行队列
    SchedTickDebug *schedDebug = &g_schedTickDebug[ArchCurrCpuid()];  // 获取当前CPU调试数据
    UINT64 currTime = OsGetCurrSchedTimeCycle();                   // 获取当前调度时间(周期数)
    LOS_ASSERT(currTime >= rq->responseTime);                      // 断言当前时间有效性
    UINT64 usedTime = currTime - rq->responseTime;                 // 计算实际响应时间
    schedDebug->responseTime += usedTime;                          // 累加总响应时间
    if (usedTime > schedDebug->responseTimeMax) {                  // 检查是否为最大响应时间
        schedDebug->responseTimeMax = usedTime;                    // 更新最大响应时间
    }
    schedDebug->count++;                                           // 增加统计次数
}

// 显示调度tick响应时间统计信息，供shell命令调用
UINT32 OsShellShowTickResponse(VOID)
{
    UINT32 intSave;               // 中断状态保存变量
    UINT16 cpu;                   // CPU核心索引

    // 计算所有CPU核心的调试数据内存大小
    UINT32 tickSize = sizeof(SchedTickDebug) * LOSCFG_KERNEL_CORE_NUM;
    // 分配临时内存用于存储调试数据（避免直接操作全局变量时的并发问题）
    SchedTickDebug *schedDebug = (SchedTickDebug *)LOS_MemAlloc(m_aucSysMem1, tickSize);
    if (schedDebug == NULL) {     // 内存分配失败检查
        return LOS_NOK;           // 返回操作失败
    }

    SCHEDULER_LOCK(intSave);      // 关闭调度器，防止数据竞争
    // 拷贝全局调试数据到临时缓冲区（原子操作）
    (VOID)memcpy_s((CHAR *)schedDebug, tickSize, (CHAR *)g_schedTickDebug, tickSize);
    SCHEDULER_UNLOCK(intSave);    // 恢复调度器

    // 打印统计信息表头（ATR=Average Tick Response）
    PRINTK("cpu   ATRTime(us) ATRTimeMax(us)  TickCount\n");
    // 遍历每个CPU核心的统计数据
    for (cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        SchedTickDebug *schedData = &schedDebug[cpu];  // 当前CPU的调试数据
        UINT64 averTime = 0;                           // 平均响应时间(微秒)
        if (schedData->count > 0) {                    // 避免除零错误
            averTime = schedData->responseTime / schedData->count;  // 计算平均周期数
            // 将周期数转换为微秒（OS_NS_PER_CYCLE=每周期纳秒数，OS_SYS_NS_PER_US=每微秒纳秒数）
            averTime = (averTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        }
        // 将最大响应时间从周期数转换为微秒
        UINT64 timeMax = (schedData->responseTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        // 打印CPU核心编号、平均响应时间、最大响应时间和统计次数
        PRINTK("%3u%14llu%15llu%11llu\n", cpu, averTime, timeMax, schedData->count);
    }

    (VOID)LOS_MemFree(m_aucSysMem1, schedDebug);  // 释放临时内存
    return LOS_OK;                               // 返回操作成功
}
#endif

#ifdef LOSCFG_SCHED_HPF_DEBUG
STATIC VOID SchedDataGet(const LosTaskCB *taskCB, UINT64 *runTime, UINT64 *timeSlice,
                         UINT64 *pendTime, UINT64 *schedWait)
{
    if (taskCB->schedStat.switchCount >= 1) {                     // 任务切换次数>=1时才计算平均运行时间，避免除零
        UINT64 averRunTime = taskCB->schedStat.runTime / taskCB->schedStat.switchCount;  // 总运行周期数/切换次数=平均运行周期
        *runTime = (averRunTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;  // 周期数→纳秒→微秒转换（1微秒=1000纳秒）
    }

    if (taskCB->schedStat.timeSliceCount > 1) {                   // 时间片计数>1时计算（首次分配不计入统计）
        UINT64 averTimeSlice = taskCB->schedStat.timeSliceTime / (taskCB->schedStat.timeSliceCount - 1);  // 总时间片周期/(计数-1)=平均时间片周期
        *timeSlice = (averTimeSlice * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;  // 转换为微秒单位
    }

    if (taskCB->schedStat.pendCount > 1) {                        // 挂起次数>1时计算（首次挂起不计入统计）
        UINT64 averPendTime = taskCB->schedStat.pendTime / taskCB->schedStat.pendCount;  // 总挂起周期/挂起次数=平均挂起周期
        *pendTime = (averPendTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;  // 转换为微秒单位
    }

    if (taskCB->schedStat.waitSchedCount > 0) {                   // 调度等待次数>0时计算
        UINT64 averSchedWait = taskCB->schedStat.waitSchedTime / taskCB->schedStat.waitSchedCount;  // 总等待周期/等待次数=平均等待周期
        *schedWait = (averSchedWait * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;  // 转换为微秒单位
    }
}

/**
 * @brief 展示系统调度统计信息
 * @return UINT32 操作结果，LOS_OK表示成功
 * @details 该函数会遍历所有任务控制块，收集并计算任务调度统计数据，最终以表格形式打印输出
 */
UINT32 OsShellShowSchedStatistics(VOID)
{
    UINT32 taskLinkNum[LOSCFG_KERNEL_CORE_NUM];  // 每个CPU核心的任务超时队列节点数量
    UINT32 intSave;                              // 中断状态保存变量
    LosTaskCB task;                              // 临时任务控制块副本（用于安全读取）
    SchedEDF *sched = NULL;                      // 调度策略指针（用于判断EDF任务）

    // 1. 加调度器锁，防止统计过程中任务状态变化导致数据不一致
    SCHEDULER_LOCK(intSave);
    // 遍历所有CPU核心，获取每个核心的超时队列长度
    for (UINT16 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        SchedRunqueue *rq = OsSchedRunqueueByID(cpu);  // 获取指定CPU的运行队列
        taskLinkNum[cpu] = OsGetSortLinkNodeNum(&rq->timeoutQueue);  // 获取超时队列节点数量
    }
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁

    // 2. 打印每个CPU核心的任务超时队列统计信息
    for (UINT16 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        PRINTK("cpu: %u Task SortMax: %u\n", cpu, taskLinkNum[cpu]);  // 输出CPU ID和对应超时队列长度
    }

    // 3. 打印任务统计表格表头
    PRINTK("  Tid    AverRunTime(us)    SwitchCount  AverTimeSlice(us)    TimeSliceCount  AverReadyWait(us)  "
           "AverPendTime(us)  TaskName \n");

    // 4. 遍历所有任务控制块，收集并打印任务统计数据
    for (UINT32 tid = 0; tid < g_taskMaxNum; tid++) {
        LosTaskCB *taskCB = g_taskCBArray + tid;  // 获取当前TID对应的任务控制块

        // 加锁保护任务控制块读取
        SCHEDULER_LOCK(intSave);
        // 过滤条件1：跳过未使用的任务控制块或空闲进程任务
        if (OsTaskIsUnused(taskCB) || (taskCB->processCB == (UINTPTR)OsGetIdleProcess())) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        // 过滤条件2：跳过EDF调度策略的任务（EDF任务有单独的统计机制）
        sched = (SchedEDF *)&taskCB->sp;          // 获取任务调度策略信息
        if (sched->policy == LOS_SCHED_DEADLINE) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        // 安全复制任务控制块数据（避免解锁后数据被修改）
        (VOID)memcpy_s(&task, sizeof(LosTaskCB), taskCB, sizeof(LosTaskCB));
        SCHEDULER_UNLOCK(intSave);  // 完成数据复制后解锁

        // 5. 定义统计数据变量
        UINT64 averRunTime = 0;       // 平均运行时间(微秒)
        UINT64 averTimeSlice = 0;     // 平均时间片(微秒)
        UINT64 averPendTime = 0;      // 平均挂起时间(微秒)
        UINT64 averSchedWait = 0;     // 平均调度等待时间(微秒)

        // 调用SchedDataGet计算各项平均统计指标
        SchedDataGet(&task, &averRunTime, &averTimeSlice, &averPendTime, &averSchedWait);

        // 6. 格式化输出任务统计数据
        PRINTK("%5u%19llu%15llu%19llu%18llu%19llu%18llu  %-32s\n", 
               taskCB->taskID,                // 任务ID
               averRunTime,                   // 平均运行时间
               taskCB->schedStat.switchCount, // 任务切换次数
               averTimeSlice,                 // 平均时间片
               taskCB->schedStat.timeSliceCount - 1, // 有效时间片计数(减1排除初始分配)
               averSchedWait,                 // 平均就绪等待时间
               averPendTime,                  // 平均挂起时间
               taskCB->taskName);             // 任务名称
    }

    return LOS_OK;  // 返回成功
}
#endif

#ifdef LOSCFG_SCHED_EDF_DEBUG
#define EDF_DEBUG_NODE 20
/**
 * @brief EDF调度策略任务的调试信息结构体
 * @details 用于记录EDF任务的调度参数、运行时间和截止时间等关键指标，支持实时系统调试与性能分析
 */
typedef struct {
    UINT32 tid;                  // 任务ID，唯一标识一个EDF任务
    INT32 runTimeUs;             // 任务实际运行时间（微秒），可能为负数表示超时
    UINT64 deadlineUs;           // 任务截止时间（微秒），相对于系统启动时刻
    UINT64 periodUs;             // 任务周期（微秒），周期性任务的时间间隔
    UINT64 startTime;            // 任务开始执行时间戳（系统时钟周期数）
    UINT64 finishTime;           // 任务完成执行时间戳（系统时钟周期数）
    UINT64 nextfinishTime;       // 下一次任务预计完成时间戳（系统时钟周期数）
    UINT64 timeSliceUnused;      // 未使用的时间片（系统时钟周期数），反映CPU资源利用率
    UINT64 timeSliceRealTime;    // 实际分配的时间片（系统时钟周期数）
    UINT64 allRuntime;           // 任务总运行时间（系统时钟周期数），累计值
    UINT64 pendTime;             // 任务挂起时间（系统时钟周期数），累计等待资源的时间
} EDFDebug;

STATIC EDFDebug g_edfNode[EDF_DEBUG_NODE];
STATIC INT32 g_edfNodePointer = 0;

/**
 * @brief 记录EDF调度任务的调试信息
 * @param[in] task 任务控制块指针（UINTPTR*类型用于兼容不同架构的指针宽度）
 * @param[in] oldFinish 任务上一次完成时间戳（系统时钟周期数）
 * @details 该函数在EDF任务调度点被调用，将任务关键调度参数记录到全局调试缓冲区，用于后续实时性分析
 */
VOID EDFDebugRecord(UINTPTR *task, UINT64 oldFinish)
{
    LosTaskCB *taskCB = (LosTaskCB *)task;       // 转换为任务控制块指针
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 获取EDF调度专用参数
    SchedParam param;                           // 通用调度参数结构体

    // 调试记录停止条件：当指针指向缓冲区末尾+1时停止记录（打印调试信息时触发）
    if (g_edfNodePointer == (EDF_DEBUG_NODE + 1)) {
        return;
    }

    // 1. 获取任务调度参数（包含运行时间、截止时间等EDF关键信息）
    taskCB->ops->schedParamGet(taskCB, &param);

    // 2. 填充EDF调试信息节点（g_edfNode为全局循环缓冲区）
    g_edfNode[g_edfNodePointer].tid = taskCB->taskID;                  // 任务ID
    g_edfNode[g_edfNodePointer].runTimeUs = param.runTimeUs;           // 任务运行时间（微秒）
    g_edfNode[g_edfNodePointer].deadlineUs = param.deadlineUs;         // 任务截止时间（微秒）
    g_edfNode[g_edfNodePointer].periodUs = param.periodUs;             // 任务周期（微秒）
    g_edfNode[g_edfNodePointer].startTime = taskCB->startTime;         // 任务开始执行时间戳（周期数）

    // 3. 时间片使用情况处理
    if (taskCB->timeSlice <= 0) {                                       // 时间片已用尽
        taskCB->irqUsedTime = 0;                                        // 清零中断占用时间
        g_edfNode[g_edfNodePointer].timeSliceUnused = 0;                // 未使用时间片为0
    } else {
        g_edfNode[g_edfNodePointer].timeSliceUnused = taskCB->timeSlice;// 记录剩余时间片（周期数）
    }

    // 4. 任务时间戳信息
    g_edfNode[g_edfNodePointer].finishTime = oldFinish;                 // 上一次完成时间戳
    g_edfNode[g_edfNodePointer].nextfinishTime = sched->finishTime;     // 下一次预计完成时间戳

    // 5. 累计统计信息
    g_edfNode[g_edfNodePointer].timeSliceRealTime = taskCB->schedStat.timeSliceRealTime; // 实际分配时间片
    g_edfNode[g_edfNodePointer].allRuntime = taskCB->schedStat.allRuntime;               // 总运行时间
    g_edfNode[g_edfNodePointer].pendTime = taskCB->schedStat.pendTime;                   // 总挂起时间

    // 6. 循环缓冲区指针更新（达到最大节点数时回绕）
    g_edfNodePointer++;
    if (g_edfNodePointer == EDF_DEBUG_NODE) {
        g_edfNodePointer = 0;  // 缓冲区满，从头部开始覆盖旧数据
    }
}

STATIC VOID EDFInfoPrint(int idx)
{
    INT32 runTimeUs;
    UINT64 deadlineUs;
    UINT64 periodUs;
    UINT64 startTime;
    UINT64 timeSlice;
    UINT64 finishTime;
    UINT64 nextfinishTime;
    UINT64 pendTime;
    UINT64 allRuntime;
    UINT64 timeSliceRealTime;
    CHAR *status = NULL;

    startTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].startTime);
    timeSlice = OS_SYS_CYCLE_TO_US(g_edfNode[idx].timeSliceUnused);
    finishTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].finishTime);
    nextfinishTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].nextfinishTime);
    pendTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].pendTime);
    allRuntime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].allRuntime);
    timeSliceRealTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].timeSliceRealTime);
    runTimeUs = g_edfNode[idx].runTimeUs;
    deadlineUs = g_edfNode[idx].deadlineUs;
    periodUs = g_edfNode[idx].periodUs;

    if (timeSlice > 0) {
        status = "TIMEOUT";
    } else if (nextfinishTime == finishTime) {
        status = "NEXT PERIOD";
    } else {
        status = "WAIT RUN";
    }

    PRINTK("%4u%9d%9llu%9llu%12llu%12llu%12llu%9llu%9llu%9llu%9llu %-12s\n",
           g_edfNode[idx].tid, runTimeUs, deadlineUs, periodUs,
           startTime, finishTime, nextfinishTime, allRuntime, timeSliceRealTime,
           timeSlice, pendTime, status);
}

VOID OsEDFDebugPrint(VOID)
{
    INT32 max;
    UINT32 intSave;
    INT32 i;

    SCHEDULER_LOCK(intSave);
    max = g_edfNodePointer;
    g_edfNodePointer = EDF_DEBUG_NODE + 1;
    SCHEDULER_UNLOCK(intSave);

    PRINTK("\r\nlast %d sched is: (in microsecond)\r\n", EDF_DEBUG_NODE);

    PRINTK(" TID  RunTime Deadline   Period   StartTime   "
           "CurPeriod  NextPeriod   AllRun  RealRun  TimeOut WaitTime Status\n");

    for (i = max; i < EDF_DEBUG_NODE; i++) {
        EDFInfoPrint(i);
    }

    for (i = 0; i < max; i++) {
        EDFInfoPrint(i);
    }

    SCHEDULER_LOCK(intSave);
    g_edfNodePointer = max;
    SCHEDULER_UNLOCK(intSave);
}

/**
 * @brief 展示EDF调度策略任务的实时统计信息
 * @return UINT32 操作结果，LOS_OK表示成功
 * @details 该函数会筛选出系统中所有采用EDF调度策略的任务，打印其ID、当前时间、截止时间、完成时间等关键参数，并调用OsEDFDebugPrint输出历史调试记录
 */
UINT32 OsShellShowEdfSchedStatistics(VOID)
{
    UINT32 intSave;                // 中断状态保存变量
    LosTaskCB task;                // 临时任务控制块副本（用于安全读取）
    UINT64 curTime;                // 当前系统时间（微秒）
    UINT64 deadline;               // 任务截止时间（微秒）
    UINT64 finishTime;             // 任务预计完成时间（微秒）
    SchedEDF *sched = NULL;        // EDF调度参数指针

    // 1. 打印EDF任务统计表格表头
    PRINTK("Now Alive EDF Thread:\n");
    PRINTK("TID        CurTime       DeadTime     FinishTime  taskName\n");

    // 2. 遍历所有任务控制块，筛选EDF任务
    for (UINT32 tid = 0; tid < g_taskMaxNum; tid++) {
        LosTaskCB *taskCB = g_taskCBArray + tid;  // 获取当前TID对应的任务控制块

        // 加调度器锁，防止任务状态在读取过程中发生变化
        SCHEDULER_LOCK(intSave);
        // 过滤条件1：跳过未使用的任务控制块
        if (OsTaskIsUnused(taskCB)) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        // 获取任务调度策略信息
        sched = (SchedEDF *)&taskCB->sp;
        // 过滤条件2：仅保留EDF调度策略任务（LOS_SCHED_DEADLINE）
        if (sched->policy != LOS_SCHED_DEADLINE) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        // 安全复制任务控制块数据（避免解锁后原数据被修改）
        (VOID)memcpy_s(&task, sizeof(LosTaskCB), taskCB, sizeof(LosTaskCB));

        // 3. 计算并转换时间戳（系统时钟周期数→微秒）
        curTime = OS_SYS_CYCLE_TO_US(HalClockGetCycles());  // 当前时间 = 硬件时钟周期数转换为微秒
        finishTime = OS_SYS_CYCLE_TO_US(sched->finishTime); // 预计完成时间 = EDF调度参数中的完成时间戳转换为微秒
        deadline = OS_SYS_CYCLE_TO_US(taskCB->ops->deadlineGet(taskCB)); // 截止时间 = 通过EDF接口获取并转换为微秒
        SCHEDULER_UNLOCK(intSave);  // 完成数据读取后释放调度器锁

        // 4. 格式化输出EDF任务关键信息
        PRINTK("%3u%15llu%15llu%15llu  %-32s\n",
               task.taskID,          // 任务ID
               curTime,              // 当前系统时间（微秒）
               deadline,             // 任务截止时间（微秒）
               finishTime,           // 任务预计完成时间（微秒）
               task.taskName);       // 任务名称
    }

    // 5. 调用EDF调试记录打印函数，输出历史统计数据
    OsEDFDebugPrint();

    return LOS_OK;  // 返回成功
}
#endif
#endif
