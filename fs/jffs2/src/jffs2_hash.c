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

#include "jffs2_hash.h"

#ifdef LOSCFG_FS_JFFS

int Jffs2HashInit(LosMux *lock, LOS_DL_LIST *heads)
{
    int ret;
    for (int i = 0; i < JFFS2_NODE_HASH_BUCKETS; i++) {
        LOS_ListInit(&heads[i]);
    }

    ret = LOS_MuxInit(lock, NULL);
    if (ret != LOS_OK) {
        PRINT_ERR("Create mutex for vnode hash list fail, status: %d", ret);
        return ret;
    }

    return LOS_OK;
}

int Jffs2HashDeinit(LosMux *lock)
{
    int ret;
    ret = LOS_MuxDestroy(lock);
    if (ret != LOS_OK) {
        PRINT_ERR("Destroy mutex for vnode hash list fail, status: %d", ret);
        return ret;
    }

    return LOS_OK;
}

void Jffs2HashDump(LosMux *lock, LOS_DL_LIST *heads)
{
    PRINTK("-------->Jffs2HashDump in\n");
    (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);
    for (int i = 0; i < JFFS2_NODE_HASH_BUCKETS; i++) {
        LIST_HEAD *nhead = &heads[i];
        struct jffs2_inode *node = NULL;

        LOS_DL_LIST_FOR_EACH_ENTRY(node, nhead, struct jffs2_inode, i_hashlist) {
            PRINTK("    vnode dump: col %d item %p\n", i, node);
        }
    }
    (void)LOS_MuxUnlock(lock);
    PRINTK("-------->Jffs2HashDump out\n");
}

static LOS_DL_LIST *Jffs2HashBucket(LOS_DL_LIST *heads, const uint32_t ino)
{
    LOS_DL_LIST *head = &(heads[ino & JFFS2_NODE_HASH_MASK]);
    return head;
}

int Jffs2HashGet(LosMux *lock, LOS_DL_LIST *heads, const void *sb, const uint32_t ino, struct jffs2_inode **ppNode)
{
    struct jffs2_inode *node = NULL;

    while (1) {
        (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);
        LOS_DL_LIST *list = Jffs2HashBucket(heads, ino);
        LOS_DL_LIST_FOR_EACH_ENTRY(node, list, struct jffs2_inode, i_hashlist) {
            if (node->i_ino != ino)
                continue;
            if (node->i_sb != sb)
                continue;
            (void)LOS_MuxUnlock(lock);
            *ppNode = node;
            return 0;
        }
        (void)LOS_MuxUnlock(lock);
        *ppNode = NULL;
        return 0;
    }
}

void Jffs2HashRemove(LosMux *lock, struct jffs2_inode *node)
{
    (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);
    LOS_ListDelete(&node->i_hashlist);
    (void)LOS_MuxUnlock(lock);
}

int Jffs2HashInsert(LosMux *lock, LOS_DL_LIST *heads, struct jffs2_inode *node, const uint32_t ino)
{
    (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);
    LOS_ListHeadInsert(Jffs2HashBucket(heads, ino), &node->i_hashlist);
    (void)LOS_MuxUnlock(lock);
    return 0;
}

#endif

