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

#include "los_crc32.h"


/**
 * @brief CRC32计算宏定义 - 处理1字节数据
 * @note val = crc32_tab[(val ^ (*src++)) & 0xff] ^ (val >> 8)
 *       1. 将当前CRC值与输入字节异或
 *       2. 取低8位作为CRC表索引
 *       3. 查表结果与右移8位的CRC值异或
 *       4. 输入指针自增，指向下一字节
 */
#define COUNT1    val = crc32_tab[(val ^ (*src++)) & 0xff] ^ (val >> 8)

/**
 * @brief CRC32计算宏定义 - 处理2字节数据
 * @note 连续调用两次COUNT1宏，高效处理2字节
 */
#define COUNT2    COUNT1; COUNT1

/**
 * @brief CRC32计算宏定义 - 处理4字节数据
 * @note 连续调用两次COUNT2宏，高效处理4字节
 */
#define COUNT4    COUNT2; COUNT2

/**
 * @brief CRC32计算宏定义 - 处理8字节数据
 * @note 连续调用两次COUNT4宏，高效处理8字节
 */
#define COUNT8    COUNT4; COUNT4

/**
 * @brief CRC32批量处理比率
 * @note 定义每次批量处理的字节数为8字节，用于循环优化
 */
#define ACCRATIO    8

/**
 * @brief 以太网CRC32累加计算函数
 * @param val [IN] 初始CRC值
 * @param src [IN] 输入数据缓冲区指针
 * @param len [IN] 数据长度（字节）
 * @return 计算后的CRC32值
 * @note 遵循以太网CRC32标准，包含初始和最终的0xffffffff异或操作
 */
UINT32 LOS_EtherCrc32Accumulate(UINT32 val, UINT8 *src, INT32 len)
{
    if (src == 0) {  // 检查输入缓冲区指针有效性
        return 0L;   // 无效指针返回0
    }

    val = val ^ 0xffffffffUL;  // CRC32初始异或（以太网标准）
    // 批量处理8字节数据（高效循环）
    while (len >= ACCRATIO) {
        COUNT8;               // 处理8字节数据
        len -= ACCRATIO;      // 剩余长度减少8字节
    }
    // 处理剩余不足8字节的数据
    while (len--) {
        COUNT1;               // 处理1字节数据
    }

    return val ^ 0xffffffffUL;  // CRC32最终异或（以太网标准）
}

/**
 * @brief 通用CRC32累加计算函数
 * @param val [IN] 初始CRC值
 * @param src [IN] 输入数据缓冲区指针
 * @param len [IN] 数据长度（字节）
 * @return 计算后的CRC32值
 * @note 不包含初始和最终的异或操作，适用于分段计算
 */
UINT32 LOS_Crc32Accumulate(UINT32 val, UINT8 *src, INT32 len)
{
    // 批量处理8字节数据（高效循环）
    while (len >= ACCRATIO) {
        COUNT8;               // 处理8字节数据
        len -= ACCRATIO;      // 剩余长度减少8字节
    }
    // 处理剩余不足8字节的数据
    while (len--) {
        COUNT1;               // 处理1字节数据
    }

    return val;  // 返回累加计算后的CRC值
}
