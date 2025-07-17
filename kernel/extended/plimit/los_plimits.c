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
 
/*
面对进程越来越多，应用环境越来越复杂的状况，需要对容器做限制，若不做限制，会发生资源浪费、争夺等。
容器配额plimits（Process Limits）是内核提供的一种可以限制单个进程或者多个进程所使用资源的机制，
可以对cpu，内存等资源实现精细化控制。plimits的接口通过plimitsfs的伪文件系统提供。
通过操作文件对进程及进程资源进行分组管理，通过配置plimits组内限制器Plimiter限制进程组的memory、sched等资源的使用。
*/

#ifdef LOSCFG_KERNEL_PLIMITS
#include "los_base.h"
#include "los_process_pri.h"
#include "hal_timer.h"
#include "los_plimits.h"
/**
 * @brief 进程限制器操作接口结构体
 * @details 定义各类限制器的通用操作函数指针，实现不同资源限制的统一管理接口
 */
typedef struct PlimiteOperations {
    VOID (*LimiterInit)(UINTPTR);                     /* 限制器初始化函数 */
    VOID *(*LimiterAlloc)(VOID);                     /* 限制器内存分配函数 */
    VOID (*LimiterFree)(UINTPTR);                    /* 限制器内存释放函数 */
    VOID (*LimiterCopy)(UINTPTR, UINTPTR);           /* 限制器数据复制函数 */
    BOOL (*LimiterAddProcessCheck)(UINTPTR, UINTPTR);/* 进程添加前检查函数 */
    VOID (*LimiterAddProcess)(UINTPTR, UINTPTR);     /* 进程添加函数 */
    VOID (*LimiterDelProcess)(UINTPTR, UINTPTR);     /* 进程移除函数 */
    BOOL (*LimiterMigrateCheck)(UINTPTR, UINTPTR);   /* 进程迁移前检查函数 */
    VOID (*LimiterMigrate)(UINTPTR, UINTPTR, UINTPTR);/* 进程迁移执行函数 */
} PlimiteOperations;

/**
 * @brief 全局限制器操作数组
 * @details 按ProcLimiterID索引，存储各类限制器的具体实现，根据配置动态启用不同限制器
 */
