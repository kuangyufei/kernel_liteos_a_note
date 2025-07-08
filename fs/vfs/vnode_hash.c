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
#include "vnode.h"
#include "fs/mount.h"

// 虚拟节点哈希表桶数量
#define VNODE_HASH_BUCKETS 128

// 虚拟节点哈希表数组，每个元素为链表头
LIST_HEAD g_vnodeHashEntrys[VNODE_HASH_BUCKETS];
// 哈希掩码，用于计算哈希表索引（桶数量-1）
uint32_t g_vnodeHashMask = VNODE_HASH_BUCKETS - 1;
// 哈希表大小（桶数量）
uint32_t g_vnodeHashSize = VNODE_HASH_BUCKETS;

// 虚拟节点哈希表互斥锁，用于线程安全访问
static LosMux g_vnodeHashMux;

/**
 * @brief 初始化虚拟节点哈希表
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VnodeHashInit(void)
{
    int ret;
    // 初始化哈希表所有桶的链表头
    for (int i = 0; i < g_vnodeHashSize; i++) {
        LOS_ListInit(&g_vnodeHashEntrys[i]);
    }

    // 初始化哈希表互斥锁
    ret = LOS_MuxInit(&g_vnodeHashMux, NULL);
    if (ret != LOS_OK) {  // 检查互斥锁初始化是否失败
        PRINT_ERR("Create mutex for vnode hash list fail, status: %d", ret);  // 输出错误信息
        return ret;                                                          // 返回错误码
    }

    return LOS_OK;  // 返回初始化成功
}

/**
 * @brief 打印虚拟节点哈希表内容（调试用）
 */
void VnodeHashDump(void)
{
    PRINTK("-------->VnodeHashDump in\n");               // 打印dump开始标记
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);  // 加锁保护哈希表访问
    // 遍历哈希表所有桶
    for (int i = 0; i < g_vnodeHashSize; i++) {
        LIST_HEAD *nhead = &g_vnodeHashEntrys[i];          // 当前桶的链表头
        struct Vnode *node = NULL;                         // 虚拟节点指针

        // 遍历当前桶中的所有虚拟节点
        LOS_DL_LIST_FOR_EACH_ENTRY(node, nhead, struct Vnode, hashEntry) {
            // 打印虚拟节点信息：桶索引、虚拟节点地址
            PRINTK("    vnode dump: col %d item %p\n", i, node);
        }
    }
    (void)LOS_MuxUnlock(&g_vnodeHashMux);                  // 解锁哈希表
    PRINTK("-------->VnodeHashDump out\n");              // 打印dump结束标记
}

/**
 * @brief 计算虚拟节点的哈希索引
 * @param vnode 虚拟节点指针
 * @return 成功返回哈希索引，失败返回-EINVAL
 */
uint32_t VfsHashIndex(struct Vnode *vnode)
{
    if (vnode == NULL) {  // 检查虚拟节点指针是否有效
        return -EINVAL;   // 返回参数无效错误码
    }
    // 哈希索引 = 虚拟节点哈希值 + 挂载点哈希种子
    return (vnode->hash + vnode->originMount->hashseed);
}

/**
 * @brief 获取哈希表中对应的桶
 * @param mp 挂载点结构体指针
 * @param hash 哈希值
 * @return 哈希桶的链表头指针
 */
static LOS_DL_LIST *VfsHashBucket(const struct Mount *mp, uint32_t hash)
{
    // 计算桶索引并返回对应桶的链表头
    return (&g_vnodeHashEntrys[(hash + mp->hashseed) & g_vnodeHashMask]);
}

/**
 * @brief 在哈希表中查找虚拟节点
 * @param mount 挂载点结构体指针
 * @param hash 要查找的哈希值
 * @param vnode [输出] 找到的虚拟节点指针
 * @param fn 比较函数指针（可为NULL）
 * @param arg 比较函数的参数
 * @return 成功返回LOS_OK，未找到返回LOS_NOK，参数无效返回-EINVAL
 */
int VfsHashGet(const struct Mount *mount, uint32_t hash, struct Vnode **vnode, VfsHashCmp *fn, void *arg)
{
    struct Vnode *curVnode = NULL;  // 当前遍历的虚拟节点

    if (mount == NULL || vnode == NULL) {  // 检查挂载点和输出指针是否有效
        return -EINVAL;                    // 返回参数无效错误码
    }

    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);  // 加锁保护哈希表访问
    LOS_DL_LIST *list = VfsHashBucket(mount, hash);         // 获取目标哈希桶
    // 遍历哈希桶中的所有虚拟节点
    LOS_DL_LIST_FOR_EACH_ENTRY(curVnode, list, struct Vnode, hashEntry) {
        if (curVnode->hash != hash) {                       // 哈希值不匹配，跳过
            continue;
        }
        if (curVnode->originMount != mount) {               // 挂载点不匹配，跳过
            continue;
        }
        // 如果提供了比较函数且返回非0（不匹配），跳过
        if (fn != NULL && fn(curVnode, arg)) {
            continue;
        }
        (void)LOS_MuxUnlock(&g_vnodeHashMux);               // 找到匹配节点，解锁
        *vnode = curVnode;                                  // 设置输出虚拟节点
        return LOS_OK;                                      // 返回查找成功
    }
    (void)LOS_MuxUnlock(&g_vnodeHashMux);                   // 遍历结束，解锁
    *vnode = NULL;                                          // 未找到节点，输出NULL
    return LOS_NOK;                                         // 返回未找到
}

/**
 * @brief 从哈希表中移除虚拟节点
 * @param vnode 要移除的虚拟节点
 */
void VfsHashRemove(struct Vnode *vnode)
{
    if (vnode == NULL) {  // 检查虚拟节点指针是否有效
        return;
    }
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);  // 加锁保护哈希表访问
    LOS_ListDelete(&vnode->hashEntry);                      // 从哈希链表中删除节点
    (void)LOS_MuxUnlock(&g_vnodeHashMux);                   // 解锁哈希表
}

/**
 * @brief 将虚拟节点插入哈希表
 * @param vnode 要插入的虚拟节点
 * @param hash 虚拟节点的哈希值
 * @return 成功返回LOS_OK，失败返回-EINVAL
 */
int VfsHashInsert(struct Vnode *vnode, uint32_t hash)
{
    if (vnode == NULL) {  // 检查虚拟节点指针是否有效
        return -EINVAL;   // 返回参数无效错误码
    }
    (void)LOS_MuxLock(&g_vnodeHashMux, LOS_WAIT_FOREVER);  // 加锁保护哈希表访问
    vnode->hash = hash;                                    // 设置虚拟节点的哈希值
    // 将虚拟节点插入到对应哈希桶的头部
    LOS_ListHeadInsert(VfsHashBucket(vnode->originMount, hash), &vnode->hashEntry);
    (void)LOS_MuxUnlock(&g_vnodeHashMux);                   // 解锁哈希表
    return LOS_OK;                                          // 返回插入成功
}