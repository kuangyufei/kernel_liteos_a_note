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

#ifdef LOSCFG_KERNEL_PLIMITS
#include "los_config.h"
#include "los_process_pri.h"
#include "los_process.h"
#include "los_plimits.h"
#include "los_task_pri.h"
#include "los_processlimit.h"
/**
 * @brief   根进程ID限制器全局实例
 * @details 指向系统根进程的PidLimit结构体，用于管理系统级进程数量和优先级限制
 */
STATIC PidLimit *g_rootPidLimit = NULL;  // 根进程ID限制器指针

/**
 * @brief   初始化进程ID限制器
 * @param   limit [IN] 指向PidLimit结构体的指针
 * @note    将系统最大进程数和默认优先级上限赋值给限制器，并设置为根限制器
 */
VOID PidLimiterInit(UINTPTR limit)
{
    PidLimit *pidLimit = (PidLimit *)limit;  // 类型转换：将无符号整数指针转为PidLimit指针
    if (pidLimit == NULL) {                  // 空指针检查
        return;
    }

    // 设置进程数量上限为系统最大进程数
    pidLimit->pidLimit = LOS_GetSystemProcessMaximum();
    // 设置用户进程最高优先级上限
    pidLimit->priorityLimit = OS_USER_PROCESS_PRIORITY_HIGHEST;
    g_rootPidLimit = pidLimit;               // 赋值为根进程限制器
}

/**
 * @brief   分配并初始化进程ID限制器内存
 * @return  成功返回指向PidLimit的指针，失败返回NULL
 * @note    使用系统内存池1(m_aucSysMem1)分配内存，并初始化为全0
 */
VOID *PidLimiterAlloc(VOID)
{
    // 从系统内存池分配PidLimit大小的内存块
    PidLimit *plimite = (PidLimit *)LOS_MemAlloc(m_aucSysMem1, sizeof(PidLimit));
    if (plimite == NULL) {                   // 内存分配失败检查
        return NULL;
    }
    // 将分配的内存块初始化为全0，防止野值
    (VOID)memset_s(plimite, sizeof(PidLimit), 0, sizeof(PidLimit));
    return (VOID *)plimite;                  // 返回分配的内存指针
}

/**
 * @brief   释放进程ID限制器内存
 * @param   limit [IN] 指向要释放的PidLimit结构体的指针
 * @note    释放前进行空指针检查，使用系统内存池接口释放
 * @attention 函数名存在拼写错误：应为PidLimiterFree
 */
VOID PidLimterFree(UINTPTR limit)
{
    PidLimit *pidLimit = (PidLimit *)limit;  // 类型转换：将无符号整数指针转为PidLimit指针
    if (pidLimit == NULL) {                  // 空指针检查，避免无效释放
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)limit);  // 释放系统内存池1中的内存块
}

/**
 * @brief   检查进程迁移时的ID限制
 * @param   curr   [IN] 当前进程的PidLimit指针
 * @param   parent [IN] 父进程的PidLimit指针
 * @return  TRUE表示允许迁移，FALSE表示不允许
 * @note    迁移条件：当前进程数+父进程数 < 父进程ID限制
 */
BOOL PidLimitMigrateCheck(UINTPTR curr, UINTPTR parent)
{
    PidLimit *currPidLimit = (PidLimit *)curr;    // 当前进程限制器指针转换
    PidLimit *parentPidLimit = (PidLimit *)parent;  // 父进程限制器指针转换
    if ((currPidLimit == NULL) || (parentPidLimit == NULL)) {  // 空指针检查
        return FALSE;
    }

    // 检查合并后的进程数是否超过父进程限制
    if ((currPidLimit->pidCount + parentPidLimit->pidCount) >= parentPidLimit->pidLimit) {
        return FALSE;                           // 超过限制，不允许迁移
    }
    return TRUE;                                // 允许迁移
}

/**
 * @brief   检查是否允许添加新进程
 * @param   limit   [IN] 指向PidLimit结构体的指针
 * @param   process [IN] 进程控制块指针(未使用)
 * @return  TRUE表示允许添加，FALSE表示已达上限
 * @note    仅检查进程数量是否小于限制值
 */
BOOL OsPidLimitAddProcessCheck(UINTPTR limit, UINTPTR process)
{
    (VOID)process;                             // 未使用参数，避免编译警告
    PidLimit *pidLimit = (PidLimit *)limit;    // 限制器指针转换
    if (pidLimit->pidCount >= pidLimit->pidLimit) {  // 检查是否已达进程数量上限
        return FALSE;                           // 已达上限，不允许添加
    }
    return TRUE;                                // 允许添加
}

/**
 * @brief   增加进程计数器
 * @param   limit   [IN] 指向PidLimit结构体的指针
 * @param   process [IN] 进程控制块指针(未使用)
 * @note    成功创建新进程后调用，递增当前进程数量
 */
VOID OsPidLimitAddProcess(UINTPTR limit, UINTPTR process)
{
    (VOID)process;                             // 未使用参数，避免编译警告
    PidLimit *plimits = (PidLimit *)limit;     // 限制器指针转换

    plimits->pidCount++;                       // 进程计数器递增
    return;
}

