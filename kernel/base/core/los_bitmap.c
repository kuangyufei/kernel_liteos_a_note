/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
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
/**
 * @brief
 * @verbatim
    基本概念
        位操作是指对二进制数的bit位进行操作。程序可以设置某一变量为状态字，状态字中的
        每一bit位（标志位）可以具有自定义的含义。

    使用场景
        系统提供标志位的置1和清0操作，可以改变标志位的内容，同时还提供获取状态字中标志位
        为1的最高位和最低位的功能。用户也可以对系统的寄存器进行位操作。

    参考
        https://www.geeksforgeeks.org/builtin-functions-gcc-compiler/
 * @endverbatim 
 */
 
#include "los_bitmap.h"
#include "los_printf.h"
#include "los_toolchain.h"

/* 位图操作掩码：用于获取位在32位字中的偏移位置（0-31） */
#define OS_BITMAP_MASK 0x1FU
/* 字掩码：全1的无符号长整型，用于操作整个字的所有位 */
#define OS_BITMAP_WORD_MASK ~0UL

/*
 * @brief 从最低位开始查找第一个0位的位置
 * @param x 待检查的无符号整数
 * @return 第一个0位的索引（从0开始），若全为1则返回31
 * @note 使用GCC内置函数__builtin_ffsl实现，~x将0位转换为1位后查找第一个1位
 */
STATIC INLINE UINT16 Ffz(UINTPTR x)
{
    return __builtin_ffsl(~x) - 1;  // __builtin_ffsl返回从LSB开始第一个1位的位置（1-based），减1转为0-based索引
}

/*
 * @brief 设置位图中指定位置的位
 * @param bitmap 指向位图缓冲区的指针
 * @param pos 要设置的位位置（从0开始）
 * @note 若bitmap为NULL则不执行任何操作，pos超出32位范围时通过掩码自动取模
 */
VOID LOS_BitmapSet(UINT32 *bitmap, UINT16 pos)
{
    if (bitmap == NULL) {  // 检查位图指针有效性
        return;            // 无效指针直接返回
    }

    *bitmap |= 1U << (pos & OS_BITMAP_MASK);  // 计算位偏移并设置对应位（自动处理pos >=32的情况）
}

/*
 * @brief 清除位图中指定位置的位
 * @param bitmap 指向位图缓冲区的指针
 * @param pos 要清除的位位置（从0开始）
 * @note 若bitmap为NULL则不执行任何操作，pos超出32位范围时通过掩码自动取模
 */
VOID LOS_BitmapClr(UINT32 *bitmap, UINT16 pos)
{
    if (bitmap == NULL) {  // 检查位图指针有效性
        return;            // 无效指针直接返回
    }

    *bitmap &= ~(1U << (pos & OS_BITMAP_MASK));  // 计算位偏移并清除对应位
}

/*
 * @brief 获取位图中最高置位位的索引
 * @param bitmap 32位位图数据
 * @return 最高置位位的索引（0-based），位图为0时返回LOS_INVALID_BIT_INDEX
 * @note 使用CLZ（Count Leading Zeros）指令计算前导零个数
 */
UINT16 LOS_HighBitGet(UINT32 bitmap)
{
    if (bitmap == 0) {                     // 检查位图是否为空
        return LOS_INVALID_BIT_INDEX;      // 空位图返回无效索引
    }

    return (OS_BITMAP_MASK - CLZ(bitmap));  // 31 - 前导零个数 = 最高置位位索引
}

/*
 * @brief 获取位图中最低置位位的索引
 * @param bitmap 32位位图数据
 * @return 最低置位位的索引（0-based），位图为0时返回LOS_INVALID_BIT_INDEX
 * @note 使用CTZ（Count Trailing Zeros）指令计算尾随零个数
 */
UINT16 LOS_LowBitGet(UINT32 bitmap)
{
    if (bitmap == 0) {                     // 检查位图是否为空
        return LOS_INVALID_BIT_INDEX;      // 空位图返回无效索引
    }

    return CTZ(bitmap);  // 尾随零个数即最低置位位索引
}

