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

/**
 * @defgroup los_perf Perf
 * @ingroup kernel
 */

#ifndef _LOS_PERF_H
#define _LOS_PERF_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_perf
 * 性能监控最大样本过滤任务数
 */
#define PERF_MAX_FILTER_TSKS                32  // 最大样本过滤任务数，允许列表容量

/**
 * @ingroup los_perf
 * 性能监控最大事件计数器数量
 */
#define PERF_MAX_EVENT                      7   // 最大事件计数器数量，支持同时监控的事件类型上限

/**
 * @ingroup los_perf
 * 性能监控最大回溯调用链深度
 */
#define PERF_MAX_CALLCHAIN_DEPTH            10  // 最大调用链回溯深度，用于函数调用栈分析

/**
 * @ingroup los_perf
 * 性能采样数据缓冲区水印比例（1/N）
 */
#define PERF_BUFFER_WATERMARK_ONE_N         2   // 缓冲区水印比例分母，用于触发数据通知机制

/**
 * @ingroup los_perf
 * 性能监控模块状态枚举
 */
enum PerfStatus {
    PERF_UNINIT,   // 性能监控未初始化状态
    PERF_STARTED,  // 性能监控已启动状态
    PERF_STOPPED,  // 性能监控已停止状态
};

/**
 * @ingroup los_perf
 * 性能采样数据缓冲区水印通知钩子函数类型定义
 * 当缓冲区数据达到水印阈值时触发
 */
typedef VOID (*PERF_BUF_NOTIFY_HOOK)(VOID);

/**
 * @ingroup los_perf
 * 性能采样数据缓冲区刷新钩子函数类型定义
 * 用于自定义数据刷新策略
 * @param addr 数据缓冲区地址
 * @param size 数据大小（字节）
 */
typedef VOID (*PERF_BUF_FLUSH_HOOK)(VOID *addr, UINT32 size);

/**
 * @ingroup los_perf
 * 性能监控错误码：无效状态
 *
 * 值：0x02002000
 *
 * 解决方案：遵循性能监控模块状态机流程操作
 */
#define LOS_ERRNO_PERF_STATUS_INVALID        LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x00)  // 状态无效错误码

/**
 * @ingroup los_perf
 * 性能监控错误码：硬件PMU初始化失败
 *
 * 值：0x02002001
 *
 * 解决方案：检查PMU硬件中断配置
 */
#define LOS_ERRNO_PERF_HW_INIT_ERROR         LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x01)  // 硬件PMU初始化错误码

/**
 * @ingroup los_perf
 * 性能监控错误码：高精度定时器初始化失败
 *
 * 值：0x02002002
 *
 * 解决方案：检查高精度定时器驱动初始化
 */
#define LOS_ERRNO_PERF_TIMED_INIT_ERROR      LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x02)  // 定时器初始化错误码

/**
 * @ingroup los_perf
 * 性能监控错误码：软件PMU初始化失败
 *
 * 值：0x02002003
 *
 * 解决方案：检查软件事件跟踪模块初始化
 */
#define LOS_ERRNO_PERF_SW_INIT_ERROR         LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x03)  // 软件PMU初始化错误码

/**
 * @ingroup los_perf
 * 性能监控错误码：缓冲区初始化失败
 *
 * 值：0x02002004
 *
 * 解决方案：检查缓冲区初始化大小参数
 */
#define LOS_ERRNO_PERF_BUF_ERROR             LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x04)  // 缓冲区错误码

/**
 * @ingroup los_perf
 * 性能监控错误码：无效PMU类型
 *
 * 值：0x02002005
 *
 * 解决方案：检查menuconfig中对应PMU模块是否启用
 */
#define LOS_ERRNO_PERF_INVALID_PMU           LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x05)  // PMU类型无效错误码

/**
 * @ingroup los_perf
 * 性能监控错误码：PMU配置错误
 *
 * 值：0x02002006
 *
 * 解决方案：检查事件ID和周期参数配置
 */
#define LOS_ERRNO_PERF_PMU_CONFIG_ERROR      LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x06)  // PMU配置错误码

/**
 * @ingroup los_perf
 * 性能监控错误码：配置属性为空
 *
 * 值：0x02002007
 *
 * 解决方案：检查输入参数是否为空指针
 */
#define LOS_ERRNO_PERF_CONFIG_NULL           LOS_ERRNO_OS_ERROR(LOS_MOD_PERF, 0x07)  // 配置属性空指针错误码

/**
 * @ingroup los_perf
 * 性能事件类型枚举
 */
enum PerfEventType {
    PERF_EVENT_TYPE_HW,      // 通用硬件事件类型
    PERF_EVENT_TYPE_TIMED,   // 高精度定时器事件类型
    PERF_EVENT_TYPE_SW,      // 软件跟踪事件类型
    PERF_EVENT_TYPE_RAW,     // 板级专用硬件事件类型，详见对应架构头文件中的PmuEventType枚举

