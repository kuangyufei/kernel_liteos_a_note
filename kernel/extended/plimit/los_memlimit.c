/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#ifdef LOSCFG_KERNEL_MEM_PLIMIT
#include <stdlib.h>
#include <securec.h>
#include "los_config.h"
#include "los_hook.h"
#include "los_process_pri.h"
#include "los_plimits.h"
/**
 * @brief 全局进程内存限制器实例
 * @details 用于跟踪和限制系统中进程的内存使用情况
 */
STATIC ProcMemLimiter *g_procMemLimiter = NULL;  // 全局内存限制器指针

/**
 * @brief 初始化内存限制器
 * @param[in] limite 内存限制器指针
 * @note 将限制值初始化为OS_NULL_INT，并赋值给全局变量
 */
VOID OsMemLimiterInit(UINTPTR limite)
{
    ProcMemLimiter *procMemLimiter = (ProcMemLimiter *)limite;  // 内存限制器指针
    procMemLimiter->limit = OS_NULL_INT;                       // 初始化限制值为无效值
    g_procMemLimiter = procMemLimiter;                         // 赋值给全局变量
}

/**
 * @brief 设置内存限制值
 * @param[in] limit 内存限制值（字节）
 */
VOID OsMemLimitSetLimit(UINT32 limit)
{
    g_procMemLimiter->limit = limit;  // 设置全局内存限制器的限制值
}

/**
 * @brief 分配并初始化内存限制器
 * @return 成功返回内存限制器指针，失败返回NULL
 */
VOID *OsMemLimiterAlloc(VOID)
{
    // 分配ProcMemLimiter结构体内存
    ProcMemLimiter *plimite = (ProcMemLimiter *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcMemLimiter));
    if (plimite == NULL) {                                      // 内存分配失败检查
        return NULL;
    }
    (VOID)memset_s(plimite, sizeof(ProcMemLimiter), 0, sizeof(ProcMemLimiter));  // 初始化结构体为0
    return (VOID *)plimite;
}

/**
 * @brief 释放内存限制器
 * @param[in] limite 内存限制器指针
 */
VOID OsMemLimiterFree(UINTPTR limite)
{
    ProcMemLimiter *plimite = (ProcMemLimiter *)limite;  // 内存限制器指针
    if (plimite == NULL) {                               // 空指针检查
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)limite);  // 释放内存
}

/**
 * @brief 复制内存限制器配置
 * @param[in] dest 目标内存限制器指针
 * @param[in] src 源内存限制器指针
 * @note 仅复制limit字段，不复制usage、peak和failcnt
 */
VOID OsMemLimiterCopy(UINTPTR dest, UINTPTR src)
{
    ProcMemLimiter *plimiteDest = (ProcMemLimiter *)dest;  // 目标内存限制器
    ProcMemLimiter *plimiteSrc = (ProcMemLimiter *)src;    // 源内存限制器
    plimiteDest->limit = plimiteSrc->limit;                // 复制限制值
    return;
}

/**
 * @brief 检查进程迁移时的内存限制
 * @param[in] curr 当前进程内存限制器指针
 * @param[in] parent 父进程内存限制器指针
 * @return 成功返回TRUE，失败返回FALSE
 */
