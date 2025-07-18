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
#include "los_hook.h"

/**
 * @brief 软件PMU（性能监控单元）全局控制块
 */
STATIC SwPmu g_perfSw;

/**
 * @brief 软件性能事件名称映射表
 * @note 索引对应PERF_COUNT_SW_xxx枚举值
 */
STATIC CHAR *g_eventName[PERF_COUNT_SW_MAX] = {
    [PERF_COUNT_SW_TASK_SWITCH]  = "task switch",
    [PERF_COUNT_SW_IRQ_RESPONSE] = "irq response",
    [PERF_COUNT_SW_MEM_ALLOC]    = "mem alloc",
    [PERF_COUNT_SW_MUX_PEND]     = "mux pend",
};

/**
 * @brief 性能事件与系统钩子类型映射表
 * @note 将性能事件ID转换为对应的LOS_HOOK_TYPE_xxx类型
 */
STATIC UINT32 g_traceEventMap[PERF_COUNT_SW_MAX] = {
    [PERF_COUNT_SW_TASK_SWITCH]  = LOS_HOOK_TYPE_TASK_SWITCHEDIN,  /* 任务切换事件对应钩子类型 */
    [PERF_COUNT_SW_IRQ_RESPONSE] = LOS_HOOK_TYPE_ISR_ENTER,        /* 中断响应事件对应钩子类型 */
    [PERF_COUNT_SW_MEM_ALLOC]    = LOS_HOOK_TYPE_MEM_ALLOC,        /* 内存分配事件对应钩子类型 */
    [PERF_COUNT_SW_MUX_PEND]     = LOS_HOOK_TYPE_MUX_PEND,         /* 互斥锁等待事件对应钩子类型 */
};

/**
 * @brief 性能事件钩子处理函数
 * @param eventType 事件类型（对应LOS_HOOK_TYPE_xxx）
 * @note 当软件PMU使能时，更新对应事件计数并处理溢出
 */
VOID OsPerfHook(UINT32 eventType)
{
    if (!g_perfSw.enable) {                       /* 检查PMU是否使能 */
        return;
    }

    PerfEvent *events = &g_perfSw.pmu.events;     /* 获取事件集合指针 */
    UINT32 eventNum = events->nr;                 /* 获取事件数量 */

    UINT32 i;
    PerfRegs regs;                                /* 寄存器信息结构体 */

    (VOID)memset_s(&regs, sizeof(PerfRegs), 0, sizeof(PerfRegs)); /* 初始化寄存器信息 */

    /* 遍历事件集合，查找匹配的事件类型 */
    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        if (event->counter == eventType) {        /* 找到匹配的事件 */
            OsPerfUpdateEventCount(event, 1);     /* 事件计数+1 */
            /* 当计数值达到周期阈值时，处理溢出并采集数据 */
            if (event->count[ArchCurrCpuid()] % event->period == 0) {
                OsPerfFetchCallerRegs(&regs);     /* 获取调用者寄存器信息 */
                OsPerfHandleOverFlow(event, &regs); /* 处理事件溢出 */
            }
            return;
        }
    }
}

/**
 * @brief 内存分配钩子回调函数
 * @param pool 内存池指针
 * @param ptr 分配的内存指针
 * @param size 分配的内存大小
 * @note 触发内存分配性能事件
 */
STATIC VOID LOS_PerfMemAlloc(VOID *pool, VOID *ptr, UINT32 size)
{
    OsPerfHook(LOS_HOOK_TYPE_MEM_ALLOC);          /* 调用钩子处理函数 */
}

/**
 * @brief 互斥锁等待钩子回调函数
 * @param muxCB 互斥锁控制块指针
 * @param timeout 超时时间
 * @note 触发互斥锁等待性能事件
 */
STATIC VOID LOS_PerfMuxPend(const LosMux *muxCB, UINT32 timeout)
{
    OsPerfHook(LOS_HOOK_TYPE_MUX_PEND);           /* 调用钩子处理函数 */
}

/**
 * @brief 中断进入钩子回调函数
 * @param hwiNum 中断号
 * @note 触发中断响应性能事件
 */
STATIC VOID LOS_PerfIsrEnter(UINT32 hwiNum)
{
    OsPerfHook(LOS_HOOK_TYPE_ISR_ENTER);          /* 调用钩子处理函数 */
}

/**
 * @brief 任务切换钩子回调函数
 * @param newTask 新任务控制块指针
 * @param runTask 运行中任务控制块指针
 * @note 触发任务切换性能事件
 */
