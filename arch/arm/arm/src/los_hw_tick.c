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

#include "los_sys_pri.h"
#include "los_hwi.h"

/**
 * @brief 系统滴答定时器初始化
 * @param systemClock 系统时钟频率(Hz)
 * @param tickPerSecond 每秒滴答数
 * @return UINT32 初始化结果，LOS_OK表示成功，其他值表示失败
 * @details 检查系统时钟和滴答数有效性，调用硬件时钟初始化函数
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsTickInit(UINT32 systemClock, UINT32 tickPerSecond)
{
    // 参数合法性检查：系统时钟为0、滴答数为0或滴答数超过系统时钟均视为无效配置
    if ((systemClock == 0) ||
        (tickPerSecond == 0) ||
        (tickPerSecond > systemClock)) {
        return LOS_ERRNO_TICK_CFG_INVALID;  // 返回滴答配置无效错误码
    }
    HalClockInit();  // 调用硬件抽象层时钟初始化函数

    return LOS_OK;  // 返回初始化成功
}

/**
 * @brief 启动系统滴答定时器
 * @details 调用硬件抽象层时钟启动函数，开始滴答计时
 */
LITE_OS_SEC_TEXT_INIT VOID OsTickStart(VOID)
{
    HalClockStart();  // 调用硬件抽象层时钟启动函数
}

/**
 * @brief 获取CPU周期计数
 * @param highCnt 输出参数，周期计数高32位
 * @param lowCnt 输出参数，周期计数低32位
 * @details 将64位CPU周期计数拆分为高低32位存储到输出参数中
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_GetCpuCycle(UINT32 *highCnt, UINT32 *lowCnt)
{
    UINT64 cycle = HalClockGetCycles();  // 获取64位CPU周期计数

    *highCnt = cycle >> 32; /* 32: offset 32 bits and retain high bits */  // 右移32位获取高32位
    *lowCnt = cycle & 0xFFFFFFFFU;  // 与掩码0xFFFFFFFFU获取低32位
}

/**
 * @brief 获取当前系统纳秒时间
 * @return UINT64 当前纳秒时间值
 * @details 根据CPU周期计数和系统时钟频率计算当前纳秒时间
 */
LITE_OS_SEC_TEXT_MINOR UINT64 LOS_CurrNanosec(VOID)
{
    UINT64 cycle = HalClockGetCycles();  // 获取CPU周期计数
    // 计算纳秒时间：(周期数/系统时钟)*1e9 + (周期数%系统时钟)*1e9/系统时钟
    return (cycle / g_sysClock) * OS_SYS_NS_PER_SECOND + (cycle % g_sysClock) * OS_SYS_NS_PER_SECOND / g_sysClock;
}

/**
 * @brief 微秒级延迟函数
 * @param usecs 延迟微秒数
 * @details 调用硬件抽象层微秒延迟函数实现精确延迟
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_Udelay(UINT32 usecs)
{
    HalDelayUs(usecs);  // 调用硬件抽象层微秒延迟函数
}

/**
 * @brief 毫秒级延迟函数
 * @param msecs 延迟毫秒数
 * @details 将毫秒转换为微秒后调用硬件抽象层延迟函数
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_Mdelay(UINT32 msecs)
{
    HalDelayUs(msecs * 1000); /* 1000 : 1ms = 1000us */  // 毫秒转微秒（1毫秒=1000微秒）
}