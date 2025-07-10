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

#ifndef _TIME_PRI_H
#define _TIME_PRI_H

#include "time.h"
#include "errno.h"
#include "los_sys_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief 信号事件结构体，用于描述信号通知的相关信息
 */
struct ksigevent {
    union sigval sigev_value;  /* 信号值，用于传递附加数据 */
    int sigev_signo;           /* 信号编号，指定要发送的信号 */
    int sigev_notify;          /* 通知方式，指定信号的通知类型 */
    int sigev_tid;             /* 线程ID，用于指定接收信号的线程（特定实现） */
};

/* internal functions */
/**
 * @brief 验证timespec结构体的有效性
 * @param[in] tp 指向timespec结构体的指针，包含秒和纳秒
 * @return BOOL - 验证结果
 *         TRUE: 有效（非空指针且纳秒在[0, 1e9)范围内且秒数非负）
 *         FALSE: 无效（空指针或纳秒/秒数值非法）
 */
STATIC INLINE BOOL ValidTimeSpec(const struct timespec *tp)
{
    /* 检查空指针 */
    if (tp == NULL) {
        return FALSE;
    }

    /* 验证纳秒范围（0 ≤ tv_nsec < 1e9）和秒数非负 */
    if ((tp->tv_nsec < 0) || (tp->tv_nsec >= OS_SYS_NS_PER_SECOND) || (tp->tv_sec < 0)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief 将timespec时间转换为系统滴答数（tick）
 * @param[in] tp 指向timespec结构体的指针，包含秒和纳秒（需先通过ValidTimeSpec验证）
 * @return UINT32 - 转换后的滴答数，超过LOS_WAIT_FOREVER时返回LOS_WAIT_FOREVER
 * @note 转换公式：tick = (ns * TICK_PER_SEC + (NS_PER_SEC - 1)) / NS_PER_SEC（向上取整）
 *       其中NS_PER_SEC = 1e9（OS_SYS_NS_PER_SECOND），TICK_PER_SEC为系统配置的滴答频率
 */
STATIC INLINE UINT32 OsTimeSpec2Tick(const struct timespec *tp)
{
    UINT64 tick, ns;

    /* 计算总纳秒数：秒数×1e9 + 纳秒数 */
    ns = (UINT64)tp->tv_sec * OS_SYS_NS_PER_SECOND + tp->tv_nsec;
    /* 纳秒转滴答数（向上取整），避免精度损失 */
    tick = (ns * LOSCFG_BASE_CORE_TICK_PER_SECOND + (OS_SYS_NS_PER_SECOND - 1)) / OS_SYS_NS_PER_SECOND;
    /* 滴答数上限检查，超过时返回LOS_WAIT_FOREVER（通常为0xFFFFFFFF） */
    if (tick > LOS_WAIT_FOREVER) {
        tick = LOS_WAIT_FOREVER;
    }
    return (UINT32)tick;
}

/**
 * @brief 将系统滴答数（tick）转换为timespec时间
 * @param[out] tp 指向timespec结构体的指针，用于存储转换结果（秒和纳秒）
 * @param[in] tick 系统滴答数（需为非负值）
 * @note 转换公式：ns = tick * (1e9 / TICK_PER_SEC)，其中1e9为OS_SYS_NS_PER_SECOND
 */
STATIC INLINE VOID OsTick2TimeSpec(struct timespec *tp, UINT32 tick)
{
    /* 计算总纳秒数：滴答数 × (1e9 / 滴答频率) */
    UINT64 ns = ((UINT64)tick * OS_SYS_NS_PER_SECOND) / LOSCFG_BASE_CORE_TICK_PER_SECOND;
    tp->tv_sec = (time_t)(ns / OS_SYS_NS_PER_SECOND);       /* 秒数 = 总纳秒数 / 1e9 */
    tp->tv_nsec = (long)(ns % OS_SYS_NS_PER_SECOND);        /* 纳秒数 = 总纳秒数 % 1e9 */
}
int OsTimerCreate(clockid_t, struct ksigevent *__restrict, timer_t *__restrict);
void OsAdjTime(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* _TIME_PRI_H */