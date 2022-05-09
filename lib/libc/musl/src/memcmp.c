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

int memcmp(const void *str1, const void *str2, size_t n)
{

    const unsigned char *s1 = str1;
    const unsigned char *s2 = str2;
    size_t num = n;

    while (num >= SIZE_U64) {
        if (*(const uint64_t *)(s1) != *(const uint64_t *)(s2)) {
            goto L4_byte_cmp;
        }
        s1 += SIZE_U64;
        s2 += SIZE_U64;
        num -= SIZE_U64;
    }
    if (num == 0) {
        return 0;
    }

L4_byte_cmp:
    if (num >= SIZE_U32) {
        if (*(const uint32_t *)(s1) != *(const uint32_t *)(s2)) {
            goto L4_byte_diff;
        }
        s1 += SIZE_U32;
        s2 += SIZE_U32;
        num -= SIZE_U32;
    }
    if (num == 0) {
        return 0;
    }
L4_byte_diff:
    for (; num && (*s1 == *s2); num--, s1++, s2++) {
    }
    return num ? *s1 - *s2 : 0;
}