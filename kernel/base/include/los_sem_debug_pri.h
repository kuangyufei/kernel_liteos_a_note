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

#ifndef _LOS_SEM_DEBUG_PRI_H
#define _LOS_SEM_DEBUG_PRI_H

#include "los_config.h"
#include "los_sem_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 信号量调试模块初始化接口
 * @return UINT32 操作结果
 *         - LOS_OK: 初始化成功
 *         - 其他值: 初始化失败
 * @note 仅当启用信号量调试功能(LOSCFG_DEBUG_SEMAPHORE)时有效
 */
extern UINT32 OsSemDbgInit(VOID);

/**
 * @brief 信号量调试初始化钩子函数
 * @return UINT32 操作结果
 *         - LOS_OK: 执行成功（无论是否启用调试功能）
 * @note 1. 该钩子函数根据LOSCFG_DEBUG_SEMAPHORE配置决定是否调用实际初始化函数
 *       2. 未启用调试功能时直接返回LOS_OK，不执行任何操作
 */
STATIC INLINE UINT32 OsSemDbgInitHook(VOID)
{
#ifdef LOSCFG_DEBUG_SEMAPHORE
    return OsSemDbgInit();  /* 调用实际的调试初始化函数 */
#else
    return LOS_OK;          /* 调试功能未启用，直接返回成功 */
#endif
}

/**
 * @brief 更新信号量最后操作时间
 * @param semID 信号量ID，标识特定信号量
 * @note 仅当启用信号量调试功能(LOSCFG_DEBUG_SEMAPHORE)时有效
 */
extern VOID OsSemDbgTimeUpdate(UINT32 semID);

/**
 * @brief 信号量操作时间更新钩子函数
 * @param semID 信号量ID，标识需要更新时间的信号量
 * @note 1. 该钩子函数根据LOSCFG_DEBUG_SEMAPHORE配置决定是否调用实际时间更新函数
 *       2. 用于跟踪信号量的最近访问时间，辅助调试信号量超时或死锁问题
 */
STATIC INLINE VOID OsSemDbgTimeUpdateHook(UINT32 semID)
{
#ifdef LOSCFG_DEBUG_SEMAPHORE
    OsSemDbgTimeUpdate(semID);  /* 调用实际的时间更新函数 */
#endif
    return;
}

/**
 * @brief 创建或删除信号量时更新调试控制块
 * @param semID 信号量ID，标识特定信号量
 * @param creator 信号量创建者任务入口函数地址
 * @param count 信号量初始计数值
 * @note 仅当启用信号量调试功能(LOSCFG_DEBUG_SEMAPHORE)时有效
 */
extern VOID OsSemDbgUpdate(UINT32 semID, TSK_ENTRY_FUNC creator, UINT16 count);

/**
 * @brief 信号量调试控制块更新钩子函数
 * @param semID 信号量ID，标识需要更新调试信息的信号量
 * @param creator 信号量创建者任务入口函数地址，用于跟踪信号量归属
 * @param count 信号量计数值，记录信号量的初始或当前状态
 * @note 1. 该钩子函数根据LOSCFG_DEBUG_SEMAPHORE配置决定是否调用实际更新函数
 *       2. 通常在信号量创建(LosSemCreate)或删除(LosSemDelete)时调用
 */
STATIC INLINE VOID OsSemDbgUpdateHook(UINT32 semID, TSK_ENTRY_FUNC creator, UINT16 count)
{
#ifdef LOSCFG_DEBUG_SEMAPHORE
    OsSemDbgUpdate(semID, creator, count);  /* 调用实际的调试信息更新函数 */
#endif
    return;
}

/**
 * @brief 获取信号量调试控制块的完整数据
 * @return UINT32 操作结果
 *         - LOS_OK: 获取成功
 *         - 其他值: 获取失败
 * @note 仅当启用信号量调试功能(LOSCFG_DEBUG_SEMAPHORE)时有效
 */
extern UINT32 OsSemInfoGetFullData(VOID);

/**
 * @brief 信号量调试数据完整获取钩子函数
 * @note 1. 该钩子函数根据LOSCFG_DEBUG_SEMAPHORE配置决定是否调用实际数据获取函数
 *       2. 用于收集信号量的完整调试信息，通常供调试工具或命令查看
 *       3. 忽略返回值是因为调试功能不影响主流程的正确性
 */
STATIC INLINE VOID OsSemInfoGetFullDataHook(VOID)
{
#ifdef LOSCFG_DEBUG_SEMAPHORE
    (VOID)OsSemInfoGetFullData();  /* 调用实际的数据获取函数，忽略返回值 */
#endif
    return;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SEM_DEBUG_PRI_H */
