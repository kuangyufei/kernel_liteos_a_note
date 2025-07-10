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

/**
 * @defgroup los_bitmap Bitmap
 * @ingroup kernel
 */

#ifndef _LOS_BITMAP_H
#define _LOS_BITMAP_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* 位操作基础宏定义 */
/* 获取与x同类型的1值，解决不同位数平台兼容性问题 */
#define _ONE(x) (1 + ((x) - (x)))

/**
 * @ingroup los_bitmap
 * @brief 设置指定位为1
 * @param[in] n 位索引(0-31)
 * @return 第n位为1的位掩码
 * @note 使用无符号左移避免符号位扩展问题
 */
#define BIT(n)  (1U << (n))

/**
 * @ingroup los_bitmap
 * @brief 获取指定位的值
 * @param[in] x 待操作的数值
 * @param[in] bit 位索引
 * @return 第bit位的值(0或非0)
 * @note 结果需配合条件判断使用，非直接返回0/1
 */
#define BIT_GET(x, bit) ((x) & (_ONE(x) << (bit)))

/**
 * @ingroup los_bitmap
 * @brief 获取指定位并右移至最低位
 * @param[in] x 待操作的数值
 * @param[in] bit 位索引
 * @return 第bit位的值(0或1)
 * @note 通过与1操作确保只返回0或1
 */
#define BIT_SHIFT(x, bit) (((x) >> (bit)) & 1)

/**
 * @ingroup los_bitmap
 * @brief 获取指定位段的值(保留原位)
 * @param[in] x 待操作的数值
 * @param[in] high 高位索引(包含)
 * @param[in] low 低位索引(包含)
 * @return 位段[low-high]的值(原位)
 * @note 结果保留原始位位置，未进行右移
 */
#define BITS_GET(x, high, low) ((x) & (((_ONE(x) << ((high) + 1)) - 1) & ~((_ONE(x) << (low)) - 1)))

/**
 * @ingroup los_bitmap
 * @brief 获取指定位段的值并右移至低位
 * @param[in] x 待操作的数值
 * @param[in] high 高位索引(包含)
 * @param[in] low 低位索引(包含)
 * @return 位段[low-high]的值(右移至最低位)
 * @note 位段宽度为(high-low+1)位
 */
#define BITS_SHIFT(x, high, low) (((x) >> (low)) & ((_ONE(x) << ((high) - (low) + 1)) - 1))

/**
 * @ingroup los_bitmap
 * @brief 检查指定位是否为1
 * @param[in] x 待操作的数值
 * @param[in] bit 位索引
 * @return 1-位为1，0-位为0
 * @note 与BIT_GET的区别是返回明确的0/1值
 */
#define BIT_SET(x, bit) (((x) & (_ONE(x) << (bit))) ? 1 : 0)

/* 位图操作宏定义 - 基于字(WORD) */
/**
 * @ingroup los_bitmap
 * @brief 每个字(WORD)包含的位数
 * @note 取决于UINTPTR类型宽度，32位系统为32，64位系统为64
 */
#define BITMAP_BITS_PER_WORD (sizeof(UINTPTR) * 8)

/**
 * @ingroup los_bitmap
 * @brief 计算存储指定位数所需的字数量
 * @param[in] x 总位数
 * @return 所需字数量
 * @note 使用向上取整算法: (x + 每字位数 - 1) / 每字位数
 */
#define BITMAP_NUM_WORDS(x) (((x) + BITMAP_BITS_PER_WORD - 1) / BITMAP_BITS_PER_WORD)

/**
 * @ingroup los_bitmap
 * @brief 计算指定位所在的字索引
 * @param[in] x 位索引
 * @return 字索引
 */
#define BITMAP_WORD(x) ((x) / BITMAP_BITS_PER_WORD)

/**
 * @ingroup los_bitmap
 * @brief 计算指定位在字内的偏移
 * @param[in] x 位索引
 * @return 字内偏移(0到BITMAP_BITS_PER_WORD-1)
 * @note 使用位运算优化: x & (每字位数 - 1)
 */
#define BITMAP_BIT_IN_WORD(x) ((x) & (BITMAP_BITS_PER_WORD - 1))

/**
 * @ingroup los_bitmap
 * @brief 计算起始位所在字的掩码
 * @param[in] start 起始位索引
 * @return 从start位开始到字尾的掩码
 * @note 高位补1，低位清0
 */
#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITMAP_BITS_PER_WORD))

/**
 * @ingroup los_bitmap
 * @brief 计算结束位所在字的掩码
 * @param[in] nbits 总位数
 * @return 从字开始到结束位的掩码
 * @note 若刚好是整字则返回全1掩码(~0UL)
 */
#define BITMAP_LAST_WORD_MASK(nbits) \
    (((nbits) % BITMAP_BITS_PER_WORD) ? (1UL << ((nbits) % BITMAP_BITS_PER_WORD)) - 1 : ~0UL)

/* 位图操作宏定义 - 基于整数(INT) */
/**
 * @ingroup los_bitmap
 * @brief 每个整数(INT)包含的位数
 * @note 取决于INTPTR类型宽度，通常与BITMAP_BITS_PER_WORD相同
 */
#define BITMAP_BITS_PER_INT (sizeof(INTPTR) * 8)

