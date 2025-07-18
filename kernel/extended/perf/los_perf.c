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

#include "los_perf_pri.h"
#include "perf_pmu_pri.h"
#include "perf_output_pri.h"
#include "los_init.h"
#include "los_process.h"
#include "los_tick.h"
#include "los_sys.h"
#include "los_spinlock.h"

/*
 * 性能监控模块核心实现文件
 * 包含PMU(Performance Monitoring Unit)初始化、配置、事件采集、数据处理等功能
 */

// 全局变量定义
STATIC Pmu *g_pmu = NULL;                                  // PMU实例指针，指向当前激活的性能监控单元
STATIC PerfCB g_perfCb = {0};                              // 性能监控控制块，维护全局监控状态

LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_perfSpin);                // 性能监控模块自旋锁，保护多线程访问共享资源
#define PERF_LOCK(state)       LOS_SpinLockSave(&g_perfSpin, &(state))  // 获取性能监控自旋锁并保存中断状态
#define PERF_UNLOCK(state)     LOS_SpinUnlockRestore(&g_perfSpin, (state))  // 恢复中断状态并释放性能监控自旋锁

#define MIN(x, y)             ((x) < (y) ? (x) : (y))      // 取两个数中的较小值

/*
 * @brief 获取当前时间戳
 * @return UINT64 当前时间值
 *         - 若LOSCFG_PERF_CALC_TIME_BY_TICK定义，则返回系统滴答数
 *         - 否则返回硬件时钟周期数
 */
STATIC INLINE UINT64 OsPerfGetCurrTime(VOID)
{
#ifdef LOSCFG_PERF_CALC_TIME_BY_TICK
    return LOS_TickCountGet();                             // 使用系统滴答计数作为时间源
#else
    return HalClockGetCycles();                            // 使用硬件时钟周期作为时间源
#endif
}

/*
 * @brief 性能监控单元(PMU)初始化入口
 * @return UINT32 初始化结果
 *         - LOS_OK: 初始化成功
 *         - LOS_ERRNO_PERF_HW_INIT_ERROR: 硬件PMU初始化失败
 *         - LOS_ERRNO_PERF_TIMED_INIT_ERROR: 定时PMU初始化失败
 *         - LOS_ERRNO_PERF_SW_INIT_ERROR: 软件PMU初始化失败
 */
STATIC UINT32 OsPmuInit(VOID)
{
#ifdef LOSCFG_PERF_HW_PMU
    if (OsHwPmuInit() != LOS_OK) {                         // 初始化硬件PMU
        return LOS_ERRNO_PERF_HW_INIT_ERROR;               // 硬件PMU初始化失败
    }
#endif

#ifdef LOSCFG_PERF_TIMED_PMU
    if (OsTimedPmuInit() != LOS_OK) {                      // 初始化定时PMU
        return LOS_ERRNO_PERF_TIMED_INIT_ERROR;            // 定时PMU初始化失败
    }
#endif

#ifdef LOSCFG_PERF_SW_PMU
    if (OsSwPmuInit() != LOS_OK) {                         // 初始化软件PMU
        return LOS_ERRNO_PERF_SW_INIT_ERROR;               // 软件PMU初始化失败
    }
#endif
    return LOS_OK;                                         // 所有使能的PMU组件初始化成功
}

/*
 * @brief 配置性能事件参数
 * @param[in] eventsCfg 性能事件配置结构体指针，包含事件类型、数量和参数
 * @return UINT32 配置结果
 *         - LOS_OK: 配置成功
 *         - LOS_ERRNO_PERF_INVALID_PMU: PMU类型无效
 *         - LOS_ERRNO_PERF_PMU_CONFIG_ERROR: PMU配置失败
 */
