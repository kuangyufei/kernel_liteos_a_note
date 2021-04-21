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
#include "vfs_config.h"
#include "sys/stat.h"
#include "fs/vnode.h"
#include "string.h"
#include "stdlib.h"
#include "utime.h"
#include "fs_other.h"

/****************************************************************************
 * Global Functions
 ****************************************************************************/

int utime(const char *path, const struct utimbuf *ptimes)
{
    int ret;
    char *fullpath = NULL;
    struct Vnode *vnode = NULL;
    time_t cur_sec;
    struct IATTR attr = {0};

    /* Sanity checks */


    if (path == NULL) {
        ret = -EINVAL;
        goto errout;
    }

    if (!path[0]) {
        ret = -ENOENT;
        goto errout;
    }

    ret = vfs_normalize_path((const char *)NULL, path, &fullpath);
    if (ret < 0) {
        goto errout;
    }

    /* Get the vnode for this file */
    VnodeHold();
    ret = VnodeLookup(fullpath, &vnode, 0);
    if (ret != LOS_OK) {
        VnodeDrop();
        goto errout_with_path;
    }

    if (vnode->vop && vnode->vop->Chattr) {
        if (ptimes == NULL) {
            /* get current seconds */
            cur_sec = time(NULL);
            attr.attr_chg_atime = cur_sec;
            attr.attr_chg_mtime = cur_sec;
        } else {
            attr.attr_chg_atime = ptimes->actime;
            attr.attr_chg_mtime = ptimes->modtime;
        }
        attr.attr_chg_valid = CHG_ATIME | CHG_MTIME;
        ret = vnode->vop->Chattr(vnode, &attr);
        if (ret != OK) {
            VnodeDrop();
            goto errout_with_path;
        }
    } else {
        ret = -ENOSYS;
        VnodeDrop();
        goto errout_with_path;
    }
    VnodeDrop();

    /* Successfully stat'ed the file */
    free(fullpath);

    return OK;

    /* Failure conditions always set the errno appropriately */

errout_with_path:
    free(fullpath);
errout:

    if (ret != 0) {
        set_errno(-ret);
    }
    return VFS_ERROR;
}
