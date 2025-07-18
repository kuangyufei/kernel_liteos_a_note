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

#include "perf_pmu_pri.h"
// 硬件性能监控单元(PMU)全局实例指针，初始化为NULL
STATIC Pmu *g_perfHw = NULL;

// 硬件性能事件名称映射表，索引对应PERF_COUNT_HW_xxx枚举值
STATIC CHAR *g_eventName[PERF_COUNT_HW_MAX] = {
    [PERF_COUNT_HW_CPU_CYCLES]              = "cycles",          // CPU周期数
    [PERF_COUNT_HW_INSTRUCTIONS]            = "instructions",    // 指令执行数
    [PERF_COUNT_HW_ICACHE_REFERENCES]       = "icache",          // 指令缓存访问次数
    [PERF_COUNT_HW_ICACHE_MISSES]           = "icache-misses",   // 指令缓存未命中次数
    [PERF_COUNT_HW_DCACHE_REFERENCES]       = "dcache",          // 数据缓存访问次数
    [PERF_COUNT_HW_DCACHE_MISSES]           = "dcache-misses",   // 数据缓存未命中次数
    [PERF_COUNT_HW_BRANCH_INSTRUCTIONS]     = "branches",        // 分支指令数
    [PERF_COUNT_HW_BRANCH_MISSES]           = "branches-misses", // 分支预测失败次数
};

/**
 * @brief 配置硬件PMU事件参数
 * @details 1. 若为硬件类型事件，将事件ID映射为实际硬件编码；2. 为每个事件分配可用计数器；3. 决定是否需要分频器(每64个周期计数一次)
 * @return UINT32 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 */
STATIC UINT32 OsPerfHwConfig(VOID)
{
    UINT32 i;  // 循环计数器
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);  // 获取ARM架构PMU实例

    UINT32 maxCounter = OsGetPmuMaxCounter();  // 获取PMU最大计数器数量
    UINT32 counter = OsGetPmuCounter0();  // 获取起始计数器ID
    UINT32 cycleCounter = OsGetPmuCycleCounter();  // 获取周期计数器ID
    UINT32 cycleCode = armPmu->mapEvent(PERF_COUNT_HW_CPU_CYCLES, PERF_EVENT_TO_CODE);  // 将周期事件映射为硬件编码
    if (cycleCode == PERF_HW_INVALID_EVENT_TYPE) {  // 检查周期事件映射是否有效
        return LOS_NOK;  // 映射失败，返回错误
    }

    PerfEvent *events = &g_perfHw->events;  // 获取事件列表
    UINT32 eventNum = events->nr;  // 获取事件数量

    for (i = 0; i < eventNum; i++) {  // 遍历所有事件
        Event *event = &(events->per[i]);  // 获取当前事件

        if (!VALID_PERIOD(event->period)) {  // 检查周期值是否有效
            PRINT_ERR("Config period: 0x%x invalid, should be in (%#x, %#x)\n", event->period,
                PERIOD_CALC(CCNT_PERIOD_UPPER_BOUND), PERIOD_CALC(CCNT_PERIOD_LOWER_BOUND));
            return LOS_NOK;  // 周期无效，返回错误
        }

        if (g_perfHw->type == PERF_EVENT_TYPE_HW) {  // 硬件事件需要映射
            UINT32 eventId = armPmu->mapEvent(event->eventId, PERF_EVENT_TO_CODE);  // 映射事件ID为硬件编码
            if (eventId == PERF_HW_INVALID_EVENT_TYPE) {  // 检查映射是否有效
                return LOS_NOK;  // 映射失败，返回错误
            }
            event->eventId = eventId;  // 更新事件硬件编码
        }

        if (event->eventId == cycleCode) {  // 判断是否为周期事件
            event->counter = cycleCounter;  // 分配周期计数器
        } else {
            event->counter = counter;  // 分配通用计数器
            counter++;  // 计数器索引递增
        }

        if (counter >= maxCounter) {  // 检查计数器是否超出最大数量
            PRINT_ERR("max events: %u excluding cycle event\n", maxCounter - 1);
            return LOS_NOK;  // 超出范围，返回错误
        }

        PRINT_DEBUG("Perf Config %u eventId = 0x%x, counter = 0x%x, period = 0x%x\n", i, event->eventId, event->counter,
            event->period);  // 打印配置调试信息
    }

    armPmu->cntDivided = events->cntDivided & armPmu->canDivided;  // 设置分频器，取事件需求与硬件能力的交集
    return LOS_OK;  // 配置成功，返回OK
}

/**
 * @brief 启动硬件PMU事件监控
 * @details 初始化并启用所有配置的硬件性能事件计数器
 * @return UINT32 操作结果，LOS_OK表示成功
 */
