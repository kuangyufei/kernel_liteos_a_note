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

#include "capability_api.h"
#include "errno.h"
#include "fs/fs_operation.h"
#include "fs/file.h"
#include "string.h"
#include "stdlib.h"
#include "sys/stat.h"
#include "vnode.h"
#include "fs/mount.h"
#include <fcntl.h>

/****************************************************************************
 * Static Functions
 ****************************************************************************/
/****************************************************************************
 * Name: chattr
 *
 * Returned Value:
 *   Zero on success; -1 on failure with errno set:
 *
 ****************************************************************************/

int chattr(const char *pathname, struct IATTR *attr)
{
    struct Vnode *vnode = NULL;
    int ret;

    if (pathname == NULL || attr == NULL) {
        set_errno(EINVAL);
        return VFS_ERROR;
    }

    VnodeHold();
    ret = VnodeLookup(pathname, &vnode, 0);
    if (ret != LOS_OK) {
        goto errout_with_lock;
    }

    if ((vnode->originMount) && (vnode->originMount->mountFlags & MS_RDONLY)) {
        ret = -EROFS;
        goto errout_with_lock;
    }

    /* The way we handle the stat depends on the type of vnode that we
     * are dealing with.
     */

    if (vnode->vop != NULL && vnode->vop->Chattr != NULL) {
        ret = vnode->vop->Chattr(vnode, attr);
    } else {
        ret = -ENOSYS;
    }
    VnodeDrop();

    if (ret < 0) {
        goto errout;
    }

    return OK;

    /* Failure conditions always set the errno appropriately */

errout_with_lock:
    VnodeDrop();
errout:
    set_errno(-ret);
    return VFS_ERROR;
}
