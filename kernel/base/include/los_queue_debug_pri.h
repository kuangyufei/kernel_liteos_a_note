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

#ifndef _LOS_QUEUE_DEBUG_PRI_H
#define _LOS_QUEUE_DEBUG_PRI_H

#include "los_config.h"
#include "los_task.h"
#include "los_queue_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 队列调试功能初始化接口
 * @return UINT32 初始化结果，LOS_OK表示成功，其他值表示失败
 */
extern UINT32 OsQueueDbgInit(VOID);

/**
 * @brief 队列调试初始化钩子函数
 * @return UINT32 初始化结果，LOS_OK表示成功
 * @details 当使能队列调试配置(LOSCFG_DEBUG_QUEUE)时调用实际初始化函数，否则直接返回成功
 */
STATIC INLINE UINT32 OsQueueDbgInitHook(VOID)
{
#ifdef LOSCFG_DEBUG_QUEUE
    return OsQueueDbgInit();
#else
    return LOS_OK;
#endif
}

/**
 * @brief 更新队列最后执行时间
 * @param queueID 队列ID
 */
extern VOID OsQueueDbgTimeUpdate(UINT32 queueID);

/**
 * @brief 队列执行时间更新钩子函数
 * @param queueID 队列ID
 * @details 当使能队列调试配置(LOSCFG_DEBUG_QUEUE)时更新队列最后执行时间，否则不执行任何操作
 */
STATIC INLINE VOID OsQueueDbgTimeUpdateHook(UINT32 queueID)
{
#ifdef LOSCFG_DEBUG_QUEUE
    OsQueueDbgTimeUpdate(queueID);
#endif
}

/**
 * @brief 更新队列调试信息中的任务入口函数
 * @param queueID 队列ID
 * @param entry 任务入口函数指针
 * @details 当队列被创建或删除时调用，用于跟踪队列关联的任务信息
 */
extern VOID OsQueueDbgUpdate(UINT32 queueID, TSK_ENTRY_FUNC entry);

/**
 * @brief 队列调试信息更新钩子函数
 * @param queueID 队列ID
 * @param entry 任务入口函数指针
 * @details 当使能队列调试配置(LOSCFG_DEBUG_QUEUE)时更新队列调试信息，否则不执行任何操作
 */
STATIC INLINE VOID OsQueueDbgUpdateHook(UINT32 queueID, TSK_ENTRY_FUNC entry)
{
#ifdef LOSCFG_DEBUG_QUEUE
    OsQueueDbgUpdate(queueID, entry);
#endif
}

/**
 * @brief 队列资源泄漏检查
 * @details 用于检测队列是否存在未释放的资源泄漏问题
 */
extern VOID OsQueueCheck(VOID);

/**
 * @brief 队列泄漏检查钩子函数
 * @details 当使能队列调试配置(LOSCFG_DEBUG_QUEUE)时执行队列泄漏检查，否则不执行任何操作
 */
STATIC INLINE VOID OsQueueCheckHook(VOID)
{
#ifdef LOSCFG_DEBUG_QUEUE
    OsQueueCheck();
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_QUEUE_DEBUG_PRI_H */
