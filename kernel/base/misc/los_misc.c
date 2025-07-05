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

#include "los_task_pri.h"


/**
 * @brief 将地址按指定边界对齐
 * @param addr [IN] 需要对齐的原始地址
 * @param boundary [IN] 对齐边界（必须是2的幂）
 * @return UINTPTR - 对齐后的地址
 * @note 若对齐计算发生溢出，则返回按边界向下对齐的原始地址
 */
LITE_OS_SEC_TEXT UINTPTR LOS_Align(UINTPTR addr, UINT32 boundary)
{
    // 检查对齐计算是否会溢出（addr + boundary - 1 是否超过UINTPTR最大值）
    if ((addr + boundary - 1) > addr) {
        // 无溢出：向上取整到最近的边界对齐地址
        return (addr + boundary - 1) & ~((UINTPTR)(boundary - 1));
    } else {
        // 有溢出：向下取整到最近的边界对齐地址
        return addr & ~((UINTPTR)(boundary - 1));
    }
}

/**
 * @brief 使当前任务睡眠指定的毫秒数
 * @param msecs [IN] 睡眠时长（毫秒），0表示不睡眠
 * @note 实际睡眠时长会转换为系统滴答数，受系统时钟频率影响
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_Msleep(UINT32 msecs)
{
    UINT32 interval;  // 转换后的睡眠滴答数

    if (msecs == 0) {  // 若睡眠时长为0，直接设置间隔为0
        interval = 0;
    } else {
        // 将毫秒转换为系统滴答数
        interval = LOS_MS2Tick(msecs);
        // 若转换后滴答数为0（小于一个滴答周期），强制设置为1个滴答
        if (interval == 0) {
            interval = 1;
        }
    }

    (VOID)LOS_TaskDelay(interval);  // 调用任务延迟函数，忽略返回值
}