static PlimiteOperations g_limiteOps[PROCESS_LIMITER_COUNT] = {
    [PROCESS_LIMITER_ID_PIDS] = {                     /* PID数量限制器操作集 */
        .LimiterInit = PidLimiterInit,                /* PID限制器初始化 */
        .LimiterAlloc = PidLimiterAlloc,              /* PID限制器内存分配 */
        .LimiterFree = PidLimterFree,                 /* PID限制器内存释放 */
        .LimiterCopy = PidLimiterCopy,                /* PID限制器数据复制 */
        .LimiterAddProcessCheck = OsPidLimitAddProcessCheck, /* 添加进程前PID检查 */
        .LimiterAddProcess = OsPidLimitAddProcess,    /* 添加进程到PID限制器 */
        .LimiterDelProcess = OsPidLimitDelProcess,    /* 从PID限制器移除进程 */
        .LimiterMigrateCheck = PidLimitMigrateCheck,  /* 进程迁移PID检查 */
        .LimiterMigrate = NULL,                       /* PID限制器不支持迁移 */
    },
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    [PROCESS_LIMITER_ID_MEM] = {                      /* 内存限制器操作集（使能内存限制时生效） */
        .LimiterInit = OsMemLimiterInit,              /* 内存限制器初始化 */
        .LimiterAlloc = OsMemLimiterAlloc,            /* 内存限制器内存分配 */
        .LimiterFree = OsMemLimiterFree,              /* 内存限制器内存释放 */
        .LimiterCopy = OsMemLimiterCopy,              /* 内存限制器数据复制 */
        .LimiterAddProcessCheck = OsMemLimitAddProcessCheck, /* 添加进程前内存检查 */
        .LimiterAddProcess = OsMemLimitAddProcess,    /* 添加进程到内存限制器 */
        .LimiterDelProcess = OsMemLimitDelProcess,    /* 从内存限制器移除进程 */
        .LimiterMigrateCheck = MemLimiteMigrateCheck, /* 进程迁移内存检查 */
        .LimiterMigrate = OsMemLimiterMigrate,        /* 执行进程内存限制迁移 */
    },
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    [PROCESS_LIMITER_ID_SCHED] = {                    /* 调度限制器操作集（使能调度限制时生效） */
        .LimiterInit = OsSchedLimitInit,              /* 调度限制器初始化 */
        .LimiterAlloc = OsSchedLimitAlloc,            /* 调度限制器内存分配 */
        .LimiterFree = OsSchedLimitFree,              /* 调度限制器内存释放 */
        .LimiterCopy = OsSchedLimitCopy,              /* 调度限制器数据复制 */
        .LimiterAddProcessCheck = NULL,               /* 调度限制不检查添加 */
        .LimiterAddProcess = NULL,                    /* 调度限制不处理添加 */
        .LimiterDelProcess = NULL,                    /* 调度限制不处理移除 */
        .LimiterMigrateCheck = NULL,                  /* 调度限制不检查迁移 */
        .LimiterMigrate = NULL,                       /* 调度限制不支持迁移 */
    },
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
    [PROCESS_LIMITER_ID_DEV] = {                      /* 设备限制器操作集（使能设备限制时生效） */
        .LimiterInit = OsDevLimitInit,                /* 设备限制器初始化 */
        .LimiterAlloc = OsDevLimitAlloc,              /* 设备限制器内存分配 */
        .LimiterFree = OsDevLimitFree,                /* 设备限制器内存释放 */
        .LimiterCopy = OsDevLimitCopy,                /* 设备限制器数据复制 */
        .LimiterAddProcessCheck = NULL,               /* 设备限制不检查添加 */
        .LimiterAddProcess = NULL,                    /* 设备限制不处理添加 */
        .LimiterDelProcess = NULL,                    /* 设备限制不处理移除 */
        .LimiterMigrateCheck = NULL,                  /* 设备限制不检查迁移 */
        .LimiterMigrate = NULL,                       /* 设备限制不支持迁移 */
    },
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    [PROCESS_LIMITER_ID_IPC] = {                      /* IPC限制器操作集（使能IPC限制时生效） */
        .LimiterInit = OsIPCLimitInit,                /* IPC限制器初始化 */
        .LimiterAlloc = OsIPCLimitAlloc,              /* IPC限制器内存分配 */
        .LimiterFree = OsIPCLimitFree,                /* IPC限制器内存释放 */
        .LimiterCopy = OsIPCLimitCopy,                /* IPC限制器数据复制 */
        .LimiterAddProcessCheck = OsIPCLimitAddProcessCheck, /* 添加进程前IPC检查 */
        .LimiterAddProcess = OsIPCLimitAddProcess,    /* 添加进程到IPC限制器 */
        .LimiterDelProcess = OsIPCLimitDelProcess,    /* 从IPC限制器移除进程 */
        .LimiterMigrateCheck = OsIPCLimiteMigrateCheck, /* 进程迁移IPC检查 */
        .LimiterMigrate = OsIPCLimitMigrate,          /* 执行进程IPC限制迁移 */
    },
#endif
};

/**
 * @brief 根进程限制器集合
 * @note 系统中所有进程限制器的根节点，构成限制器层级结构的顶层
 */
STATIC ProcLimiterSet *g_rootPLimite = NULL;

/**
 * @brief 获取根进程限制器集合
 * @return 根进程限制器集合指针
 */
ProcLimiterSet *OsRootPLimitsGet(VOID)
{
    return g_rootPLimite;  /* 返回全局根限制器指针 */
}

