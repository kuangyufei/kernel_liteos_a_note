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

#ifndef _LOS_SYS_PRI_H
#define _LOS_SYS_PRI_H

#include "los_sys.h"
#include "los_base_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @ingroup los_sys
 * @brief 时间单位常量定义 - 一秒包含的毫秒数
 * @details 用于毫秒与秒之间的单位转换，固定值为1000
 */
#define OS_SYS_MS_PER_SECOND   1000  /* 1秒 = 1000毫秒 */

/**
 * @ingroup los_sys
 * @brief 时间单位常量定义 - 一秒包含的微秒数
 * @details 用于微秒与秒之间的单位转换，固定值为1,000,000
 */
#define OS_SYS_US_PER_SECOND   1000000  /* 1秒 = 1000000微秒 */

/**
 * @ingroup los_sys
 * @brief 时间单位常量定义 - 一秒包含的纳秒数
 * @details 用于纳秒与秒之间的单位转换，固定值为1,000,000,000
 */
#define OS_SYS_NS_PER_SECOND   1000000000  /* 1秒 = 1000000000纳秒 */

/**
 * @ingroup los_sys
 * @brief 时间单位常量定义 - 一毫秒包含的微秒数
 * @details 用于微秒与毫秒之间的单位转换，固定值为1000
 */
#define OS_SYS_US_PER_MS        1000  /* 1毫秒 = 1000微秒 */

/**
 * @ingroup los_sys
 * @brief 时间单位常量定义 - 一毫秒包含的纳秒数
 * @details 用于纳秒与毫秒之间的单位转换，固定值为1,000,000
 */
#define OS_SYS_NS_PER_MS        1000000  /* 1毫秒 = 1000000纳秒 */

/**
 * @ingroup los_sys
 * @brief 时间单位常量定义 - 一微秒包含的纳秒数
 * @details 用于纳秒与微秒之间的单位转换，固定值为1000
 */
#define OS_SYS_NS_PER_US        1000  /* 1微秒 = 1000纳秒 */

/**
 * @ingroup los_sys
 * @brief 系统滴答常量 - 每个滴答包含的时钟周期数
 * @details 计算公式：系统时钟频率 / 每秒滴答数
 * @note 依赖OS_SYS_CLOCK(系统时钟频率)和LOSCFG_BASE_CORE_TICK_PER_SECOND(配置的滴答频率)
 */
#define OS_CYCLE_PER_TICK       (OS_SYS_CLOCK / LOSCFG_BASE_CORE_TICK_PER_SECOND)  /* 每个滴答的周期数 = 系统时钟 / 滴答频率 */

/**
 * @ingroup los_sys
 * @brief 系统时钟常量 - 每个时钟周期包含的纳秒数
 * @details 计算公式：1秒的纳秒数 / 系统时钟频率
 * @note 表示CPU一个时钟周期的时间长度
 */
#define OS_NS_PER_CYCLE         (OS_SYS_NS_PER_SECOND / OS_SYS_CLOCK)  /* 每个周期的纳秒数 = 1e9纳秒 / 系统时钟频率 */

/**
 * @ingroup los_sys
 * @brief 系统滴答常量 - 每个滴答包含的微秒数
 * @details 计算公式：1秒的微秒数 / 每秒滴答数
 * @note 表示一个系统滴答的时间长度(微秒级)
 */
#define OS_US_PER_TICK          (OS_SYS_US_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)  /* 每个滴答的微秒数 = 1e6微秒 / 滴答频率 */

/**
 * @ingroup los_sys
 * @brief 系统滴答常量 - 每个滴答包含的纳秒数
 * @details 计算公式：1秒的纳秒数 / 每秒滴答数
 * @note 表示一个系统滴答的时间长度(纳秒级)
 */
#define OS_NS_PER_TICK          (OS_SYS_NS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)  /* 每个滴答的纳秒数 = 1e9纳秒 / 滴答频率 */

