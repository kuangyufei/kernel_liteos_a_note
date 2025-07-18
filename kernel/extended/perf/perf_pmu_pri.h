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

#ifndef _PERF_PMU_PRI_H
#define _PERF_PMU_PRI_H

#include "los_perf_pri.h"

#ifdef LOSCFG_HRTIMER_ENABLE
#include "linux/hrtimer.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief 硬件性能监控单元(PMU)结构体
 * @details 继承基础Pmu结构体，扩展硬件PMU特有功能，包含事件控制、计数器操作等接口
 */
typedef struct { 
    Pmu pmu;                                  // 基础PMU结构体，包含通用属性
    BOOL canDivided;                          // 是否支持计数器分频
    UINT32 cntDivided;                        // 计数器分频系数
    VOID (*enable)(Event *event);             // 事件使能回调函数
    VOID (*disable)(Event *event);            // 事件禁用回调函数
    VOID (*start)(VOID);                      // 启动PMU计数器回调
    VOID (*stop)(VOID);                       // 停止PMU计数器回调
    VOID (*clear)(VOID);                      // 清除PMU计数器回调
    VOID (*setPeriod)(Event *event);          // 设置事件周期回调函数
    UINTPTR (*readCnt)(Event *event);         // 读取计数器值回调函数
    UINT32 (*mapEvent)(UINT32 eventType, BOOL reverse); // 事件类型映射回调函数
} HwPmu; 

/**
 * @brief 软件性能监控单元(PMU)结构体
 * @details 继承基础Pmu结构体，支持跟踪事件和定时器事件两种工作模式
 */
typedef struct { 
    Pmu pmu;                                  // 基础PMU结构体，包含通用属性
    union { 
        struct { /* trace event */ 
            BOOL enable;                      // 跟踪事件使能标志
        }; 
#ifdef LOSCFG_HRTIMER_ENABLE 
        struct { /* timer event */ 
            struct hrtimer hrtimer;           // 高精度定时器实例
            union ktime time;                 // 当前时间戳
            union ktime cfgTime;              // 配置的定时器周期
        }; 
#endif 
    }; 
} SwPmu; 

/**
 * @brief 通过Pmu成员指针获取HwPmu结构体实例
 * @param item Pmu结构体指针
 * @return HwPmu结构体指针
 */
#define GET_HW_PMU(item)                    LOS_DL_LIST_ENTRY(item, HwPmu, pmu)  // 从pmu成员反向获取HwPmu实例

/**
 * @brief 定时器事件周期下限(微秒)
 * @note 最小支持100微秒周期
 */
#define TIMER_PERIOD_LOWER_BOUND_US         100  // 定时器事件最小周期(微秒)

/**
 * @brief 周期计数器满值
 * @note 32位计数器最大值0xFFFFFFFF
 */
#define CCNT_FULL                           0xFFFFFFFF  // 周期计数器最大值
#define CCNT_PERIOD_LOWER_BOUND             0x00000000  // 周期计数器下限值
#define CCNT_PERIOD_UPPER_BOUND             0xFFFFFF00  // 周期计数器上限值

/**
 * @brief 计算周期计数器实际值
 * @param p 用户设置的周期值
 * @return 转换后的计数器值(CCNT_FULL - p)
 */
#define PERIOD_CALC(p)                      (CCNT_FULL - (p))  // 周期值转换公式

/**
 * @brief 验证周期值有效性
 * @param p 用户设置的周期值
 * @return TRUE-有效，FALSE-无效
 * @note 需同时满足大于下限和小于上限
 */
#define VALID_PERIOD(p)                     ((PERIOD_CALC(p) > CCNT_PERIOD_LOWER_BOUND) \
                                            && (PERIOD_CALC(p) < CCNT_PERIOD_UPPER_BOUND))  // 周期有效性检查

/**
 * @brief 无效硬件事件类型定义
 */
#define PERF_HW_INVALID_EVENT_TYPE          0xFFFFFFFF  // 无效硬件事件类型标识

/**
 * @brief 获取数组元素数量
 * @param array 数组名
 * @return 数组元素个数
 */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))  // 计算数组长度宏

/**
 * @brief PMU中断标签宏定义(1个中断)
 * @note 展开为NUM_HAL_INTERRUPT_PMU_0,
 */
#define PMU_LABEL_INT_1                \
    NUM_HAL_INTERRUPT_PMU_0,          // PMU中断0定义
/**
 * @brief PMU中断标签宏定义(2个中断)
 * @note 展开为PMU_LABEL_INT_1 + NUM_HAL_INTERRUPT_PMU_1,
 */
#define PMU_LABEL_INT_2                \
    PMU_LABEL_INT_1                    \
    NUM_HAL_INTERRUPT_PMU_1,          // PMU中断1定义
/**
 * @brief PMU中断标签宏定义(3个中断)
 * @note 展开为PMU_LABEL_INT_2 + NUM_HAL_INTERRUPT_PMU_2,
 */
#define PMU_LABEL_INT_3                \
    PMU_LABEL_INT_2                    \
    NUM_HAL_INTERRUPT_PMU_2,          // PMU中断2定义
/**
 * @brief PMU中断标签宏定义(4个中断)
 * @note 展开为PMU_LABEL_INT_3 + NUM_HAL_INTERRUPT_PMU_3,
 */
#define PMU_LABEL_INT_4                \
    PMU_LABEL_INT_3                    \
    NUM_HAL_INTERRUPT_PMU_3,          // PMU中断3定义

/**
 * @brief PMU中断标签宏选择器
 * @param _num 中断数量(1-4)
 * @note 根据数量选择对应的PMU_LABEL_INT_xxx宏
 */
#define PMU_INT(_num)    PMU_LABEL_INT_##_num  // 中断数量宏选择器

/**
 * @brief 定义PMU中断数组
 * @param _num 中断数量
 * @param _list 数组名
 * @note 静态定义指定数量的PMU中断数组
 */
#define OS_PMU_INTS(_num, _list)      \
    STATIC UINT32 _list[_num] = {     \
        PMU_INT(_num)                 \
    }  // PMU中断数组定义宏
    
extern UINT32 OsPerfPmuRegister(Pmu *pmu);
extern VOID OsPerfPmuRm(UINT32 type);
extern Pmu *OsPerfPmuGet(UINT32 type);

extern UINT32 OsHwPmuInit(VOID);
extern UINT32 OsSwPmuInit(VOID);
extern UINT32 OsTimedPmuInit(VOID);

extern UINT32 OsGetPmuCounter0(VOID);
extern UINT32 OsGetPmuMaxCounter(VOID);
extern UINT32 OsGetPmuCycleCounter(VOID);
extern UINT32 OsPerfHwInit(HwPmu *hwPmu);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _PERF_PMU_PRI_H */
