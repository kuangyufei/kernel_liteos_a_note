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

#include "los_mux.h"
#include "fs/vnode.h"


#define VNODE_HASH_BUCKETS 128

LIST_HEAD g_vnodeHashEntrys[VNODE_HASH_BUCKETS];
uint32_t g_vnodeHashMask = VNODE_HASH_BUCKETS - 1;
uint32_t g_vnodeHashSize = VNODE_HASH_BUCKETS;

static LosMux g_vnodeHashMux;

int VnodeHashInit(void)
{
    int ret;
    for (int i = 0; i < g_vnodeHashSize; i++) {
        LOS_ListInit(&g_vnodeHashEntrys[i]);
    }

    ret = LOS_MuxInit(&g_vnodeHashMux, NULL);
    if (ret != LOS_OK) {
        PRINT_ERR("Create mutex for vnode hash list fail, status: %d", ret);
        return ret;
    }

    return LOS_OK;
}

void VnodeHashDump(void)
{
    PRINTK("-------->VnodeHashDump in\n");
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);
    for (int i = 0; i < g_vnodeHashSize; i++) {
        LIST_HEAD *nhead = &g_vnodeHashEntrys[i];
        struct Vnode *node = NULL;

        LOS_DL_LIST_FOR_EACH_ENTRY(node, nhead, struct Vnode, hashEntry) {
            PRINTK("    vnode dump: col %d item %p\n", i, node);
        }
    }
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
    PRINTK("-------->VnodeHashDump out\n");
}

uint32_t VfsHashIndex(struct Vnode *vnode)
{
    if (vnode == NULL) {
        return -EINVAL;
    }
    return (vnode->hash + vnode->originMount->hashseed);
}

static LOS_DL_LIST *VfsHashBucket(const struct Mount *mp, uint32_t hash)
{
    return (&g_vnodeHashEntrys[(hash + mp->hashseed) & g_vnodeHashMask]);
}

int VfsHashGet(const struct Mount *mount, uint32_t hash, struct Vnode **vnode, VfsHashCmp *fn, void *arg)
{
    struct Vnode *curVnode = NULL;

    if (mount == NULL || vnode == NULL) {
        return -EINVAL;
    }

    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);
    LOS_DL_LIST *list = VfsHashBucket(mount, hash);
    LOS_DL_LIST_FOR_EACH_ENTRY(curVnode, list, struct Vnode, hashEntry) {
        if (curVnode->hash != hash) {
            continue;
        }
        if (curVnode->originMount != mount) {
            continue;
        }
        if (fn != NULL && fn(curVnode, arg)) {
            continue;
        }
        (void)LOS_MuxUnlock(&g_vnodeHashMux);
        *vnode = curVnode;
        return LOS_OK;
    }
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
    *vnode = NULL;
    return LOS_NOK;
}

void VfsHashRemove(struct Vnode *vnode)
{
    if (vnode == NULL) {
        return;
    }
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);
    LOS_ListDelete(&vnode->hashEntry);
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
}

int VfsHashInsert(struct Vnode *vnode, uint32_t hash)
{
    if (vnode == NULL) {
        return -EINVAL;
    }
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);
    vnode->hash = hash;
    LOS_ListHeadInsert(VfsHashBucket(vnode->originMount, hash), &vnode->hashEntry);
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
    return LOS_OK;
}