/**
 * @brief 初始化进程限制器集合
 * @details 创建并初始化根限制器集合，为各类限制器分配资源并初始化
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsProcLimiterSetInit(VOID)
{
    /* 分配根限制器集合内存（从系统内存池1分配） */
    g_rootPLimite = (ProcLimiterSet *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcLimiterSet));
    if (g_rootPLimite == NULL) {                       /* 内存分配失败检查 */
        return ENOMEM;                                 /* 返回内存不足错误码 */
    }
    (VOID)memset_s(g_rootPLimite, sizeof(ProcLimiterSet), 0, sizeof(ProcLimiterSet)); /* 初始化内存为0 */

    LOS_ListInit(&g_rootPLimite->childList);            /* 初始化子限制器链表 */
    LOS_ListInit(&g_rootPLimite->processList);          /* 初始化进程链表 */

    ProcLimiterSet *procLimiterSet = g_rootPLimite;     /* 根限制器集合指针 */
    /* 遍历所有限制器类型，初始化各类限制器 */
    for (INT32 plimiteID = 0; plimiteID < PROCESS_LIMITER_COUNT; ++plimiteID) {
        /* 为当前限制器分配内存 */
        procLimiterSet->limitsList[plimiteID] = (UINTPTR)g_limiteOps[plimiteID].LimiterAlloc();
        if (procLimiterSet->limitsList[plimiteID] == (UINTPTR)NULL) { /* 分配失败检查 */
            OsPLimitsFree(procLimiterSet);             /* 释放已分配的限制器资源 */
            return ENOMEM;                             /* 返回内存不足错误码 */
        }

        if (g_limiteOps[plimiteID].LimiterInit != NULL) { /* 检查是否需要初始化 */
            g_limiteOps[plimiteID].LimiterInit(procLimiterSet->limitsList[plimiteID]); /* 调用限制器初始化函数 */
        }
    }
    return LOS_OK;                                      /* 所有限制器初始化成功 */
}

/**
 * @brief 从限制器集合中删除进程
 * @param processCB 进程控制块指针
 * @note 从进程当前所属的限制器集合中移除该进程，并更新相关统计信息
 */
STATIC VOID PLimitsDeleteProcess(LosProcessCB *processCB)
{
    if ((processCB == NULL) || (processCB->plimits == NULL)) { /* 参数有效性检查 */
        return;                                         /* 无效参数，直接返回 */
    }

    ProcLimiterSet *plimits = processCB->plimits;       /* 获取进程所属的限制器集合 */
    /* 遍历所有限制器类型，执行进程移除操作 */
    for (UINT32 limitsID = 0; limitsID < PROCESS_LIMITER_COUNT; limitsID++) {
        if (g_limiteOps[limitsID].LimiterDelProcess == NULL) { /* 检查是否支持移除操作 */
            continue;                                  /* 不支持则跳过当前限制器 */
        }
        /* 调用限制器的进程移除函数 */
        g_limiteOps[limitsID].LimiterDelProcess(plimits->limitsList[limitsID], (UINTPTR)processCB);
    }
    plimits->pidCount--;                                /* 限制器进程计数减1 */
    LOS_ListDelete(&processCB->plimitsList);            /* 从限制器进程链表中删除节点 */
    processCB->plimits = NULL;                          /* 清除进程的限制器指针 */
    return;
}

/**
 * @brief 添加进程到限制器集合
 * @param plimits 目标限制器集合指针，为NULL时使用根限制器
 * @param processCB 待添加的进程控制块指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 * @note 内部接口，需在调度锁保护下调用
 */
