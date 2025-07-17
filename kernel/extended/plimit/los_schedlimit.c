/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
#include "los_schedlimit.h"
#include "los_process_pri.h"

/**
 * @brief   全局进程调度限制器实例指针
 * @details 指向系统中当前活动的进程调度限制器，用于跟踪和控制进程CPU时间配额
 */
STATIC ProcSchedLimiter *g_procSchedLimit = NULL;  // 进程调度限制器全局实例

/**
 * @brief   初始化进程调度限制器
 * @param   limit [IN] 指向ProcSchedLimiter结构体的指针
 * @note    将调度周期和配额初始化为0，并设置为全局当前限制器
 */
VOID OsSchedLimitInit(UINTPTR limit)
{
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)limit;  // 类型转换：将无符号整数指针转为调度限制器指针
    schedLimit->period = 0;                                    // 初始化调度周期为0（无限制）
    schedLimit->quota = 0;                                     // 初始化CPU时间配额为0（无限制）
    g_procSchedLimit = schedLimit;                             // 设置为当前活动的调度限制器
}

/**
 * @brief   分配并初始化进程调度限制器内存
 * @return  成功返回指向ProcSchedLimiter的指针，失败返回NULL
 * @note    使用系统内存池1(m_aucSysMem1)分配内存，并初始化为全0
 */
VOID *OsSchedLimitAlloc(VOID)
{
    // 从系统内存池分配ProcSchedLimiter大小的内存块
    ProcSchedLimiter *plimit = (ProcSchedLimiter *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcSchedLimiter));
    if (plimit == NULL) {                                      // 内存分配失败检查
        return NULL;
    }
    // 将分配的内存块初始化为全0，防止野值
    (VOID)memset_s(plimit, sizeof(ProcSchedLimiter), 0, sizeof(ProcSchedLimiter));
    return (VOID *)plimit;                                     // 返回分配的内存指针
}

/**
 * @brief   释放进程调度限制器内存
 * @param   limit [IN] 指向要释放的ProcSchedLimiter结构体的指针
 * @note    释放前进行空指针检查，使用系统内存池接口释放
 */
VOID OsSchedLimitFree(UINTPTR limit)
{
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)limit;  // 类型转换：将无符号整数指针转为调度限制器指针
    if (schedLimit == NULL) {                                  // 空指针检查，避免无效释放
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)limit);            // 释放系统内存池1中的内存块
}

/**
 * @brief   复制进程调度限制器配置
 * @param   dest [OUT] 目标ProcSchedLimiter结构体指针
 * @param   src  [IN]  源ProcSchedLimiter结构体指针
 * @note    仅复制调度周期和配额参数，不包含运行时统计信息
 */
VOID OsSchedLimitCopy(UINTPTR dest, UINTPTR src)
{
    ProcSchedLimiter *plimitDest = (ProcSchedLimiter *)dest;   // 目标限制器指针转换
    ProcSchedLimiter *plimitSrc = (ProcSchedLimiter *)src;     // 源限制器指针转换
    plimitDest->period = plimitSrc->period;                    // 复制调度周期配置
    plimitDest->quota = plimitSrc->quota;                      // 复制CPU时间配额配置
    return;
}

/**
 * @brief   更新进程调度运行时间统计
 * @param   runTask  [IN] 指向当前运行任务控制块的指针
 * @param   currTime [IN] 当前系统时间戳(微秒)
 * @param   incTime  [IN] 本次增加的运行时间(微秒)
 * @note    根据调度周期和配额自动重置运行时间，实现CPU时间限制
 */