STATIC UINT32 OsPerfConfig(PerfEventConfig *eventsCfg)
{
    UINT32 i;                                              // 循环索引变量
    UINT32 ret;                                            // 函数返回值

    g_pmu = OsPerfPmuGet(eventsCfg->type);                 // 根据事件类型获取对应的PMU实例
    if (g_pmu == NULL) {                                   // 检查PMU实例是否有效
        PRINT_ERR("perf config type error %u!\n", eventsCfg->type);
        return LOS_ERRNO_PERF_INVALID_PMU;                 // 返回无效PMU类型错误
    }

    UINT32 eventNum = MIN(eventsCfg->eventsNr, PERF_MAX_EVENT);  // 计算实际可配置事件数量(不超过最大支持数)

    (VOID)memset_s(&g_pmu->events, sizeof(PerfEvent), 0, sizeof(PerfEvent));  // 清零PMU事件配置结构体

    for (i = 0; i < eventNum; i++) {                       // 遍历配置每个性能事件
        g_pmu->events.per[i].eventId = eventsCfg->events[i].eventId;  // 设置事件ID
        g_pmu->events.per[i].period = eventsCfg->events[i].period;    // 设置事件采样周期
    }
    g_pmu->events.nr = i;                                  // 记录有效事件数量
    g_pmu->events.cntDivided = eventsCfg->predivided;      // 设置计数器分频标志
    g_pmu->type = eventsCfg->type;                         // 记录PMU类型

    ret = g_pmu->config();                                 // 调用PMU特定的配置函数
    if (ret != LOS_OK) {                                   // 检查配置结果
        PRINT_ERR("perf config failed!\n");
        (VOID)memset_s(&g_pmu->events, sizeof(PerfEvent), 0, sizeof(PerfEvent));  // 配置失败时清除事件配置
        return LOS_ERRNO_PERF_PMU_CONFIG_ERROR;            // 返回PMU配置失败错误
    }
    return LOS_OK;                                         // 配置成功
}

/*
 * @brief 打印性能事件计数结果
 * @details 遍历所有已配置的性能事件，打印事件名称、类型、CPU核心号和计数值
 */
STATIC VOID OsPerfPrintCount(VOID)
{
    UINT32 index;                                          // 事件索引
    UINT32 intSave;                                        // 中断状态保存变量
    UINT32 cpuid = ArchCurrCpuid();                        // 获取当前CPU核心ID

    PerfEvent *events = &g_pmu->events;                    // 获取PMU事件配置指针
    UINT32 eventNum = events->nr;                          // 获取事件数量

    PERF_LOCK(intSave);                                    // 加锁保护共享资源访问
    for (index = 0; index < eventNum; index++) {           // 遍历所有事件
        Event *event = &(events->per[index]);              // 获取当前事件指针

        /* filter out event counter with no event binded. */
        if (event->period == 0) {                          // 跳过未绑定周期的事件
            continue;
        }
        PRINT_EMG("[%s] eventType: 0x%x [core %u]: %llu\n", g_pmu->getName(event), event->eventId, cpuid,
            event->count[cpuid]);  // 打印事件信息和计数值
    }
    PERF_UNLOCK(intSave);                                  // 解锁
}

/*
 * @brief 打印性能监控耗时信息
 * @details 根据配置计算并打印性能监控的总耗时，单位为秒
 */
STATIC INLINE VOID OsPerfPrintTs(VOID)
{
#ifdef LOSCFG_PERF_CALC_TIME_BY_TICK
    DOUBLE time = (g_perfCb.endTime - g_perfCb.startTime) * 1.0 / LOSCFG_BASE_CORE_TICK_PER_SECOND;  // 计算耗时(滴答数转秒)
#else
    DOUBLE time = (g_perfCb.endTime - g_perfCb.startTime) * 1.0 / OS_SYS_CLOCK;  // 计算耗时(时钟周期转秒)
#endif
    PRINT_EMG("time used: %.6f(s)\n", time);              // 打印耗时，保留6位小数
}

/*
 * @brief 启动当前CPU核心的PMU性能监控
 * @details 检查PMU状态并调用PMU启动函数，更新核心监控状态
 */
