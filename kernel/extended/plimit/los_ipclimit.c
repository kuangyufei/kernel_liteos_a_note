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

#ifdef LOSCFG_KERNEL_IPC_PLIMIT
#include "los_ipclimit.h"
#include "los_process_pri.h"
/**
 * @brief 根进程IPC资源限制器实例
 * @details 用于跟踪和限制系统中根进程的IPC资源使用情况
 */
STATIC ProcIPCLimit *g_rootIPCLimit = NULL;  // 根进程IPC限制器指针

/**
 * @brief 共享内存限制最大值
 * @note 十进制值为4294967295字节(4GB)，表示共享内存的最大限制阈值
 */
#define PLIMIT_IPC_SHM_LIMIT_MAX 0xFFFFFFFF  // 共享内存限制最大值

/**
 * @brief 初始化IPC资源限制器
 * @param[in] limite IPC限制器指针
 * @note 将消息队列限制初始化为配置值，共享内存限制初始化为最大值，并赋值给根限制器
 */
VOID OsIPCLimitInit(UINTPTR limite)
{
    ProcIPCLimit *plimite = (ProcIPCLimit *)limite;  // IPC限制器指针
    // 初始化消息队列限制为配置值
    plimite->mqCountLimit = LOSCFG_BASE_IPC_QUEUE_LIMIT;
    plimite->shmSizeLimit = PLIMIT_IPC_SHM_LIMIT_MAX;  // 初始化共享内存限制
    g_rootIPCLimit = plimite;                          // 赋值给根限制器
}

/**
 * @brief 分配并初始化IPC限制器
 * @return 成功返回IPC限制器指针，失败返回NULL
 */
VOID *OsIPCLimitAlloc(VOID)
{
    // 分配ProcIPCLimit结构体内存
    ProcIPCLimit *plimite = (ProcIPCLimit *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcIPCLimit));
    if (plimite == NULL) {                                      // 内存分配失败检查
        return NULL;
    }
    (VOID)memset_s(plimite, sizeof(ProcIPCLimit), 0, sizeof(ProcIPCLimit));  // 初始化结构体为0
    return (VOID *)plimite;
}

/**
 * @brief 释放IPC限制器
 * @param[in] limite IPC限制器指针
 */
VOID OsIPCLimitFree(UINTPTR limite)
{
    ProcIPCLimit *plimite = (ProcIPCLimit *)limite;  // IPC限制器指针
    if (plimite == NULL) {                           // 空指针检查
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)plimite);  // 释放内存
}

/**
 * @brief 复制IPC限制器配置
 * @param[in] dest 目标IPC限制器指针
 * @param[in] src 源IPC限制器指针
 * @note 仅复制限制阈值，不复制使用量和失败计数
 */
VOID OsIPCLimitCopy(UINTPTR dest, UINTPTR src)
{
    ProcIPCLimit *plimiteDest = (ProcIPCLimit *)dest;  // 目标IPC限制器
    ProcIPCLimit *plimiteSrc = (ProcIPCLimit *)src;    // 源IPC限制器
    plimiteDest->mqCountLimit = plimiteSrc->mqCountLimit;  // 复制消息队列限制
    plimiteDest->shmSizeLimit = plimiteSrc->shmSizeLimit;  // 复制共享内存限制
    return;
}

/**
 * @brief 检查进程迁移时的IPC资源限制
 * @param[in] curr 当前进程IPC限制器指针
 * @param[in] parent 父进程IPC限制器指针
 * @return 成功返回TRUE，失败返回FALSE
 * @note 同时检查消息队列数量和共享内存大小是否超过父进程限制
 */