/**
 * @brief   减少进程计数器
 * @param   limit   [IN] 指向PidLimit结构体的指针
 * @param   process [IN] 进程控制块指针(未使用)
 * @note    进程退出后调用，递减当前进程数量
 */
VOID OsPidLimitDelProcess(UINTPTR limit, UINTPTR process)
{
    (VOID)process;                             // 未使用参数，避免编译警告
    PidLimit *plimits = (PidLimit *)limit;     // 限制器指针转换

    plimits->pidCount--;                       // 进程计数器递减
    return;
}

/**
 * @brief   复制父进程的ID限制配置到子进程
 * @param   curr   [OUT] 子进程的PidLimit指针
 * @param   parent [IN]  父进程的PidLimit指针
 * @note    子进程继承父进程的限制值，但重置进程计数器为0
 */
VOID PidLimiterCopy(UINTPTR curr, UINTPTR parent)
{
    PidLimit *currPidLimit = (PidLimit *)curr;    // 子进程限制器指针转换
    PidLimit *parentPidLimit = (PidLimit *)parent;  // 父进程限制器指针转换
    if ((currPidLimit == NULL) || (parentPidLimit == NULL)) {  // 空指针检查
        return;
    }

    currPidLimit->pidLimit = parentPidLimit->pidLimit;        // 继承进程数量限制
    currPidLimit->priorityLimit = parentPidLimit->priorityLimit;  // 继承优先级限制
    currPidLimit->pidCount = 0;                               // 重置进程计数器
    return;
}

/**
 * @brief   设置进程数量上限
 * @param   pidLimit [IN] 指向PidLimit结构体的指针
 * @param   pidMax   [IN] 新的进程数量上限值
 * @return  LOS_OK表示成功，EINVAL表示参数无效，EPERM表示无权限
 * @note    根进程限制器(g_rootPidLimit)不允许修改，且新上限不能小于当前进程数
 */
UINT32 PidLimitSetPidLimit(PidLimit *pidLimit, UINT32 pidMax)
{
    UINT32 intSave;                            // 中断状态保存变量

    // 参数检查：限制器为空或新上限超过系统最大值
    if ((pidLimit == NULL) || (pidMax > LOS_GetSystemProcessMaximum())) {
        return EINVAL;                         // 参数无效
    }

    if (pidLimit == g_rootPidLimit) {          // 检查是否为根进程限制器
        return EPERM;                          // 无权限修改根限制器
    }

    SCHEDULER_LOCK(intSave);                   // 关调度器，保证操作原子性
    if (pidLimit->pidCount >= pidMax) {        // 检查当前进程数是否超过新上限
        SCHEDULER_UNLOCK(intSave);             // 开调度器
        return EPERM;                          // 不允许设置更小的上限
    }

    pidLimit->pidLimit = pidMax;               // 更新进程数量上限
    SCHEDULER_UNLOCK(intSave);                 // 开调度器
    return LOS_OK;                             // 操作成功
}

/**
 * @brief   设置进程优先级上限
 * @param   pidLimit [IN] 指向PidLimit结构体的指针
 * @param   priority [IN] 新的优先级上限值
 * @return  LOS_OK表示成功，EINVAL表示参数无效，EPERM表示无权限
 * @note    优先级范围：OS_USER_PROCESS_PRIORITY_HIGHEST ~ OS_PROCESS_PRIORITY_LOWEST
 */
UINT32 PidLimitSetPriorityLimit(PidLimit *pidLimit, UINT32 priority)
{
    UINT32 intSave;                            // 中断状态保存变量

    // 参数检查：限制器为空或优先级超出有效范围
    if ((pidLimit == NULL) ||
        (priority < OS_USER_PROCESS_PRIORITY_HIGHEST) ||
        (priority > OS_PROCESS_PRIORITY_LOWEST)) {
        return EINVAL;                         // 参数无效
    }

    if (pidLimit == g_rootPidLimit) {          // 检查是否为根进程限制器
        return EPERM;                          // 返回无权限
    }

    SCHEDULER_LOCK(intSave);                   // 关调度器，保证操作原子性
    pidLimit->priorityLimit = priority;        // 更新优先级上限
    SCHEDULER_UNLOCK(intSave);                 // 开调度器
    return LOS_OK;                             // 操作成功
}

/**
 * @brief   获取当前进程的优先级上限
 * @return  当前进程的优先级上限值
 * @note    通过当前进程控制块获取对应的PidLimit配置
 */
UINT16 OsPidLimitGetPriorityLimit(VOID)
{
    UINT32 intSave;                            // 中断状态保存变量
    SCHEDULER_LOCK(intSave);                   // 关调度器，防止进程切换影响
    LosProcessCB *run = OsCurrProcessGet();    // 获取当前运行进程控制块
    // 从进程限制器列表中获取PID限制器(索引PROCESS_LIMITER_ID_PIDS)
    PidLimit *pidLimit = (PidLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_PIDS];
    UINT16 priority = pidLimit->priorityLimit; // 获取优先级上限值
    SCHEDULER_UNLOCK(intSave);                 // 开调度器
    return priority;                           // 返回优先级上限
}
#endif
