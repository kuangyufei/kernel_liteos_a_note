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

#include "bcache.h"
#include "assert.h"
#include "stdlib.h"
#include "linux/delay.h"
#include "disk_pri.h"
#include "user_copy.h"
#undef HALARC_ALIGNMENT
#define DMA_ALLGN          64  // DMA对齐大小，64字节
#define HALARC_ALIGNMENT   DMA_ALLGN  // HALARC对齐方式，使用DMA对齐
#define BCACHE_MAGIC_NUM   20132016  // 块缓存魔数，用于标识有效缓存块
#define BCACHE_STATCK_SIZE 0x3000  // 块缓存栈大小
#define ASYNC_EVENT_BIT    0x01  // 异步事件标志位

#ifdef DEBUG
#define D(args) printf args  // 调试模式下启用打印输出
#else
#define D(args)  // 非调试模式下禁用打印输出
#endif

#ifdef BCACHE_ANALYSE
UINT32 g_memSize;  // 内存大小
volatile UINT32 g_blockNum;  // 块数量
volatile UINT32 g_dataSize;  // 数据大小
volatile UINT8 *g_memStart;  // 内存起始地址
volatile UINT32 g_switchTimes[CONFIG_FS_FAT_BLOCK_NUMS] = { 0 };  // 块切换次数统计数组
volatile UINT32 g_hitTimes[CONFIG_FS_FAT_BLOCK_NUMS] = { 0 };  // 块命中次数统计数组
#endif

/**
 * @brief 块缓存分析函数，打印缓存统计信息
 *
 * @param level 分析级别（未使用）
 * @return 无
 */
VOID BcacheAnalyse(UINT32 level)
{
    (VOID)level;  // 未使用的参数，避免编译警告
#ifdef BCACHE_ANALYSE
    int i;

    PRINTK("Bcache information:\n");
    PRINTK("    mem: %u\n", g_memSize);
    PRINTK("    block number: %u\n", g_blockNum);
    PRINTK("index, switch, hit\n");
    for (i = 0; i < g_blockNum; i++) {
        PRINTK("%5d, %6d, %3d\n", i, g_switchTimes[i], g_hitTimes[i]);
    }
#else
    PRINTK("Bcache hasn't started\n");  // BCACHE_ANALYSE未定义时提示缓存未启动
#endif
}

#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD

UINT32 g_syncThreadPrio = CONFIG_FS_FAT_SYNC_THREAD_PRIO;  // 同步线程优先级
UINT32 g_dirtyRatio = CONFIG_FS_FAT_DIRTY_RATIO;  // 脏块比例阈值
UINT32 g_syncInterval = CONFIG_FS_FAT_SYNC_INTERVAL;  // 同步间隔（毫秒）

/**
 * @brief 设置脏块比例阈值
 *
 * @param dirtyRatio 新的脏块比例阈值（0-100）
 * @return 无
 */
VOID LOS_SetDirtyRatioThreshold(UINT32 dirtyRatio)
{
    if ((dirtyRatio != g_dirtyRatio) && (dirtyRatio <= 100)) { /* 比例不能超过100% */
        g_dirtyRatio = dirtyRatio;  // 更新脏块比例阈值
    }
}

/**
 * @brief 设置同步线程间隔
 *
 * @param interval 新的同步间隔（毫秒）
 * @return 无
 */
VOID LOS_SetSyncThreadInterval(UINT32 interval)
{
    g_syncInterval = interval;  // 更新同步间隔
}

/**
 * @brief 设置同步线程优先级
 *
 * @param prio 新的线程优先级
 * @param name 磁盘名称（NULL表示只更新全局变量）
 * @return 成功返回0；失败返回-1
 */
INT32 LOS_SetSyncThreadPrio(UINT32 prio, const CHAR *name)
{
    INT32 ret = VFS_ERROR;  // 初始化返回值为错误
    INT32 diskID;  // 磁盘ID
    los_disk *disk = NULL;  // 磁盘结构体指针
    if ((prio == 0) || (prio >= OS_TASK_PRIORITY_LOWEST)) { /* 优先级不能为0或超出最低优先级 */
        return ret;  // 返回错误
    }

    g_syncThreadPrio = prio;  // 更新全局优先级变量

    /*
     * 如果名称为NULL，仅设置全局变量的值，
     * 下次创建线程时生效。
     */
    if (name == NULL) {
        return ENOERR;  // 成功返回
    }

    /* 如果名称不为NULL，找不到对应磁盘时应返回错误 */
    diskID = los_get_diskid_byname(name);  // 通过名称获取磁盘ID
    disk = get_disk(diskID);  // 通过磁盘ID获取磁盘结构体
    if (disk == NULL) {
        return ret;  // 磁盘不存在，返回错误
    }

    if (pthread_mutex_lock(&disk->disk_mutex) != ENOERR) {  // 加锁保护磁盘操作
        PRINT_ERR("%s %d, mutex lock fail!\n", __FUNCTION__, __LINE__);
        return ret;  // 加锁失败返回错误
    }
    if ((disk->disk_status == STAT_INUSED) && (disk->bcache != NULL)) {  // 检查磁盘状态和缓存是否有效
        ret = LOS_TaskPriSet(disk->bcache->syncTaskId, prio);  // 设置线程优先级
    }
    if (pthread_mutex_unlock(&disk->disk_mutex) != ENOERR) {  // 解锁
        PRINT_ERR("%s %d, mutex unlock fail!\n", __FUNCTION__, __LINE__);
        return VFS_ERROR;  // 解锁失败返回错误
    }
    return ret;  // 返回操作结果
}
#endif

/**
 * @brief 在红黑树中查找指定编号的块
 *
 * @param bc 块缓存结构体指针
 * @param num 要查找的块编号
 * @return 找到的块结构体指针；未找到返回NULL
 */
static OsBcacheBlock *RbFindBlock(const OsBcache *bc, UINT64 num)
{
    OsBcacheBlock *block = NULL;  // 块结构体指针
    struct rb_node *node = bc->rbRoot.rb_node;  // 红黑树根节点

    for (; node != NULL; node = (block->num < num) ? node->rb_right : node->rb_left) {  // 遍历红黑树
        block = rb_entry(node, OsBcacheBlock, rbNode);  // 获取节点对应的块结构体
        if (block->num == num) {  // 找到匹配的块编号
            return block;  // 返回找到的块
        }
    }
    return NULL;  // 未找到返回NULL
}

/**
 * @brief 将块添加到红黑树中
 *
 * @param bc 块缓存结构体指针
 * @param block 要添加的块结构体指针
 * @return 无
 */
static VOID RbAddBlock(OsBcache *bc, OsBcacheBlock *block)
{
    struct rb_node *node = bc->rbRoot.rb_node;  // 红黑树根节点
    struct rb_node **link = NULL;  // 节点链接指针
    OsBcacheBlock *b = NULL;  // 临时块结构体指针

    if (node == NULL) {  // 树为空
        rb_link_node(&block->rbNode, NULL, &bc->rbRoot.rb_node);  // 添加为根节点
    } else {
        for (; node != NULL; link = (b->num > block->num) ? &node->rb_left : &node->rb_right, node = *link) {  // 查找插入位置
            b = rb_entry(node, OsBcacheBlock, rbNode);  // 获取当前节点对应的块
            if (b->num == block->num) {  // 块编号已存在
                PRINT_ERR("RbAddBlock fail, b->num = %llu, block->num = %llu\n", b->num, block->num);
                return;  // 添加失败返回
            }
        }
        rb_link_node(&block->rbNode, &b->rbNode, link);  // 链接新节点
    }
    rb_insert_color(&block->rbNode, &bc->rbRoot);  // 调整红黑树平衡
}

/**
 * @brief 从红黑树中删除块
 *
 * @param bc 块缓存结构体指针
 * @param block 要删除的块结构体指针
 * @return 无
 */
static inline VOID RbDelBlock(OsBcache *bc, OsBcacheBlock *block)
{
    rb_erase(&block->rbNode, &bc->rbRoot);  // 从红黑树中删除节点
}