    PERF_EVENT_TYPE_MAX      // 事件类型数量上限
};

/**
 * @ingroup los_perf
 * 通用硬件PMU事件枚举
 */
enum PmuHwId {
    PERF_COUNT_HW_CPU_CYCLES = 0,      // CPU周期事件
    PERF_COUNT_HW_INSTRUCTIONS,        // 指令执行事件
    PERF_COUNT_HW_DCACHE_REFERENCES,   // 数据缓存访问事件
    PERF_COUNT_HW_DCACHE_MISSES,       // 数据缓存未命中事件
    PERF_COUNT_HW_ICACHE_REFERENCES,   // 指令缓存访问事件
    PERF_COUNT_HW_ICACHE_MISSES,       // 指令缓存未命中事件
    PERF_COUNT_HW_BRANCH_INSTRUCTIONS, // 分支指令事件
    PERF_COUNT_HW_BRANCH_MISSES,       // 分支预测失败事件

    PERF_COUNT_HW_MAX,                  // 硬件事件数量上限
};

/**
 * @ingroup los_perf
 * 高精度定时器事件枚举
 */
enum PmuTimedId {
    PERF_COUNT_CPU_CLOCK = 0,      // CPU时钟定时器事件
};

/**
 * @ingroup los_perf
 * 软件PMU事件枚举
 */
enum PmuSwId {
    PERF_COUNT_SW_TASK_SWITCH = 1, // 任务切换事件
    PERF_COUNT_SW_IRQ_RESPONSE,    // 中断响应事件
    PERF_COUNT_SW_MEM_ALLOC,       // 内存分配事件
    PERF_COUNT_SW_MUX_PEND,        // 互斥锁等待事件

    PERF_COUNT_SW_MAX,              // 软件事件数量上限
};

/**
 * @ingroup los_perf
 * 性能采样数据类型枚举
 * 通过PerfConfigAttr->sampleType配置采样类型
 */
enum PerfSampleType {
    PERF_RECORD_CPU       = 1U << 0, // 记录当前CPU ID
    PERF_RECORD_TID       = 1U << 1, // 记录当前任务ID
    PERF_RECORD_TYPE      = 1U << 2, // 记录事件类型
    PERF_RECORD_PERIOD    = 1U << 3, // 记录事件周期
    PERF_RECORD_TIMESTAMP = 1U << 4, // 记录时间戳
    PERF_RECORD_IP        = 1U << 5, // 记录指令指针（当前PC值）
    PERF_RECORD_CALLCHAIN = 1U << 6, // 记录调用链回溯
    PERF_RECORD_PID       = 1U << 7, // 记录当前进程ID
};

/**
 * @ingroup los_perf
 * 性能事件配置结构体
 * 用于配置特定事件的属性信息
 */
typedef struct {
    UINT32 type;              // 事件类型，取值为PerfEventType枚举
    struct {
        UINT32 eventId;       // 特定事件ID，与事件类型对应
        UINT32 period;        // 事件周期，每发生period次事件记录一次样本
    } events[PERF_MAX_EVENT]; // 性能事件列表，最多支持PERF_MAX_EVENT个事件
    UINT32 eventsNr;          // 实际配置的事件数量
    BOOL predivided;          // 是否启用预分频（每64次计数一次），仅对CPU周期硬件事件有效
} PerfEventConfig;

/**
 * @ingroup los_perf
 * 性能监控主配置结构体
 * 用于设置性能采样的整体属性，包括事件、任务过滤等信息
 */
typedef struct {
    PerfEventConfig         eventsCfg;                      // 性能事件配置
    UINT32                  taskIds[PERF_MAX_FILTER_TSKS];  // 任务过滤列表（允许列表）
    UINT32                  taskIdsNr;                      // 任务过滤列表数量，0表示监控所有任务
    UINT32                  processIds[PERF_MAX_FILTER_TSKS];  // 进程过滤列表（允许列表）
    UINT32                  processIdsNr;                      // 进程过滤列表数量，0表示监控所有进程
    UINT32                  sampleType;                     // 采样数据类型，取值为PerfSampleType枚举的位掩码组合
    BOOL                    needSample;                     // 是否需要采样数据
} PerfConfigAttr;
/**
 * @ingroup los_perf
 * @brief 初始化性能监控模块
 *
 * @par 功能描述
 * <ul>
 * <li>用于初始化性能监控模块，包括初始化PMU（性能监控单元）、分配内存等操作，在系统初始化阶段调用</li>
 * </ul>
 * @attention
 * <ul>
 * <li>如果buf不为NULL，用户必须确保size不大于缓冲区实际长度</li>
 * </ul>
 *
 * @param  buf     [IN] 采样数据缓冲区指针；若为NULL则使用动态分配的内存
 * @param  size    [IN] 采样数据缓冲区大小（字节）
 *
 * @retval #LOS_ERRNO_PERF_STATUS_INVALID      性能监控模块状态错误
 * @retval #LOS_ERRNO_PERF_HW_INIT_ERROR       硬件PMU初始化失败
 * @retval #LOS_ERRNO_PERF_TIMED_INIT_ERROR    定时器事件初始化失败
 * @retval #LOS_ERRNO_PERF_SW_INIT_ERROR       软件事件初始化失败
 * @retval #LOS_ERRNO_PERF_BUF_ERROR           缓冲区初始化失败
 * @retval #LOS_OK                             初始化成功
 * @par 依赖
 * <ul>
 * <li>los_perf.h: 包含本接口声明的头文件</li>
 * </ul>
 */