BOOL MemLimiteMigrateCheck(UINTPTR curr, UINTPTR parent)
{
    ProcMemLimiter *currMemLimit = (ProcMemLimiter *)curr;   // 当前进程内存限制器
    ProcMemLimiter *parentMemLimit = (ProcMemLimiter *)parent;  // 父进程内存限制器
    // 检查合并后是否超过父进程限制
    if ((currMemLimit->usage + parentMemLimit->usage) >= parentMemLimit->limit) {
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief 迁移进程内存使用统计
 * @param[in] currLimit 当前限制器指针
 * @param[in] parentLimit 父限制器指针
 * @param[in] process 进程控制块指针
 */
VOID OsMemLimiterMigrate(UINTPTR currLimit, UINTPTR parentLimit, UINTPTR process)
{
    ProcMemLimiter *currMemLimit = (ProcMemLimiter *)currLimit;   // 当前限制器
    ProcMemLimiter *parentMemLimit = (ProcMemLimiter *)parentLimit;  // 父限制器
    LosProcessCB *pcb = (LosProcessCB *)process;                  // 进程控制块

    if (pcb == NULL) {  // 进程控制块为空时直接合并统计
        parentMemLimit->usage += currMemLimit->usage;             // 合并使用量
        parentMemLimit->failcnt += currMemLimit->failcnt;         // 合并失败计数
        if (parentMemLimit->peak < parentMemLimit->usage) {       // 更新峰值
            parentMemLimit->peak = parentMemLimit->usage;
        }
        return;
    }

    // 调整父子限制器的使用量
    parentMemLimit->usage -= pcb->limitStat.memUsed;  // 父限制器减少进程使用量
    currMemLimit->usage += pcb->limitStat.memUsed;    // 当前限制器增加进程使用量
}

/**
 * @brief 检查添加进程是否超过内存限制
 * @param[in] limit 内存限制器指针
 * @param[in] process 进程控制块指针
 * @return 成功返回TRUE，失败返回FALSE
 */
BOOL OsMemLimitAddProcessCheck(UINTPTR limit, UINTPTR process)
{
    ProcMemLimiter *memLimit = (ProcMemLimiter *)limit;  // 内存限制器
    LosProcessCB *pcb = (LosProcessCB *)process;         // 进程控制块
    // 检查添加后是否超过限制
    if ((memLimit->usage + pcb->limitStat.memUsed) > memLimit->limit) {
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief 添加进程到内存限制组
 * @param[in] limit 内存限制器指针
 * @param[in] process 进程控制块指针
 */
VOID OsMemLimitAddProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;         // 进程控制块
    ProcMemLimiter *plimits = (ProcMemLimiter *)limit;   // 内存限制器
    plimits->usage += pcb->limitStat.memUsed;            // 增加使用量
    if (plimits->peak < plimits->usage) {                // 更新峰值
        plimits->peak = plimits->usage;
    }
    return;
}

/**
 * @brief 从内存限制组移除进程
 * @param[in] limit 内存限制器指针
 * @param[in] process 进程控制块指针
 */
VOID OsMemLimitDelProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;         // 进程控制块
    ProcMemLimiter *plimits = (ProcMemLimiter *)limit;   // 内存限制器

    plimits->usage -= pcb->limitStat.memUsed;  // 减少使用量
    return;
}

/**
 * @brief 设置内存限制值
 * @param[in] memLimit 内存限制器指针
 * @param[in] value 限制值（字节）
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 不能设置根进程的内存限制，且新限制值不能小于当前使用量
 */
UINT32 OsMemLimitSetMemLimit(ProcMemLimiter *memLimit, UINT64 value)
{
    UINT32 intSave;  // 中断状态
    if ((memLimit == NULL) || (value == 0)) {  // 参数合法性检查
        return EINVAL;
    }

    if (memLimit == g_procMemLimiter) {  // 禁止修改根进程限制
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);  // 调度器上锁
    if (value < memLimit->usage) {  // 检查新限制是否小于当前使用量
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    memLimit->limit = value;  // 设置新限制值
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return LOS_OK;
}

/**
 * @brief 内存限制器上锁宏
 * @param[in] state 保存中断状态的变量
 * @param[in] locked 是否已上锁的标志
 * @note 如果调度器已上锁则直接设置标志，否则执行上锁操作
 */
#define MEM_LIMIT_LOCK(state, locked) do { \
    if (SCHEDULER_HELD()) {                \
        locked = TRUE;                     \
    } else {                               \
        SCHEDULER_LOCK(state);             \
    }                                      \
} while (0)

/**
 * @brief 内存限制器解锁宏
 * @param[in] state 中断状态变量
 * @param[in] locked 是否已上锁的标志
 * @note 只有未上锁状态才执行解锁操作
 */
#define MEM_LIMIT_UNLOCK(state, locked) do { \
    if (!locked) {                           \
        SCHEDULER_UNLOCK(state);             \
    }                                        \
} while (0)

/**
 * @brief 检查并增加内存使用量
 * @param[in] size 申请的内存大小（字节）
 * @return 成功返回LOS_OK，失败返回ENOMEM
 */
UINT32 OsMemLimitCheckAndMemAdd(UINT32 size)
{
    UINT32 intSave;  // 中断状态
    BOOL locked = FALSE;  // 上锁标志
    MEM_LIMIT_LOCK(intSave, locked);  // 内存限制器上锁
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程
    UINT32 currProcessID = run->processID;   // 当前进程ID
    if ((run == NULL) || (run->plimits == NULL)) {  // 进程或限制器不存在
        MEM_LIMIT_UNLOCK(intSave, locked);          // 解锁
        return LOS_OK;
    }

    // 获取内存限制器
    ProcMemLimiter *memLimit = (ProcMemLimiter *)run->plimits->limitsList[PROCESS_LIMITER_ID_MEM];
    // 检查是否超过限制
    if ((memLimit->usage + size) > memLimit->limit) {
        memLimit->failcnt++;  // 增加失败计数
        MEM_LIMIT_UNLOCK(intSave, locked);  // 解锁
        PRINT_ERR("plimits: process %u adjust the memory limit of Plimits group\n", currProcessID);
        return ENOMEM;  // 返回内存不足错误
    }

    memLimit->usage += size;  // 增加使用量
    run->limitStat.memUsed += size;  // 更新进程内存使用统计
    if (memLimit->peak < memLimit->usage) {  // 更新峰值
        memLimit->peak = memLimit->usage;
    }
    MEM_LIMIT_UNLOCK(intSave, locked);  // 解锁
    return LOS_OK;
}

/**
 * @brief 减少内存使用量
 * @param[in] size 释放的内存大小（字节）
 */
VOID OsMemLimitMemFree(UINT32 size)
{
    UINT32 intSave;  // 中断状态
    BOOL locked = FALSE;  // 上锁标志
    MEM_LIMIT_LOCK(intSave, locked);  // 内存限制器上锁
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程
    if ((run == NULL) || (run->plimits == NULL)) {  // 进程或限制器不存在
        MEM_LIMIT_UNLOCK(intSave, locked);          // 解锁
        return;
    }

    // 获取内存限制器
    ProcMemLimiter *memLimit = (ProcMemLimiter *)run->plimits->limitsList[PROCESS_LIMITER_ID_MEM];
    if (run->limitStat.memUsed > size) {  // 防止下溢
        run->limitStat.memUsed -= size;   // 减少进程内存使用统计
        memLimit->usage -= size;          // 减少限制器使用量
    }
    MEM_LIMIT_UNLOCK(intSave, locked);  // 解锁
}
#endif