/**
 * @brief 将块移动到链表头部（更新LRU顺序）
 *
 * @param bc 块缓存结构体指针
 * @param block 要移动的块结构体指针
 * @return 无
 */
static inline VOID ListMoveBlockToHead(OsBcache *bc, OsBcacheBlock *block)
{
    LOS_ListDelete(&block->listNode);  // 从当前位置删除
    LOS_ListAdd(&bc->listHead, &block->listNode);  // 添加到头部
}

/**
 * @brief 将块标记为未使用并添加到空闲链表
 *
 * @param bc 块缓存结构体指针
 * @param block 要释放的块结构体指针
 * @return 无
 */
static inline VOID FreeBlock(OsBcache *bc, OsBcacheBlock *block)
{
    block->used = FALSE;  // 标记为未使用
    LOS_ListAdd(&bc->freeListHead, &block->listNode);  // 添加到空闲链表
}

/**
 * @brief 计算数值的以2为底的对数（仅适用于2的幂）
 *
 * @param val 输入数值（必须是2的幂）
 * @return 对数值；非2的幂返回0
 */
static UINT32 GetValLog2(UINT32 val)
{
    UINT32 i, log2;  // i:临时变量，log2:对数值

    i = val;
    log2 = 0;
    while ((i & 1) == 0) { /* 检查最后一位是否为1 */
        i >>= 1;  // 右移一位
        log2++;  // 增加对数计数
    }
    if (i != 1) { /* 不是2的幂 */
        return 0;  // 返回0表示错误
    }

    return log2;  // 返回对数值
}

/**
 * @brief 在标志数组中查找连续的标志位位置
 *
 * @param arr 标志数组
 * @param len 数组长度
 * @param p1 输出参数，起始位置
 * @param p2 输出参数，结束位置
 * @return 成功返回0；失败返回-1
 */
static INT32 FindFlagPos(const UINT32 *arr, UINT32 len, UINT32 *p1, UINT32 *p2)
{
    UINT32 *start = p1;  // 起始位置指针
    UINT32 *end = p2;  // 结束位置指针
    UINT32 i, j, tmp;  // i,j:循环变量，tmp:临时变量
    UINT32 val = 1;  // 标志值（0或1）

    *start = BCACHE_MAGIC_NUM;  // 初始化起始位置为魔数（无效值）
    *end = 0;  // 初始化结束位置为0
    for (i = 0; i < len; i++) {  // 遍历数组
        for (j = 0; j < UNSIGNED_INTEGER_BITS; j++) {  // 遍历位
            tmp = arr[i] << j;  // 左移j位
            tmp = tmp >> UNINT_MAX_SHIFT_BITS;  // 右移最高位
            if (tmp != val) {  // 不匹配当前标志值
                continue;
            }
            if (val && (*start == BCACHE_MAGIC_NUM)) {  // 找到起始标志
                *start = (i << UNINT_LOG2_SHIFT) + j;  // 计算起始位置
                val = 1 - val; /* 通过0和1控制奇偶性 */
            } else if (val && (*start != BCACHE_MAGIC_NUM)) {  // 起始标志已设置但再次找到起始位
                *start = 0;
                return VFS_ERROR;  // 返回错误
            } else {  // 找到结束标志
                *end = (i << UNINT_LOG2_SHIFT) + j;  // 计算结束位置
                val = 1 - val; /* 通过0和1控制奇偶性 */
            }
        }
    }
    if (*start == BCACHE_MAGIC_NUM) {  // 未找到起始位置
        *start = 0;
        return VFS_ERROR;  // 返回错误
    }
    if (*end == 0) {  // 未找到结束位置
        *end = len << UNINT_LOG2_SHIFT;  // 设置结束位置为数组末尾
    }

    return ENOERR;  // 成功返回
}

/**
 * @brief 从磁盘读取块数据
 *
 * @param bc 块缓存结构体指针
 * @param block 要读取的块结构体指针
 * @param buf 数据缓冲区
 * @return 成功返回0；失败返回错误码
 */
static INT32 BlockRead(OsBcache *bc, OsBcacheBlock *block, UINT8 *buf)
{
    INT32 ret = bc->breadFun(bc->priv, buf, bc->sectorPerBlock,
                             (block->num) << GetValLog2(bc->sectorPerBlock));  // 调用底层读函数
    if (ret) {  // 读取失败
        PRINT_ERR("BlockRead, brread_fn error, ret = %d\n", ret);
        if (block->modified == FALSE) {  // 块未被修改
            if (block->listNode.pstNext != NULL) {  // 块在链表中
                LOS_ListDelete(&block->listNode); /* 从链表中删除块 */
                RbDelBlock(bc, block);  // 从红黑树中删除
            }
            FreeBlock(bc, block);  // 释放块
        }
        return ret;  // 返回错误码
    }

    block->readFlag = TRUE;  // 设置读取标志
    return ENOERR;  // 成功返回
}

/**
 * @brief 获取块的标志信息并读取相应扇区数据
 *
 * @param bc 块缓存结构体指针
 * @param block 块结构体指针
 * @return 成功返回0；失败返回错误码
 */
static INT32 BcacheGetFlag(OsBcache *bc, OsBcacheBlock *block)
{
    UINT32 i, n, f, sectorPos, val, start, pos, currentSize;  // 临时变量
    UINT32 flagUse = bc->sectorPerBlock >> UNINT_LOG2_SHIFT;  // 标志使用数量
    UINT32 flag = UINT_MAX;  // 标志值
    INT32 ret, bits;  // ret:返回值，bits:剩余位数

    if (block->readFlag == TRUE) {  // 已读取过数据
        return ENOERR;  // 直接返回成功
    }

    for (i = 0; i < flagUse; i++) {  // 计算标志的与运算结果
        flag &= block->flag[i];
    }

    if (flag == UINT_MAX) {  // 标志全为1，表示无需读取
        return ENOERR;  // 返回成功
    }

    ret = BlockRead(bc, block, bc->rwBuffer);  // 读取整个块数据
    if (ret != ENOERR) {
        return ret;  // 读取失败返回错误码
    }

    for (i = 0, sectorPos = 0; i < flagUse; i++) {  // 处理每个标志
        val = block->flag[i];
        /* 使用无符号整数作为位图 */
        for (f = 0, bits = UNSIGNED_INTEGER_BITS; bits > 0; val = ~(val << n), f++, bits = bits - (INT32)n) {
            if (val == 0) {  // 标志值为0
                n = UNSIGNED_INTEGER_BITS;  // 所有位都为0
            } else {
                n = (UINT32)CLZ(val);  // 计算前导零数量
            }
            sectorPos += n;  // 更新扇区位置
            if (((f % EVEN_JUDGED) != 0) || (n == 0)) { /* n的前导零数量为零 */
                goto LOOP;  // 跳转到循环结束处
            }
            if (sectorPos > ((i + 1) << UNINT_LOG2_SHIFT)) {  // 扇区位置超出当前标志范围
                start = sectorPos - n;  // 计算起始位置
                currentSize = (((i + 1) << UNINT_LOG2_SHIFT) - start) * bc->sectorSize;  // 计算数据大小
            } else {
                start = sectorPos - n;  // 计算起始位置
                currentSize = n * bc->sectorSize;  // 计算数据大小
            }
            pos = start * bc->sectorSize;  // 计算数据在块中的偏移
            if (memcpy_s(block->data + pos, bc->blockSize - pos, bc->rwBuffer + pos, currentSize) != EOK) {  // 复制数据
                return VFS_ERROR;  // 复制失败返回错误
            }
LOOP:
            if (sectorPos > ((i + 1) << UNINT_LOG2_SHIFT)) {  // 扇区位置超出当前标志范围
                sectorPos = (i + 1) << UNINT_LOG2_SHIFT;  // 调整扇区位置
            }
        }
    }

    return ENOERR;  // 成功返回
}

/**
 * @brief 设置块的标志信息（标记已修改的扇区）
 *
 * @param bc 块缓存结构体指针
 * @param block 块结构体指针
 * @param pos 起始位置（字节）
 * @param size 数据大小（字节）
 * @return 无
 */
