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

#ifndef _LOS_TICK_PRI_H
#define _LOS_TICK_PRI_H

#include "los_base.h"
#include "los_tick.h"
#include "los_spinlock.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_tick
 * @brief 时钟周期到纳秒的转换比例因子
 * @core 用于将硬件时钟周期数转换为纳秒时间单位的全局缩放系数
 * @note 通常在系统初始化时根据CPU主频计算得出，计算公式：g_cycle2NsScale = 1000000000.0 / CPU_FREQ
 */
extern DOUBLE g_cycle2NsScale;

/**
* @ingroup  los_tick
* @brief 系统时钟中断处理函数
* @par Description
* 当系统时钟超时产生中断时被调用，负责处理时钟滴答事件，包括任务调度触发、定时器超时检查等核心功能
* @attention
* <ul>
* <li>此函数在中断上下文执行，应尽量缩短执行时间</li>
* <li>禁止在此函数中调用可能引起阻塞的API</li>
* </ul>
* @param 无
* @retval 无
* @par 依赖
* <ul><li>los_tick.h: 包含此API声明的头文件</li></ul>
* @see OsTickInit
*/
extern VOID OsTickHandler(VOID);

/**
 * @ingroup los_tick
 * @brief 将时钟周期数转换为纳秒
 * @param[in] cycles 输入的时钟周期数，通常来自硬件计数器
 * @return DOUBLE 转换后的纳秒数，精度取决于g_cycle2NsScale的校准
 * @par 计算公式
 * 纳秒数 = 时钟周期数 × 周期-纳秒转换因子
 * @note 结果为浮点数，如需整数表示需进行四舍五入
 */
#define CYCLE_TO_NS(cycles) ((cycles) * g_cycle2NsScale)

/**
 * @ingroup los_tick
 * @brief 定时器最大加载值
 * @core 定义32位定时器寄存器的最大计数值
 * @par 数值说明
 * 0xffffffff表示32位无符号整数的最大值，十进制为4294967295
 * @note 此宏定义主要为避免歧义，明确指出系统定时器为32位硬件实现
 */
#define TIMER_MAXLOAD 0xffffffff

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TICK_PRI_H */
