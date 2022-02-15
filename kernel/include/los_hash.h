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

#ifndef _LOS_HASH_H
#define _LOS_HASH_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/*
http://www.isthe.com/chongo/tech/comp/fnv/
FNV算法简介
FNV算法属于非密码学哈希函数，它最初由Glenn Fowler和Kiem-Phong Vo于1991年在IEEE POSIX P1003.2上首先提出，
最后由Landon Curt Noll 完善，故该算法以三人姓的首字母命名。

FNV算法目前有三种，分别是FNV-1，FNV-1a和FNV-0，但是FNV-0算法已经被丢弃了。FNV算法的哈希结果有32、64、128、256、512和1024位等长度。
如果需要哈希结果长度不属于以上任意一种，也可以采用根据Changing the FNV hash size - xor-folding上面的指导进行变换得到。
FNV-1算法过程如下:
	hash = offset_basis
	for each octet_of_data to be hashed
	hash = hash * FNV_prime
	hash = hash xor octet_of_data
	return hash
	
FNV-1a算法过程如下:
	hash = offset_basis
	for each octet_of_data to be hashed
	hash = hash xor octet_of_data
	hash = hash * FNV_prime
	return hash
FNV-0算法过程如下:	
	hash = 0
	for each octet_of_data to be hashed
	hash = hash * FNV_prime
	hash = hash XOR octet_of_data
	return hash
*/

#define FNV1_32A_INIT ((UINT32)0x811c9dc5)

/*
 * 32 bit magic FNV-1 prime
 */
#define FNV_32_PRIME ((UINT32)0x01000193)

LITE_OS_SEC_ALW_INLINE STATIC INLINE UINT32 LOS_HashFNV32aBuf(const VOID *buf, size_t len, UINT32 hval)
{
    const UINT8 *hashbuf = (const UINT8 *)buf;

    /*
     * FNV-1a hash each octet in the buffer
     */
    while (len-- != 0) {
        /* xor the bottom with the current octet */
        hval ^= (UINT32)*hashbuf++;

        /* multiply by the 32 bit FNV magic prime mod 2^32 */
        hval *= FNV_32_PRIME;
    }

    /* return our new hash value */
    return hval;
}

LITE_OS_SEC_ALW_INLINE STATIC INLINE UINT32 LOS_HashFNV32aStr(CHAR *str, UINT32 hval)
{
    UINT8 *s = (UINT8 *)str;

    /*
     * FNV-1a hash each octet in the buffer
     */
    while (*s) {
        /* xor the bottom with the current octet */
        hval ^= (UINT32)*s++;

        /* multiply by the 32 bit FNV magic prime mod 2^32 */
        hval *= FNV_32_PRIME;
    }

    /* return our new hash value */
    return hval;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HASH_H */
