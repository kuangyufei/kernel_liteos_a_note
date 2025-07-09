/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
/**
 * @brief 将内存块的前n个字节设置为指定值
 *
 * @param dest 指向要填充的内存块的指针
 * @param c 要设置的值，该值以int形式传递，但函数使用该值的无符号字符形式
 * @param n 要设置为该值的字节数
 * @return void* 返回指向目标内存块的指针
 */
void *memset(void *dest, int c, size_t n)
{
    char *pos = dest;                  // 指向当前填充位置的指针
    uint32_t c32 = 0;                  // 32位填充值（用于优化对齐填充）
    uint64_t c64 = 0;                  // 64位填充值（用于优化对齐填充）
    size_t num = n;                    // 剩余需要填充的字节数

    if (num == 0) {                    // 如果填充字节数为0，直接返回
        return dest;
    }

    c = c & 0xFF;                      // 将输入值截断为无符号8位字符
    if (c) {                           // 如果填充值非0，构建32位和64位填充值
        c32 = c;
        c32 |= c32 << 8;               /* 8, 将8位字符扩展为16位 */
        c32 |= c32 << 16;              /* 16, 将16位值扩展为32位 */
        c64 = c32;
        c64 |= c64 << 32;              /* 32, 将32位值扩展为64位 */
    }

    // 处理地址未按8字节对齐的情况
    if (((uintptr_t)(pos) & 7) != 0) { /* 7, 检查地址低3位判断是否8字节对齐 */
        // 计算需要填充的字节数以使地址对齐（1-7字节）
        int unalignedCnt = 8 - ((uintptr_t)(pos) & 7); /* 7, 8, 计算对齐所需字节数 */
        if (num >= unalignedCnt) {
            num = num - unalignedCnt;  // 更新剩余字节数
        } else {
            unalignedCnt = num;        // 如果剩余字节不足，仅填充剩余部分
            num = 0;
        }
        // 逐个字节填充以实现地址对齐
        for (int loop = 1; loop <= unalignedCnt; ++loop) {
            *pos = (char)c;
            pos++;
        }
    }

    /* L32_byte_aligned - 按32字节块填充（最高效的批量填充） */
    while (num >= 32) { /* 32, 当剩余字节数大于等于32时 */
        *(uint64_t *)(pos) = c64;                      // 填充第1个64位（8字节）
        *(uint64_t *)(pos + 8) = c64; /* 8, 填充第2个64位（8字节） */
        *(uint64_t *)(pos + 16) = c64; /* 16, 填充第3个64位（8字节） */
        *(uint64_t *)(pos + 24) = c64; /* 24, 填充第4个64位（8字节） */
        num -= 32; /* 32, 减少32字节的剩余计数 */
        pos += 32; /* 32, 移动指针32字节 */
    }
    if (num == 0) {                    // 如果已填充完成，返回结果
        return dest;
    }

    /* L16_byte_aligned - 按16字节块填充 */
    if (num >= 16) { /* 16, 当剩余字节数大于等于16时 */
        *(uint64_t *)(pos) = c64;                      // 填充第1个64位（8字节）
        *(uint64_t *)(pos + 8) = c64; /* 8, 填充第2个64位（8字节） */
        num -= 16; /* 16, 减少16字节的剩余计数 */
        pos += 16; /* 16, 移动指针16字节 */
        if (num == 0) {
            return dest;
        }
    }

    /* L8_byte_aligned - 按8字节块填充 */
    if (num >= 8) { /* 8, 当剩余字节数大于等于8时 */
        *(uint64_t *)(pos) = c64;                      // 填充64位（8字节）
        num -= 8; /* 8, 减少8字节的剩余计数 */
        pos += 8; /* 8, 移动指针8字节 */
        if (num == 0) {
            return dest;
        }
    }

    /* L4_byte_aligned - 按4字节块填充 */
    if (num >= 4) { /* 4, 当剩余字节数大于等于4时 */
        *(uint32_t *)(pos) = c32;                      // 填充32位（4字节）
        num -= 4; /* 4, 减少4字节的剩余计数 */
        pos += 4; /* 4, 移动指针4字节 */
        if (num == 0) {
            return dest;
        }
    }
    // 填充剩余的1-3字节
    while (num--) {
        *pos++ = c;
    }

    return dest;
}