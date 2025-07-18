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

/**
 * @brief 时间单位转换：微秒/秒
 * @note 1秒 = 1000000微秒
 */
#define US_PER_SECOND               1000000
/**
 * @brief 高精度定时器默认周期（微秒）
 * @note 默认值为1000微秒（1毫秒）
 */
#define HRTIMER_DEFAULT_PERIOD_US   1000

/**
 * @brief 软件PMU（性能监控单元）定时采样全局实例
 */
STATIC SwPmu g_perfTimed;

/**
 * @brief 检查定时采样周期是否有效
 * @param period 待检查的周期值（微秒）
 * @return BOOL - 周期有效返回TRUE，否则返回FALSE
 * @note 有效周期需大于等于TIMER_PERIOD_LOWER_BOUND_US宏定义值
 */
STATIC BOOL OsPerfTimedPeriodValid(UINT32 period)
{
    return period >= TIMER_PERIOD_LOWER_BOUND_US;
}

/**
 * @brief 启动定时性能采样
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 * @note 仅在CPU核心0上启动定时器，其他核心仅初始化事件计数
 */
STATIC UINT32 OsPerfTimedStart(VOID)
{
    UINT32 i;
    UINT32 cpuid = ArchCurrCpuid();                     /* 获取当前CPU核心ID */
    PerfEvent *events = &g_perfTimed.pmu.events;        /* 获取事件集合指针 */
    UINT32 eventNum = events->nr;                       /* 获取事件数量 */

    /* 初始化当前CPU核心的所有事件计数 */
    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        event->count[cpuid] = 0;                        /* 重置事件计数 */
    }

    if (cpuid != 0) {                                   /* 仅需在一个核心上启动定时器 */
        return LOS_OK;
    }

    /* 启动高精度定时器 */
    if (hrtimer_start(&g_perfTimed.hrtimer, g_perfTimed.time, HRTIMER_MODE_REL) != 0) {
        PRINT_ERR("Hrtimer start failed\n");
        return LOS_NOK;
    }

    /* 向前调整定时器 */
    if (hrtimer_forward(&g_perfTimed.hrtimer, g_perfTimed.cfgTime) == 0) {
        PRINT_ERR("Hrtimer forward failed\n");
        return LOS_NOK;
    }

    g_perfTimed.time = g_perfTimed.cfgTime;             /* 更新当前定时器时间 */
    return LOS_OK;
}

/**
 * @brief 配置定时性能采样参数
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 * @note 仅配置PERF_COUNT_CPU_CLOCK类型事件的采样周期
 */
STATIC UINT32 OsPerfTimedConfig(VOID)
{
    UINT32 i;
    PerfEvent *events = &g_perfTimed.pmu.events;        /* 获取事件集合指针 */
    UINT32 eventNum = events->nr;                       /* 获取事件数量 */

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        UINT32 period = event->period;                  /* 获取事件周期 */
        if (event->eventId == PERF_COUNT_CPU_CLOCK) {   /* 仅处理CPU时钟事件 */
            /* 检查周期有效性，无效则使用默认值 */
            if (!OsPerfTimedPeriodValid(period)) {
                period = TIMER_PERIOD_LOWER_BOUND_US;
                PRINT_ERR("config period invalid, should be >= 100, use default period:%u us\n", period);
            }

            /* 配置高精度定时器时间 */
            g_perfTimed.cfgTime = (union ktime) {
                .tv.sec  = period / US_PER_SECOND,      /* 秒部分 */
                .tv.usec = period % US_PER_SECOND       /* 微秒部分 */
            };
            PRINT_INFO("hrtimer config period - sec:%d, usec:%d\n", g_perfTimed.cfgTime.tv.sec,
                g_perfTimed.cfgTime.tv.usec);
            return LOS_OK;
        }
    }
    return LOS_NOK;                                     /* 未找到CPU时钟事件 */
}

/**
 * @brief 停止定时性能采样
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 * @note 仅在CPU核心0上停止定时器，其他核心直接返回成功
 */