/**
 * @ingroup los_bitmap
 * @brief 计算指定位在整数内的偏移
 * @param[in] x 位索引
 * @return 整数内偏移(0到BITMAP_BITS_PER_INT-1)
 */
#define BITMAP_BIT_IN_INT(x) ((x) & (BITMAP_BITS_PER_INT - 1))

/**
 * @ingroup los_bitmap
 * @brief 计算指定位所在的整数索引
 * @param[in] x 位索引
 * @return 整数索引
 */
#define BITMAP_INT(x) ((x) / BITMAP_BITS_PER_INT)

/**
 * @ingroup los_bitmap
 * @brief 生成指定位数的位掩码
 * @param[in] x 位数
 * @return 位掩码值
 * @note 若x大于等于字长则返回全1掩码(0UL - 1)
 */
#define BIT_MASK(x) (((x) >= sizeof(UINTPTR) * 8) ? (0UL - 1) : ((1UL << (x)) - 1))

/**
 * @ingroup los_bitmap
 * @brief 无效位索引标识
 * @note 有效位索引范围是0到31，超出此范围返回该值
 */
#define LOS_INVALID_BIT_INDEX 32

/**
 * @ingroup los_bitmap
 * @brief 设置位图中指定的位
 *
 * @par 描述
 * 该API用于将位图变量中指定位置的位设置为1
 * @attention
 * <ul>
 * <li>当pos值大于31时，将设置位图的(pos & 0x1f)位(即对32取模)</li>
 * </ul>
 * @param[in] bitmap 位图变量指针
 * @param[in] pos    要设置的位编号
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_bitmap.h: 包含该API声明的头文件</li></ul>
 * @see LOS_BitmapClr
 */
VOID LOS_BitmapSet(UINT32 *bitmap, UINT16 pos);

/**
 * @ingroup los_bitmap
 * @brief 清除位图中指定的位
 *
 * @par 描述
 * 该API用于将位图变量中指定位置的位清除为0
 * @attention
 * <ul>
 * <li>当pos值大于31时，将清除位图的(pos & 0x1f)位(即对32取模)</li>
 * </ul>
 * @param[in] bitmap 位图变量指针
 * @param[in] pos    要清除的位编号
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_bitmap.h: 包含该API声明的头文件</li></ul>
 * @see LOS_BitmapSet
 */
VOID LOS_BitmapClr(UINT32 *bitmap, UINT16 pos);

/**
 * @ingroup los_bitmap
 * @brief 查找位图中最低的置位(1)位
 *
 * @par 描述
 * 该API用于查找位图变量中值为1的最低位，并返回其位索引
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 * @param[in] bitmap 位图变量
 *
 * @retval UINT16 最低置位位的索引
 * @par 依赖
 * <ul><li>los_bitmap.h: 包含该API声明的头文件</li></ul>
 * @see LOS_HighBitGet
 */
UINT16 LOS_LowBitGet(UINT32 bitmap);

/**
 * @ingroup los_bitmap
 * @brief 查找位图中最高的置位(1)位
 *
 * @par 描述
 * 该API用于查找位图变量中值为1的最高位，并返回其位索引
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 * @param[in] bitmap 位图变量
 *
 * @retval UINT16 最高置位位的索引
 * @par 依赖
 * <ul><li>los_bitmap.h: 包含该API声明的头文件</li></ul>
 * @see LOS_LowBitGet
 */
UINT16 LOS_HighBitGet(UINT32 bitmap);

/**
 * @ingroup los_bitmap
 * @brief 从最低位开始查找第一个清零(0)位
 *
 * @par 描述
 * 该API用于从最低有效位(LSB)开始查找第一个值为0的位，并返回其位索引
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 * @param[in] bitmap 位图指针
 * @param[in] numBits 要检查的总位数
 *
 * @retval int 从LSB开始的第一个清零位索引
 * @par 依赖
 * <ul><li>los_bitmap.h: 包含该API声明的头文件</li></ul>
 * @see LOS_BitmapFfz
 */
int LOS_BitmapFfz(UINTPTR *bitmap, UINT32 numBits);

/**
 * @ingroup los_bitmap
 * @brief 从起始位开始连续设置指定位数
 *
 * @par 描述
 * 该API用于从指定起始位开始，连续将指定位数的位设置为1
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 * @param[in] bitmap 位图指针
 * @param[in] start 起始位
 * @param[in] numsSet 要设置的位数
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_bitmap.h: 包含该API声明的头文件</li></ul>
 * @see LOS_BitmapClrNBits
 */
VOID LOS_BitmapSetNBits(UINTPTR *bitmap, UINT32 start, UINT32 numsSet);

/**
 * @ingroup los_bitmap
 * @brief 从起始位开始连续清除指定位数
 *
 * @par 描述
 * 该API用于从指定起始位开始，连续将指定位数的位清除为0
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 * @param[in] bitmap 位图指针
 * @param[in] start 起始位
 * @param[in] numsClear 要清除的位数
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_bitmap.h: 包含该API声明的头文件</li></ul>
 * @see LOS_BitmapSetNBits
 */
VOID LOS_BitmapClrNBits(UINTPTR *bitmap, UINT32 start, UINT32 numsClear);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_BITMAP_H */