STATIC UINT32 PLimitsAddProcess(ProcLimiterSet *plimits, LosProcessCB *processCB)
{
    UINT32 limitsID;                                    /* 限制器ID循环变量 */
    if (plimits == NULL) {                              /* 检查目标限制器是否为空 */
        plimits = g_rootPLimite;                        /* 使用根限制器作为默认目标 */
    }

    if (processCB->plimits == g_rootPLimite) {          /* 检查进程是否已在根限制器中 */
        return EPERM;                                  /* 不允许从根限制器迁移，返回权限错误 */
    }

    if (processCB->plimits == plimits) {                /* 检查进程是否已在目标限制器中 */
        return LOS_OK;                                 /* 无需重复添加，返回成功 */
    }

    /* 遍历所有限制器类型，执行添加前检查 */
    for (limitsID = 0; limitsID < PROCESS_LIMITER_COUNT; limitsID++) {
        if (g_limiteOps[limitsID].LimiterAddProcessCheck == NULL) { /* 检查是否需要添加检查 */
            continue;                                  /* 不需要则跳过当前限制器 */
        }

        /* 调用限制器的添加检查函数 */
        if (!g_limiteOps[limitsID].LimiterAddProcessCheck(plimits->limitsList[limitsID], (UINTPTR)processCB)) {
            return EACCES;                             /* 检查失败，返回访问拒绝错误 */
        }
    }

    PLimitsDeleteProcess(processCB);                    /* 从原限制器中删除进程 */

    /* 遍历所有限制器类型，执行添加进程操作 */
    for (limitsID = 0; limitsID < PROCESS_LIMITER_COUNT; limitsID++) {
        if (g_limiteOps[limitsID].LimiterAddProcess == NULL) { /* 检查是否支持添加操作 */
            continue;                                  /* 不支持则跳过当前限制器 */
        }
        /* 调用限制器的进程添加函数 */
        g_limiteOps[limitsID].LimiterAddProcess(plimits->limitsList[limitsID], (UINTPTR)processCB);
    }

    LOS_ListTailInsert(&plimits->processList, &processCB->plimitsList); /* 将进程添加到限制器进程链表尾部 */
    plimits->pidCount++;                                /* 限制器进程计数加1 */
    processCB->plimits = plimits;                       /* 更新进程的限制器指针 */
    return LOS_OK;                                      /* 添加成功 */
}

/**
 * @brief 添加进程到限制器集合（带调度锁保护）
 * @param plimits 目标限制器集合指针
 * @param processCB 待添加的进程控制块指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsPLimitsAddProcess(ProcLimiterSet *plimits, LosProcessCB *processCB)
{
    UINT32 intSave;                                     /* 中断状态保存变量 */
    SCHEDULER_LOCK(intSave);                            /* 获取调度器锁，禁止调度 */
    UINT32 ret = PLimitsAddProcess(plimits, processCB); /* 调用内部添加进程函数 */
    SCHEDULER_UNLOCK(intSave);                          /* 释放调度器锁，恢复调度 */
    return ret;                                         /* 返回操作结果 */
}
/**
 * @brief 根据PID添加进程到限制器集合
 * @param plimits 目标限制器集合指针
 * @param pid 进程ID
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsPLimitsAddPid(ProcLimiterSet *plimits, UINT32 pid)
{
    UINT32 intSave, ret;  
    if ((plimits == NULL) || OS_PID_CHECK_INVALID(pid) || (pid == 0)) {  // 参数有效性检查：限制器为空、PID无效或为0（内核进程）
        return EINVAL;  
    }

    if (plimits == g_rootPLimite) {  // 禁止直接添加进程到根限制器
        return EPERM;  
    }

    SCHEDULER_LOCK(intSave);  // 获取调度器锁，防止并发修改
    LosProcessCB *processCB = OS_PCB_FROM_PID((unsigned int)pid);  // 通过PID获取进程控制块
    if (OsProcessIsInactive(processCB)) {  // 检查进程是否处于非活动状态
        SCHEDULER_UNLOCK(intSave);  
        return EINVAL;  
    }

    ret = PLimitsAddProcess(plimits, processCB);  // 调用内部添加进程函数
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁
    return ret;  
}

/**
 * @brief 从限制器集合中删除进程（带调度锁保护）
 * @param processCB 进程控制块指针
 */
VOID OsPLimitsDeleteProcess(LosProcessCB *processCB)
{
    UINT32 intSave;  
    SCHEDULER_LOCK(intSave);  // 获取调度器锁，确保操作原子性
    PLimitsDeleteProcess(processCB);  // 调用内部删除进程函数
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁
}

