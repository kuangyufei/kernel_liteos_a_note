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

#ifndef _LOS_SWTMR_PRI_H
#define _LOS_SWTMR_PRI_H

#include "los_swtmr.h"
#include "los_spinlock.h"
#include "los_sched_pri.h"

#ifdef LOSCFG_SECURITY_VID
#include "vid_api.h"
#else
#define MAX_INVALID_TIMER_VID OS_SWTMR_MAX_TIMERID
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_swtmr_pri
 * @brief 软件定时器状态枚举
 * @core 定义软件定时器的生命周期状态，用于状态机管理
 */
enum SwtmrState {
    OS_SWTMR_STATUS_UNUSED,     /**< 未使用状态：定时器资源未被分配 */
    OS_SWTMR_STATUS_CREATED,    /**< 已创建状态：定时器已初始化但未启动 */
    OS_SWTMR_STATUS_TICKING     /**< 计时中状态：定时器正在运行且计时中 */
};

/**
 * @ingroup los_swtmr_pri
 * @brief 软件定时器超时回调函数结构体
 * @core 封装定时器超时处理的回调函数及其参数，用于定时器事件分发
 */
typedef struct {
    SWTMR_PROC_FUNC handler;    /**< 超时回调函数：定时器到期时执行的处理函数 */
    UINTPTR arg;                /**< 回调参数：传递给超时处理函数的实参，支持指针或整数类型 */
    LOS_DL_LIST node;           /**< 双向链表节点：用于将回调函数挂载到定时器事件链表 */
#ifdef LOSCFG_SWTMR_DEBUG
    UINT32 swtmrID;             /**< 定时器ID：调试模式下记录所属定时器ID，用于问题定位 */
#endif
} SwtmrHandlerItem;

/**
 * @ingroup los_swtmr_pri
 * @brief 软件定时器回调函数结构体指针类型
 * @note 用于函数参数传递或链表操作时的指针声明
 */
typedef SwtmrHandlerItem *SwtmrHandlerItemPtr;

/**
 * @brief 软件定时器控制块数组指针
 * @note 指向系统中所有软件定时器控制块的首地址，全局唯一
 */
extern SWTMR_CTRL_S *g_swtmrCBArray;

/**
 * @brief 通过定时器ID获取对应的控制块指针
 * @param[in] swtmrID 软件定时器ID，由系统统一分配
 * @return SWTMR_CTRL_S* 对应的定时器控制块指针
 * @par 实现原理：
 * 计算公式：控制块地址 = 数组首地址 + (定时器ID % 最大定时器数量)
 * 其中 LOSCFG_BASE_CORE_SWTMR_LIMIT 为系统配置的最大定时器数量
 * @warning 仅在定时器ID有效时返回正确指针，无效ID可能导致越界访问
 */
#define OS_SWT_FROM_SID(swtmrID) ((SWTMR_CTRL_S *)g_swtmrCBArray + ((swtmrID) % LOSCFG_BASE_CORE_SWTMR_LIMIT))

/**
 * @ingroup los_swtmr_pri
 * @brief Scan a software timer.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to scan a software timer when a Tick interrupt occurs and determine whether
 * the software timer expires.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param  None.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_swtmr_pri.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SwtmrStop
 */

extern UINT32 OsSwtmrGetNextTimeout(VOID);
extern BOOL OsIsSwtmrTask(const LosTaskCB *taskCB);
extern VOID OsSwtmrResponseTimeReset(UINT64 startTime);
extern UINT32 OsSwtmrInit(VOID);
extern VOID OsSwtmrRecycle(UINTPTR ownerID);
extern BOOL OsSwtmrWorkQueueFind(SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg);
extern SPIN_LOCK_S g_swtmrSpin;
extern UINT32 OsSwtmrTaskIDGetByCpuid(UINT16 cpuid);

/**
 * @brief 软件定时器调试功能条件编译块
 * @core 当配置宏 LOSCFG_SWTMR_DEBUG 启用时，编译以下调试统计结构体
 * @note 用于记录定时器运行时的详细性能指标，辅助问题定位和性能优化
 */
#ifdef LOSCFG_SWTMR_DEBUG
/**
 * @brief 软件定时器调试基础统计结构体
 * @core 记录定时器的核心时间指标和事件计数，构成调试数据的基础
 */
typedef struct {
    UINT64 startTime;           /**< 定时器启动时间戳，单位：系统时钟周期 */
    UINT64 waitTimeMax;         /**< 最大等待时间，单位：系统时钟周期 */
    UINT64 waitTime;            /**< 累计等待时间，单位：系统时钟周期 */
    UINT64 waitCount;           /**< 等待事件计数，记录定时器进入等待状态的次数 */
    UINT64 readyStartTime;      /**< 就绪状态开始时间戳，单位：系统时钟周期 */
    UINT64 readyTime;           /**< 累计就绪时间，单位：系统时钟周期 */
    UINT64 readyTimeMax;        /**< 最大就绪时间，单位：系统时钟周期 */
    UINT64 runTime;             /**< 累计运行时间，单位：系统时钟周期 */
    UINT64 runTimeMax;          /**< 最大运行时间，单位：系统时钟周期 */
    UINT64 runCount;            /**< 运行事件计数，记录定时器回调函数执行次数 */
    UINT32 times;               /**< 定时器触发总次数，包含周期触发的累计次数 */
} SwtmrDebugBase;

/**
 * @brief 软件定时器调试扩展数据结构体
 * @core 在基础统计信息上增加定时器属性，提供完整调试视图
 * @note 继承 SwtmrDebugBase 结构体，形成调试数据的层级结构
 */
typedef struct {
    SwtmrDebugBase  base;       /**< 基础统计信息，包含各类时间和计数指标 */
    SWTMR_PROC_FUNC handler;    /**< 定时器回调函数指针，指向超时处理逻辑 */
    UINT32          period;     /**< 定时器周期值，单位：系统滴答数 (tick) */
    UINT32          cpuid;      /**< CPU核心ID，记录定时器运行的核心编号 */
    BOOL            swtmrUsed;  /**< 定时器使用状态标记：TRUE-正在使用，FALSE-已释放 */
} SwtmrDebugData;

extern BOOL OsSwtmrDebugDataUsed(UINT32 swtmrID);
extern UINT32 OsSwtmrDebugDataGet(UINT32 swtmrID, SwtmrDebugData *data, UINT32 len, UINT8 *mode);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SWTMR_PRI_H */
