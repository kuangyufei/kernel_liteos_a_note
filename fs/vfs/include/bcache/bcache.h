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

#ifndef _BCACHE_H
#define _BCACHE_H

#include "pthread.h"
#include "linux/rbtree.h"
#include "los_list.h"
#include "vnode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define ALIGN_LIB(x)          (((x) + (HALARC_ALIGNMENT - 1)) & ~(HALARC_ALIGNMENT - 1))  // 内存对齐宏，将x按HALARC_ALIGNMENT大小对齐
#define ALIGN_DISP(x)         (HALARC_ALIGNMENT - ((x) & (HALARC_ALIGNMENT - 1)))  // 计算对齐偏移量
#define BCACHE_PREREAD_PRIO   12  // 预读任务优先级
#define UNSIGNED_INTEGER_BITS 32  // 无符号整数位数
#define UNINT_MAX_SHIFT_BITS  31  // 无符号整数最大移位位数
#define UNINT_LOG2_SHIFT      5  // 用于计算log2的移位值
#define PREREAD_BLOCK_NUM     2  // 预读块数量
#define EVEN_JUDGED           2  // 偶数判断值
#define PERCENTAGE            100  // 百分比基数
#define PREREAD_EVENT_MASK    0xf  // 预读事件掩码

#if CONFIG_FS_FAT_SECTOR_PER_BLOCK < UNSIGNED_INTEGER_BITS  // 检查每块扇区数是否小于无符号整数位数
#error cache too small  // 缓存太小错误
#else
#define BCACHE_BLOCK_FLAGS (CONFIG_FS_FAT_SECTOR_PER_BLOCK / UNSIGNED_INTEGER_BITS)  // 计算块标志数组大小
#endif

typedef struct {  // 缓存块结构体，代表一个缓存块的元数据和数据
    LOS_DL_LIST listNode;   /* list node */  // 链表节点
    LOS_DL_LIST numNode;    /* num node */  // 块号链表节点
    struct rb_node rbNode;  /* red-black tree node */  // 红黑树节点
    UINT64 num;             /* block number */  // 块号
    UINT32 flag[BCACHE_BLOCK_FLAGS];  // 块标志数组
    UINT32 pgHit;  // 页面命中次数
    UINT8 *data;            /* block data */  // 块数据指针
    BOOL modified;          /* is this block data modified (needs write) */  // 数据是否被修改
    BOOL readFlag;          /* is the block data have read from sd(real data) */  // 是否已从SD卡读取真实数据
    BOOL readBuff;          /* read write buffer */  // 读写缓冲区标志
    BOOL used;              /* used or free for write buf */  // 是否被写缓冲区使用
    BOOL allDirty;          /* the whole block is dirty */  // 整个块是否为脏数据
} OsBcacheBlock;

typedef INT32 (*BcacheReadFun)(struct Vnode *, /* private data */  // 块读取函数指针类型
                               UINT8 *,        /* block buffer */  // 块缓冲区
                               UINT32,         /* number of blocks to read */  // 要读取的块数
                               UINT64);        /* starting block number */  // 起始块号

typedef INT32 (*BcacheWriteFun)(struct Vnode *, /* private data */  // 块写入函数指针类型
                                const UINT8 *,  /* block buffer */  // 块缓冲区
                                UINT32,         /* number of blocks to write */  // 要写入的块数
                                UINT64);        /* starting block number */  // 起始块号

struct tagOsBcache;  // 前向声明块缓存结构体

typedef VOID (*BcachePrereadFun)(struct tagOsBcache *,   /* block cache instance space holder */  // 块预读函数指针类型
                                 const OsBcacheBlock *); /* block data */  // 块数据

typedef struct tagOsBcache {  // 块缓存实例结构体，管理整个块缓存系统
    VOID *priv;                   /* private data */  // 私有数据
    LOS_DL_LIST listHead;         /* head of block list */  // 块列表头
    LOS_DL_LIST numHead;          /* block num list */  // 块号列表头
    struct rb_root rbRoot;        /* block red-black tree root */  // 块红黑树的根
    UINT32 blockSize;             /* block size in bytes */  // 块大小(字节)
    UINT32 blockSizeLog2;         /* block size log2 */  // 块大小的log2值
    UINT64 blockCount;            /* block count of the disk */  // 磁盘总块数
    UINT32 sectorSize;            /* device sector size in bytes */  // 设备扇区大小(字节)
    UINT32 sectorPerBlock;        /* sector count per block */  // 每块包含的扇区数
    UINT8 *memStart;              /* memory base */  // 内存起始地址
    UINT32 prereadTaskId;         /* preread task id */  // 预读任务ID
    UINT64 curBlockNum;           /* current preread block number */  // 当前预读块号
    LOS_DL_LIST freeListHead;     /* list of free blocks */  // 空闲块列表头
    BcacheReadFun breadFun;       /* block read function */  // 块读取函数
    BcacheWriteFun bwriteFun;     /* block write function */  // 块写入函数
    BcachePrereadFun prereadFun;  /* block preread function */  // 块预读函数
    UINT8 *rwBuffer;              /* buffer for bcache block */  // 块缓存读写缓冲区
    pthread_mutex_t bcacheMutex;  /* mutex for bcache */  // 块缓存互斥锁
    EVENT_CB_S bcacheEvent;       /* event for bcache */  // 块缓存事件
    UINT32 modifiedBlock;         /* number of modified blocks */  // 修改过的块数量
#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD
    UINT32 syncTaskId;            /* sync task id */  // 同步任务ID
#endif
    OsBcacheBlock *wStart;        /* write start block */  // 写起始块
    OsBcacheBlock *wEnd;          /* write end block */  // 写结束块
    UINT64 sumNum;                /* block num sum val */  // 块号总和值
    UINT32 nBlock;                /* current block count */  // 当前块数量
} OsBcache;  // 块缓存结构体结束

