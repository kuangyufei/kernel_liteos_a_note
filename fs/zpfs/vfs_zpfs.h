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

#ifndef ZPFS_VFS_ZPFS_H
#define ZPFS_VFS_ZPFS_H

#include <fs/fs.h>

#include "compiler.h"
#include "los_base.h"
#include "los_typedef.h"

#ifdef LOSCFG_FS_ZPFS

#define ZPFS_NAME "zpfs"
#define ZPFS_LEVELS 3

typedef struct ZpfsDir {
    char *relPath;
    int  index;
    int  openEntry;
} ZpfsDir;

typedef struct ZpfsEntry {
    struct inode *mountedInode;
    char         *mountedPath;
    char         *mountedRelpath;
} ZpfsEntry;

typedef struct ZpfsConfig {
    char *patchTarget;
    struct inode *patchTargetInode;
    struct inode *patchInode;

    int  entryCount;
    ZpfsEntry orgEntry[ZPFS_LEVELS];
} ZpfsConfig;

ZpfsConfig *GetZpfsConfig(const struct inode *inode);
int ZpfsPrepare(const char *source, const char *target, struct inode **inodePtr, bool force);
void ZpfsFreeConfig(ZpfsConfig *zpfsCfg);
void ZpfsCleanUp(const void *node, const char *target);
bool IsZpfsFileSystem(struct inode *inode);

#endif /* LOSCFG_FS_ZPFS */

#endif /* ZPFS_VFS_ZPFS_H */