STATIC VOID OsPerfStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();                        // 获取当前CPU核心ID

    if (g_pmu == NULL) {                                   // 检查PMU实例是否有效
        PRINT_ERR("pmu not registered!\n");
        return;
    }

    if (g_perfCb.pmuStatusPerCpu[cpuid] != PERF_PMU_STARTED) {  // 检查PMU状态是否未启动
        UINT32 ret = g_pmu->start();                       // 调用PMU启动函数
        if (ret != LOS_OK) {                               // 检查启动结果
            PRINT_ERR("perf start on core:%u failed, ret = 0x%x\n", cpuid, ret);
            return;
        }

        g_perfCb.pmuStatusPerCpu[cpuid] = PERF_PMU_STARTED;  // 更新PMU状态为已启动
    } else {
        PRINT_ERR("percpu status err %d\n", g_perfCb.pmuStatusPerCpu[cpuid]);  // 打印状态错误信息
    }
}

/*
 * @brief 停止当前CPU核心的PMU性能监控
 * @details 检查PMU状态并调用PMU停止函数，更新核心监控状态，必要时打印计数结果
 */
STATIC VOID OsPerfStop(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();                        // 获取当前CPU核心ID

    if (g_pmu == NULL) {                                   // 检查PMU实例是否有效
        PRINT_ERR("pmu not registered!\n");
        return;
    }

    if (g_perfCb.pmuStatusPerCpu[cpuid] != PERF_PMU_STOPED) {  // 检查PMU状态是否未停止
        UINT32 ret = g_pmu->stop();                        // 调用PMU停止函数
        if (ret != LOS_OK) {                               // 检查停止结果
            PRINT_ERR("perf stop on core:%u failed, ret = 0x%x\n", cpuid, ret);
            return;
        }

        if (!g_perfCb.needSample) {                        // 如果不需要采样，直接打印计数结果
            OsPerfPrintCount();
        }

        g_perfCb.pmuStatusPerCpu[cpuid] = PERF_PMU_STOPED;   // 更新PMU状态为已停止
    } else {
        PRINT_ERR("percpu status err %d\n", g_perfCb.pmuStatusPerCpu[cpuid]);  // 打印状态错误信息
    }
}

/*
 * @brief 保存指令指针(IP)信息到缓冲区
 * @param[in] buf 目标缓冲区指针
 * @param[in] info IP信息结构体指针，包含指令地址和路径信息
 * @return UINT32 保存的字节数
 */
STATIC INLINE UINT32 OsPerfSaveIpInfo(CHAR *buf, IpInfo *info)
{
    UINT32 size = 0;                                       // 已保存字节数计数器
#ifdef LOSCFG_KERNEL_VM
    UINT32 len = ALIGN(info->len, sizeof(size_t));         // 按size_t对齐路径长度

    *(UINTPTR *)buf = info->ip; /* save ip */              // 保存指令指针地址
    size += sizeof(UINTPTR);

    *(UINT32 *)(buf + size) = len; /* save f_path length */  // 保存文件路径长度
    size += sizeof(UINT32);

    if (strncpy_s(buf + size, REGION_PATH_MAX, info->f_path, info->len) != EOK) { /* save f_path */  // 保存文件路径
        PRINT_ERR("copy f_path failed, %s\n", info->f_path);
    }
    size += len;
#else
    *(UINTPTR *)buf = info->ip; /* save ip */              // 保存指令指针地址
    size += sizeof(UINTPTR);
#endif
    return size;                                           // 返回总保存字节数
}

/*
 * @brief 获取函数调用栈回溯信息
 * @param[out] callChain 调用栈信息结构体指针，用于存储回溯结果
 * @param[in] maxDepth 最大回溯深度
 * @param[in] regs 寄存器上下文指针，包含栈帧信息
 * @return UINT32 实际回溯深度
 */
STATIC UINT32 OsPerfBackTrace(PerfBackTrace *callChain, UINT32 maxDepth, PerfRegs *regs)
{
    UINT32 count = BackTraceGet(regs->fp, (IpInfo *)(callChain->ip), maxDepth);  // 获取调用栈信息
    PRINT_DEBUG("backtrace depth = %u, fp = 0x%x\n", count, regs->fp);  // 打印调试信息
    return count;                                          // 返回实际回溯深度
}