static VOID BcacheSetFlag(const OsBcache *bc, OsBcacheBlock *block, UINT32 pos, UINT32 size)
{
    UINT32 start, num, i, j, k;  // 临时变量

    if (bc->sectorSize == 0) {  // 扇区大小为0（无效）
        PRINT_ERR("BcacheSetFlag sectorSize is equal to zero! \n");
        return;  // 直接返回
    }

    start = pos / bc->sectorSize;  // 计算起始扇区
    num = size / bc->sectorSize;  // 计算扇区数量

    i = start / UNSIGNED_INTEGER_BITS;  // 计算标志数组索引
    j = start % UNSIGNED_INTEGER_BITS;  // 计算位索引
    for (k = 0; k < num; k++) {  // 设置每个扇区的标志位
        block->flag[i] |= 1u << (UNINT_MAX_SHIFT_BITS - j);  // 设置标志位
        j++;  // 位索引加1
        if (j == UNSIGNED_INTEGER_BITS) {  // 位索引超出范围
            j = 0;  // 重置位索引
            i++;  // 数组索引加1
        }
    }
}

/**
 * @brief 同步块数据到磁盘（将脏块写入磁盘）
 *
 * @param bc 块缓存结构体指针
 * @param block 要同步的块结构体指针
 * @return 成功返回0；失败返回错误码
 */
static INT32 BcacheSyncBlock(OsBcache *bc, OsBcacheBlock *block)
{
    INT32 ret = ENOERR;  // 返回值
    UINT32 len, start, end;  // len:数据长度，start:起始扇区，end:结束扇区

    if (block->modified == TRUE) {  // 块已被修改
        D(("bcache writing block = %llu\n", block->num));  // 调试打印

        ret = FindFlagPos(block->flag, bc->sectorPerBlock >> UNINT_LOG2_SHIFT, &start, &end);  // 查找标志位置
        if (ret == ENOERR) {  // 查找成功
            len = end - start;  // 计算数据长度
        } else {
            ret = BcacheGetFlag(bc, block);  // 获取标志信息
            if (ret != ENOERR) {
                return ret;  // 获取失败返回错误码
            }

            len = bc->sectorPerBlock;  // 设置数据长度为整个块
        }

        ret = bc->bwriteFun(bc->priv, (const UINT8 *)(block->data + (start * bc->sectorSize)),
                            len, (block->num * bc->sectorPerBlock) + start);  // 调用底层写函数
        if (ret == ENOERR) {  // 写入成功
            block->modified = FALSE;  // 清除脏块标志
            bc->modifiedBlock--;  // 减少脏块计数
        } else {
            PRINT_ERR("BcacheSyncBlock fail, ret = %d, len = %u, block->num = %llu, start = %u\n",
                      ret, len, block->num, start);  // 打印错误信息
        }
    }
    return ret;  // 返回操作结果
}

/**
 * @brief 将块按编号顺序添加到链表
 *
 * @param bc 块缓存结构体指针
 * @param block 要添加的块结构体指针
 * @return 无
 */
static void NumListAdd(OsBcache *bc, OsBcacheBlock *block)
{
    OsBcacheBlock *temp = NULL;  // 临时块结构体指针

    LOS_DL_LIST_FOR_EACH_ENTRY(temp, &bc->numHead, OsBcacheBlock, numNode) {  // 遍历编号链表
        if (temp->num > block->num) {  // 找到插入位置
            LOS_ListTailInsert(&temp->numNode, &block->numNode);  // 插入到当前节点之后
            return;
        }
    }

    LOS_ListTailInsert(&bc->numHead, &block->numNode);  // 添加到链表尾部
}

/**
 * @brief 将块添加到缓存（红黑树、编号链表和LRU链表）
 *
 * @param bc 块缓存结构体指针
 * @param block 要添加的块结构体指针
 * @return 无
 */
static void AddBlock(OsBcache *bc, OsBcacheBlock *block)
{
    RbAddBlock(bc, block);  // 添加到红黑树
    NumListAdd(bc, block);  // 添加到编号链表
    bc->sumNum += block->num;  // 更新总块编号和
    bc->nBlock++;  // 增加块计数
    LOS_ListAdd(&bc->listHead, &block->listNode);  // 添加到LRU链表头部
}

/**
 * @brief 从缓存中删除块（红黑树、编号链表和LRU链表）
 *
 * @param bc 块缓存结构体指针
 * @param block 要删除的块结构体指针
 * @return 无
 */
static void DelBlock(OsBcache *bc, OsBcacheBlock *block)
{
    LOS_ListDelete(&block->listNode); /* 从LRU链表删除 */
    LOS_ListDelete(&block->numNode);  /* 从编号链表删除 */
    bc->sumNum -= block->num;  /* 更新总块编号和 */
    bc->nBlock--;  /* 减少块计数 */
    RbDelBlock(bc, block);            /* 从红黑树删除 */
    FreeBlock(bc, block);             /* 添加到空闲链表 */
}

/**
 * @brief 检查块是否完全脏（所有扇区都被修改）
 *
 * @param bc 块缓存结构体指针
 * @param block 要检查的块结构体指针
 * @return 完全脏返回TRUE；否则返回FALSE
 */
static BOOL BlockAllDirty(const OsBcache *bc, OsBcacheBlock *block)
{
    UINT32 start = 0;  // 起始扇区
    UINT32 end = 0;  // 结束扇区
    UINT32 len = bc->sectorPerBlock >> UNINT_LOG2_SHIFT;  // 标志长度

    if (block->modified == TRUE) {  // 块已被修改
        if (block->allDirty) {  // 已标记为完全脏
            return TRUE;  // 返回TRUE
        }

        if (FindFlagPos(block->flag, len, &start, &end) == ENOERR) {  // 查找标志位置
            if ((end - start) == bc->sectorPerBlock) {  // 脏扇区数量等于块的总扇区数
                block->allDirty = TRUE;  // 标记为完全脏
                return TRUE;  // 返回TRUE
            }
        }
    }

    return FALSE;  // 返回FALSE
}

/**
 * @brief 获取基础块（从写缓冲区获取未使用的块）
 *
 * @param bc 块缓存结构体指针
 * @return 成功返回块结构体指针；失败返回NULL
 */
static OsBcacheBlock *GetBaseBlock(OsBcache *bc)
{
    OsBcacheBlock *base = bc->wStart;  // 写缓冲区起始地址
    OsBcacheBlock *end = bc->wEnd;  // 写缓冲区结束地址
    while (base < end) {  // 遍历写缓冲区
        if (base->used == FALSE) {  // 找到未使用的块
            base->used = TRUE;  // 标记为已使用
            LOS_ListDelete(&base->listNode);  // 从链表中删除
            return base;  // 返回该块
        }
        base++;  // 移动到下一个块
    }

    return NULL;  // 未找到返回NULL
}

/* 首先尝试获取空闲块，如果失败则释放无用块 */
/**
 * @brief 获取慢路径块（先尝试空闲块，失败则释放LRU块）
 *
 * @param bc 块缓存结构体指针
 * @param read TRUE表示读缓冲区，FALSE表示写缓冲区
 * @return 成功返回块结构体指针；失败返回NULL
 */
static OsBcacheBlock *GetSlowBlock(OsBcache *bc, BOOL read)
{
    LOS_DL_LIST *node = NULL;  // 链表节点指针
    OsBcacheBlock *block = NULL;  // 块结构体指针

    LOS_DL_LIST_FOR_EACH_ENTRY(block, &bc->freeListHead, OsBcacheBlock, listNode) {  // 遍历空闲链表
        if (block->readBuff == read) {  // 找到匹配类型的块
            block->used = TRUE;  // 标记为已使用
            LOS_ListDelete(&block->listNode);  // 从空闲链表删除
            return block; /* 获取空闲块 */
        }
    }

    node = bc->listHead.pstPrev;  // 从LRU链表尾部开始
    while (node != &bc->listHead) {  // 遍历LRU链表
        block = LOS_DL_LIST_ENTRY(node, OsBcacheBlock, listNode);  // 获取块结构体
        node = block->listNode.pstPrev;  // 移动到前一个节点

        if (block->readBuff == read) {  // 找到匹配类型的块
            if (block->modified == TRUE) {  // 块是脏的
                BcacheSyncBlock(bc, block);  // 同步到磁盘
            }

            DelBlock(bc, block);  // 从缓存中删除
            block->used = TRUE;  // 标记为已使用
            LOS_ListDelete(&block->listNode);  // 从链表中删除
            return block; /* 获取已使用块 */
        }
    }

    return NULL;  // 未找到返回NULL
}