/**
 * @brief 获取限制器集合中的所有进程PID
 * @param plimits 限制器集合指针
 * @param pids 存储PID结果的缓冲区
 * @param size 缓冲区大小
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsPLimitsPidsGet(const ProcLimiterSet *plimits, UINT32 *pids, UINT32 size)
{
    UINT32 intSave;  
    LosProcessCB *processCB = NULL;  
    UINT32 minSize = LOS_GetSystemProcessMaximum() * sizeof(UINT32);  // 计算最小所需缓冲区大小
    if ((plimits == NULL) || (pids == NULL) || (size < minSize)) {  // 参数有效性检查
        return EINVAL;  
    }

    SCHEDULER_LOCK(intSave);  // 获取调度器锁，防止进程列表变化
    LOS_DL_LIST *listHead = (LOS_DL_LIST *)&plimits->processList;  
    if (LOS_ListEmpty(listHead)) {  // 检查进程列表是否为空
        SCHEDULER_UNLOCK(intSave);  
        return EINVAL;  
    }

    // 遍历进程链表，标记存在的PID
    LOS_DL_LIST_FOR_EACH_ENTRY(processCB, listHead, LosProcessCB, plimitsList) {
        pids[OsGetPid(processCB)] = 1;  // 以PID为索引标记进程存在
    }
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁
    return LOS_OK;  
}

/**
 * @brief 将当前限制器集合的进程合并到父限制器
 * @param currPLimits 当前限制器集合
 * @param parentPLimits 父限制器集合
 * @note 内部函数，需在调度锁保护下调用
 */
STATIC VOID PLimitsProcessMerge(ProcLimiterSet *currPLimits, ProcLimiterSet *parentPLimits)
{
    LOS_DL_LIST *head = &currPLimits->processList;  
    while (!LOS_ListEmpty(head)) {  // 循环处理直到当前限制器进程列表为空
        LosProcessCB *processCB = LOS_DL_LIST_ENTRY(head->pstNext, LosProcessCB, plimitsList);  // 获取链表第一个进程
        PLimitsDeleteProcess(processCB);  // 从当前限制器删除进程
        PLimitsAddProcess(parentPLimits, processCB);  // 添加进程到父限制器
    }
    LOS_ListDelete(&currPLimits->childList);  // 从父限制器的子列表中删除当前限制器
    currPLimits->parent = NULL;  // 清除父指针
    return;  
}

/**
 * @brief 检查进程从当前限制器迁移到父限制器的可行性
 * @param currPLimits 当前限制器集合
 * @param parentPLimits 父限制器集合
 * @return 操作结果，LOS_OK表示允许迁移，EPERM表示拒绝迁移
 * @note 内部函数，需在调度锁保护下调用
 */
STATIC UINT32 PLimitsMigrateCheck(ProcLimiterSet *currPLimits, ProcLimiterSet *parentPLimits)
{
    for (UINT32 limiteID = 0; limiteID < PROCESS_LIMITER_COUNT; limiteID++) {  // 遍历所有限制器类型
        UINTPTR currLimit = currPLimits->limitsList[limiteID];  // 当前限制器配置
        UINTPTR parentLimit = parentPLimits->limitsList[limiteID];  // 父限制器配置
        if (g_limiteOps[limiteID].LimiterMigrateCheck == NULL) {  // 检查是否支持迁移检查
            continue;  // 不支持则跳过当前限制器
        }

        // 调用特定类型限制器的迁移检查函数
        if (!g_limiteOps[limiteID].LimiterMigrateCheck(currLimit, parentLimit)) {
            return EPERM;  // 检查失败，返回权限错误
        }
    }
    return LOS_OK;  // 所有限制器均通过迁移检查
}