/*
 * @brief 将调用栈回溯信息保存到缓冲区
 * @param[in] buf 目标缓冲区指针
 * @param[in] callChain 调用栈信息结构体指针
 * @param[in] count 实际回溯深度
 * @return UINT32 保存的总字节数
 */
STATIC INLINE UINT32 OsPerfSaveBackTrace(CHAR *buf, PerfBackTrace *callChain, UINT32 count)
{
    UINT32 i;                                              // 循环索引
    *(UINT32 *)buf = count;                                // 保存回溯深度
    UINT32 size = sizeof(UINT32);                          // 已保存字节数，初始为深度字段大小
    for (i = 0; i < count; i++) {                           // 遍历每个回溯帧
        size += OsPerfSaveIpInfo(buf + size, &(callChain->ip[i]));  // 保存当前帧IP信息
    }
    return size;                                           // 返回总保存字节数
}

/*
 * @brief 收集性能事件采样数据
 * @param[in] event 触发采样的事件指针
 * @param[out] data 采样数据缓冲区指针
 * @param[in] regs 寄存器上下文指针
 * @return UINT32 收集的采样数据大小(字节)
 */
STATIC UINT32 OsPerfCollectData(Event *event, PerfSampleData *data, PerfRegs *regs)
{
    UINT32 size = 0;                                       // 已收集数据大小
    UINT32 depth;                                          // 调用栈回溯深度
    IpInfo pc = {0};                                       // 指令指针信息结构体
    PerfBackTrace callChain = {0};                         // 调用栈信息结构体
    UINT32 sampleType = g_perfCb.sampleType;               // 采样类型(位掩码)
    CHAR *p = (CHAR *)data;                                // 数据缓冲区指针

    if (sampleType & PERF_RECORD_CPU) {                     // 如果需要记录CPU ID
        *(UINT32 *)(p + size) = ArchCurrCpuid();           // 保存当前CPU核心ID
        size += sizeof(data->cpuid);
    }

    if (sampleType & PERF_RECORD_TID) {                     // 如果需要记录任务ID
        *(UINT32 *)(p + size) = LOS_CurTaskIDGet();        // 保存当前任务ID
        size += sizeof(data->taskId);
    }

    if (sampleType & PERF_RECORD_PID) {                     // 如果需要记录进程ID
        *(UINT32 *)(p + size) = LOS_GetCurrProcessID();    // 保存当前进程ID
        size += sizeof(data->processId);
    }

    if (sampleType & PERF_RECORD_TYPE) {                    // 如果需要记录事件类型
        *(UINT32 *)(p + size) = event->eventId;            // 保存事件ID
        size += sizeof(data->eventId);
    }

    if (sampleType & PERF_RECORD_PERIOD) {                  // 如果需要记录采样周期
        *(UINT32 *)(p + size) = event->period;             // 保存事件周期
        size += sizeof(data->period);
    }

    if (sampleType & PERF_RECORD_TIMESTAMP) {               // 如果需要记录时间戳
        *(UINT64 *)(p + size) = OsPerfGetCurrTime();       // 保存当前时间戳
        size += sizeof(data->time);
    }

    if (sampleType & PERF_RECORD_IP) {                      // 如果需要记录指令指针
        OsGetUsrIpInfo(regs->pc, &pc);                     // 获取用户空间指令指针信息
        size += OsPerfSaveIpInfo(p + size, &pc);           // 保存指令指针信息
    }

    if (sampleType & PERF_RECORD_CALLCHAIN) {               // 如果需要记录调用栈
        depth = OsPerfBackTrace(&callChain, PERF_MAX_CALLCHAIN_DEPTH, regs);  // 获取调用栈
        size += OsPerfSaveBackTrace(p + size, &callChain, depth);  // 保存调用栈信息
    }

    return size;                                           // 返回总数据大小
}

