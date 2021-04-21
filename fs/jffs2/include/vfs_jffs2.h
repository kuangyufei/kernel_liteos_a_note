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

#ifndef __VFS_JFFS2_H__
#define __VFS_JFFS2_H__

#include <dirent.h>
#include <time.h>
#include "los_config.h"
#include "los_typedef.h"
#include "los_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifndef NOR_FLASH_BOOT_SIZE
#define NOR_FLASH_BOOT_SIZE 0x100000
#endif

#define BLOCK_SIZE    4096
#define JFFS2_NODE_HASH_BUCKETS 128
#define JFFS2_NODE_HASH_MASK (JFFS2_NODE_HASH_BUCKETS - 1)


#define JFFS2_WAITING_FOREVER              -1    /* Block forever until get resource. */

/* block/char not support */
#define JFFS2_F_I_RDEV_MIN(f) (0)
#define JFFS2_F_I_RDEV_MAJ(f) (0)

static inline unsigned int full_name_hash(const unsigned char *name, unsigned int len)
{
    unsigned hash = 0;
    while (len--) {
        hash = (hash << 4) | (hash >> 28);
        hash ^= *(name++);
    }
    return hash;
}

int Jffs2MutexCreate(void);
void Jffs2MutexDelete(void);
void Jffs2NodeLock(void);   /* lock for inode ops */
void Jffs2NodeUnlock(void);
time_t Jffs2CurSec(void);


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
