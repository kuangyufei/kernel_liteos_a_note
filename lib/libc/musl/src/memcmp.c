/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include <string.h>
#include <stdint.h>

#define SIZE_U64    (sizeof(uint64_t))
#define SIZE_U32    (sizeof(uint32_t))
/**
 * @brief 比较两个内存区域的内容
 * 
 * 按字节比较两个内存块的前n个字节，返回第一个不相等字节的差值
 * 若所有字节相等则返回0
 * 
 * @param str1 指向第一个内存块的指针
 * @param str2 指向第二个内存块的指针
 * @param n    要比较的字节数
 * @return int 若str1 > str2返回正数，str1 < str2返回负数，相等返回0
 */
int memcmp(const void *str1, const void *str2, size_t n)
{
    // 将void指针转换为unsigned char指针以支持字节操作
    const unsigned char *s1 = str1;  // 第一个内存块的字节指针
    const unsigned char *s2 = str2;  // 第二个内存块的字节指针
    size_t num = n;                  // 剩余需要比较的字节数

    // 优化：先按64位(8字节)块比较，提高大内存块比较效率
    while (num >= SIZE_U64) {        // SIZE_U64通常定义为8(64位系统)
        // 比较当前64位块，若不相等则跳转到字节级比较
        if (*(const uint64_t *)(s1) != *(const uint64_t *)(s2)) {
            goto L4_byte_cmp;        // 发现64位块不相等，跳转到L4_byte_cmp标签
        }
        s1 += SIZE_U64;              // 移动指针到下一个64位块
        s2 += SIZE_U64;
        num -= SIZE_U64;             // 更新剩余字节数
    }
    if (num == 0) {                  // 所有字节比较完成且相等
        return 0;
    }

L4_byte_cmp:                          // 64位块比较不相等后的处理标签
    // 处理剩余字节(1-7字节)，先尝试32位(4字节)块比较
    if (num >= SIZE_U32) {           // SIZE_U32通常定义为4
        // 比较当前32位块，若不相等则跳转到字节级详细比较
        if (*(const uint32_t *)(s1) != *(const uint32_t *)(s2)) {
            goto L4_byte_diff;       // 发现32位块不相等，跳转到L4_byte_diff标签
        }
        s1 += SIZE_U32;              // 移动指针到下一个32位块
        s2 += SIZE_U32;
        num -= SIZE_U32;             // 更新剩余字节数
    }
    if (num == 0) {                  // 所有字节比较完成且相等
        return 0;
    }

L4_byte_diff:                        // 字节级详细比较标签
    // 逐个字节比较，直到发现不相等或比较完成
    for (; num && (*s1 == *s2); num--, s1++, s2++) {
        // 循环体为空，所有操作在for循环条件中完成
    }
    // 返回结果：若num>0表示找到不相等字节，返回其差值；否则返回0
    return num ? *s1 - *s2 : 0;
}