/*
 * @brief 任务/进程ID过滤检查
 * @param[in] id 要检查的任务/进程ID
 * @param[in] ids ID过滤列表
 * @param[in] idsNr 过滤列表中ID的数量
 * @return BOOL 过滤结果
 *         - TRUE: ID在过滤列表中或列表为空(表示监控整个系统)
 *         - FALSE: ID不在过滤列表中
 */
STATIC INLINE BOOL OsFilterId(UINT32 id, const UINT32 *ids, UINT8 idsNr)
{
    UINT32 i;                                              // 循环索引
    if (!idsNr) {                                           // 如果过滤列表为空
        return TRUE;                                       // 返回TRUE，表示不过滤
    }

    for (i = 0; i < idsNr; i++) {                           // 遍历过滤列表
        if (ids[i] == id) {                                // 如果ID匹配
            return TRUE;                                   // 返回TRUE，表示通过过滤
        }
    }
    return FALSE;                                          // 返回FALSE，表示未通过过滤
}

/*
 * @brief 性能监控任务/进程过滤检查
 * @param[in] taskId 任务ID
 * @param[in] processId 进程ID
 * @return BOOL 过滤结果
 *         - TRUE: 任务和进程ID均通过过滤
 *         - FALSE: 任一ID未通过过滤
 */
STATIC INLINE BOOL OsPerfFilter(UINT32 taskId, UINT32 processId)
{
    return OsFilterId(taskId, g_perfCb.taskIds, g_perfCb.taskIdsNr) &&  // 任务ID过滤
            OsFilterId(processId, g_perfCb.processIds, g_perfCb.processIdsNr);  // 进程ID过滤
}

/*
 * @brief 检查性能事件参数是否有效
 * @return UINT32 检查结果
 *         - 0: 参数无效(无事件或PMU未初始化)
 *         - 非0: 参数有效(存在至少一个有效事件周期)
 */
STATIC INLINE UINT32 OsPerfParamValid(VOID)
{
    UINT32 index;                                          // 事件索引
    UINT32 res = 0;                                        // 检查结果，初始为无效

    if (g_pmu == NULL) {                                   // 如果PMU未初始化
        return 0;                                          // 返回无效
    }
    PerfEvent *events = &g_pmu->events;                    // 获取事件配置
    UINT32 eventNum = events->nr;                          // 获取事件数量

    for (index = 0; index < eventNum; index++) {           // 遍历所有事件
        res |= events->per[index].period;                  // 检查事件周期是否有效(非0)
    }
    return res;                                            // 返回检查结果
}

/*
 * @brief 初始化性能数据头部信息
 * @param[in] id 性能数据段ID
 * @return UINT32 初始化结果
 *         - LOS_OK: 成功
 *         - 其他值: 失败
 */
STATIC UINT32 OsPerfHdrInit(UINT32 id)
{
    PerfDataHdr head = {                                   // 性能数据头部结构体
        .magic      = PERF_DATA_MAGIC_WORD,                // 数据魔数，用于验证数据有效性
        .sampleType = g_perfCb.sampleType,                 // 采样类型
        .sectionId  = id,                                  // 数据段ID
        .eventType  = g_pmu->type,                         // 事件类型
        .len        = sizeof(PerfDataHdr),                 // 头部长度
    };
    return OsPerfOutputWrite((CHAR *)&head, head.len);     // 写入头部信息
}

/*
 * @brief 更新性能事件计数值
 * @param[in] event 事件指针
 * @param[in] value 要增加的计数值
 */
VOID OsPerfUpdateEventCount(Event *event, UINT32 value)
{
    if (event == NULL) {                                   // 检查事件指针有效性
        return;
    }
    event->count[ArchCurrCpuid()] += (value & 0xFFFFFFFF); /* event->count is UINT64 */  // 更新当前CPU的事件计数值
}

/*
 * @brief handle性能事件溢出
 * @param[in] event 触发溢出的事件指针
 * @param[in] regs 寄存器上下文指针
 */
