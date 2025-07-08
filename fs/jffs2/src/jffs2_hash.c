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
/**
 * @brief   初始化JFFS2哈希表和互斥锁
 * @param   lock  [in/out] 哈希表操作互斥锁指针
 * @param   heads [out]    哈希表头数组指针
 * @return  int  成功返回LOS_OK，失败返回错误码
 * @details 初始化哈希表所有桶的双向链表，并创建保护哈希表操作的互斥锁
 */
int Jffs2HashInit(LosMux *lock, LOS_DL_LIST *heads)
{
    int ret;                                 // 函数返回值
    // 初始化哈希表所有桶的双向链表
    for (int i = 0; i < JFFS2_NODE_HASH_BUCKETS; i++) {
        LOS_ListInit(&heads[i]);             // 初始化单个哈希桶链表
    }

    ret = LOS_MuxInit(lock, NULL);           // 初始化互斥锁
    if (ret != LOS_OK) {                     // 检查互斥锁初始化是否失败
        PRINT_ERR("Create mutex for vnode hash list fail, status: %d", ret);  // 打印错误信息
        return ret;                          // 返回错误码
    }

    return LOS_OK;                           // 初始化成功
}

/**
 * @brief   销毁JFFS2哈希表互斥锁
 * @param   lock [in] 要销毁的互斥锁指针
 * @return  int  成功返回LOS_OK，失败返回错误码
 * @details 释放哈希表操作使用的互斥锁资源
 */
int Jffs2HashDeinit(LosMux *lock)
{
    int ret;                                 // 函数返回值
    ret = LOS_MuxDestroy(lock);              // 销毁互斥锁
    if (ret != LOS_OK) {                     // 检查销毁操作是否失败
        PRINT_ERR("Destroy mutex for vnode hash list fail, status: %d", ret);  // 打印错误信息
        return ret;                          // 返回错误码
    }

    return LOS_OK;                           // 销毁成功
}

/**
 * @brief   打印JFFS2哈希表所有节点信息（调试用）
 * @param   lock  [in] 哈希表操作互斥锁指针
 * @param   heads [in] 哈希表头数组指针
 * @return  void
 * @details 遍历哈希表所有桶，打印每个inode节点的地址和所在桶索引
 */
void Jffs2HashDump(LosMux *lock, LOS_DL_LIST *heads)
{
    PRINTK("-------->Jffs2HashDump in\n");  // 打印调试开始标记
    (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);  // 获取互斥锁，永久等待
    // 遍历所有哈希桶
    for (int i = 0; i < JFFS2_NODE_HASH_BUCKETS; i++) {
        LIST_HEAD *nhead = &heads[i];        // 当前桶的链表头
        struct jffs2_inode *node = NULL;     // inode节点指针

        // 遍历当前桶中的所有inode节点
        LOS_DL_LIST_FOR_EACH_ENTRY(node, nhead, struct jffs2_inode, i_hashlist) {
            PRINTK("    vnode dump: col %d item %p\n", i, node);  // 打印桶索引和节点地址
        }
    }
    (void)LOS_MuxUnlock(lock);               // 释放互斥锁
    PRINTK("-------->Jffs2HashDump out\n"); // 打印调试结束标记
}

/**
 * @brief   根据inode号计算哈希桶索引
 * @param   heads [in] 哈希表头数组指针
 * @param   ino   [in] inode号
 * @return  LOS_DL_LIST* 哈希桶链表头指针
 * @details 使用inode号与哈希掩码进行位运算，得到哈希桶索引
 */
static LOS_DL_LIST *Jffs2HashBucket(LOS_DL_LIST *heads, const uint32_t ino)
{
    // 计算哈希桶索引：inode号 & 哈希掩码（取低位）
    LOS_DL_LIST *head = &(heads[ino & JFFS2_NODE_HASH_MASK]);
    return head;                             // 返回哈希桶链表头
}

/**
 * @brief   从哈希表中查找inode节点
 * @param   lock    [in]  哈希表操作互斥锁指针
 * @param   heads   [in]  哈希表头数组指针
 * @param   sb      [in]  超级块指针（用于节点匹配）
 * @param   ino     [in]  要查找的inode号
 * @param   ppNode  [out] 查找到的inode节点指针
 * @return  int  成功返回0，未找到返回0且ppNode为NULL
 * @details 先计算哈希桶，再遍历桶内节点匹配inode号和超级块，确保线程安全
 */
int Jffs2HashGet(LosMux *lock, LOS_DL_LIST *heads, const void *sb, const uint32_t ino, struct jffs2_inode **ppNode)
{
    struct jffs2_inode *node = NULL;         // 临时inode节点指针

    while (1) {                              // 使用循环结构确保只返回一次
        (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);  // 获取互斥锁
        LOS_DL_LIST *list = Jffs2HashBucket(heads, ino);  // 计算目标哈希桶
        // 遍历哈希桶内所有节点
        LOS_DL_LIST_FOR_EACH_ENTRY(node, list, struct jffs2_inode, i_hashlist) {
            if (node->i_ino != ino)          // 检查inode号是否匹配
                continue;
            if (node->i_sb != sb)            // 检查超级块是否匹配
                continue;
            (void)LOS_MuxUnlock(lock);       // 找到匹配节点，释放互斥锁
            *ppNode = node;                  // 设置输出节点指针
            return 0;                        // 返回成功
        }
        (void)LOS_MuxUnlock(lock);           // 未找到节点，释放互斥锁
        *ppNode = NULL;                      // 设置输出为NULL
        return 0;                            // 返回成功（未找到）
    }
}

/**
 * @brief   从哈希表中移除inode节点
 * @param   lock [in] 哈希表操作互斥锁指针
 * @param   node [in] 要移除的inode节点指针
 * @return  void
 * @details 从节点所在的哈希桶链表中删除节点，操作期间持有互斥锁
 */
void Jffs2HashRemove(LosMux *lock, struct jffs2_inode *node)
{
    (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);  // 获取互斥锁
    LOS_ListDelete(&node->i_hashlist);        // 从哈希链表中删除节点
    (void)LOS_MuxUnlock(lock);               // 释放互斥锁
}

/**
 * @brief   将inode节点插入哈希表
 * @param   lock [in] 哈希表操作互斥锁指针
 * @param   heads [in] 哈希表头数组指针
 * @param   node [in] 要插入的inode节点指针
 * @param   ino  [in] inode号（用于计算哈希桶）
 * @return  int  成功返回0
 * @details 根据inode号计算哈希桶，将节点插入桶链表头部，确保线程安全
 */
int Jffs2HashInsert(LosMux *lock, LOS_DL_LIST *heads, struct jffs2_inode *node, const uint32_t ino)
{
    (void)LOS_MuxLock(lock, LOS_WAIT_FOREVER);  // 获取互斥锁
    // 计算哈希桶并将节点插入链表头部
    LOS_ListHeadInsert(Jffs2HashBucket(heads, ino), &node->i_hashlist);
    (void)LOS_MuxUnlock(lock);               // 释放互斥锁
    return 0;                                // 返回成功
}
#endif