VOID OsSchedLimitUpdateRuntime(LosTaskCB *runTask, UINT64 currTime, INT32 incTime)
{
    LosProcessCB *run = (LosProcessCB *)runTask->processCB;    // 获取任务所属进程控制块
    if ((run == NULL) || (run->plimits == NULL)) {             // 进程或限制器未初始化检查
        return;
    }

    // 获取进程调度限制器实例（从限制器列表中索引PROCESS_LIMITER_ID_SCHED）
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)run->plimits->limitsList[PROCESS_LIMITER_ID_SCHED];

    run->limitStat.allRuntime += incTime;                      // 更新进程累计运行时间
    if ((schedLimit->period <= 0) || (schedLimit->quota <= 0)) { // 调度周期或配额未设置（0表示无限制）
        return;
    }

    // 在周期内且未超过配额，累加运行时间
    if ((schedLimit->startTime <= currTime) && (schedLimit->allRuntime < schedLimit->quota)) {
        schedLimit->allRuntime += incTime;                     // 更新限制器内的运行时间统计
        return;
    }

    // 周期已结束，重置周期开始时间和运行时间
    if (schedLimit->startTime <= currTime) {
        schedLimit->startTime = currTime + schedLimit->period; // 设置新周期开始时间（当前时间+周期长度）
        schedLimit->allRuntime = 0;                            // 重置周期内运行时间统计
    }
}

/**
 * @brief   检查进程是否允许继续调度
 * @param   task [IN] 指向任务控制块的指针
 * @return  TRUE表示允许调度，FALSE表示已超出时间配额
 * @note    基于当前时间和周期开始时间判断是否在允许调度窗口内
 */
BOOL OsSchedLimitCheckTime(LosTaskCB *task)
{
    UINT64 currTime = OsGetCurrSchedTimeCycle();               // 获取当前调度时间戳(微秒)
    LosProcessCB *processCB = (LosProcessCB *)task->processCB; // 获取任务所属进程控制块
    if ((processCB == NULL) || (processCB->plimits == NULL)) { // 进程或限制器未初始化检查
        return TRUE;                                           // 无限制时允许调度
    }
    // 获取进程调度限制器实例
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)processCB->plimits->limitsList[PROCESS_LIMITER_ID_SCHED];
    if (schedLimit->startTime >= currTime) {                   // 周期开始时间未到（处于限制期）
        return FALSE;                                          // 不允许调度
    }
    return TRUE;                                               // 允许调度
}

/**
 * @brief   设置进程调度周期
 * @param   schedLimit [IN] 指向ProcSchedLimiter结构体的指针
 * @param   value      [IN] 周期值(微秒)
 * @return  LOS_OK表示成功，EINVAL表示参数无效，EPERM表示无权限
 * @note    全局限制器(g_procSchedLimit)不允许修改周期
 */
UINT32 OsSchedLimitSetPeriod(ProcSchedLimiter *schedLimit, UINT64 value)
{
    UINT32 intSave;                                            // 中断状态保存变量
    if (schedLimit == NULL) {                                  // 限制器指针空检查
        return EINVAL;                                         // 参数无效
    }

    if (schedLimit == g_procSchedLimit) {                      // 检查是否为全局限制器
        return EPERM;                                          // 无权限修改全局限制器
    }

    SCHEDULER_LOCK(intSave);                                   // 关调度器，保证操作原子性
    schedLimit->period = value;                                // 设置新的调度周期
    SCHEDULER_UNLOCK(intSave);                                 // 开调度器
    return LOS_OK;                                             // 操作成功
}

/**
 * @brief   设置进程CPU时间配额
 * @param   schedLimit [IN] 指向ProcSchedLimiter结构体的指针
 * @param   value      [IN] 配额值(微秒)，必须小于等于周期值
 * @return  LOS_OK表示成功，EINVAL表示参数无效，EPERM表示无权限
 * @note    配额必须小于等于周期，否则返回参数无效
 */
UINT32 OsSchedLimitSetQuota(ProcSchedLimiter *schedLimit, UINT64 value)
{
    UINT32 intSave;                                            // 中断状态保存变量
    if (schedLimit == NULL) {                                  // 限制器指针空检查
        return EINVAL;                                         // 参数无效
    }

    if (schedLimit == g_procSchedLimit) {                      // 检查是否为全局限制器
        return EPERM;                                          // 无权限修改全局限制器
    }

    SCHEDULER_LOCK(intSave);                                   // 关调度器，保证操作原子性
    if (value > (UINT64)schedLimit->period) {                  // 配额必须小于等于周期
        SCHEDULER_UNLOCK(intSave);                             // 开调度器
        return EINVAL;                                         // 参数无效
    }

    schedLimit->quota = value;                                 // 设置新的CPU时间配额
    SCHEDULER_UNLOCK(intSave);                                 // 开调度器
    return LOS_OK;                                             // 操作成功
}
#endif