/* 刷新合并的块 */
/**
 * @brief 写入合并的连续块
 *
 * @param bc 块缓存结构体指针
 * @param begin 起始块结构体指针
 * @param blocks 块数量
 * @return 无
 */
static VOID WriteMergedBlocks(OsBcache *bc, OsBcacheBlock *begin, int blocks)
{
    INT32 ret;  // 返回值
    OsBcacheBlock *cur = NULL;  // 当前块指针
    OsBcacheBlock *next = NULL;  // 下一个块指针
    UINT32 len = blocks * bc->sectorPerBlock;  // 总数据长度
    UINT64 pos = begin->num * bc->sectorPerBlock;  // 起始扇区位置

    ret = bc->bwriteFun(bc->priv, (const UINT8 *)begin->data, len, pos);  // 调用底层写函数
    if (ret != ENOERR) {
        PRINT_ERR("WriteMergedBlocks bwriteFun failed ret %d\n", ret);  // 打印错误信息
        return;
    }

    bc->modifiedBlock -= blocks;  // 减少脏块计数
    cur = begin;
    while (blocks > 0) {  // 遍历所有合并的块
        next = LOS_DL_LIST_ENTRY(cur->numNode.pstNext, OsBcacheBlock, numNode);  // 获取下一个块
        DelBlock(bc, cur);  // 从缓存中删除当前块
        cur->modified = FALSE;  // 清除脏块标志
        blocks--;  // 减少块计数
        cur = next;  // 移动到下一个块
    }
}

/* 查找连续块并刷新它们 */
/**
 * @brief 合并并同步连续的脏块
 *
 * @param bc 块缓存结构体指针
 * @param start 起始块结构体指针
 * @return 无
 */
static VOID MergeSyncBlocks(OsBcache *bc, OsBcacheBlock *start)
{
    INT32 mergedBlock = 0;  // 合并的块数量
    OsBcacheBlock *cur = start;  // 当前块指针
    OsBcacheBlock *last = NULL;  // 上一个块指针

    while (cur <= bc->wEnd) {  // 遍历写缓冲区
        if (!cur->used || !BlockAllDirty(bc, cur)) {  // 块未使用或未完全脏
            break;
        }

        if (last && (last->num + 1 != cur->num)) {  // 块编号不连续
            break;
        }

        mergedBlock++;  // 增加合并块计数
        last = cur;  // 更新上一个块
        cur++;  // 移动到下一个块
    }

    if (mergedBlock > 0) {  // 有可合并的块
        WriteMergedBlocks(bc, start, mergedBlock);  // 写入合并的块
    }
}

/* 获取块缓存缓冲区的最小写块编号 */
/**
 * @brief 获取写缓冲区中的最小块编号
 *
 * @param bc 块缓存结构体指针
 * @return 最小块编号；无写块返回0
 */
static inline UINT64 GetMinWriteNum(OsBcache *bc)
{
    UINT64 ret = 0;  // 返回值
    OsBcacheBlock *block = NULL;  // 块结构体指针

    LOS_DL_LIST_FOR_EACH_ENTRY(block, &bc->numHead, OsBcacheBlock, numNode) {  // 遍历编号链表
        if (!block->readBuff) {  // 找到写缓冲区的块
            ret = block->num;  // 记录块编号
            break;
        }
    }

    return ret;  // 返回最小块编号
}

/**
 * @brief 分配新块（根据读/写类型和块编号）
 *
 * @param bc 块缓存结构体指针
 * @param read TRUE表示读缓冲区，FALSE表示写缓冲区
 * @param num 块编号
 * @return 成功返回块结构体指针；失败返回NULL
 */
static OsBcacheBlock *AllocNewBlock(OsBcache *bc, BOOL read, UINT64 num)
{
    OsBcacheBlock *last = NULL;  // 上一个块指针
    OsBcacheBlock *prefer = NULL;  // 首选块指针

    if (read) { /* 读操作 */
        return GetSlowBlock(bc, TRUE);  // 从读缓冲区获取块
    }

    /* 回退，当块之前被刷新时可能发生，使用读缓冲区 */
    if (bc->nBlock && num < GetMinWriteNum(bc)) {
        return GetSlowBlock(bc, TRUE);  // 从读缓冲区获取块
    }

    last = RbFindBlock(bc, num - 1);  /* num=0时也可以 */  // 查找前一个块
    if (last == NULL || last->readBuff) {
        return GetBaseBlock(bc);      /* 新块 */  // 获取新的写缓冲区块
    }

    prefer = last + 1;  // 首选下一个连续块
    if (prefer > bc->wEnd) {  // 超出写缓冲区范围
        prefer = bc->wStart;  // 回绕到起始位置
    }

    /* 这是一个同步线程同步过的块！ */
    if (prefer->used && !prefer->modified) {  // 块已使用但不脏
        prefer->used = FALSE;  // 标记为未使用
        DelBlock(bc, prefer);  // 从缓存中删除
    }

    if (prefer->used) { /* 不要与下一个检查合并 */
        MergeSyncBlocks(bc, prefer); /* prefer->used可能在此处改变 */  // 合并同步块
    }

    if (prefer->used) {  // 块仍在使用
        BcacheSyncBlock(bc, prefer);  // 同步块数据
        DelBlock(bc, prefer);  // 从缓存中删除
    }

    prefer->used = TRUE;  // 标记为已使用
    LOS_ListDelete(&prefer->listNode); /* 从空闲链表删除 */

    return prefer;  // 返回分配的块
}
/**
 * @brief 同步缓存中的所有脏块到物理设备
 * @param bc 块缓存控制结构体指针
 * @return 成功返回ENOERR，失败返回错误码
 */