/**
 * @ingroup los_sys
 * @brief 时间转换宏 - 微秒转时钟周期
 * @param[in] time 输入时间(微秒)
 * @param[in] freq 时钟频率(Hz)
 * @return 转换后的时钟周期数
 * @details 计算公式：(time / 1e6) * freq + (time % 1e6) * freq / 1e6
 * @note 分为整数秒部分和剩余微秒部分分别计算后相加
 */
#define OS_US_TO_CYCLE(time, freq)  ((((time) / OS_SYS_US_PER_SECOND) * (freq)) + \
    (((time) % OS_SYS_US_PER_SECOND) * (freq) / OS_SYS_US_PER_SECOND))  /* 微秒转周期：整数秒部分*频率 + 剩余微秒*频率/1e6 */

/**
 * @ingroup los_sys
 * @brief 时间转换宏 - 微秒转系统时钟周期
 * @param[in] time 输入时间(微秒)
 * @return 转换后的系统时钟周期数
 * @details 调用OS_US_TO_CYCLE宏，使用系统时钟频率OS_SYS_CLOCK
 */
#define OS_SYS_US_TO_CYCLE(time) OS_US_TO_CYCLE((time), OS_SYS_CLOCK)  /* 微秒转系统周期，使用系统时钟频率 */

/**
 * @ingroup los_sys
 * @brief 时间转换宏 - 时钟周期转微秒
 * @param[in] cycle 输入时钟周期数
 * @param[in] freq 时钟频率(Hz)
 * @return 转换后的微秒数
 * @details 计算公式：(cycle / freq) * 1e6 + (cycle % freq) * 1e6 / freq
 * @note 分为完整秒部分和剩余周期部分分别计算后相加
 */
#define OS_CYCLE_TO_US(cycle, freq) ((((cycle) / (freq)) * OS_SYS_US_PER_SECOND) + \
    ((cycle) % (freq) * OS_SYS_US_PER_SECOND / (freq)))  /* 周期转微秒：完整秒部分*1e6微秒 + 剩余周期*1e6微秒/频率 */

/**
 * @ingroup los_sys
 * @brief 时间转换宏 - 系统时钟周期转微秒
 * @param[in] cycle 输入系统时钟周期数
 * @return 转换后的微秒数
 * @details 调用OS_CYCLE_TO_US宏，使用系统时钟频率OS_SYS_CLOCK
 */
#define OS_SYS_CYCLE_TO_US(cycle) OS_CYCLE_TO_US((cycle), OS_SYS_CLOCK)  /* 系统周期转微秒，使用系统时钟频率 */

/**
 * @ingroup los_sys
 * @brief 系统常量 - 应用版本名称最大长度
 * @details 定义应用版本名称字符串的最大长度，包含终止符
 */
#define OS_SYS_APPVER_NAME_MAX 64  /* 应用版本名称最大长度(含终止符) */

/**
 * @ingroup los_sys
 * @brief 系统常量 - 魔术字
 * @details 用于内存校验或数据结构有效性验证的魔术数字
 * @note 固定值为0xAAAAAAAA，通常用于检测内存越界或非法访问
 */
#define OS_SYS_MAGIC_WORD      0xAAAAAAAA  /* 系统魔术字，用于内存校验 */

/**
 * @ingroup los_sys
 * @brief 系统常量 - 栈空间初始化值
 * @details 栈内存空间的初始填充值，用于栈使用情况分析和调试
 * @note 固定值为0xCACACACA，可通过内存扫描检测栈溢出
 */
#define OS_SYS_EMPTY_STACK     0xCACACACA  /* 栈空间初始化填充值 */

/**
 * @ingroup los_sys
 * @brief 时间转换函数 - 将纳秒转换为系统滴答数
 * @param[in] nanoseconds 输入时间(纳秒)，范围：0 ~ UINT64_MAX
 * @return UINT32 转换后的滴答数
 * @details 内部实现使用OS_NS_PER_TICK常量进行转换，向上取整
 * @note 当输入值超过UINT32_MAX所能表示的最大滴答数时，返回UINT32_MAX
 */
extern UINT32 OsNS2Tick(UINT64 nanoseconds);  /* 纳秒转滴答数核心函数 */

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SYS_PRI_H */
