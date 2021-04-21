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

#ifndef _VFS_UTIL_H_
#define _VFS_UTIL_H_

#include "los_list.h"

typedef LOS_DL_LIST LIST_HEAD;
typedef LOS_DL_LIST LIST_ENTRY;

#define FNV1_32_INIT ((uint32_t) 33554467UL)
#define FNV1_64_INIT ((uint64_t) 0xcbf29ce484222325ULL)

#define FNV_32_PRIME ((uint32_t) 0x01000193UL)
#define FNV_64_PRIME ((uint64_t) 0x100000001b3ULL)

#define V_CREATE     0x0001
#define V_CACHE      0x0002
#define V_DUMMY      0x0004


static __inline uint32_t fnv_32_buf(const void *buf, size_t len, uint32_t hval)
{
    const uint8_t *s = (const uint8_t *)buf;

    while (len-- != 0) {
        hval *= FNV_32_PRIME;
        hval ^= *s++;
    }
    return hval;
}

int vfs_normalize_path(const char *directory, const char *filename, char **pathname);
int vfs_normalize_pathat(int fd, const char *filename, char **pathname);

#endif /* !_VFS_UTIL_H_ */
