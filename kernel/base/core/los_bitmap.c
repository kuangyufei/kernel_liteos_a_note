/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "los_bitmap.h"
#include "los_printf.h"
#include "los_toolchain.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_BITMAP_MASK 0x1FU //
#define OS_BITMAP_WORD_MASK ~0UL

/* find first zero bit starting from LSB */
STATIC INLINE UINT16 Ffz(UINTPTR x)
{//__builtin_ffsl: 返回右起第一个1的位置，函数来自 glibc
    return __builtin_ffsl(~x) - 1;//从LSB开始查找第一个零位 LSB(最低有效位) 对应 最高有效位(MSB)
}
//设置对应位置
VOID LOS_BitmapSet(UINT32 *bitmap, UINT16 pos)
{
    if (bitmap == NULL) {
        return;
    }

    *bitmap |= 1U << (pos & OS_BITMAP_MASK);//在对应位上置1
}
//对应位上置0
VOID LOS_BitmapClr(UINT32 *bitmap, UINT16 pos)
{
    if (bitmap == NULL) {
        return;
    }

    *bitmap &= ~(1U << (pos & OS_BITMAP_MASK));//在对应位上置0
}
/********************************************************
杂项算术指令
CLZ 用于计算操作数最高端0的个数，这条指令主要用于一下两个场合
　　计算操作数规范化（使其最高位为1）时需要左移的位数
　　确定一个优先级掩码中最高优先级

********************************************************/
//从高到低 返回第一个为1的位数， 例如: 00110110 返回 5
UINT16 LOS_HighBitGet(UINT32 bitmap)
{
    if (bitmap == 0) {
        return LOS_INVALID_BIT_INDEX;
    }

    return (OS_BITMAP_MASK - CLZ(bitmap));
}
//从低到高 返回第一个为1的位数， 例如: 00110110 返回 2
UINT16 LOS_LowBitGet(UINT32 bitmap)
{
    if (bitmap == 0) {
        return LOS_INVALID_BIT_INDEX;
    }

    return CTZ(bitmap);//
}
//从start位置开始设置numsSet个bit位 置1 
VOID LOS_BitmapSetNBits(UINTPTR *bitmap, UINT32 start, UINT32 numsSet)
{
    UINTPTR *p = bitmap + BITMAP_WORD(start);
    const UINT32 size = start + numsSet;
    UINT16 bitsToSet = BITMAP_BITS_PER_WORD - (start % BITMAP_BITS_PER_WORD);
    UINTPTR maskToSet = BITMAP_FIRST_WORD_MASK(start);

    while (numsSet > bitsToSet) {
        *p |= bitsToSet;
        numsSet -= bitsToSet;
        bitsToSet = BITMAP_BITS_PER_WORD;
        maskToSet = OS_BITMAP_WORD_MASK;
        p++;
    }
    if (numsSet) {
        maskToSet &= BITMAP_LAST_WORD_MASK(size);
        *p |= maskToSet;
    }
}
//从start位置开始清除numsSet个bit位 置0
VOID LOS_BitmapClrNBits(UINTPTR *bitmap, UINT32 start, UINT32 numsClear)
{
    UINTPTR *p = bitmap + BITMAP_WORD(start);
    const UINT32 size = start + numsClear;
    UINT16 bitsToClear = BITMAP_BITS_PER_WORD - (start % BITMAP_BITS_PER_WORD);
    UINTPTR maskToClear = BITMAP_FIRST_WORD_MASK(start);

    while (numsClear >= bitsToClear) {
        *p &= ~maskToClear;
        numsClear -= bitsToClear;
        bitsToClear = BITMAP_BITS_PER_WORD;
        maskToClear = OS_BITMAP_WORD_MASK;
        p++;
    }
    if (numsClear) {
        maskToClear &= BITMAP_LAST_WORD_MASK(size);
        *p &= ~maskToClear;
    }
}
//从numBits位置开始找到第一个0位
INT32 LOS_BitmapFfz(UINTPTR *bitmap, UINT32 numBits)
{
    INT32 bit, i;

    for (i = 0; i < BITMAP_NUM_WORDS(numBits); i++) {
        if (bitmap[i] == OS_BITMAP_WORD_MASK) {
            continue;
        }
        bit = i * BITMAP_BITS_PER_WORD + Ffz(bitmap[i]);
        if (bit < numBits) {
            return bit;
        }
        return -1;
    }
    return -1;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
