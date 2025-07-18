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

#include "perf_pmu_pri.h"
/**
 * @brief PMU管理器数组，用于存储不同类型的PMU实例
 * @note 数组大小为PERF_EVENT_TYPE_MAX，索引对应事件类型，初始值均为NULL
 */
STATIC Pmu *g_pmuMgr[PERF_EVENT_TYPE_MAX] = { NULL };  // PMU实例管理器，按事件类型索引

/**
 * @brief 注册PMU实例到管理器
 * @param pmu 待注册的PMU实例指针
 * @return LOS_OK 注册成功；LOS_NOK 参数无效或该类型已注册
 * @note 同一事件类型只能注册一个PMU实例
 */
UINT32 OsPerfPmuRegister(Pmu *pmu)
{
    UINT32 type;                                          // 事件类型变量

    if ((pmu == NULL) || (pmu->type >= PERF_EVENT_TYPE_MAX)) {  // 检查PMU实例有效性及类型范围
        return LOS_NOK;                                   // 参数无效，返回失败
    }

    type = pmu->type;                                     // 获取PMU事件类型
    if (g_pmuMgr[type] == NULL) {                         // 检查该类型是否未被注册
        g_pmuMgr[type] = pmu;                             // 存储PMU实例指针
        return LOS_OK;                                    // 注册成功
    }
    return LOS_NOK;                                       // 该类型已注册，返回失败
}

/**
 * @brief 根据事件类型获取PMU实例
 * @param type 事件类型
 * @return 成功返回PMU实例指针；失败返回NULL
 * @note 当类型为PERF_EVENT_TYPE_RAW时，自动转换为PERF_EVENT_TYPE_HW处理
 */
Pmu *OsPerfPmuGet(UINT32 type)
{
    if (type >= PERF_EVENT_TYPE_MAX) {                    // 检查事件类型有效性
        return NULL;                                     // 类型越界，返回NULL
    }

    if (type == PERF_EVENT_TYPE_RAW) { /* 硬件原始事件使用硬件PMU处理 */
        type = PERF_EVENT_TYPE_HW;                        // 转换RAW类型为HW类型
    }
    return g_pmuMgr[type];                                // 返回对应类型的PMU实例
}

/**
 * @brief 从管理器中移除指定类型的PMU实例
 * @param type 要移除的事件类型
 * @return 无
 * @note 移除后对应位置将设为NULL，不释放PMU实例内存
 */
VOID OsPerfPmuRm(UINT32 type)
{
    if (type >= PERF_EVENT_TYPE_MAX) {                    // 检查事件类型有效性
        return;                                          // 类型越界，直接返回
    }
    g_pmuMgr[type] = NULL;                                // 清除PMU实例指针
}