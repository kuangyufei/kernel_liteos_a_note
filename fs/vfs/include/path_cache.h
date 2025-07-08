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

#ifndef _PATH_CACHE_H
#define _PATH_CACHE_H

#include "los_list.h"
#include "fs/mount.h"
#include "vnode.h"

/**
 * @brief 路径缓存结构体
 * @details 用于缓存文件系统路径与vnode之间的映射关系，提高路径查找效率
 */
struct PathCache {
    struct Vnode *parentVnode;    /* 指向父vnode的指针，即缓存项所属的目录vnode */
    struct Vnode *childVnode;     /* 指向子vnode的指针，即缓存项指向的目标vnode */
    LIST_ENTRY parentEntry;       /* 父vnode中缓存列表的链表项 */
    LIST_ENTRY childEntry;        /* 子vnode中缓存列表的链表项 */
    LIST_ENTRY hashEntry;         /* 哈希表桶中的链表项，用于快速查找 */
    uint8_t nameLen;              /* 路径组件名称的长度 */
#ifdef LOSCFG_DEBUG_VERSION
    int hit;                      /* 缓存命中计数器，调试版本有效 */
#endif
    char name[0];                 /* 路径组件名称，柔性数组存储实际字符串 */
};

int PathCacheInit(void);
int PathCacheFree(struct PathCache *cache);
struct PathCache *PathCacheAlloc(struct Vnode *parent, struct Vnode *vnode, const char *name, uint8_t len);
int PathCacheLookup(struct Vnode *parent, const char *name, int len, struct Vnode **vnode);
void VnodePathCacheFree(struct Vnode *vnode);
void PathCacheMemoryDump(void);
void PathCacheDump(void);
LIST_HEAD* GetPathCacheList(void);
#ifdef LOSCFG_DEBUG_VERSION
void ResetPathCacheHitInfo(int *hit, int *try);
#endif

#endif /* _PATH_CACHE_H */