static INT32 BcacheSync(OsBcache *bc)
{
    LOS_DL_LIST *node = NULL;  // 双向链表节点指针，用于遍历缓存块列表
    OsBcacheBlock *block = NULL;  // 缓存块结构体指针
    INT32 ret = ENOERR;  // 函数返回值，默认为成功

    D(("bcache cache sync\n"));

    (VOID)pthread_mutex_lock(&bc->bcacheMutex);  // 加锁保护缓存操作
    node = bc->listHead.pstPrev;  // 从链表尾部开始遍历（LRU策略：最近最少使用的块在尾部）
    while (&bc->listHead != node) {  // 遍历整个缓存块列表
        block = LOS_DL_LIST_ENTRY(node, OsBcacheBlock, listNode);  // 将链表节点转换为缓存块结构体
        ret = BcacheSyncBlock(bc, block);  // 同步单个缓存块到物理设备
        if (ret != ENOERR) {  // 检查同步是否失败
            PRINT_ERR("BcacheSync error, ret = %d\n", ret);  // 打印错误信息
            break;  // 同步失败，跳出循环
        }
        node = node->pstPrev;  // 移动到前一个节点
    }
    (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁缓存操作

    return ret;  // 返回同步结果
}

/**
 * @brief 初始化缓存块结构
 * @param bc 块缓存控制结构体指针
 * @param block 待初始化的缓存块指针
 * @param num 缓存块对应的物理块编号
 */
static VOID BlockInit(OsBcache *bc, OsBcacheBlock *block, UINT64 num)
{
    (VOID)memset_s(block->flag, sizeof(block->flag), 0, sizeof(block->flag));  // 清零标志数组（用于标记脏数据区域）
    block->num = num;  // 设置缓存块对应的物理块编号
    block->readFlag = FALSE;  // 清除读标志
    if (block->modified == TRUE) {  // 如果块被标记为修改（脏块）
        block->modified = FALSE;  // 清除脏标记
        bc->modifiedBlock--;  // 减少脏块计数
    }
    block->allDirty = FALSE;  // 清除全脏标志（表示整个块都被修改）
}

/**
 * @brief 从缓存中获取指定编号的块，如果不存在则分配新块
 * @param bc 块缓存控制结构体指针
 * @param num 要获取的物理块编号
 * @param readData 是否需要从物理设备读取数据
 * @param dblock 输出参数，返回获取到的缓存块指针
 * @return 成功返回ENOERR，失败返回错误码
 */
static INT32 BcacheGetBlock(OsBcache *bc, UINT64 num, BOOL readData, OsBcacheBlock **dblock)
{
    INT32 ret;  // 函数返回值
    OsBcacheBlock *block = NULL;  // 缓存块结构体指针
    OsBcacheBlock *first = NULL;  // 链表头节点（最近使用的块）

    /*
     * 首先检查最近使用的块是否为请求的块，
     * 这可以提高字节访问函数的性能。
     */
    if (LOS_ListEmpty(&bc->listHead) == FALSE) {  // 如果缓存列表不为空
        first = LOS_DL_LIST_ENTRY(bc->listHead.pstNext, OsBcacheBlock, listNode);  // 获取头节点（最近使用的块）
        block = (first->num == num) ? first : RbFindBlock(bc, num);  // 检查头节点是否为目标块，否则通过红黑树查找
    }

    if (block != NULL) {  // 如果找到缓存块
        D(("bcache block = %llu found in cache\n", num));  // 调试日志：缓存命中
#ifdef BCACHE_ANALYSE
        UINT32 index = ((UINT32)(block->data - g_memStart)) / g_dataSize;  // 计算缓存块索引
        PRINTK(", [HIT], %llu, %u\n", num, index);  // 打印缓存命中信息
        g_hitTimes[index]++;  // 增加命中计数
#endif

        if (first != block) {  // 如果找到的块不是最近使用的块
            ListMoveBlockToHead(bc, block);  // 将块移动到链表头部（更新为最近使用）
        }
        *dblock = block;  // 设置输出参数

        if ((bc->prereadFun != NULL) && (readData == TRUE) && (block->pgHit == 1)) {  // 如果预读功能启用且需要预读
            block->pgHit = 0;  // 重置预读标志
            bc->prereadFun(bc, block);  // 调用预读函数
        }

        return ENOERR;  // 返回成功
    }

    D(("bcache block = %llu NOT found in cache\n", num));  // 调试日志：缓存未命中

    block = AllocNewBlock(bc, readData, num);  // 分配新的缓存块
    if (block == NULL) {  // 如果分配失败
        block = GetSlowBlock(bc, readData);  // 获取慢路径块（可能需要同步脏块）
    }

    if (block == NULL) {  // 如果仍然无法获取块
        return -ENOMEM;  // 返回内存不足错误
    }
#ifdef BCACHE_ANALYSE
    UINT32 index = ((UINT32)(block->data - g_memStart)) / g_dataSize;  // 计算缓存块索引
    PRINTK(", [MISS], %llu, %u\n", num, index);  // 打印缓存未命中信息
    g_switchTimes[index]++;  // 增加切换计数
#endif
    BlockInit(bc, block, num);  // 初始化新分配的缓存块

    if (readData == TRUE) {  // 如果需要从物理设备读取数据
        D(("bcache reading block = %llu\n", block->num));  // 调试日志：开始读取块

        ret = BlockRead(bc, block, block->data);  // 从物理设备读取数据到缓存块
        if (ret != ENOERR) {  // 检查读取是否失败
            return ret;  // 返回错误码
        }
        if (bc->prereadFun != NULL) {  // 如果预读功能启用
            bc->prereadFun(bc, block);  // 调用预读函数
        }
    }

    AddBlock(bc, block);  // 将新块添加到缓存管理结构

    *dblock = block;  // 设置输出参数
    return ENOERR;  // 返回成功
}

/**
 * @brief 清除缓存中的所有块
 * @param bc 块缓存控制结构体指针
 * @return 始终返回0
 */
INT32 BcacheClearCache(OsBcache *bc)
{
    OsBcacheBlock *block = NULL;  // 当前缓存块指针
    OsBcacheBlock *next = NULL;  // 下一个缓存块指针（用于安全遍历）
    // 安全遍历缓存块列表并删除每个块
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(block, next, &bc->listHead, OsBcacheBlock, listNode) {
        DelBlock(bc, block);  // 从缓存中删除块
    }
    return 0;  // 返回成功
}

/**
 * @brief 初始化块缓存
 * @param bc 块缓存控制结构体指针
 * @param memStart 缓存内存起始地址
 * @param memSize 缓存内存总大小
 * @param blockSize 每个缓存块的大小
 * @return 成功返回ENOERR，失败返回错误码
 */
static INT32 BcacheInitCache(OsBcache *bc,
                             UINT8 *memStart,
                             UINT32 memSize,
                             UINT32 blockSize)
{
    UINT8 *blockMem = NULL;  // 块描述符内存起始地址
    UINT8 *dataMem = NULL;  // 块数据内存起始地址
    OsBcacheBlock *block = NULL;  // 缓存块结构体指针
    UINT32 blockNum, i;  // blockNum: 缓存块数量; i: 循环计数器

    LOS_ListInit(&bc->listHead);  // 初始化缓存块列表
    LOS_ListInit(&bc->numHead);  // 初始化块编号列表
    bc->sumNum = 0;  // 重置总块数
    bc->nBlock = 0;  // 重置当前使用块数

    if (!GetValLog2(blockSize)) {  // 检查块大小是否为2的幂
        PRINT_ERR("GetValLog2(%u) return 0.\n", blockSize);  // 打印错误信息
        return -EINVAL;  // 返回无效参数错误
    }

    bc->rbRoot.rb_node = NULL;  // 初始化红黑树根节点
    bc->memStart = memStart;  // 设置缓存内存起始地址
    bc->blockSize = blockSize;  // 设置块大小
    bc->blockSizeLog2 = GetValLog2(blockSize);  // 计算块大小的对数（用于移位操作）
    bc->modifiedBlock = 0;  // 重置脏块计数

    /* 初始化块内存池 */
    LOS_ListInit(&bc->freeListHead);  // 初始化空闲块列表

    // 计算可分配的块数量（总内存减去对齐空间，除以每个块的总大小）
    blockNum = (memSize - DMA_ALLGN) / (sizeof(OsBcacheBlock) + bc->blockSize);
    blockMem = bc->memStart;  // 块描述符内存起始地址
    dataMem = blockMem + (sizeof(OsBcacheBlock) * blockNum);  // 块数据内存起始地址
    dataMem += ALIGN_DISP((UINTPTR)dataMem);  // 内存地址对齐

#ifdef BCACHE_ANALYSE
    g_memSize = memSize;  // 全局变量：缓存总大小
    g_blockNum = blockNum;  // 全局变量：缓存块数量
    g_dataSize = bc->blockSize;  // 全局变量：块数据大小
    g_memStart = dataMem;  // 全局变量：数据内存起始地址
#endif

    for (i = 0; i < blockNum; i++) {  // 遍历所有块
        block = (OsBcacheBlock *)(VOID *)blockMem;  // 获取当前块描述符
        block->data = dataMem;  // 设置块数据地址
        // 如果是前CONFIG_FS_FAT_READ_NUMS个块，则标记为读缓冲区
        block->readBuff = (i < CONFIG_FS_FAT_READ_NUMS) ? TRUE : FALSE;

        if (i == CONFIG_FS_FAT_READ_NUMS) {  // 如果达到读缓冲区数量
            bc->wStart = block;  // 设置写缓冲区起始位置
        }

        LOS_ListTailInsert(&bc->freeListHead, &block->listNode);  // 将块添加到空闲列表尾部

        blockMem += sizeof(OsBcacheBlock);  // 移动到下一个块描述符
        dataMem += bc->blockSize;  // 移动到下一个块数据区域
    }

    bc->wEnd = block;  // 设置写缓冲区结束位置

    return ENOERR;  // 返回成功
}

/**
 * @brief 通过块设备驱动读取数据
 * @param priv Vnode结构体指针，指向块设备
 * @param buf 数据缓冲区
 * @param len 要读取的数据长度
 * @param pos 读取起始位置
 * @return 成功返回ENOERR，失败返回错误码
 */
static INT32 DrvBread(struct Vnode *priv, UINT8 *buf, UINT32 len, UINT64 pos)
{
    // 获取块设备操作函数集
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)priv->data)->ops;

    INT32 ret = bops->read(priv, buf, pos, len);  // 调用块设备读函数
    if (ret != (INT32)len) {  // 检查读取长度是否匹配请求长度
        PRINT_ERR("%s failure\n", __FUNCTION__);  // 打印错误信息
        return ret;  // 返回错误码
    }
    return ENOERR;  // 返回成功
}
/**
 * @brief 块设备写操作驱动函数
 * @param priv Vnode节点指针，指向块设备
 * @param buf 待写入数据缓冲区
 * @param len 写入数据长度
 * @param pos 写入起始位置（字节偏移）
 * @return 成功返回写入长度，失败返回错误码
 */