STATIC UINT32 OsPerfTimedStop(VOID)
{
    UINT32 ret;

    if (ArchCurrCpuid() != 0) {                         /* 仅需在一个核心上停止定时器 */
        return LOS_OK;
    }

    ret = hrtimer_cancel(&g_perfTimed.hrtimer);         /* 取消高精度定时器 */
    if (ret != 1) {
        PRINT_ERR("Hrtimer stop failed!, 0x%x\n", ret);
        return LOS_NOK;
    }
    return LOS_OK;
}

/**
 * @brief 定时性能采样事件处理函数
 * @details 定时器触发时调用，更新事件计数并处理溢出
 */
STATIC VOID OsPerfTimedHandle(VOID)
{
    UINT32 index;
    PerfRegs regs;                                      /* 寄存器信息结构体 */

    PerfEvent *events = &g_perfTimed.pmu.events;        /* 获取事件集合指针 */
    UINT32 eventNum = events->nr;                       /* 获取事件数量 */

    (VOID)memset_s(&regs, sizeof(PerfRegs), 0, sizeof(PerfRegs)); /* 初始化寄存器信息 */
    OsPerfFetchIrqRegs(&regs);                          /* 获取中断时的寄存器信息 */

    /* 遍历所有事件，更新计数并检查溢出 */
    for (index = 0; index < eventNum; index++) {
        Event *event = &(events->per[index]);
        OsPerfUpdateEventCount(event, 1);               /* 事件计数+1 */
        OsPerfHandleOverFlow(event, &regs);             /* 处理事件溢出 */
    }
}

/**
 * @brief 高精度定时器回调函数
 * @param hrtimer 定时器结构体指针
 * @return enum hrtimer_restart - 定时器重启策略，HRTIMER_RESTART表示重启
 * @note 通过SMP_CALL_PERF_FUNC宏在所有CPU核心上执行采样处理
 */
STATIC enum hrtimer_restart OsPerfHrtimer(struct hrtimer *hrtimer)
{
    SMP_CALL_PERF_FUNC(OsPerfTimedHandle);              /* 发送到所有CPU核心收集数据 */
    return HRTIMER_RESTART;                             /* 重启定时器 */
}

/**
 * @brief 获取性能事件名称
 * @param event 事件结构体指针
 * @return CHAR* - 事件名称字符串
 * @note 当前仅支持PERF_COUNT_CPU_CLOCK类型事件，返回"timed"
 */
STATIC CHAR *OsPerfGetEventName(Event *event)
{
    if (event->eventId == PERF_COUNT_CPU_CLOCK) {
        return "timed";
    } else {
        return "unknown";
    }
}

/**
 * @brief 初始化定时性能监控单元(PMU)
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 * @details 初始化定时器、配置PMU操作函数并注册到系统
 */
UINT32 OsTimedPmuInit(VOID)
{
    UINT32 ret;

    /* 初始化默认定时器时间 */
    g_perfTimed.time = (union ktime) {
        .tv.sec = 0,
        .tv.usec = HRTIMER_DEFAULT_PERIOD_US,
    };

    hrtimer_init(&g_perfTimed.hrtimer, 1, HRTIMER_MODE_REL); /* 初始化高精度定时器 */

    /* 创建高精度定时器 */
    ret = hrtimer_create(&g_perfTimed.hrtimer, g_perfTimed.time, OsPerfHrtimer);
    if (ret != LOS_OK) {
        return ret;
    }

    /* 初始化PMU操作结构体 */
    g_perfTimed.pmu = (Pmu) {
        .type    = PERF_EVENT_TYPE_TIMED,                /* 事件类型：定时采样 */
        .config  = OsPerfTimedConfig,                    /* 配置函数 */
        .start   = OsPerfTimedStart,                     /* 启动函数 */
        .stop    = OsPerfTimedStop,                      /* 停止函数 */
        .getName = OsPerfGetEventName,                   /* 获取事件名称函数 */
    };

    (VOID)memset_s(&g_perfTimed.pmu.events, sizeof(PerfEvent), 0, sizeof(PerfEvent)); /* 初始化事件集合 */
    ret = OsPerfPmuRegister(&g_perfTimed.pmu);           /* 注册PMU到系统 */
    return ret;
}