/**
 * @brief 释放进程限制器集合
 * @param currPLimits 要释放的限制器集合
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsPLimitsFree(ProcLimiterSet *currPLimits)
{
    UINT32 intSave, ret;  
    if (currPLimits == NULL) {  // 参数有效性检查
        return EINVAL;  
    }

    SCHEDULER_LOCK(intSave);  // 获取调度器锁，确保操作原子性
    ProcLimiterSet *parentPLimits = currPLimits->parent;  // 获取父限制器
    ret = PLimitsMigrateCheck(currPLimits, parentPLimits);  // 检查迁移可行性
    if (ret != LOS_OK) {  // 迁移检查失败
        SCHEDULER_UNLOCK(intSave);  
        return ret;  
    }

    PLimitsProcessMerge(currPLimits, parentPLimits);  // 合并进程到父限制器
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁

    // 释放各类限制器资源
    for (INT32 limiteID = 0; limiteID < PROCESS_LIMITER_COUNT; ++limiteID) {
        UINTPTR procLimiter = currPLimits->limitsList[limiteID];  
        if (g_limiteOps[limiteID].LimiterFree != NULL) {  // 检查是否支持释放操作
            g_limiteOps[limiteID].LimiterFree(procLimiter);  // 调用特定类型限制器的释放函数
        }
    }
    (VOID)LOS_MemFree(m_aucSysMem1, currPLimits);  // 释放限制器集合本身内存
    return LOS_OK;  
}

/**
 * @brief 创建新的进程限制器集合
 * @param parentPLimits 父限制器集合
 * @return 新创建的限制器集合指针，失败返回NULL
 */
ProcLimiterSet *OsPLimitsCreate(ProcLimiterSet *parentPLimits)
{
    UINT32 intSave;  

    // 分配新限制器集合内存（从系统内存池1分配）
    ProcLimiterSet *newPLimits = (ProcLimiterSet *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcLimiterSet));
    if (newPLimits == NULL) {  // 内存分配失败
        return NULL;  
    }
    (VOID)memset_s(newPLimits, sizeof(ProcLimiterSet), 0, sizeof(ProcLimiterSet));  // 初始化内存为0
    LOS_ListInit(&newPLimits->childList);  // 初始化子限制器链表
    LOS_ListInit(&newPLimits->processList);  // 初始化进程链表

    SCHEDULER_LOCK(intSave);  // 获取调度器锁，确保操作原子性
    newPLimits->parent = parentPLimits;  // 设置父限制器
    newPLimits->level = parentPLimits->level + 1;  // 层级深度+1
    newPLimits->mask = parentPLimits->mask;  // 继承父限制器的使能掩码

    // 为各类限制器分配并复制配置
    for (INT32 plimiteID = 0; plimiteID < PROCESS_LIMITER_COUNT; ++plimiteID) {
        newPLimits->limitsList[plimiteID] = (UINTPTR)g_limiteOps[plimiteID].LimiterAlloc();  // 分配限制器内存
        if (newPLimits->limitsList[plimiteID] == (UINTPTR)NULL) {  // 分配失败
            SCHEDULER_UNLOCK(intSave);  
            OsPLimitsFree(newPLimits);  // 释放已分配资源
            return NULL;  
        }

        if (g_limiteOps[plimiteID].LimiterCopy != NULL) {  // 检查是否支持复制操作
            // 从父限制器复制配置到新限制器
            g_limiteOps[plimiteID].LimiterCopy(newPLimits->limitsList[plimiteID],
                                               parentPLimits->limitsList[plimiteID]);
        }
    }
    LOS_ListTailInsert(&g_rootPLimite->childList, &newPLimits->childList);  // 添加到根限制器的子列表

    (VOID)PLimitsDeleteProcess(OsCurrProcessGet());  // 将当前进程从原限制器中删除
    (VOID)PLimitsAddProcess(newPLimits, OsCurrProcessGet());  // 将当前进程添加到新限制器
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁
    return newPLimits;  
}

