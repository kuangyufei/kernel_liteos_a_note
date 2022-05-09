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

void *memset(void *dest, int c, size_t n)
{
    char *pos = dest;
    uint32_t c32 = 0;
    uint64_t c64 = 0;
    size_t num = n;

    if (num == 0) {
        return dest;
    }

    c = c & 0xFF;
    if (c) {
        c32 = c;
        c32 |= c32 << 8; /* 8, Processed bits */
        c32 |= c32 << 16; /* 16, Processed bits */
        c64 = c32;
        c64 |= c64 << 32; /* 32, Processed bits */
    }

    if (((uintptr_t)(pos) & 7) != 0) { /* 7, Processed align */
        int unalignedCnt = 8 - ((uintptr_t)(pos) & 7); /* 7, 8, for calculate addr bits align */
        if (num >= unalignedCnt) {
            num = num - unalignedCnt;
        } else {
            unalignedCnt = num;
            num = 0;
        }
        for (int loop = 1; loop <= unalignedCnt; ++loop) {
            *pos = (char)c;
            pos++;
        }
    }

    /* L32_byte_aligned */
    while (num >= 32) { /* 32, byte aligned */
        *(uint64_t *)(pos) = c64;
        *(uint64_t *)(pos + 8) = c64; /* 8, size of uint64_t */
        *(uint64_t *)(pos + 16) = c64; /* 16, size of two uint64_t data */
        *(uint64_t *)(pos + 24) = c64; /* 24, size of three uint64_t data */
        num -= 32; /* 32, size of four uint64_t data */
        pos += 32; /* 32, size of four uint64_t data */
    }
    if (num == 0) {
        return dest;
    }

    /* L16_byte_aligned */
    if (num >= 16) { /* 16, byte aligned */
        *(uint64_t *)(pos) = c64;
        *(uint64_t *)(pos + 8) = c64; /* 8, size of uint64_t */
        num -= 16; /* 16, size of two uint64_t data */
        pos += 16; /* 16, size of two uint64_t data */
        if (num == 0) {
            return dest;
        }
    }

    /* L8_byte_aligned */
    if (num >= 8) { /* 8, byte aligned */
        *(uint64_t *)(pos) = c64;
        num -= 8; /* 8, size of uint64_t */
        pos += 8; /* 8, size of uint64_t */
        if (num == 0) {
            return dest;
        }
    }

    /* L4_byte_aligned */
    if (num >= 4) { /* 4, byte aligned */
        *(uint32_t *)(pos) = c32;
        num -= 4; /* 4, size of uint32_t */
        pos += 4; /* 4, size of uint32_t */
        if (num == 0) {
            return dest;
        }
    }
    while (num--) {
        *pos++ = c;
    }

    return dest;
}