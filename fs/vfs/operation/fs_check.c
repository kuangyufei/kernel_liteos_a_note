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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "errno.h"
#include "stdlib.h"
#include "string.h"
#include "dirent.h"
#include "unistd.h"
#include "sys/select.h"
#include "sys/stat.h"
#include "sys/prctl.h"
#include "fs/dirent_fs.h"
#include "fs/vnode.h"

/****************************************************************************
 * Name: fscheck
 ****************************************************************************/
int fscheck(const char *path)
{
    int ret;
    struct Vnode *vnode = NULL;
    struct fs_dirent_s *dir = NULL;

    /* Find the node matching the path. */
    VnodeHold();
    ret = VnodeLookup(path, &vnode, 0);
    if (ret != OK) {
        VnodeDrop();
        goto errout;
    }

    dir = (struct fs_dirent_s *)zalloc(sizeof(struct fs_dirent_s));
    if (!dir) {
        /* Insufficient memory to complete the operation.*/
        ret = -ENOMEM;
        VnodeDrop();
        goto errout;
    }

    if (vnode->vop && vnode->vop->Fscheck) {
        ret = vnode->vop->Fscheck(vnode, dir);
        if (ret != OK) {
            VnodeDrop();
            goto errout_with_direntry;
        }
    } else {
        ret = -ENOSYS;
        VnodeDrop();
        goto errout_with_direntry;
    }
    VnodeDrop();

    free(dir);
    return 0;

errout_with_direntry:
    free(dir);
errout:
    set_errno(-ret);
    return VFS_ERROR;
}