BOOL OsIPCLimiteMigrateCheck(UINTPTR curr, UINTPTR parent)
{
    ProcIPCLimit *currIpcLimit = (ProcIPCLimit *)curr;   // 当前进程IPC限制器
    ProcIPCLimit *parentIpcLimit = (ProcIPCLimit *)parent;  // 父进程IPC限制器
    // 检查消息队列数量是否超过限制
    if ((currIpcLimit->mqCount + parentIpcLimit->mqCount) >= parentIpcLimit->mqCountLimit) {
        return FALSE;
    }

    // 检查共享内存大小是否超过限制
    if ((currIpcLimit->shmSize + parentIpcLimit->shmSize) >= parentIpcLimit->shmSizeLimit) {
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief 迁移进程IPC资源使用统计
 * @param[in] currLimit 当前限制器指针
 * @param[in] parentLimit 父限制器指针
 * @param[in] process 进程控制块指针
 * @note 进程控制块为空时合并所有统计数据，否则调整父子限制器的使用量
 */
VOID OsIPCLimitMigrate(UINTPTR currLimit, UINTPTR parentLimit, UINTPTR process)
{
    ProcIPCLimit *currIpcLimit = (ProcIPCLimit *)currLimit;   // 当前限制器
    ProcIPCLimit *parentIpcLimit = (ProcIPCLimit *)parentLimit;  // 父限制器
    LosProcessCB *pcb = (LosProcessCB *)process;                  // 进程控制块

    if (pcb == NULL) {  // 进程控制块为空时直接合并统计
        parentIpcLimit->mqCount += currIpcLimit->mqCount;         // 合并消息队列数量
        parentIpcLimit->mqFailedCount += currIpcLimit->mqFailedCount;  // 合并消息队列失败计数
        parentIpcLimit->shmSize += currIpcLimit->shmSize;         // 合并共享内存大小
        parentIpcLimit->shmFailedCount += currIpcLimit->shmFailedCount;  // 合并共享内存失败计数
        return;
    }

    // 调整父子限制器的使用量
    parentIpcLimit->mqCount -= pcb->limitStat.mqCount;   // 父限制器减少消息队列数量
    parentIpcLimit->shmSize -= pcb->limitStat.shmSize;   // 父限制器减少共享内存大小
    currIpcLimit->mqCount += pcb->limitStat.mqCount;     // 当前限制器增加消息队列数量
    currIpcLimit->shmSize += pcb->limitStat.shmSize;     // 当前限制器增加共享内存大小
}

/**
 * @brief 检查添加进程是否超过IPC资源限制
 * @param[in] limit IPC限制器指针
 * @param[in] process 进程控制块指针
 * @return 成功返回TRUE，失败返回FALSE
 * @note 同时检查消息队列和共享内存是否超过限制
 */
BOOL OsIPCLimitAddProcessCheck(UINTPTR limit, UINTPTR process)
{
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)limit;  // IPC限制器
    LosProcessCB *pcb = (LosProcessCB *)process;     // 进程控制块
    // 检查消息队列数量是否超过限制
    if ((ipcLimit->mqCount + pcb->limitStat.mqCount) >= ipcLimit->mqCountLimit) {
        return FALSE;
    }

    // 检查共享内存大小是否超过限制
    if ((ipcLimit->shmSize + pcb->limitStat.shmSize) >= ipcLimit->shmSizeLimit) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief 添加进程到IPC限制组
 * @param[in] limit IPC限制器指针
 * @param[in] process 进程控制块指针
 */
VOID OsIPCLimitAddProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;     // 进程控制块
    ProcIPCLimit *plimits = (ProcIPCLimit *)limit;   // IPC限制器
    plimits->mqCount += pcb->limitStat.mqCount;      // 增加消息队列数量
    plimits->shmSize += pcb->limitStat.shmSize;      // 增加共享内存大小
    return;
}

/**
 * @brief 从IPC限制组移除进程
 * @param[in] limit IPC限制器指针
 * @param[in] process 进程控制块指针
 */
VOID OsIPCLimitDelProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;     // 进程控制块
    ProcIPCLimit *plimits = (ProcIPCLimit *)limit;   // IPC限制器

    plimits->mqCount -= pcb->limitStat.mqCount;  // 减少消息队列数量
    plimits->shmSize -= pcb->limitStat.shmSize;  // 减少共享内存大小
    return;
}

/**
 * @brief 设置消息队列限制值
 * @param[in] ipcLimit IPC限制器指针
 * @param[in] value 限制值（个）
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 不能设置根进程的限制，且新限制值不能小于当前使用量或大于配置最大值
 */
