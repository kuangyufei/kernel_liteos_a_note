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
//用哈希表只有一个目的,就是加快对索引节点对象的搜索
LIST_HEAD g_vnodeHashEntrys[VNODE_HASH_BUCKETS];//哈希桶链表组
uint32_t g_vnodeHashMask = VNODE_HASH_BUCKETS - 1;	//哈希掩码
uint32_t g_vnodeHashSize = VNODE_HASH_BUCKETS;	//哈希大小

static LosMux g_vnodeHashMux;//哈希表互斥量
//索引节点哈希表初始化
int VnodeHashInit(void)
{
    int ret;
    for (int i = 0; i < g_vnodeHashSize; i++) {//遍历初始化 128个双向链表
        LOS_ListInit(&g_vnodeHashEntrys[i]);
    }

    ret = LOS_MuxInit(&g_vnodeHashMux, NULL);
    if (ret != LOS_OK) {
        PRINT_ERR("Create mutex for vnode hash list fail, status: %d", ret);
        return ret;
    }

    return LOS_OK;
}
//打印全部 hash 表
void VnodeHashDump(void)
{
    PRINTK("-------->VnodeHashDump in\n");
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);//拿锁方式,永等
    for (int i = 0; i < g_vnodeHashSize; i++) {
        LIST_HEAD *nhead = &g_vnodeHashEntrys[i];
        struct Vnode *node = NULL;

        LOS_DL_LIST_FOR_EACH_ENTRY(node, nhead, struct Vnode, hashEntry) {//循环打印链表
            PRINTK("    vnode dump: col %d item %p\n", i, node);//类似矩阵
        }
    }
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
    PRINTK("-------->VnodeHashDump out\n");
}
//通过节点获取哈希索引值
uint32_t VfsHashIndex(struct Vnode *vnode)
{
    if (vnode == NULL) {
        return -EINVAL;
    }
    return (vnode->hash + vnode->originMount->hashseed);//用于定位在哈希表的下标
}
//通过哈希值和装载设备哈希种子获取哈希表索引
static LOS_DL_LIST *VfsHashBucket(const struct Mount *mp, uint32_t hash)
{
    return (&g_vnodeHashEntrys[(hash + mp->hashseed) & g_vnodeHashMask]);//g_vnodeHashMask确保始终范围在[0~g_vnodeHashMask]之间
}
//通过哈希值获取节点信息
int VfsHashGet(const struct Mount *mount, uint32_t hash, struct Vnode **vnode, VfsHashCmp *fn, void *arg)
{
    struct Vnode *curVnode = NULL;

    if (mount == NULL || vnode == NULL) {
        return -EINVAL;
    }

    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);//先上锁
    LOS_DL_LIST *list = VfsHashBucket(mount, hash);//获取哈希表对应的链表项
    LOS_DL_LIST_FOR_EACH_ENTRY(curVnode, list, struct Vnode, hashEntry) {//遍历链表
        if (curVnode->hash != hash) {//对比哈希值找
            continue;
        }
        if (curVnode->originMount != mount) {//对比原始mount值必须一致
            continue;
        }
        if (fn != NULL && fn(curVnode, arg)) {//哈希值比较函数,fn由具体的文件系统提供.
            continue;
        }
        (void)LOS_MuxUnlock(&g_vnodeHashMux);
        *vnode = curVnode;//找到对应索引节点
        return LOS_OK;
    }
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
    *vnode = NULL;
    return LOS_NOK;
}
//从哈希链表中摘除索引节点
void VfsHashRemove(struct Vnode *vnode)
{
    if (vnode == NULL) {
        return;
    }
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);
    LOS_ListDelete(&vnode->hashEntry);//直接把自己摘掉就行了
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
}
//插入哈希表
int VfsHashInsert(struct Vnode *vnode, uint32_t hash)
{
    if (vnode == NULL) {
        return -EINVAL;
    }
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);
    vnode->hash = hash;//设置节点哈希值
    LOS_ListHeadInsert(VfsHashBucket(vnode->originMount, hash), &vnode->hashEntry);//通过节点hashEntry 挂入哈希表对于索引链表中
    (void)LOS_MuxUnlock(&g_vnodeHashMux);
    return LOS_OK;
}