VOID OsPerfHandleOverFlow(Event *event, PerfRegs *regs)
{
    PerfSampleData data;                                   // 采样数据结构体
    UINT32 len;                                            // 采样数据长度

    (VOID)memset_s(&data, sizeof(PerfSampleData), 0, sizeof(PerfSampleData));  // 清零采样数据
    if ((g_perfCb.needSample) && OsPerfFilter(LOS_CurTaskIDGet(), LOS_GetCurrProcessID())) {  // 如果需要采样且通过过滤
        len = OsPerfCollectData(event, &data, regs);       // 收集采样数据
        OsPerfOutputWrite((CHAR *)&data, len);             // 写入采样数据
    }
}

/*
 * @brief 性能监控模块初始化
 * @return UINT32 初始化结果
 *         - LOS_OK: 初始化成功
 *         - LOS_ERRNO_PERF_STATUS_INVALID: 状态无效
 *         - LOS_ERRNO_PERF_BUF_ERROR: 输出缓冲区初始化失败
 */
STATIC UINT32 OsPerfInit(VOID)
{
    UINT32 ret;                                            // 返回值
    if (g_perfCb.status != PERF_UNINIT) {                  // 检查当前状态是否为未初始化
        ret = LOS_ERRNO_PERF_STATUS_INVALID;               // 设置状态无效错误
        goto PERF_INIT_ERROR;                              // 跳转到错误处理
    }

    ret = OsPmuInit();                                     // 初始化PMU
    if (ret != LOS_OK) {                                   // 检查PMU初始化结果
        goto PERF_INIT_ERROR;                              // 跳转到错误处理
    }

    ret = OsPerfOutputInit(NULL, LOSCFG_PERF_BUFFER_SIZE);  // 初始化性能数据输出缓冲区
    if (ret != LOS_OK) {                                   // 检查缓冲区初始化结果
        ret = LOS_ERRNO_PERF_BUF_ERROR;                    // 设置缓冲区错误
        goto PERF_INIT_ERROR;                              // 跳转到错误处理
    }
    g_perfCb.status = PERF_STOPPED;                        // 设置状态为已停止
PERF_INIT_ERROR:
    return ret;                                            // 返回初始化结果
}

/*
 * @brief 打印性能监控配置信息
 * @details 打印PMU类型、事件配置、采样类型、过滤ID等调试信息
 */
STATIC VOID PerfInfoDump(VOID)
{
    UINT32 i;                                              // 循环索引
    if (g_pmu != NULL) {                                   // 如果PMU已初始化
        PRINTK("type: %d\n", g_pmu->type);                 // 打印PMU类型
        for (i = 0; i < g_pmu->events.nr; i++) {           // 遍历事件
            PRINTK("events[%d]: %d, 0x%x\n", i, g_pmu->events.per[i].eventId, g_pmu->events.per[i].period);  // 打印事件ID和周期
        }
        PRINTK("predivided: %d\n", g_pmu->events.cntDivided); // 打印分频标志
    } else {
        PRINTK("pmu is NULL\n");                           // PMU未初始化提示
    }

    PRINTK("sampleType: 0x%x\n", g_perfCb.sampleType);     // 打印采样类型(十六进制)
    for (i = 0; i < g_perfCb.taskIdsNr; i++) {             // 遍历任务过滤ID
        PRINTK("filter taskIds[%d]: %d\n", i, g_perfCb.taskIds[i]);  // 打印任务ID
    }
    for (i = 0; i < g_perfCb.processIdsNr; i++) {          // 遍历进程过滤ID
        PRINTK("filter processIds[%d]: %d\n", i, g_perfCb.processIds[i]);  // 打印进程ID
    }
    PRINTK("needSample: %d\n", g_perfCb.needSample);       // 打印是否需要采样标志
}

/*
 * @brief 设置过滤ID列表
 * @param[out] dstIds 目标ID数组
 * @param[out] dstIdsNr 目标ID数量指针
 * @param[in] ids 源ID数组
 * @param[in] idsNr 源ID数量
 */