static INT32 DrvBwrite(struct Vnode *priv, const UINT8 *buf, UINT32 len, UINT64 pos)
{
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)priv->data)->ops;  // 获取块设备操作接口
    INT32 ret = bops->write(priv, buf, pos, len);  // 调用底层写操作
    if (ret != (INT32)len) {  // 检查写入长度是否匹配
        PRINT_ERR("%s failure\n", __FUNCTION__);  // 打印错误信息
        return ret;  // 返回错误码
    }
    return ENOERR;  // 返回成功
}

/**
 * @brief 创建块缓存驱动实例
 * @param handle 块设备句柄
 * @param memStart 缓存内存起始地址
 * @param memSize 缓存内存大小
 * @param blockSize 块大小
 * @param bc 块缓存控制结构体指针
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 BlockCacheDrvCreate(VOID *handle,
                          UINT8 *memStart,
                          UINT32 memSize,
                          UINT32 blockSize,
                          OsBcache *bc)
{
    INT32 ret;
    bc->priv = handle;  // 设置设备句柄
    bc->breadFun = DrvBread;  // 绑定读操作函数
    bc->bwriteFun = DrvBwrite;  // 绑定写操作函数

    ret = BcacheInitCache(bc, memStart, memSize, blockSize);  // 初始化缓存
    if (ret != ENOERR) {  // 检查初始化结果
        return ret;  // 返回错误码
    }

    if (pthread_mutex_init(&bc->bcacheMutex, NULL) != ENOERR) {  // 初始化缓存互斥锁
        return VFS_ERROR;  // 返回错误码
    }
    bc->bcacheMutex.attr.type = PTHREAD_MUTEX_RECURSIVE;  // 设置为可重入互斥锁

    return ENOERR;  // 返回成功
}

/**
 * @brief 从块缓存读取数据
 * @param bc 块缓存控制结构体指针
 * @param buf 数据接收缓冲区
 * @param len 输入输出参数，输入请求长度，输出实际读取长度
 * @param sector 起始扇区
 * @param useRead 是否使用预读标记
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 BlockCacheRead(OsBcache *bc, UINT8 *buf, UINT32 *len, UINT64 sector, BOOL useRead)
{
    OsBcacheBlock *block = NULL;  // 块缓存节点指针
    UINT8 *tempBuf = buf;  // 临时缓冲区指针
    UINT32 size;  // 剩余读取大小
    UINT32 currentSize;  // 当前块读取大小
    INT32 ret = ENOERR;  // 返回值
    UINT64 pos;  // 块内偏移
    UINT64 num;  // 块编号
#ifdef BCACHE_ANALYSE
    PRINTK("bcache read:\n");  // 调试信息打印
#endif

    if (bc == NULL || buf == NULL || len == NULL) {  // 参数合法性检查
        return -EPERM;  // 返回权限错误
    }

    size = *len;  // 获取请求读取长度
    pos = sector * bc->sectorSize;  // 计算起始字节位置
    num = pos >> bc->blockSizeLog2;  // 计算块编号（右移等价于除以块大小）
    pos = pos & (bc->blockSize - 1);  // 计算块内偏移（取模运算）

    while (size > 0) {  // 循环读取直到满足请求长度
        if ((size + pos) > bc->blockSize) {  // 判断是否跨块
            currentSize = bc->blockSize - (UINT32)pos;  // 当前块可读取大小
        } else {
            currentSize = size;  // 剩余数据可在当前块读取
        }

        (VOID)pthread_mutex_lock(&bc->bcacheMutex);  // 加锁保护缓存操作

        /* 读取大连续数据时useRead应设为FALSE */
        ret = BcacheGetBlock(bc, num, useRead, &block);  // 获取缓存块
        if (ret != ENOERR) {  // 检查获取结果
            (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁
            break;  // 退出循环
        }

        if ((block->readFlag == FALSE) && (block->modified == TRUE)) {  // 块未读取但已修改
            ret = BcacheGetFlag(bc, block);  // 获取块标志
            if (ret != ENOERR) {  // 检查标志获取结果
                (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁
                return ret;  // 返回错误
            }
        } else if ((block->readFlag == FALSE) && (block->modified == FALSE)) {  // 块未读取且未修改
            ret = BlockRead(bc, block, block->data);  // 从设备读取块数据
            if (ret != ENOERR) {  // 检查读取结果
                (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁
                return ret;  // 返回错误
            }
        }

        if (LOS_CopyFromKernel((VOID *)tempBuf, size, (VOID *)(block->data + pos), currentSize) != EOK) {  // 从内核空间复制数据
            (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁
            return VFS_ERROR;  // 返回错误
        }

        (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁

        tempBuf += currentSize;  // 移动缓冲区指针
        size -= currentSize;  // 更新剩余读取大小
        pos = 0;  // 重置块内偏移
        num++;  // 块编号递增
    }
    *len -= size;  // 更新实际读取长度
    return ret;  // 返回结果
}

/**
 * @brief 向块缓存写入数据
 * @param bc 块缓存控制结构体指针
 * @param buf 待写入数据缓冲区
 * @param len 输入输出参数，输入请求长度，输出实际写入长度
 * @param sector 起始扇区
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 BlockCacheWrite(OsBcache *bc, const UINT8 *buf, UINT32 *len, UINT64 sector)
{
    OsBcacheBlock *block = NULL;  // 块缓存节点指针
    const UINT8 *tempBuf = buf;  // 临时缓冲区指针
    UINT32 size = *len;  // 剩余写入大小
    INT32 ret = ENOERR;  // 返回值
    UINT32 currentSize;  // 当前块写入大小
    UINT64 pos;  // 块内偏移
    UINT64 num;  // 块编号
#ifdef BCACHE_ANALYSE
    PRINTK("bcache write:\n");  // 调试信息打印
#endif

    pos = sector * bc->sectorSize;  // 计算起始字节位置
    num = pos >> bc->blockSizeLog2;  // 计算块编号
    pos = pos & (bc->blockSize - 1);  // 计算块内偏移

    D(("bcache write len = %u pos = %llu bnum = %llu\n", *len, pos, num));  // 调试日志

    while (size > 0) {  // 循环写入直到满足请求长度
        if ((size + pos) > bc->blockSize) {  // 判断是否跨块
            currentSize = bc->blockSize - (UINT32)pos;  // 当前块可写入大小
        } else {
            currentSize = size;  // 剩余数据可在当前块写入
        }

        (VOID)pthread_mutex_lock(&bc->bcacheMutex);  // 加锁保护缓存操作
        ret = BcacheGetBlock(bc, num, FALSE, &block);  // 获取缓存块
        if (ret != ENOERR) {  // 检查获取结果
            (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁
            break;  // 退出循环
        }

        if (LOS_CopyToKernel((VOID *)(block->data + pos), bc->blockSize - (UINT32)pos,
            (VOID *)tempBuf, currentSize) != EOK) {  // 复制数据到内核空间
            (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁
            return VFS_ERROR;  // 返回错误
        }
        if (block->modified == FALSE) {  // 检查块是否已标记为修改
            block->modified = TRUE;  // 标记块为已修改
            bc->modifiedBlock++;  // 增加修改块计数
        }
        if ((pos == 0) && (currentSize == bc->blockSize)) {  // 整块写入
            (void)memset_s(block->flag, sizeof(block->flag), 0xFF, sizeof(block->flag));  // 标记所有扇区为脏
            block->allDirty = TRUE;  // 标记整块为脏
        } else {
            BcacheSetFlag(bc, block, (UINT32)pos, currentSize);  // 设置部分扇区脏标志
        }
        (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁

        tempBuf += currentSize;  // 移动缓冲区指针
        size -= currentSize;  // 更新剩余写入大小
        pos = 0;  // 重置块内偏移
        num++;  // 块编号递增
    }
    *len -= size;  // 更新实际写入长度
    return ret;  // 返回结果
}

/**
 * @brief 同步块缓存数据到设备
 * @param bc 块缓存控制结构体指针
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 BlockCacheSync(OsBcache *bc)
{
    return BcacheSync(bc);  // 调用底层同步函数
}

/**
 * @brief 根据磁盘ID同步块缓存
 * @param id 磁盘ID
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 OsSdSync(INT32 id)
{
#ifdef LOSCFG_FS_FAT_CACHE
    INT32 ret;
    los_disk *disk = get_disk(id);  // 获取磁盘信息
    if ((disk == NULL) || (disk->disk_status == STAT_UNUSED)) {  // 检查磁盘状态
        return VFS_ERROR;  // 返回错误
    }
    if (pthread_mutex_lock(&disk->disk_mutex) != ENOERR) {  // 加锁保护磁盘操作
        PRINT_ERR("%s %d, mutex lock fail!\n", __FUNCTION__, __LINE__);  // 打印错误信息
        return VFS_ERROR;  // 返回错误
    }
    if ((disk->disk_status == STAT_INUSED) && (disk->bcache != NULL)) {  // 检查磁盘状态和缓存
        ret = BcacheSync(disk->bcache);  // 同步缓存
    } else {
        ret = VFS_ERROR;  // 返回错误
    }
    if (pthread_mutex_unlock(&disk->disk_mutex) != ENOERR) {  // 解锁
        PRINT_ERR("%s %d, mutex unlock fail!\n", __FUNCTION__, __LINE__);  // 打印错误信息
        return VFS_ERROR;  // 返回错误
    }
    return ret;  // 返回结果
#else
    return VFS_ERROR;  // 未启用FAT缓存时返回错误
#endif
}

/**
 * @brief 根据磁盘名称同步块缓存
 * @param name 磁盘名称
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 LOS_BcacheSyncByName(const CHAR *name)
{
    INT32 diskID = los_get_diskid_byname(name);  // 根据名称获取磁盘ID
    return OsSdSync(diskID);  // 调用ID同步接口
}

/**
 * @brief 获取磁盘脏块比例
 * @param id 磁盘ID
 * @return 成功返回脏块百分比，失败返回错误码
 */
INT32 BcacheGetDirtyRatio(INT32 id)
{
#ifdef LOSCFG_FS_FAT_CACHE
    INT32 ret;
    los_disk *disk = get_disk(id);  // 获取磁盘信息
    if (disk == NULL) {  // 检查磁盘有效性
        return VFS_ERROR;  // 返回错误
    }

    if (pthread_mutex_lock(&disk->disk_mutex) != ENOERR) {  // 加锁保护
        PRINT_ERR("%s %d, mutex lock fail!\n", __FUNCTION__, __LINE__);  // 打印错误
        return VFS_ERROR;  // 返回错误
    }
    if ((disk->disk_status == STAT_INUSED) && (disk->bcache != NULL)) {  // 检查磁盘状态和缓存
        ret = (INT32)((disk->bcache->modifiedBlock * PERCENTAGE) / GetFatBlockNums());  // 计算脏块比例
    } else {
        ret = VFS_ERROR;  // 返回错误
    }
    if (pthread_mutex_unlock(&disk->disk_mutex) != ENOERR) {  // 解锁
        PRINT_ERR("%s %d, mutex unlock fail!\n", __FUNCTION__, __LINE__);  // 打印错误
        return VFS_ERROR;  // 返回错误
    }
    return ret;  // 返回脏块比例
#else
    return VFS_ERROR;  // 未启用FAT缓存时返回错误
#endif
}

/**
 * @brief 根据磁盘名称获取脏块比例
 * @param name 磁盘名称
 * @return 成功返回脏块百分比，失败返回错误码
 */
INT32 LOS_GetDirtyRatioByName(const CHAR *name)
{
    INT32 diskID = los_get_diskid_byname(name);  // 根据名称获取磁盘ID
    return BcacheGetDirtyRatio(diskID);  // 调用ID获取接口
}

#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD  // 条件编译：缓存同步线程
/**
 * @brief 块缓存同步线程函数
 * @param id 磁盘ID
 */
static VOID BcacheSyncThread(UINT32 id)
{
    INT32 diskID = (INT32)id;  // 磁盘ID
    INT32 dirtyRatio;  // 脏块比例
    while (1) {  // 无限循环
        dirtyRatio = BcacheGetDirtyRatio(diskID);  // 获取脏块比例
        if (dirtyRatio > (INT32)g_dirtyRatio) {  // 超过阈值时同步
            (VOID)OsSdSync(diskID);  // 同步缓存
        }
        msleep(g_syncInterval);  // 等待同步间隔
    }
}

/**
 * @brief 初始化块缓存同步线程
 * @param bc 块缓存控制结构体指针
 * @param id 磁盘ID
 */
VOID BcacheSyncThreadInit(OsBcache *bc, INT32 id)
{
    UINT32 ret;
    TSK_INIT_PARAM_S appTask;  // 任务参数结构体

    (VOID)memset_s(&appTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));  // 初始化任务参数
    appTask.pfnTaskEntry = (TSK_ENTRY_FUNC)BcacheSyncThread;  // 设置任务入口函数
    appTask.uwStackSize = BCACHE_STATCK_SIZE;  // 设置栈大小
    appTask.pcName = "bcache_sync_task";  // 设置任务名称
    appTask.usTaskPrio = g_syncThreadPrio;  // 设置任务优先级
    appTask.auwArgs[0] = (UINTPTR)id;  // 设置任务参数（磁盘ID）
    appTask.uwResved = LOS_TASK_STATUS_DETACHED;  // 设置为分离状态
    ret = LOS_TaskCreate(&bc->syncTaskId, &appTask);  // 创建任务
    if (ret != ENOERR) {  // 检查创建结果
        PRINT_ERR("Bcache sync task create failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误
    }
}

/**
 * @brief 反初始化块缓存同步线程
 * @param bc 块缓存控制结构体指针
 */
VOID BcacheSyncThreadDeinit(const OsBcache *bc)
{
    if (bc != NULL) {  // 检查缓存有效性
        if (LOS_TaskDelete(bc->syncTaskId) != ENOERR) {  // 删除任务
            PRINT_ERR("Bcache sync task delete failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误
        }
    }
}
#endif  // LOSCFG_FS_FAT_CACHE_SYNC_THREAD

/**
 * @brief 初始化块缓存
 * @param devNode 设备Vnode节点
 * @param sectorSize 扇区大小
 * @param sectorPerBlock 每块扇区数
 * @param blockNum 块数量
 * @param blockCount 总块数
 * @return 成功返回块缓存指针，失败返回NULL
 */
OsBcache *BlockCacheInit(struct Vnode *devNode, UINT32 sectorSize, UINT32 sectorPerBlock,
                         UINT32 blockNum, UINT64 blockCount)
{
    OsBcache *bcache = NULL;  // 块缓存控制结构体指针
    struct Vnode *blkDriver = devNode;  // 块设备驱动节点
    UINT8 *bcacheMem = NULL;  // 缓存内存
    UINT8 *rwBuffer = NULL;  // 读写缓冲区
    UINT32 blockSize, memSize;  // 块大小和内存大小

    if ((blkDriver == NULL) || (sectorSize * sectorPerBlock * blockNum == 0) || (blockCount == 0)) {  // 参数检查
        return NULL;  // 返回NULL
    }

    blockSize = sectorSize * sectorPerBlock;  // 计算块大小
    if ((((UINT64)(sizeof(OsBcacheBlock) + blockSize) * blockNum) + DMA_ALLGN) > UINT_MAX) {  // 内存溢出检查
        return NULL;  // 返回NULL
    }
    memSize = ((sizeof(OsBcacheBlock) + blockSize) * blockNum) + DMA_ALLGN;  // 计算总内存大小

    bcache = (OsBcache *)zalloc(sizeof(OsBcache));  // 分配缓存控制结构体
    if (bcache == NULL) {  // 检查分配结果
        PRINT_ERR("bcache_init : malloc %u Bytes failed!\n", sizeof(OsBcache));  // 打印错误
        return NULL;  // 返回NULL
    }

    bcacheMem = (UINT8 *)zalloc(memSize);  // 分配缓存内存
    if (bcacheMem == NULL) {  // 检查分配结果
        PRINT_ERR("bcache_init : malloc %u Bytes failed!\n", memSize);  // 打印错误
        goto ERROR_OUT_WITH_BCACHE;  // 跳转到错误处理
    }

    rwBuffer = (UINT8 *)memalign(DMA_ALLGN, blockSize);  // 分配DMA对齐的读写缓冲区
    if (rwBuffer == NULL) {  // 检查分配结果
        PRINT_ERR("bcache_init : malloc %u Bytes failed!\n", blockSize);  // 打印错误
        goto ERROR_OUT_WITH_MEM;  // 跳转到错误处理
    }

    bcache->rwBuffer = rwBuffer;  // 设置读写缓冲区
    bcache->sectorSize = sectorSize;  // 设置扇区大小
    bcache->sectorPerBlock = sectorPerBlock;  // 设置每块扇区数
    bcache->blockCount = blockCount;  // 设置总块数

    if (BlockCacheDrvCreate(blkDriver, bcacheMem, memSize, blockSize, bcache) != ENOERR) {  // 创建缓存驱动
        goto ERROR_OUT_WITH_BUFFER;  // 跳转到错误处理
    }

    return bcache;  // 返回缓存指针

ERROR_OUT_WITH_BUFFER:  // 缓冲区错误处理标签
    free(rwBuffer);  // 释放读写缓冲区
ERROR_OUT_WITH_MEM:  // 内存错误处理标签
    free(bcacheMem);  // 释放缓存内存
ERROR_OUT_WITH_BCACHE:  // 缓存错误处理标签
    free(bcache);  // 释放缓存控制结构体
    return NULL;  // 返回NULL
}

/**
 * @brief 反初始化块缓存
 * @param bcache 块缓存控制结构体指针
 */
VOID BlockCacheDeinit(OsBcache *bcache)
{
    if (bcache != NULL) {  // 检查缓存有效性
        (VOID)pthread_mutex_destroy(&bcache->bcacheMutex);  // 销毁互斥锁
        free(bcache->memStart);  // 释放缓存内存
        bcache->memStart = NULL;  // 置空指针
        free(bcache->rwBuffer);  // 释放读写缓冲区
        bcache->rwBuffer = NULL;  // 置空指针
        free(bcache);  // 释放缓存控制结构体
    }
}

/**
 * @brief 块缓存异步预读线程
 * @param arg 块缓存控制结构体指针
 */
static VOID BcacheAsyncPrereadThread(VOID *arg)
{
    OsBcache *bc = (OsBcache *)arg;  // 块缓存控制结构体指针
    OsBcacheBlock *block = NULL;  // 块缓存节点指针
    INT32 ret;  // 返回值
    UINT32 i;  // 循环变量

    for (;;) {  // 无限循环
        ret = (INT32)LOS_EventRead(&bc->bcacheEvent, PREREAD_EVENT_MASK,
                                   LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);  // 等待预读事件
        if (ret != ASYNC_EVENT_BIT) {  // 检查事件标志
            PRINT_ERR("The event read in %s, %d is error!!!\n", __FUNCTION__, __LINE__);  // 打印错误
            continue;  // 继续循环
        }

        for (i = 1; i <= PREREAD_BLOCK_NUM; i++) {  // 预读多个块
            if ((bc->curBlockNum + i) >= bc->blockCount) {  // 检查是否超出总块数
                break;  // 退出循环
            }

            (VOID)pthread_mutex_lock(&bc->bcacheMutex);  // 加锁保护
            ret = BcacheGetBlock(bc, bc->curBlockNum + i, TRUE, &block);  // 获取预读块
            if (ret != ENOERR) {  // 检查获取结果
                PRINT_ERR("read block %llu error : %d!\n", bc->curBlockNum, ret);  // 打印错误
            }

            (VOID)pthread_mutex_unlock(&bc->bcacheMutex);  // 解锁
        }

        if (block != NULL) {  // 检查块有效性
            block->pgHit = 1; /* 预读完成标记 */
        }
    }
}

/**
 * @brief 恢复异步预读操作
 * @param arg1 块缓存控制结构体指针
 * @param arg2 当前块缓存节点
 */
VOID ResumeAsyncPreread(OsBcache *arg1, const OsBcacheBlock *arg2)
{
    UINT32 ret;
    OsBcache *bc = arg1;  // 块缓存控制结构体指针
    const OsBcacheBlock *block = arg2;  // 当前块缓存节点

    if (OsCurrTaskGet()->taskID != bc->prereadTaskId) {  // 检查当前任务是否为预读任务
        bc->curBlockNum = block->num;  // 更新当前块编号
        ret = LOS_EventWrite(&bc->bcacheEvent, ASYNC_EVENT_BIT);  // 写入预读事件
        if (ret != ENOERR) {  // 检查事件写入结果
            PRINT_ERR("Write event failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误
        }
    }
}

/**
 * @brief 初始化块缓存异步预读
 * @param bc 块缓存控制结构体指针
 * @return 成功返回ENOERR，失败返回错误码
 */
UINT32 BcacheAsyncPrereadInit(OsBcache *bc)
{
    UINT32 ret;
    TSK_INIT_PARAM_S appTask;  // 任务参数结构体

    ret = LOS_EventInit(&bc->bcacheEvent);  // 初始化事件
    if (ret != ENOERR) {  // 检查初始化结果
        PRINT_ERR("Async event init failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误
        return ret;  // 返回错误码
    }

    (VOID)memset_s(&appTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));  // 初始化任务参数
    appTask.pfnTaskEntry = (TSK_ENTRY_FUNC)BcacheAsyncPrereadThread;  // 设置任务入口函数
    appTask.uwStackSize = BCACHE_STATCK_SIZE;  // 设置栈大小
    appTask.pcName = "bcache_async_task";  // 设置任务名称
    appTask.usTaskPrio = BCACHE_PREREAD_PRIO;  // 设置任务优先级
    appTask.auwArgs[0] = (UINTPTR)bc;  // 设置任务参数
    appTask.uwResved = LOS_TASK_STATUS_DETACHED;  // 设置为分离状态
    ret = LOS_TaskCreate(&bc->prereadTaskId, &appTask);  // 创建任务
    if (ret != ENOERR) {  // 检查创建结果
        PRINT_ERR("Bcache async task create failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误
    }

    return ret;  // 返回结果
}

/**
 * @brief 反初始化块缓存异步预读
 * @param bc 块缓存控制结构体指针
 * @return 成功返回ENOERR，失败返回错误码
 */
UINT32 BcacheAsyncPrereadDeinit(OsBcache *bc)
{
    UINT32 ret = LOS_NOK;  // 返回值

    if (bc != NULL) {  // 检查缓存有效性
        ret = LOS_TaskDelete(bc->prereadTaskId);  // 删除预读任务
        if (ret != ENOERR) {  // 检查删除结果
            PRINT_ERR("Bcache async task delete failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误
        }

        ret = LOS_EventDestroy(&bc->bcacheEvent);  // 销毁事件
        if (ret != ENOERR) {  // 检查销毁结果
            PRINT_ERR("Async event destroy failed in %s, %d\n", __FUNCTION__, __LINE__);  // 打印错误
            return ret;  // 返回错误码
        }
    }

    return ret;  // 返回结果
}