/**
 * @ingroup  bcache
 *
 * @par Description:
 * The BlockCacheRead() function shall read data from the bcache, and if it doesn't hit, read the data from disk.
 *
 * @param  bc    [IN]  block cache instance
 * @param  buf   [OUT] data buffer ptr
 * @param  len   [IN]  number of bytes to read
 * @param  num   [IN]  starting block number
 * @param  pos   [IN]  starting position inside starting block
 * @param  useRead [IN]  whether use the read block or write block
 *
 * @attention
 * <ul>
 * <li>The block number is automatically adjusted if position is greater than block size.</li>
 * </ul>
 *
 * @retval #0           read succeded
 * @retval #INT32       read failed
 *
 * @par Dependency:
 * <ul><li>bcache.h</li></ul>
 *
 */
INT32 BlockCacheRead(OsBcache *bc,
                     UINT8 *buf,
                     UINT32 *len,
                     UINT64 pos,
                     BOOL useRead);

/**
 * @ingroup  bcache
 *
 * @par Description:
 * The BlockCacheWrite() function shall write data to the bcache.
 *
 * @param  bc    [IN]  block cache instance
 * @param  buf   [IN]  data buffer ptr
 * @param  len   [IN]  number of bytes to write
 * @param  num   [IN]  starting block number
 * @param  pos   [IN]  starting position inside starting block
 *
 * @attention
 * <ul>
 * <li>The block number is automatically adjusted if position is greater than block size.</li>
 * </ul>
 *
 * @retval #0           write succeded
 * @retval #INT32       write failed
 *
 * @par Dependency:
 * <ul><li>bcache.h</li></ul>
 *
 */
INT32 BlockCacheWrite(OsBcache *bc,
                      const UINT8 *buf,
                      UINT32 *len,
                      UINT64 pos);

/**
 * @ingroup  bcache
 *
 * @par Description:
 * The BlockCacheSync() function shall write-back all dirty data in the bcache into the disk.
 *
 * @param  bc    [IN]  block cache instance
 *
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @retval #0           sync succeded
 * @retval #INT32       sync failed
 *
 * @par Dependency:
 * <ul><li>bcache.h</li></ul>
 *
 */
INT32 BlockCacheSync(OsBcache *bc);

/**
 * @ingroup  bcache
 *
 * @par Description:
 * The BlockCacheInit() function shall alloc memory for bcache and init it.
 *
 * @param  devNode          [IN]  dev node instance
 * @param  sectorSize       [IN]  size of a sector
 * @param  sectorPerBlock   [IN]  sector count per block in bcache
 * @param  blockNum         [IN]  block number of bcache
 * @param  blockCount       [IN]  block count of the disk
 *
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @retval #OsBcache *      init succeded
 * @retval #NULL            init failed
 *
 * @par Dependency:
 * <ul><li>bcache.h</li></ul>
 *
 */
OsBcache *BlockCacheInit(struct Vnode *devNode,
                         UINT32 sectorSize,
                         UINT32 sectorPerBlock,
                         UINT32 blockNum,
                         UINT64 blockCount);

/**
 * @ingroup  bcache
 *
 * @par Description:
 * The BlockCacheDeinit() function shall deinit the bcache and release resources.
 *
 * @param  bc    [IN]  block cache instance
 *
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @retval #VOID None.
 *
 * @par Dependency:
 * <ul><li>bcache.h</li></ul>
 *
 */
VOID BlockCacheDeinit(OsBcache *bc);

INT32 BcacheClearCache(OsBcache *bc);
INT32 OsSdSync(INT32 id);

#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD
VOID BcacheSyncThreadInit(OsBcache *bc, INT32 id);
VOID BcacheSyncThreadDeinit(const OsBcache *bc);
#endif

UINT32 BcacheAsyncPrereadInit(OsBcache *bc);

VOID ResumeAsyncPreread(OsBcache *arg1, const OsBcacheBlock *arg2);

UINT32 BcacheAsyncPrereadDeinit(OsBcache *bc);

#ifdef __cplusplus
#if __cplusplus
}
#endif  /* __cplusplus */
#endif  /* __cplusplus */
#endif  /* _BCACHE_H */