STATIC INLINE VOID OsPerfSetFilterIds(UINT32 *dstIds, UINT8 *dstIdsNr, UINT32 *ids, UINT32 idsNr)
{
    errno_t ret;                                           // 函数返回值
    if (idsNr) {                                           // 如果源ID数量大于0
        ret = memcpy_s(dstIds, PERF_MAX_FILTER_TSKS * sizeof(UINT32), ids, idsNr * sizeof(UINT32));  // 复制ID列表
        if (ret != EOK) {                                  // 检查复制结果
            PRINT_ERR("In %s At line:%d execute memcpy_s error\n", __FUNCTION__, __LINE__);
            *dstIdsNr = 0;                                 // 重置目标ID数量
            return;
        }
        *dstIdsNr = MIN(idsNr, PERF_MAX_FILTER_TSKS);      // 确保不超过最大过滤ID数量
    } else {
        *dstIdsNr = 0;                                     // 源ID数量为0，目标ID数量也设为0
    }
}

/*
 * @brief 性能监控配置接口
 * @param[in] attr 性能配置属性结构体指针
 * @return UINT32 配置结果
 *         - LOS_OK: 配置成功
 *         - LOS_ERRNO_PERF_CONFIG_NULL: 参数为空
 *         - LOS_ERRNO_PERF_STATUS_INVALID: 状态无效
 */
UINT32 LOS_PerfConfig(PerfConfigAttr *attr)
{
    UINT32 ret;                                            // 返回值
    UINT32 intSave;                                        // 中断状态保存变量

    if (attr == NULL) {                                    // 检查参数有效性
        return LOS_ERRNO_PERF_CONFIG_NULL;                 // 返回参数为空错误
    }

    PERF_LOCK(intSave);                                    // 加锁保护共享资源
    if (g_perfCb.status != PERF_STOPPED) {                 // 检查当前状态是否为已停止
        ret = LOS_ERRNO_PERF_STATUS_INVALID;               // 设置状态无效错误
        PRINT_ERR("perf config status error : 0x%x\n", g_perfCb.status);
        goto PERF_CONFIG_ERROR;                            // 跳转到错误处理
    }

    g_pmu = NULL;                                          // 重置PMU指针

    g_perfCb.needSample = attr->needSample;                // 设置是否需要采样
    g_perfCb.sampleType = attr->sampleType;                // 设置采样类型

    OsPerfSetFilterIds(g_perfCb.taskIds, &g_perfCb.taskIdsNr, attr->taskIds, attr->taskIdsNr);  // 设置任务过滤ID
    OsPerfSetFilterIds(g_perfCb.processIds, &g_perfCb.processIdsNr, attr->processIds, attr->processIdsNr);  // 设置进程过滤ID

    ret = OsPerfConfig(&attr->eventsCfg);                  // 配置性能事件
    PerfInfoDump();                                        // 打印配置信息
PERF_CONFIG_ERROR:
    PERF_UNLOCK(intSave);                                  // 解锁
    return ret;                                            // 返回配置结果
}

/*
 * @brief 启动性能监控接口
 * @param[in] sectionId 性能数据段ID
 */
VOID LOS_PerfStart(UINT32 sectionId)
{
    UINT32 intSave;                                        // 中断状态保存变量
    UINT32 ret;                                            // 返回值

    PERF_LOCK(intSave);                                    // 加锁保护共享资源
    if (g_perfCb.status != PERF_STOPPED) {                 // 如果当前状态不是已停止
        PRINT_ERR("perf start status error : 0x%x\n", g_perfCb.status);
        goto PERF_START_ERROR;                             // 跳转到错误处理
    }

    if (!OsPerfParamValid()) {                             // 检查性能参数是否有效
        PRINT_ERR("forgot call `LOS_PerfConfig(...)` before perf start?\n");  // 提示未配置
        goto PERF_START_ERROR;                             // 跳转到错误处理
    }

    if (g_perfCb.needSample) {
        ret = OsPerfHdrInit(sectionId); /* section header init */  // 初始化数据头部
        if (ret != LOS_OK) {                               // 检查头部初始化结果
            PRINT_ERR("perf hdr init error 0x%x\n", ret);
            goto PERF_START_ERROR;                         // 跳转到错误处理
        }
    }

    SMP_CALL_PERF_FUNC(OsPerfStart); /* send to all cpu to start pmu */  // 在所有CPU核心上启动PMU
    g_perfCb.status = PERF_STARTED;                        // 设置状态为已启动
    g_perfCb.startTime = OsPerfGetCurrTime();              // 记录开始时间戳
PERF_START_ERROR:
    PERF_UNLOCK(intSave);                                  // 解锁
    return;
}

