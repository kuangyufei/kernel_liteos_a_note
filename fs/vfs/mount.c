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

#include "fs/mount.h"
#include "fs/vfs_util.h"
#include "fs/path_cache.h"
#include "fs/vnode.h"
#ifdef LOSCFG_DRIVERS_RANDOM
#include "hisoc/random.h"
#else
#include "stdlib.h"
#endif

static LIST_HEAD *g_mountList = NULL;

struct Mount* MountAlloc(struct Vnode* vnodeBeCovered, struct MountOps* fsop)
{
    struct Mount* mnt = (struct Mount*)zalloc(sizeof(struct Mount));
    if (mnt == NULL) {
        PRINT_ERR("MountAlloc failed no memory!\n");
        return NULL;
    }

    LOS_ListInit(&mnt->activeVnodeList);
    LOS_ListInit(&mnt->vnodeList);

    mnt->vnodeBeCovered = vnodeBeCovered;
    vnodeBeCovered->newMount = mnt;
#ifdef LOSCFG_DRIVERS_RANDOM
    HiRandomHwInit();
    (VOID)HiRandomHwGetInteger(&mnt->hashseed);
    HiRandomHwDeinit();
#else
    mnt->hashseed = (uint32_t)random();
#endif
    return mnt;
}

LIST_HEAD* GetMountList()
{
    if (g_mountList == NULL) {
        g_mountList = zalloc(sizeof(LIST_HEAD));
        if (g_mountList == NULL) {
            PRINT_ERR("init mount list failed, no memory.");
            return NULL;
        }
        LOS_ListInit(g_mountList);
    }
    return g_mountList;
}
