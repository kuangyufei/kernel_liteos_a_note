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

#ifndef _LOS_SEM_PRI_H
#define _LOS_SEM_PRI_H

#include "los_sem.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_sem
 * @brief 信号量控制块结构体
 * @details 用于管理和描述系统中的信号量资源，包含信号量状态、计数、等待队列等关键信息
 * @note 每个信号量对应一个LosSemCB实例，由内核统一管理
 */
typedef struct {
    UINT8 semStat;         /**< 信号量状态：OS_SEM_UNUSED(0)表示未使用，OS_SEM_USED(1)表示已使用 */
    UINT16 semCount;       /**< 当前可用信号量数量，取值范围[0, maxSemCount] */
    UINT16 maxSemCount;    /**< 最大信号量数量，创建时指定且不可修改 */
    UINT32 semID;          /**< 信号量ID，系统中唯一标识信号量的编号 */
    LOS_DL_LIST semList;   /**< 等待该信号量的任务双向链表，按优先级排序 */
} LosSemCB;

/**
 * @ingroup los_sem
 * @brief 信号量未使用状态
 * @note 用于semStat字段，表示该信号量控制块未被分配使用
 */
#define OS_SEM_UNUSED        0

/**
 * @ingroup los_sem
 * @brief 信号量已使用状态
 * @note 用于semStat字段，表示该信号量控制块已被分配使用
 */
#define OS_SEM_USED          1

/**
 * @ingroup los_sem
 * @brief 从信号量双向链表节点获取信号量控制块指针
 * @param ptr 信号量链表节点指针（通常为semList成员地址）
 * @return LosSemCB* 信号量控制块指针
 * @note 通过容器_of机制实现，用于遍历信号量链表时获取完整控制块
 */
#define GET_SEM_LIST(ptr) LOS_DL_LIST_ENTRY(ptr, LosSemCB, semList)

/**
 * @ingroup los_sem
 * @brief 全局信号量控制块数组指针
 * @note 指向系统中所有信号量控制块的起始地址，通过信号量索引访问具体控制块
 */
extern LosSemCB *g_allSem;

/**
 * @ingroup los_sem
 * @brief 信号量ID中计数与索引的分隔位
 * @note 用于将32位semID划分为两部分：高16位表示计数，低16位表示索引
 */
#define SEM_SPLIT_BIT        16

/**
 * @ingroup los_sem
 * @brief 组合信号量计数和索引生成信号量ID
 * @param count 信号量计数（高16位）
 * @param semID 信号量索引（低16位）
 * @return UINT32 组合后的32位信号量ID
 * @note 计算公式：(count << 16) | (semID & 0xFFFF)
 */
#define SET_SEM_ID(count, semID)    (((count) << SEM_SPLIT_BIT) | (semID))

/**
 * @ingroup los_sem
 * @brief 从信号量ID中提取索引
 * @param semID 完整的32位信号量ID
 * @return UINT32 信号量索引（低16位），范围[0, 65535]
 * @note 计算公式：semID & 0xFFFF
 */
#define GET_SEM_INDEX(semID)        ((semID) & ((1U << SEM_SPLIT_BIT) - 1))

/**
 * @ingroup los_sem
 * @brief 从信号量ID中提取计数
 * @param semID 完整的32位信号量ID
 * @return UINT32 信号量计数（高16位），范围[0, 65535]
 * @note 计算公式：semID >> 16
 */
#define GET_SEM_COUNT(semID)        ((semID) >> SEM_SPLIT_BIT)

/**
 * @ingroup los_sem
 * @brief 通过信号量ID获取信号量控制块指针
 * @param semID 完整的32位信号量ID
 * @return LosSemCB* 信号量控制块指针，NULL表示无效ID
 * @note 实现逻辑：g_allSem[GET_SEM_INDEX(semID)]
 */
#define GET_SEM(semID)              (((LosSemCB *)g_allSem) + GET_SEM_INDEX(semID))

/**
 * @ingroup los_sem
 * @brief 最大等待任务信息数量
 * @note 表示可记录的等待信号量任务的最大数量，用于调试和信息查询
 */
#define OS_MAX_PENDTASK_INFO 4

extern UINT32 OsSemInit(VOID);
extern UINT32 OsSemPostUnsafe(UINT32 semHandle, BOOL *needSched);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SEM_PRI_H */