STATIC UINT32 OsPerfHwStart(VOID)
{
    UINT32 i;  // 循环计数器
    UINT32 cpuid = ArchCurrCpuid();  // 获取当前CPU核心ID
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);  // 获取ARM架构PMU实例

    PerfEvent *events = &g_perfHw->events;  // 获取事件列表
    UINT32 eventNum = events->nr;  // 获取事件数量

    armPmu->clear();  // 清除PMU所有计数器

    for (i = 0; i < eventNum; i++) {  // 遍历所有事件
        Event *event = &(events->per[i]);  // 获取当前事件
        armPmu->setPeriod(event);  // 设置事件采样周期
        armPmu->enable(event);  // 启用事件计数器
        event->count[cpuid] = 0;  // 重置当前CPU的事件计数
    }

    armPmu->start();  // 启动PMU监控
    return LOS_OK;  // 启动成功，返回OK
}

/**
 * @brief 停止硬件PMU事件监控
 * @details 停止计数器并读取当前计数值，对周期事件进行分频校正
 * @return UINT32 操作结果，LOS_OK表示成功
 */
STATIC UINT32 OsPerfHwStop(VOID)
{
    UINT32 i;  // 循环计数器
    UINT32 cpuid = ArchCurrCpuid();  // 获取当前CPU核心ID
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);  // 获取ARM架构PMU实例

    PerfEvent *events = &g_perfHw->events;  // 获取事件列表
    UINT32 eventNum = events->nr;  // 获取事件数量

    armPmu->stop();  // 停止PMU监控

    for (i = 0; i < eventNum; i++) {  // 遍历所有事件
        Event *event = &(events->per[i]);  // 获取当前事件
        UINTPTR value = armPmu->readCnt(event);  // 读取计数器当前值
        PRINT_DEBUG("perf stop readCnt value = 0x%x\n", value);  // 打印调试信息
        event->count[cpuid] += value;  // 累加当前CPU的事件计数

        UINT32 eventId = armPmu->mapEvent(event->eventId, PERF_CODE_TO_EVENT);  // 将硬件编码映射回事件ID
        if ((eventId == PERF_COUNT_HW_CPU_CYCLES) && (armPmu->cntDivided != 0)) {  // 若为周期事件且启用了分频
            PRINT_DEBUG("perf stop is cycle\n");  // 打印调试信息
            event->count[cpuid] = event->count[cpuid] << 6;  // CCNT每64个CPU周期计数一次，需左移6位校正
        }
        PRINT_DEBUG("perf stop eventCount[0x%x] : [%s] = %llu\n", event->eventId, g_eventName[eventId],
            event->count[cpuid]);  // 打印事件计数结果
    }
    return LOS_OK;  // 停止成功，返回OK
}

/**
 * @brief 获取性能事件的名称
 * @param event 事件指针
 * @return CHAR* 事件名称字符串，未知事件返回"unknown"
 */
STATIC CHAR *OsPerfGetEventName(Event *event)
{
    UINT32 eventId;  // 事件ID
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);  // 获取ARM架构PMU实例
    eventId = armPmu->mapEvent(event->eventId, PERF_CODE_TO_EVENT);  // 将硬件编码映射为事件ID
    if (eventId < PERF_COUNT_HW_MAX) {  // 检查事件ID是否有效
        return g_eventName[eventId];  // 返回事件名称
    } else {
        return "unknown";  // 返回未知事件标识
    }
}

/**
 * @brief 初始化硬件PMU模块
 * @param hwPmu 硬件PMU实例指针
 * @return UINT32 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 */
UINT32 OsPerfHwInit(HwPmu *hwPmu)
{
    UINT32 ret;  // 返回值
    if (hwPmu == NULL) {  // 检查输入参数
        return LOS_NOK;  // 参数无效，返回错误
    }

    hwPmu->pmu.type    = PERF_EVENT_TYPE_HW;  // 设置事件类型为硬件事件
    hwPmu->pmu.config  = OsPerfHwConfig;  // 注册配置函数
    hwPmu->pmu.start   = OsPerfHwStart;  // 注册启动函数
    hwPmu->pmu.stop    = OsPerfHwStop;  // 注册停止函数
    hwPmu->pmu.getName = OsPerfGetEventName;  // 注册事件名称获取函数

    (VOID)memset_s(&hwPmu->pmu.events, sizeof(PerfEvent), 0, sizeof(PerfEvent));  // 初始化事件结构体
    ret = OsPerfPmuRegister(&hwPmu->pmu);  // 注册PMU模块

    g_perfHw = OsPerfPmuGet(PERF_EVENT_TYPE_HW);  // 获取硬件PMU实例
    return ret;  // 返回注册结果
}