UINT32 LOS_PerfInit(VOID *buf, UINT32 size);  // 初始化性能监控模块

/**
 * @ingroup los_perf
 * @brief 启动性能采样
 *
 * @par 功能描述
 * 启动性能数据采样过程，开始记录指定事件
 * @attention
 * 无
 *
 * @param  sectionId          [IN] 数据段标识ID，用于标记缓冲区中该段采样数据
 * @retval 无
 * @par 依赖
 * <ul>
 * <li>los_perf.h: 包含本接口声明的头文件</li>
 * </ul>
 */
VOID LOS_PerfStart(UINT32 sectionId);  // 启动性能采样

/**
 * @ingroup los_perf
 * @brief 停止性能采样
 *
 * @par 功能描述
 * 停止性能数据采样过程，暂停记录事件
 * @attention
 * 无
 *
 * @param  无
 *
 * @retval 无
 * @par 依赖
 * <ul>
 * <li>los_perf.h: 包含本接口声明的头文件</li>
 * </ul>
 */
VOID LOS_PerfStop(VOID);  // 停止性能采样

/**
 * @ingroup los_perf
 * @brief 配置性能监控参数
 *
 * @par 功能描述
 * 在采样前配置性能监控参数，如采样事件、采样任务等。本接口需在LOS_PerfStart前调用
 * @attention
 * 无
 *
 * @param  attr                      [IN] 性能事件属性结构体指针
 *
 * @retval #LOS_ERRNO_PERF_STATUS_INVALID          性能监控模块状态错误
 * @retval #LOS_ERRNO_PERF_CONFIG_NULL             属性结构体指针为空
 * @retval #LOS_ERRNO_PERF_INVALID_PMU             PMU类型配置错误
 * @retval #LOS_ERRNO_PERF_PMU_CONFIG_ERROR        事件ID或周期配置无效
 * @retval #LOS_OK                                 配置成功
 * @par 依赖
 * <ul>
 * <li>los_perf.h: 包含本接口声明的头文件</li>
 * </ul>
 */
UINT32 LOS_PerfConfig(PerfConfigAttr *attr);  // 配置性能监控参数

/**
 * @ingroup los_perf
 * @brief 从性能采样缓冲区读取数据
 *
 * @par 功能描述
 * 从性能采样数据缓冲区读取采样结果，由于缓冲区采用环形结构（ringbuffer），读取后数据可能被覆盖
 * @attention
 * 无
 *
 * @param  dest                      [IN] 目标缓冲区地址
 * @param  size                      [IN] 读取数据大小（字节）
 * @retval #UINT32                   实际读取的字节数
 * @par 依赖
 * <ul>
 * <li>los_perf.h: 包含本接口声明的头文件</li>
 * </ul>
 */
UINT32 LOS_PerfDataRead(CHAR *dest, UINT32 size);  // 读取采样数据

/**
 * @ingroup los_perf
 * @brief 注册性能采样缓冲区水印通知钩子函数
 *
 * @par 功能描述
 * <ul>
 * <li>注册性能采样数据缓冲区的水印通知钩子函数</li>
 * <li>当缓冲区数据量达到水印阈值时，注册的钩子函数将被调用</li>
 * </ul>
 * @attention
 * 无
 *
 * @param  func                      [IN] 缓冲区水印通知钩子函数
 *
 * @retval 无
 * @par 依赖
 * <ul>
 * <li>los_perf.h: 包含本接口声明的头文件</li>
 * </ul>
 */
VOID LOS_PerfNotifyHookReg(const PERF_BUF_NOTIFY_HOOK func);  // 注册水印通知钩子

/**
 * @ingroup los_perf
 * @brief 注册性能采样缓冲区刷新钩子函数
 *
 * @par 功能描述
 * <ul>
 * <li>注册性能采样数据缓冲区的刷新钩子函数</li>
 * <li>当缓冲区进行读写操作时，刷新钩子函数将被调用</li>
 * </ul>
 * @attention
 * 无
 *
 * @param  func                      [IN] 缓冲区刷新钩子函数
 *
 * @retval 无
 * @par 依赖
 * <ul>
 * <li>los_perf.h: 包含本接口声明的头文件</li>
 * </ul>
 */
VOID LOS_PerfFlushHookReg(const PERF_BUF_FLUSH_HOOK func);  // 注册刷新钩子

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_PERF_H */