/*
 * @brief 停止性能监控接口
 */
VOID LOS_PerfStop(VOID)
{
    UINT32 intSave;                                        // 中断状态保存变量

    PERF_LOCK(intSave);                                    // 加锁保护共享资源
    if (g_perfCb.status != PERF_STARTED) {                 // 如果当前状态不是已启动
        PRINT_ERR("perf stop status error : 0x%x\n", g_perfCb.status);
        goto PERF_STOP_ERROR;                              // 跳转到错误处理
    }

    SMP_CALL_PERF_FUNC(OsPerfStop); /* send to all cpu to stop pmu */  // 在所有CPU核心上停止PMU

    OsPerfOutputFlush();                                   // 刷新输出缓冲区

    if (g_perfCb.needSample) {
        OsPerfOutputInfo();                                // 输出采样信息
    }

    g_perfCb.status = PERF_STOPPED;                        // 设置状态为已停止
    g_perfCb.endTime = OsPerfGetCurrTime();                // 记录结束时间戳

    OsPerfPrintTs();                                       // 打印监控耗时
PERF_STOP_ERROR:
    PERF_UNLOCK(intSave);                                  // 解锁
    return;
}

/**
 * @brief 读取性能采样数据
 * @param[out] dest 数据存储缓冲区指针
 * @param[in] size 要读取的数据大小(字节)
 * @return UINT32 实际读取的字节数
 */
UINT32 LOS_PerfDataRead(CHAR *dest, UINT32 size)
{
    return OsPerfOutputRead(dest, size);  // 调用输出模块读取采样数据
}

/**
 * @brief 注册性能缓冲区通知钩子函数
 * @param[in] func 钩子函数指针，缓冲区状态变化时触发
 * @note 线程安全：内部通过自旋锁保证原子操作
 */
VOID LOS_PerfNotifyHookReg(const PERF_BUF_NOTIFY_HOOK func)
{
    UINT32 intSave;                       // 中断状态保存变量

    PERF_LOCK(intSave);                   // 获取性能模块自旋锁
    OsPerfNotifyHookReg(func);            // 注册钩子函数到输出模块
    PERF_UNLOCK(intSave);                 // 释放性能模块自旋锁
}

/**
 * @brief 注册性能缓冲区刷新钩子函数
 * @param[in] func 钩子函数指针，缓冲区刷新时触发
 * @note 线程安全：内部通过自旋锁保证原子操作
 */
VOID LOS_PerfFlushHookReg(const PERF_BUF_FLUSH_HOOK func)
{
    UINT32 intSave;                       // 中断状态保存变量

    PERF_LOCK(intSave);                   // 获取性能模块自旋锁
    OsPerfFlushHookReg(func);             // 注册钩子函数到输出模块
    PERF_UNLOCK(intSave);                 // 释放性能模块自旋锁
}

/**
 * @brief 设置中断上下文的程序计数器和帧指针
 * @param[in] pc 程序计数器(指令地址)
 * @param[in] fp 帧指针(栈基地址)
 * @note 用于中断场景下的调用栈追踪
 */
VOID OsPerfSetIrqRegs(UINTPTR pc, UINTPTR fp)
{
    LosTaskCB *runTask = (LosTaskCB *)ArchCurrTaskGet();  // 获取当前运行任务控制块
    runTask->pc = pc;                                      // 更新任务PC值
    runTask->fp = fp;                                      // 更新任务FP值
}

// 性能模块初始化入口，内核扩展模块级别初始化
LOS_MODULE_INIT(OsPerfInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
