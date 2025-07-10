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


#ifndef _PERF_H
#define _PERF_H

#include <stdlib.h>

#ifdef  __cplusplus
#if  __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @def PERF_MAX_EVENT
 * @brief 性能事件的最大数量
 * @note 定义了系统支持的最大性能事件数量，十进制值为7
 */
#define PERF_MAX_EVENT          7

/**
 * @def PERF_MAX_FILTER_TSKS
 * @brief 任务过滤的最大数量
 * @note 定义了性能监控中可过滤的最大任务/进程数量，十进制值为32
 */
#define PERF_MAX_FILTER_TSKS    32

/**
 * @def printf_debug
 * @brief 调试信息打印宏
 * @note 当PERF_DEBUG宏定义时，输出调试信息；否则不输出任何内容
 * @param fmt 格式化字符串
 * @param ... 可变参数列表
 */
#ifdef PERF_DEBUG
#define printf_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define printf_debug(fmt, ...)
#endif

/*
 * Perf事件类型枚举
 * 定义了性能监控支持的事件类型分类
 */
enum PerfEventType {
    PERF_EVENT_TYPE_HW,      /* 板级通用硬件事件，如CPU周期、缓存命中等 */
    PERF_EVENT_TYPE_TIMED,   /* 高精度定时器事件，基于hrtimer实现的定时采样 */
    PERF_EVENT_TYPE_SW,      /* 软件跟踪事件，如任务切换、中断响应等 */
    PERF_EVENT_TYPE_RAW,     /* 板级特殊硬件事件，需参考对应架构头文件中的PmuEventType枚举 */

    PERF_EVENT_TYPE_MAX      /* 事件类型数量上限，不用于实际事件表示 */
};

/*
 * 通用硬件PMU事件枚举
 * 定义了处理器通用的硬件性能监控事件
 */
enum PmuHwId {
    PERF_COUNT_HW_CPU_CYCLES = 0,      /* CPU周期事件，记录CPU执行周期数 */
    PERF_COUNT_HW_INSTRUCTIONS,        /* 指令执行事件，记录指令完成执行的数量 */
    PERF_COUNT_HW_DCACHE_REFERENCES,   /* 数据缓存访问事件，记录数据缓存被访问次数 */
     PERF_COUNT_HW_DCACHE_MISSES,       /* 数据缓存未命中事件 */
    PERF_COUNT_HW_ICACHE_REFERENCES,   /* 指令缓存访问事件 */
    PERF_COUNT_HW_ICACHE_MISSES,       /* 指令缓存未命中事件 */
    PERF_COUNT_HW_BRANCH_INSTRUCTIONS, /* 分支指令执行事件（程序计数器软件变更） */
    PERF_COUNT_HW_BRANCH_MISSES,       /* 分支预测失败事件 */

    PERF_COUNT_HW_MAX,                 /* 硬件事件类型数量上限 */
}; /* enum PmuHwId */

/*
 * 通用高精度定时器计时事件定义
 * 基于系统高精度定时器(hrtimer)实现的定时采样事件
 */
enum PmuTimedId {
    PERF_COUNT_CPU_CLOCK = 0,      /* CPU时钟计时事件（单位：纳秒） */
}; /* enum PmuTimedId */

/*
 * 通用软件性能监控事件定义
 * 内核软件行为跟踪事件，用于分析系统软件层面性能瓶颈
 */
enum PmuSwId {
    PERF_COUNT_SW_TASK_SWITCH = 1, /* 任务切换事件（上下文切换） */
    PERF_COUNT_SW_IRQ_RESPONSE,    /* 中断响应延迟事件 */
    PERF_COUNT_SW_MEM_ALLOC,       /* 内存分配事件（包括kmalloc等接口调用） */
    PERF_COUNT_SW_MUX_PEND,        /* 互斥锁等待事件（mutex阻塞） */

    PERF_COUNT_SW_MAX,             /* 软件事件类型数量上限 */
}; /* enum PmuSwId */

/*
 * 性能采样数据类型枚举
 * 通过PerfConfigAttr->sampleType配置需要采集的数据类型
 * 支持多类型组合（按位或操作）
 */
enum PerfSampleType {
    PERF_RECORD_CPU       = 1U << 0, /* 记录当前CPU ID (0x01) */
    PERF_RECORD_TID       = 1U << 1, /* 记录当前任务ID (0x02) */
    PERF_RECORD_TYPE      = 1U << 2, /* 记录事件类型 (0x04) */
    PERF_RECORD_PERIOD    = 1U << 3, /* 记录事件周期值 (0x08) */
    PERF_RECORD_TIMESTAMP = 1U << 4, /* 记录事件时间戳 (0x10) */
    PERF_RECORD_IP        = 1U << 5, /* 记录指令指针（当前PC值） (0x20) */
    PERF_RECORD_CALLCHAIN = 1U << 6, /* 记录调用栈回溯信息 (0x40) */
    PERF_RECORD_PID       = 1U << 7, /* 记录当前进程ID (0x80) */
}; /* enum PerfSampleType */

/*
 * 性能事件配置子结构
 *
 * 该结构用于配置特定类型事件的属性参数，包括事件ID和采样周期
 */
typedef struct {
    unsigned int type;              /* 事件类型，对应PerfEventType枚举值 */
    struct {
        unsigned int eventId;       /* 特定事件ID，与PerfEventType类型相对应 */
        unsigned int period;        /* 事件采样周期，每发生period次事件记录一次样本
                                       例如：period=100表示每100次事件触发一次采样 */
    } events[PERF_MAX_EVENT];       /* 性能事件列表，最多支持PERF_MAX_EVENT(7)个事件 */
    unsigned int eventsNr;          /* 实际配置的事件数量 */
    size_t predivided;              /* 是否启用预分频（每64次计数采样一次）
                                       仅对CPU周期硬件事件(PERF_COUNT_HW_CPU_CYCLES)生效 */
} PerfEventConfig; /* struct PerfEventConfig */

/*
 * 性能采样主配置结构
 *
 * 该结构用于设置性能采样的整体属性，包括事件配置、任务过滤和采样数据类型等
 */
typedef struct {
    PerfEventConfig         eventsCfg;                      /* 性能事件配置子结构 */
    unsigned int            taskIds[PERF_MAX_FILTER_TSKS];  /* 任务过滤列表（白名单）
                                                               PERF_MAX_FILTER_TSKS=32 */
    unsigned int            taskIdsNr;                      /* 任务过滤列表中任务数量
                                                               设为0表示对所有任务采样 */
    unsigned int            processIds[PERF_MAX_FILTER_TSKS];  /* 进程过滤列表（白名单） */
    unsigned int            processIdsNr;                      /* 进程过滤列表中进程数量
                                                                   设为0表示对所有进程采样 */
    unsigned int            sampleType;                     /* 采样数据类型，PerfSampleType枚举的按位或组合 */
    size_t                  needSample;                     /* 是否需要采样数据
                                                               0:仅计数不采样 1:开启数据采样 */
} PerfConfigAttr; /* struct PerfConfigAttr */

void PerfUsage(void);
void PerfDumpAttr(PerfConfigAttr *attr);
int PerfConfig(int fd, PerfConfigAttr *attr);
void PerfStart(int fd, size_t sectionId);
void PerfStop(int fd);
ssize_t PerfRead(int fd, char *buf, size_t size);
void PerfPrintBuffer(const char *buf, ssize_t num);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _PERF_H */