STATIC VOID LOS_PerfTaskSwitchedIn(const LosTaskCB *newTask, const LosTaskCB *runTask)
{
    OsPerfHook(LOS_HOOK_TYPE_TASK_SWITCHEDIN);    /* 调用钩子处理函数 */
}

/**
 * @brief 性能事件钩子初始化函数
 * @details 注册系统钩子与性能事件的关联关系
 */
STATIC VOID OsPerfCnvInit(VOID)
{
    LOS_HookReg(LOS_HOOK_TYPE_MEM_ALLOC, LOS_PerfMemAlloc);          /* 注册内存分配钩子 */
    LOS_HookReg(LOS_HOOK_TYPE_MUX_PEND, LOS_PerfMuxPend);            /* 注册互斥锁等待钩子 */
    LOS_HookReg(LOS_HOOK_TYPE_ISR_ENTER, LOS_PerfIsrEnter);          /* 注册中断进入钩子 */
    LOS_HookReg(LOS_HOOK_TYPE_TASK_SWITCHEDIN, LOS_PerfTaskSwitchedIn); /* 注册任务切换钩子 */
}

/**
 * @brief 软件PMU配置函数
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 * @note 验证事件有效性并设置事件计数器
 */
STATIC UINT32 OsPerfSwConfig(VOID)
{
    UINT32 i;
    PerfEvent *events = &g_perfSw.pmu.events;     /* 获取事件集合指针 */
    UINT32 eventNum = events->nr;                 /* 获取事件数量 */

    /* 遍历所有事件，配置事件计数器 */
    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        /* 检查事件ID有效性和周期值 */
        if ((event->eventId < PERF_COUNT_SW_TASK_SWITCH) || (event->eventId >= PERF_COUNT_SW_MAX) ||
            (event->period == 0)) {
            return LOS_NOK;                       /* 事件配置无效 */
        }
        event->counter = g_traceEventMap[event->eventId]; /* 设置事件计数器为对应的钩子类型 */
    }
    return LOS_OK;
}

/**
 * @brief 启动软件PMU采样
 * @return UINT32 - 操作结果，LOS_OK表示成功
 * @note 初始化当前CPU核心的事件计数并使能PMU
 */
STATIC UINT32 OsPerfSwStart(VOID)
{
    UINT32 i;
    UINT32 cpuid = ArchCurrCpuid();               /* 获取当前CPU核心ID */
    PerfEvent *events = &g_perfSw.pmu.events;     /* 获取事件集合指针 */
    UINT32 eventNum = events->nr;                 /* 获取事件数量 */

    /* 初始化当前CPU核心的所有事件计数 */
    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        event->count[cpuid] = 0;                  /* 重置事件计数 */
    }

    g_perfSw.enable = TRUE;                       /* 使能软件PMU */
    return LOS_OK;
}

/**
 * @brief 停止软件PMU采样
 * @return UINT32 - 操作结果，LOS_OK表示成功
 * @note 禁用PMU以停止事件计数和采样
 */
STATIC UINT32 OsPerfSwStop(VOID)
{
    g_perfSw.enable = FALSE;                      /* 禁用软件PMU */
    return LOS_OK;
}

/**
 * @brief 获取性能事件名称
 * @param event 事件结构体指针
 * @return CHAR* - 事件名称字符串，未知事件返回"unknown"
 */
STATIC CHAR *OsPerfGetEventName(Event *event)
{
    UINT32 eventId = event->eventId;              /* 获取事件ID */
    if (eventId < PERF_COUNT_SW_MAX) {
        return g_eventName[eventId];              /* 返回预定义的事件名称 */
    }
    return "unknown";
}

/**
 * @brief 初始化软件性能监控单元(PMU)
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 * @details 配置PMU操作函数、初始化钩子并注册到系统
 */
UINT32 OsSwPmuInit(VOID)
{
    /* 初始化PMU操作结构体 */
    g_perfSw.pmu = (Pmu) {
        .type    = PERF_EVENT_TYPE_SW,            /* 事件类型：软件事件 */
        .config  = OsPerfSwConfig,                /* 配置函数 */
        .start   = OsPerfSwStart,                 /* 启动函数 */
        .stop    = OsPerfSwStop,                  /* 停止函数 */
        .getName = OsPerfGetEventName,            /* 获取事件名称函数 */
    };

    g_perfSw.enable = FALSE;                      /* 默认禁用PMU */

    OsPerfCnvInit();                              /* 初始化性能事件钩子 */

    (VOID)memset_s(&g_perfSw.pmu.events, sizeof(PerfEvent), 0, sizeof(PerfEvent)); /* 初始化事件集合 */
    return OsPerfPmuRegister(&g_perfSw.pmu);      /* 注册PMU到系统 */
}
