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

#ifndef _MOUNT_H_
#define _MOUNT_H_

#include "los_mux.h"
#include "fs/vfs_util.h"
#include "fs/vnode.h"
#include <sys/stat.h>
#include <limits.h>

struct MountOps;

struct Mount {
    LIST_ENTRY mountList;              /* mount list */
    const struct MountOps *ops;        /* operations of mount */
    struct Vnode *vnodeBeCovered;      /* vnode we mounted on */
    struct Vnode *vnodeCovered;        /* syncer vnode */
    LIST_HEAD vnodeList;               /* list of vnodes */
    int vnodeSize;                     /* size of vnode list */
    LIST_HEAD activeVnodeList;         /* list of active vnodes */
    int activeVnodeSize;               /* szie of active vnodes list */
    void *data;                        /* private data */
    uint32_t hashseed;                 /* Random seed for vfshash */
    unsigned long mountFlags;          /* Flags for mount */
    char pathName[PATH_MAX];           /* path name of mount point */
};

struct MountOps {
    int (*Mount)(struct Mount *mount, struct Vnode *vnode, const void *data);
    int (*Unmount)(struct Mount *mount, struct Vnode **blkdriver);
    int (*Statfs)(struct Mount *mount, struct statfs *sbp);
};

struct Mount* MountAlloc(struct Vnode* vnode, struct MountOps* mop);
LIST_HEAD* GetMountList(void);
#endif