UINT32 OsIPCLimitSetMqLimit(ProcIPCLimit *ipcLimit, UINT32 value)
{
    UINT32 intSave;  // 中断状态

    // 参数合法性检查：空指针、值为0或超过配置最大值
    if ((ipcLimit == NULL) || (value == 0) || (value > LOSCFG_BASE_IPC_QUEUE_LIMIT)) {
        return EINVAL;
    }

    if (ipcLimit == g_rootIPCLimit) {  // 禁止修改根进程限制
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);  // 调度器上锁
    if (value < ipcLimit->mqCount) {  // 检查新限制是否小于当前使用量
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    ipcLimit->mqCountLimit = value;  // 设置新限制值
    SCHEDULER_UNLOCK(intSave);       // 调度器解锁
    return LOS_OK;
}

/**
 * @brief 设置共享内存限制值
 * @param[in] ipcLimit IPC限制器指针
 * @param[in] value 限制值（字节）
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 不能设置根进程的限制，且新限制值不能小于当前使用量或大于最大限制值
 */
UINT32 OsIPCLimitSetShmLimit(ProcIPCLimit *ipcLimit, UINT32 value)
{
    UINT32 intSave;  // 中断状态

    // 参数合法性检查：空指针、值为0或超过最大限制值
    if ((ipcLimit == NULL) || (value == 0) || (value > PLIMIT_IPC_SHM_LIMIT_MAX)) {
        return EINVAL;
    }

    if (ipcLimit == g_rootIPCLimit) {  // 禁止修改根进程限制
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);  // 调度器上锁
    if (value < ipcLimit->shmSize) {  // 检查新限制是否小于当前使用量
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    ipcLimit->shmSizeLimit = value;  // 设置新限制值
    SCHEDULER_UNLOCK(intSave);       // 调度器解锁
    return LOS_OK;
}

/**
 * @brief 分配消息队列并检查限制
 * @return 成功返回LOS_OK，失败返回EINVAL
 */
UINT32 OsIPCLimitMqAlloc(VOID)
{
    UINT32 intSave;  // 中断状态
    SCHEDULER_LOCK(intSave);  // 调度器上锁
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程
    // 获取当前进程的IPC限制器
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    if (ipcLimit->mqCount >= ipcLimit->mqCountLimit) {  // 检查是否超过限制
        ipcLimit->mqFailedCount++;                      // 增加失败计数
        SCHEDULER_UNLOCK(intSave);                      // 调度器解锁
        return EINVAL;
    }

    run->limitStat.mqCount++;  // 更新进程消息队列统计
    ipcLimit->mqCount++;       // 更新限制器消息队列数量
    SCHEDULER_UNLOCK(intSave); // 调度器解锁
    return LOS_OK;
}

/**
 * @brief 释放消息队列并更新统计
 */
VOID OsIPCLimitMqFree(VOID)
{
    UINT32 intSave;  // 中断状态
    SCHEDULER_LOCK(intSave);  // 调度器上锁
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程
    // 获取当前进程的IPC限制器
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    ipcLimit->mqCount--;       // 减少限制器消息队列数量
    run->limitStat.mqCount--;  // 更新进程消息队列统计
    SCHEDULER_UNLOCK(intSave); // 调度器解锁
    return;
}

/**
 * @brief 分配共享内存并检查限制
 * @param[in] size 申请的共享内存大小（字节）
 * @return 成功返回LOS_OK，失败返回EINVAL
 */
UINT32 OsIPCLimitShmAlloc(UINT32 size)
{
    UINT32 intSave;  // 中断状态
    SCHEDULER_LOCK(intSave);  // 调度器上锁
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程
    // 获取当前进程的IPC限制器
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    // 检查是否超过共享内存限制
    if ((ipcLimit->shmSize + size) >= ipcLimit->shmSizeLimit) {
        ipcLimit->shmFailedCount++;                     // 增加失败计数
        SCHEDULER_UNLOCK(intSave);                      // 调度器解锁
        return EINVAL;
    }

    run->limitStat.shmSize += size;  // 更新进程共享内存统计
    ipcLimit->shmSize += size;       // 更新限制器共享内存大小
    SCHEDULER_UNLOCK(intSave);       // 调度器解锁
    return LOS_OK;
}

/**
 * @brief 释放共享内存并更新统计
 * @param[in] size 释放的共享内存大小（字节）
 */
VOID OsIPCLimitShmFree(UINT32 size)
{
    UINT32 intSave;  // 中断状态
    SCHEDULER_LOCK(intSave);  // 调度器上锁
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程
    // 获取当前进程的IPC限制器
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    ipcLimit->shmSize -= size;       // 减少限制器共享内存大小
    run->limitStat.shmSize -= size;  // 更新进程共享内存统计
    SCHEDULER_UNLOCK(intSave);       // 调度器解锁
    return;
}

#endif
