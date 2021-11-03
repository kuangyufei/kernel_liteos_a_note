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
 * Semaphore control structure.
 */
typedef struct {
    UINT8 semStat; /**< Semaphore state | 信号量的状态 */
    UINT16 semCount; /**< Number of available semaphores | 有效信号量的数量 */
    UINT16 maxSemCount;  /**< Max number of available semaphores | 有效信号量的最大数量 */
    UINT32 semID; /**< Semaphore control structure ID | 信号量索引号 */
    LOS_DL_LIST semList; /**< Queue of tasks that are waiting on a semaphore | 等待信号量的任务队列,任务通过阻塞节点挂上去 */
} LosSemCB;

/**
 * @ingroup los_sem
 * The semaphore is not in use.
 *
 */
#define OS_SEM_UNUSED        0
/**
 * @ingroup los_sem
 * The semaphore is used.
 *
 */
#define OS_SEM_USED          1
/**
 * @ingroup los_sem
 * Obtain the head node in a semaphore doubly linked list.
 *
 */
#define GET_SEM_LIST(ptr) LOS_DL_LIST_ENTRY(ptr, LosSemCB, semList)
extern LosSemCB *g_allSem;
/**
 * @ingroup los_sem
 * COUNT | INDEX  split bit
 */
#define SEM_SPLIT_BIT        16
/**
 * @ingroup los_sem
 * Set the semaphore id
 */
#define SET_SEM_ID(count, semID)    (((count) << SEM_SPLIT_BIT) | (semID))

/**
 * @ingroup los_sem
 * get the semaphore index
 */
#define GET_SEM_INDEX(semID)        ((semID) & ((1U << SEM_SPLIT_BIT) - 1))

/**
 * @ingroup los_sem
 * get the semaphore count
 */
#define GET_SEM_COUNT(semID)        ((semID) >> SEM_SPLIT_BIT)

/**
 * @ingroup los_sem
 * Obtain a semaphore ID.
 *
 */
#define GET_SEM(semID)              (((LosSemCB *)g_allSem) + GET_SEM_INDEX(semID))

/**
 * @ingroup los_sem
 * Maximum value of task information.
 *
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