/*
 * @brief 在位图中连续设置多个位
 * @param bitmap 指向位图缓冲区的指针（UINTPTR类型支持不同字长架构）
 * @param start 起始位位置（从0开始）
 * @param numsSet 要设置的位数量
 * @note 支持跨多个字（32位或64位）的连续位设置，自动处理边界情况
 */
VOID LOS_BitmapSetNBits(UINTPTR *bitmap, UINT32 start, UINT32 numsSet)
{
    UINTPTR *p = bitmap + BITMAP_WORD(start);  // 计算起始位所在的字指针
    const UINT32 size = start + numsSet;       // 计算结束位置（含）
    UINT16 bitsToSet = BITMAP_BITS_PER_WORD - (start % BITMAP_BITS_PER_WORD);  // 首个字中待设置的位数
    UINTPTR maskToSet = BITMAP_FIRST_WORD_MASK(start);  // 首个字的设置掩码

    while (numsSet > bitsToSet) {  // 处理完整字的设置
        *p |= maskToSet;           // 设置当前字中的位
        numsSet -= bitsToSet;      // 更新剩余待设置位数
        bitsToSet = BITMAP_BITS_PER_WORD;  // 后续字需要设置完整位宽
        maskToSet = OS_BITMAP_WORD_MASK;   // 完整字掩码（全1）
        p++;                       // 移动到下一个字
    }
    if (numsSet) {  // 处理剩余不足一个字的位
        maskToSet &= BITMAP_LAST_WORD_MASK(size);  // 计算结束位置的掩码
        *p |= maskToSet;                           // 设置剩余的位
    }
}

/*
 * @brief 在位图中连续清除多个位
 * @param bitmap 指向位图缓冲区的指针（UINTPTR类型支持不同字长架构）
 * @param start 起始位位置（从0开始）
 * @param numsClear 要清除的位数量
 * @note 支持跨多个字（32位或64位）的连续位清除，自动处理边界情况
 */
VOID LOS_BitmapClrNBits(UINTPTR *bitmap, UINT32 start, UINT32 numsClear)
{
    UINTPTR *p = bitmap + BITMAP_WORD(start);  // 计算起始位所在的字指针
    const UINT32 size = start + numsClear;     // 计算结束位置（含）
    UINT16 bitsToClear = BITMAP_BITS_PER_WORD - (start % BITMAP_BITS_PER_WORD);  // 首个字中待清除的位数
    UINTPTR maskToClear = BITMAP_FIRST_WORD_MASK(start);  // 首个字的清除掩码

    while (numsClear >= bitsToClear) {  // 处理完整字的清除
        *p &= ~maskToClear;             // 清除当前字中的位
        numsClear -= bitsToClear;       // 更新剩余待清除位数
        bitsToClear = BITMAP_BITS_PER_WORD;  // 后续字需要清除完整位宽
        maskToClear = OS_BITMAP_WORD_MASK;   // 完整字掩码（全1）
        p++;                           // 移动到下一个字
    }
    if (numsClear) {  // 处理剩余不足一个字的位
        maskToClear &= BITMAP_LAST_WORD_MASK(size);  // 计算结束位置的掩码
        *p &= ~maskToClear;                          // 清除剩余的位
    }
}

/*
 * @brief 查找位图中第一个未置位（0）的位
 * @param bitmap 指向位图缓冲区的指针（UINTPTR类型支持不同字长架构）
 * @param numBits 位图的总位数
 * @return 第一个0位的索引（0-based），未找到返回-1
 * @note 按字遍历位图，使用Ffz函数查找每个字中的第一个0位
 */
INT32 LOS_BitmapFfz(UINTPTR *bitmap, UINT32 numBits)
{
    INT32 bit, i;  // bit: 找到的0位索引，i: 字索引

    for (i = 0; i < BITMAP_NUM_WORDS(numBits); i++) {  // 遍历所有字
        if (bitmap[i] == OS_BITMAP_WORD_MASK) {        // 跳过全1的字
            continue;
        }
        bit = i * BITMAP_BITS_PER_WORD + Ffz(bitmap[i]);  // 计算全局位索引
        if (bit < numBits) {  // 检查是否在有效范围内
            return bit;       // 返回找到的0位索引
        }
        return -1;  // 超出范围，返回-1
    }
    return -1;  // 未找到0位，返回-1
}
