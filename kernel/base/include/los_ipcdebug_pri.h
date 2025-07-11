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

#ifndef _LOS_IPCDEBUG_PRI_H
#define _LOS_IPCDEBUG_PRI_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief IPC调试排序参数结构体，用于控制IPC控制块数组的排序操作
 * @note 该结构体封装了排序所需的缓冲区信息、控制块元数据和排序偏移量
 */
typedef struct {
    CHAR *buf;             /**< 控制块数组总缓冲区，存储所有IPC控制块数据 */
    size_t ipcDebugCBSize; /**< 单个控制块大小，单位：字节 */
    size_t ipcDebugCBCnt;  /**< 控制块数量，表示缓冲区中包含的控制块总数 */
    UINT32 sortElemOff;    /**< 控制块中待比较成员的偏移量，用于计算成员地址 */
} IpcSortParam;

/**
 * @brief 比较函数指针类型，用于比较两个控制块中指定成员的大小
 * @param[in] sortParam 排序参数结构体指针，包含缓冲区和偏移量信息
 * @param[in] left 左侧控制块索引
 * @param[in] right 右侧控制块索引
 * @return BOOL 比较结果：TRUE表示left > right，FALSE表示left <= right
 * @note 该函数指针主要用于比较最后访问时间的大小
 */
typedef BOOL (*OsCompareFunc)(const IpcSortParam *sortParam, UINT32 left, UINT32 right);

/**
 * @brief 获取比较成员变量的地址
 * @param[in] sortParam 排序参数结构体指针
 * @param[in] index 控制块索引
 * @return 指向比较成员变量的指针
 * @par 计算逻辑：缓冲区基地址 + (索引 × 单个控制块大小) + 成员偏移量
 */
#define SORT_ELEM_ADDR(sortParam, index) \
    ((sortParam)->buf + ((index) * (sortParam)->ipcDebugCBSize) + (sortParam)->sortElemOff)

/* Sort this index array. */
extern VOID OsArraySortByTime(UINT32 *sortArray, UINT32 start, UINT32 end, const IpcSortParam *sortParam,
                              OsCompareFunc compareFunc);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_IPCDEBUG_PRI_H */