#ifdef LOSCFG_KERNEL_MEM_PLIMIT  // 使能内存限制时编译此段
/**
 * @brief 获取限制器集合的内存使用情况
 * @param plimits 限制器集合指针
 * @param usage 存储内存使用数据的缓冲区
 * @param size 缓冲区大小
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsPLimitsMemUsageGet(ProcLimiterSet *plimits, UINT64 *usage, UINT32 size)
{
    UINT32 intSave;  
    LosProcessCB *processCB = NULL;  
    // 计算最小所需缓冲区大小：进程数*UINT64 + 内存限制器结构大小
    UINT32 minSize = LOS_GetSystemProcessMaximum() * sizeof(UINT64) + sizeof(ProcMemLimiter);
    ProcMemLimiter *memLimit = (ProcMemLimiter *)usage;  // 内存限制器配置指针
    UINT64 *memSuage = (UINT64 *)((UINTPTR)usage + sizeof(ProcMemLimiter));  // 进程内存使用数组指针
    if ((plimits == NULL) || (usage == NULL) || (size < minSize)) {  // 参数有效性检查
        return EINVAL;  
    }

    SCHEDULER_LOCK(intSave);  // 获取调度器锁，防止数据不一致
    // 复制内存限制器配置
    (VOID)memcpy_s(memLimit, sizeof(ProcMemLimiter),
                   (VOID *)plimits->limitsList[PROCESS_LIMITER_ID_MEM], sizeof(ProcMemLimiter));
    LOS_DL_LIST *listHead = (LOS_DL_LIST *)&plimits->processList;  
    if (LOS_ListEmpty(listHead)) {  // 检查进程列表是否为空
        SCHEDULER_UNLOCK(intSave);  
        return EINVAL;  
    }

    // 遍历进程链表，收集每个进程的内存使用情况
    LOS_DL_LIST_FOR_EACH_ENTRY(processCB, listHead, LosProcessCB, plimitsList) {
        memSuage[OsGetPid(processCB)] = processCB->limitStat.memUsed;  // 记录进程内存使用量
    }
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁
    return LOS_OK;  
}
#endif

#ifdef LOSCFG_KERNEL_IPC_PLIMIT  // 使能IPC限制时编译此段
/**
 * @brief 获取限制器集合的IPC使用统计信息
 * @param plimits 限制器集合指针
 * @param ipc 存储IPC统计信息的缓冲区
 * @param size 缓冲区大小（必须等于ProcIPCLimit结构体大小）
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsPLimitsIPCStatGet(ProcLimiterSet *plimits, ProcIPCLimit *ipc, UINT32 size)
{
    UINT32 intSave;  
    if ((plimits == NULL) || (ipc == NULL) || (size != sizeof(ProcIPCLimit))) {  // 参数有效性检查
        return EINVAL;  
    }

    SCHEDULER_LOCK(intSave);  // 获取调度器锁，确保数据一致性
    // 复制IPC限制器配置
    (VOID)memcpy_s(ipc, sizeof(ProcIPCLimit),
                   (VOID *)plimits->limitsList[PROCESS_LIMITER_ID_IPC], sizeof(ProcIPCLimit));
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁
    return LOS_OK;  
}
#endif

#ifdef LOSCFG_KERNEL_SCHED_PLIMIT  // 使能调度限制时编译此段
/**
 * @brief 获取限制器集合的调度运行时间统计
 * @param plimits 限制器集合指针
 * @param usage 存储运行时间的缓冲区
 * @param size 缓冲区大小
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsPLimitsSchedUsageGet(ProcLimiterSet *plimits, UINT64 *usage, UINT32 size)
{
    UINT32 intSave;  
    LosProcessCB *processCB = NULL;  
    UINT32 minSize = LOS_GetSystemProcessMaximum() * sizeof(UINT64);  // 计算最小所需缓冲区大小
    if ((plimits == NULL) || (usage == NULL) || (size < minSize)) {  // 参数有效性检查
        return EINVAL;  
    }

    SCHEDULER_LOCK(intSave);  // 获取调度器锁，防止数据不一致
    LOS_DL_LIST *listHead = (LOS_DL_LIST *)&plimits->processList;  
    if (LOS_ListEmpty(listHead)) {  // 检查进程列表是否为空
        SCHEDULER_UNLOCK(intSave);  
        return EINVAL;  
    }

    // 遍历进程链表，收集每个进程的运行时间
    LOS_DL_LIST_FOR_EACH_ENTRY(processCB, listHead, LosProcessCB, plimitsList) {
        usage[OsGetPid(processCB)] = processCB->limitStat.allRuntime;  // 记录进程总运行时间
    }
    SCHEDULER_UNLOCK(intSave);  // 释放调度器锁
    return LOS_OK;  
}
#endif
#endif
