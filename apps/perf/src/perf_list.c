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

#include <stdio.h>
#include "perf.h"
#include "perf_list.h"
/**
 * @file perf_list.c
 * @brief 性能事件列表定义及打印功能实现
 */

/**
 * @brief 事件类型描述字符串数组
 * @details 与PERF_EVENT_TYPE_xxx枚举值一一对应，用于事件类型的可读性展示
 *          索引0:硬件事件, 索引1:定时事件, 索引2:软件事件
 */
static const char *g_eventTypeStr[] = {
    "[Hardware event]",   // 硬件事件类型描述
    "[Timed event]",      // 定时事件类型描述
    "[Software event]",   // 软件事件类型描述
};

/**
 * @brief 性能事件定义数组
 * @details 包含系统支持的所有性能事件，每个事件包含名称、ID和类型
 *          数组以{.name = "", .event = -1, ...}作为结束标志
 * @note 根据编译配置(LOSCFG_PERF_xxx)条件包含不同类型的事件
 */
const PerfEvent g_events[] = {
#ifdef LOSCFG_PERF_HW_PMU  // 硬件性能监控单元(PMU)配置开关
    {
        .name = "cycles",             // 事件名称：CPU周期
        .event = PERF_COUNT_HW_CPU_CYCLES,  // 事件ID：硬件CPU周期计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
    {
        .name = "instruction",        // 事件名称：指令数
        .event = PERF_COUNT_HW_INSTRUCTIONS,  // 事件ID：硬件指令计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
    {
        .name = "dcache",             // 事件名称：数据缓存访问
        .event = PERF_COUNT_HW_DCACHE_REFERENCES,  // 事件ID：数据缓存引用计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
    {
        .name = "dcache-miss",        // 事件名称：数据缓存未命中
        .event = PERF_COUNT_HW_DCACHE_MISSES,  // 事件ID：数据缓存未命中计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
    {
        .name = "icache",             // 事件名称：指令缓存访问
        .event = PERF_COUNT_HW_ICACHE_REFERENCES,  // 事件ID：指令缓存引用计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
    {
        .name = "icache-miss",        // 事件名称：指令缓存未命中
        .event = PERF_COUNT_HW_ICACHE_MISSES,  // 事件ID：指令缓存未命中计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
    {
        .name = "branch",             // 事件名称：分支指令
        .event = PERF_COUNT_HW_BRANCH_INSTRUCTIONS,  // 事件ID：分支指令计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
    {
        .name = "branch-miss",        // 事件名称：分支预测失败
        .event = PERF_COUNT_HW_BRANCH_MISSES,  // 事件ID：分支预测失败计数
        .type = PERF_EVENT_TYPE_HW,    // 事件类型：硬件事件
    },
#endif  // LOSCFG_PERF_HW_PMU

#ifdef LOSCFG_PERF_TIMED_PMU  // 定时性能监控配置开关
    {
        .name = "clock",              // 事件名称：CPU时钟
        .event = PERF_COUNT_CPU_CLOCK,  // 事件ID：CPU时钟计数
        .type = PERF_EVENT_TYPE_TIMED,  // 事件类型：定时事件
    },
#endif  // LOSCFG_PERF_TIMED_PMU

#ifdef LOSCFG_PERF_SW_PMU  // 软件性能监控配置开关
    {
        .name = "task-switch",        // 事件名称：任务切换
        .event = PERF_COUNT_SW_TASK_SWITCH,  // 事件ID：任务切换计数
        .type = PERF_EVENT_TYPE_SW,    // 事件类型：软件事件
    },
    {
        .name = "irq-in",             // 事件名称：中断响应
        .event = PERF_COUNT_SW_IRQ_RESPONSE,  // 事件ID：中断响应计数
        .type = PERF_EVENT_TYPE_SW,    // 事件类型：软件事件
    },
    {
        .name = "mem-alloc",          // 事件名称：内存分配
        .event = PERF_COUNT_SW_MEM_ALLOC,  // 事件ID：内存分配计数
        .type = PERF_EVENT_TYPE_SW,    // 事件类型：软件事件
    },
    {
        .name = "mux-pend",           // 事件名称：互斥锁挂起
        .event = PERF_COUNT_SW_MUX_PEND,  // 事件ID：互斥锁挂起计数
        .type = PERF_EVENT_TYPE_SW,    // 事件类型：软件事件
    },
#endif  // LOSCFG_PERF_SW_PMU

    // 数组结束标志（事件ID为-1）
    {
        .name = "",                   // 空名称表示结束
        .event = -1,                    // 事件ID为-1作为结束标记
        .type = PERF_EVENT_TYPE_MAX,    // 事件类型为最大值
    }
};

/**
 * @brief 打印所有支持的性能事件列表
 * @details 遍历g_events数组，格式化输出事件名称和对应的事件类型
 *          事件类型通过g_eventTypeStr数组转换为可读性字符串
 * @note 仅打印事件ID不为-1的有效事件
 */
void PerfList(void)
{
    const PerfEvent *evt = &g_events[0];  // 事件指针，从第一个事件开始遍历
    printf("\n");                       // 打印空行，美化输出格式
    // 遍历事件数组，直到遇到结束标志（event == -1）
    for (; evt->event != -1; evt++) {
        // 格式化输出：事件名称左对齐(25字符)，事件类型右对齐(30字符)
        printf("\t %-25s%30s\n", evt->name, g_eventTypeStr[evt->type]);
    }
    printf("\n");                       // 打印空行，美化输出格式
}
