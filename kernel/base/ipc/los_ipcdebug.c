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

#include "los_ipcdebug_pri.h"


#if defined(LOSCFG_DEBUG_SEMAPHORE) || defined(LOSCFG_DEBUG_QUEUE)

/**
 * @brief 根据时间对IPC调试数组进行排序
 * @details 使用快速排序算法对IPC调试信息数组按时间维度排序，支持自定义比较函数
 * @param[in,out] sortArray 待排序的索引数组，排序后存储排序结果
 * @param[in]     start     排序起始索引（包含）
 * @param[in]     end       排序结束索引（包含）
 * @param[in]     sortParam 排序参数结构体，包含IPC调试控制块计数等信息
 * @param[in]     compareFunc 比较函数指针，定义元素间的比较规则
 * @return 无
 */
VOID OsArraySortByTime(UINT32 *sortArray, UINT32 start, UINT32 end, const IpcSortParam *sortParam,
                       OsCompareFunc compareFunc)
{
    UINT32 left = start;          // 左指针，从起始位置开始
    UINT32 right = end;           // 右指针，从结束位置开始
    UINT32 idx = start;           // 基准值最终位置索引
    UINT32 pivot = sortArray[start];  // 选择第一个元素作为基准值

    // 快速排序主循环：左右指针未相遇时继续
    while (left < right) {
        // 从右向左找第一个小于等于基准值的元素（需满足索引有效且比较条件）
        while ((left < right) && (sortArray[right] < sortParam->ipcDebugCBCnt) && (pivot < sortParam->ipcDebugCBCnt) &&
               compareFunc(sortParam, sortArray[right], pivot)) {
            right--;  // 右指针左移
        }

        // 找到符合条件的元素，移动到左指针位置
        if (left < right) {
            sortArray[left] = sortArray[right];
            idx = right;          // 更新基准值位置为当前右指针
            left++;               // 左指针右移
        }

        // 从左向右找第一个大于等于基准值的元素（需满足索引有效且比较条件）
        while ((left < right) && (sortArray[left] < sortParam->ipcDebugCBCnt) && (pivot < sortParam->ipcDebugCBCnt) &&
               compareFunc(sortParam, pivot, sortArray[left])) {
            left++;   // 左指针右移
        }

        // 找到符合条件的元素，移动到右指针位置
        if (left < right) {
            sortArray[right] = sortArray[left];
            idx = left;           // 更新基准值位置为当前左指针
            right--;              // 右指针左移
        }
    }

    sortArray[idx] = pivot;  // 将基准值放到最终正确位置

    // 递归排序左子数组（起始位置到基准值前一位）
    if (start < idx) {
        OsArraySortByTime(sortArray, start, idx - 1, sortParam, compareFunc);
    }
    // 递归排序右子数组（基准值后一位到结束位置）
    if (idx < end) {
        OsArraySortByTime(sortArray, idx + 1, end, sortParam, compareFunc);
    }
}

#endif

