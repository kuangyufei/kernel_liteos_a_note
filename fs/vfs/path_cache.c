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

#include "path_cache.h"
#include "los_config.h"
#include "los_hash.h"
#include "stdlib.h"
#include "limits.h"
#include "vnode.h"

#define PATH_CACHE_HASH_MASK (LOSCFG_MAX_PATH_CACHE_SIZE - 1)
LIST_HEAD g_pathCacheHashEntrys[LOSCFG_MAX_PATH_CACHE_SIZE];
#ifdef LOSCFG_DEBUG_VERSION
static int g_totalPathCacheHit = 0;
static int g_totalPathCacheTry = 0;
#define TRACE_TRY_CACHE() do { g_totalPathCacheTry++; } while (0)
#define TRACE_HIT_CACHE(pc) do { pc->hit++; g_totalPathCacheHit++; } while (0)

void ResetPathCacheHitInfo(int *hit, int *try)
{
    *hit = g_totalPathCacheHit;
    *try = g_totalPathCacheTry;
    g_totalPathCacheHit = 0;
    g_totalPathCacheTry = 0;
}
#else
#define TRACE_TRY_CACHE()
#define TRACE_HIT_CACHE(pc)
#endif
//路径缓存初始化
int PathCacheInit(void)
{
    for (int i = 0; i < LOSCFG_MAX_PATH_CACHE_SIZE; i++) {
        LOS_ListInit(&g_pathCacheHashEntrys[i]);
    }
    return LOS_OK;
}

void PathCacheDump(void)
{
    PRINTK("-------->pathCache dump in\n");
    for (int i = 0; i < LOSCFG_MAX_PATH_CACHE_SIZE; i++) {
        struct PathCache *pc = NULL;
        LIST_HEAD *nhead = &g_pathCacheHashEntrys[i];

        LOS_DL_LIST_FOR_EACH_ENTRY(pc, nhead, struct PathCache, hashEntry) {
            PRINTK("    pathCache dump hash %d item %s %p %p %d\n", i,
                pc->name, pc->parentVnode, pc->childVnode, pc->nameLen);
        }
    }
    PRINTK("-------->pathCache dump out\n");
}

void PathCacheMemoryDump(void)
{
    int pathCacheNum = 0;
    int nameSum = 0;
    for (int i = 0; i < LOSCFG_MAX_PATH_CACHE_SIZE; i++) {
        LIST_HEAD *dhead = &g_pathCacheHashEntrys[i];
        struct PathCache *dent = NULL;

        LOS_DL_LIST_FOR_EACH_ENTRY(dent, dhead, struct PathCache, hashEntry) {
            pathCacheNum++;
            nameSum += dent->nameLen;
        }
    }
    PRINTK("pathCache number = %d\n", pathCacheNum);
    PRINTK("pathCache memory size = %d(B)\n", pathCacheNum * sizeof(struct PathCache) + nameSum);
}

static uint32_t NameHash(const char *name, int len, struct Vnode *dvp)
{
    uint32_t hash;
    hash = LOS_HashFNV32aBuf(name, len, FNV1_32A_INIT);
    hash = LOS_HashFNV32aBuf(&dvp, sizeof(struct Vnode *), hash);
    return hash;
}

static void PathCacheInsert(struct Vnode *parent, struct PathCache *cache, const char* name, int len)
{
    int hash = NameHash(name, len, parent) & PATH_CACHE_HASH_MASK;
    LOS_ListAdd(&g_pathCacheHashEntrys[hash], &cache->hashEntry);
}

struct PathCache *PathCacheAlloc(struct Vnode *parent, struct Vnode *vnode, const char *name, uint8_t len)
{
    struct PathCache *pc = NULL;
    size_t pathCacheSize;
    int ret;

    if (name == NULL || len > NAME_MAX || parent == NULL || vnode == NULL) {
        return NULL;
    }
    pathCacheSize = sizeof(struct PathCache) + len + 1;

    pc = (struct PathCache*)zalloc(pathCacheSize);
    if (pc == NULL) {
        PRINT_ERR("pathCache alloc failed, no memory!\n");
        return NULL;
    }

    ret = strncpy_s(pc->name, len + 1, name, len);
    if (ret != LOS_OK) {
        return NULL;
    }

    pc->parentVnode = parent;
    pc->nameLen = len;
    pc->childVnode = vnode;

    LOS_ListAdd((&(parent->childPathCaches)), (&(pc->childEntry)));
    LOS_ListAdd((&(vnode->parentPathCaches)), (&(pc->parentEntry)));

    PathCacheInsert(parent, pc, name, len);

    return pc;
}

int PathCacheFree(struct PathCache *pc)
{
    if (pc == NULL) {
        PRINT_ERR("pathCache free: invalid pathCache\n");
        return -ENOENT;
    }

    LOS_ListDelete(&pc->hashEntry);
    LOS_ListDelete(&pc->parentEntry);
    LOS_ListDelete(&pc->childEntry);
    free(pc);

    return LOS_OK;
}

int PathCacheLookup(struct Vnode *parent, const char *name, int len, struct Vnode **vnode)
{
    struct PathCache *pc = NULL;
    int hash = NameHash(name, len, parent) & PATH_CACHE_HASH_MASK;
    LIST_HEAD *dhead = &g_pathCacheHashEntrys[hash];

    TRACE_TRY_CACHE();
    LOS_DL_LIST_FOR_EACH_ENTRY(pc, dhead, struct PathCache, hashEntry) {
        if (pc->parentVnode == parent && pc->nameLen == len && !strncmp(pc->name, name, len)) {
            *vnode = pc->childVnode;
            TRACE_HIT_CACHE(pc);
            return LOS_OK;
        }
    }
    return -ENOENT;
}

static void FreeChildPathCache(struct Vnode *vnode)
{
    struct PathCache *item = NULL;
    struct PathCache *nextItem = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, nextItem, &(vnode->childPathCaches), struct PathCache, childEntry) {
        PathCacheFree(item);
    }
}

static void FreeParentPathCache(struct Vnode *vnode)
{
    struct PathCache *item = NULL;
    struct PathCache *nextItem = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, nextItem, &(vnode->parentPathCaches), struct PathCache, parentEntry) {
        PathCacheFree(item);
    }
}
//和长辈,晚辈告别,从此不再是父亲和孩子.
void VnodePathCacheFree(struct Vnode *vnode)
{
    if (vnode == NULL) {
        return;
    }
    FreeParentPathCache(vnode);
    FreeChildPathCache(vnode);
}

LIST_HEAD* GetPathCacheList()
{
    return g_pathCacheHashEntrys;
}
