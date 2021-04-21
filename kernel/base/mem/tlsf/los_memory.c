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

#include "los_memory.h"
#include "los_memory_pri.h"
#include "sys/param.h"
#include "los_spinlock.h"
#include "los_vm_phys.h"
#include "los_vm_boot.h"
#include "los_vm_filemap.h"
#include "los_task_pri.h"
#ifdef LOSCFG_KERNEL_TRACE
#include "los_trace_frame.h"
#include "los_trace.h"
#endif


/* Used to cut non-essential functions. */
#define OS_MEM_FREE_BY_TASKID   0
#ifdef LOSCFG_KERNEL_VM
#define OS_MEM_EXPAND_ENABLE    1
#else
#define OS_MEM_EXPAND_ENABLE    0
#endif

/* the dump size of current broken node when memcheck error */
#define OS_MEM_NODE_DUMP_SIZE   64
/* column num of the output info of mem node */
#define OS_MEM_COLUMN_NUM       8

UINT8 *m_aucSysMem0 = NULL;
UINT8 *m_aucSysMem1 = NULL;

#ifdef LOSCFG_MEM_MUL_POOL
VOID *g_poolHead = NULL;
#endif

/* The following is the macro definition and interface implementation related to the TLSF. */

/* Supposing a Second Level Index: SLI = 3. */
#define OS_MEM_SLI                      3
/* Giving 1 free list for each small bucket: 4, 8, 12, up to 124. */
#define OS_MEM_SMALL_BUCKET_COUNT       31
#define OS_MEM_SMALL_BUCKET_MAX_SIZE    128
/* Giving OS_MEM_FREE_LIST_NUM free lists for each large bucket. */
#define OS_MEM_LARGE_BUCKET_COUNT       24
#define OS_MEM_FREE_LIST_NUM            (1 << OS_MEM_SLI)
/* OS_MEM_SMALL_BUCKET_MAX_SIZE to the power of 2 is 7. */
#define OS_MEM_LARGE_START_BUCKET       7

/* The count of free list. */
#define OS_MEM_FREE_LIST_COUNT  (OS_MEM_SMALL_BUCKET_COUNT + (OS_MEM_LARGE_BUCKET_COUNT << OS_MEM_SLI))
/* The bitmap is used to indicate whether the free list is empty, 1: not empty, 0: empty. */
#define OS_MEM_BITMAP_WORDS     ((OS_MEM_FREE_LIST_COUNT >> 5) + 1)

#define OS_MEM_BITMAP_MASK 0x1FU

/* Used to find the first bit of 1 in bitmap. */
STATIC INLINE UINT16 OsMemFFS(UINT32 bitmap)
{
    bitmap &= ~bitmap + 1;
    return (OS_MEM_BITMAP_MASK - CLZ(bitmap));
}

/* Used to find the last bit of 1 in bitmap. */
STATIC INLINE UINT16 OsMemFLS(UINT32 bitmap)
{
    return (OS_MEM_BITMAP_MASK - CLZ(bitmap));
}

STATIC INLINE UINT32 OsMemLog2(UINT32 size)
{
    return OsMemFLS(size);
}

/* Get the first level: f = log2(size). */
STATIC INLINE UINT32 OsMemFlGet(UINT32 size)
{
    if (size < OS_MEM_SMALL_BUCKET_MAX_SIZE) {
        return ((size >> 2) - 1); /* 2: The small bucket setup is 4. */
    }
    return OsMemLog2(size);
}

/* Get the second level: s = (size - 2^f) * 2^SLI / 2^f. */
STATIC INLINE UINT32 OsMemSlGet(UINT32 size, UINT32 fl)
{
    return (((size << OS_MEM_SLI) >> fl) - OS_MEM_FREE_LIST_NUM);
}

/* The following is the memory algorithm related macro definition and interface implementation. */

struct OsMemNodeHead {
    UINT32 magic;
    union {
        struct OsMemNodeHead *prev; /* The prev is used for current node points to the previous node */
        struct OsMemNodeHead *next; /* The next is used for last node points to the expand node */
    } ptr;
#ifdef LOSCFG_MEM_LEAKCHECK
    UINTPTR linkReg[LOS_RECORD_LR_CNT];
#endif
    UINT32 sizeAndFlag;
};

struct OsMemUsedNodeHead {
    struct OsMemNodeHead header;
#if OS_MEM_FREE_BY_TASKID
    UINT32 taskID;
#endif
};

struct OsMemFreeNodeHead {
    struct OsMemNodeHead header;
    struct OsMemFreeNodeHead *prev;
    struct OsMemFreeNodeHead *next;
};

struct OsMemPoolInfo {
    VOID *pool;
    UINT32 totalSize;
    UINT32 attr;
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    UINT32 waterLine;   /* Maximum usage size in a memory pool */
    UINT32 curUsedSize; /* Current usage size in a memory pool */
#endif
};

struct OsMemPoolHead {
    struct OsMemPoolInfo info;
    UINT32 freeListBitmap[OS_MEM_BITMAP_WORDS];
    struct OsMemFreeNodeHead *freeList[OS_MEM_FREE_LIST_COUNT];
    SPIN_LOCK_S spinlock;
#ifdef LOSCFG_MEM_MUL_POOL
    VOID *nextPool;
#endif
};

/* Spinlock for mem module, only available on SMP mode */
#define MEM_LOCK(pool, state)       LOS_SpinLockSave(&(pool)->spinlock, &(state))
#define MEM_UNLOCK(pool, state)     LOS_SpinUnlockRestore(&(pool)->spinlock, (state))

/* The memory pool support expand. */
#define OS_MEM_POOL_EXPAND_ENABLE  0x01
/* The memory pool ssupport no lock. */
#define OS_MEM_POOL_LOCK_ENABLE    0x02

#define OS_MEM_NODE_MAGIC        0xABCDDCBA
#define OS_MEM_MIN_ALLOC_SIZE    (sizeof(struct OsMemFreeNodeHead) - sizeof(struct OsMemUsedNodeHead))

#define OS_MEM_NODE_USED_FLAG      0x80000000U
#define OS_MEM_NODE_ALIGNED_FLAG   0x40000000U
#define OS_MEM_NODE_LAST_FLAG      0x20000000U  /* Sentinel Node */
#define OS_MEM_NODE_ALIGNED_AND_USED_FLAG (OS_MEM_NODE_USED_FLAG | OS_MEM_NODE_ALIGNED_FLAG | OS_MEM_NODE_LAST_FLAG)

#define OS_MEM_NODE_GET_ALIGNED_FLAG(sizeAndFlag) \
            ((sizeAndFlag) & OS_MEM_NODE_ALIGNED_FLAG)
#define OS_MEM_NODE_SET_ALIGNED_FLAG(sizeAndFlag) \
            ((sizeAndFlag) = ((sizeAndFlag) | OS_MEM_NODE_ALIGNED_FLAG))
#define OS_MEM_NODE_GET_ALIGNED_GAPSIZE(sizeAndFlag) \
            ((sizeAndFlag) & ~OS_MEM_NODE_ALIGNED_FLAG)
#define OS_MEM_NODE_GET_USED_FLAG(sizeAndFlag) \
            ((sizeAndFlag) & OS_MEM_NODE_USED_FLAG)
#define OS_MEM_NODE_SET_USED_FLAG(sizeAndFlag) \
            ((sizeAndFlag) = ((sizeAndFlag) | OS_MEM_NODE_USED_FLAG))
#define OS_MEM_NODE_GET_SIZE(sizeAndFlag) \
            ((sizeAndFlag) & ~OS_MEM_NODE_ALIGNED_AND_USED_FLAG)
#define OS_MEM_NODE_SET_LAST_FLAG(sizeAndFlag) \
                        ((sizeAndFlag) = ((sizeAndFlag) | OS_MEM_NODE_LAST_FLAG))
#define OS_MEM_NODE_GET_LAST_FLAG(sizeAndFlag) \
            ((sizeAndFlag) & OS_MEM_NODE_LAST_FLAG)

#define OS_MEM_ALIGN_SIZE           sizeof(UINTPTR)
#define OS_MEM_IS_POW_TWO(value)    ((((UINTPTR)(value)) & ((UINTPTR)(value) - 1)) == 0)
#define OS_MEM_ALIGN(p, alignSize)  (((UINTPTR)(p) + (alignSize) - 1) & ~((UINTPTR)((alignSize) - 1)))
#define OS_MEM_IS_ALIGNED(a, b)     (!(((UINTPTR)(a)) & (((UINTPTR)(b)) - 1)))
#define OS_MEM_NODE_HEAD_SIZE       sizeof(struct OsMemUsedNodeHead)
#define OS_MEM_MIN_POOL_SIZE        (OS_MEM_NODE_HEAD_SIZE + sizeof(struct OsMemPoolHead))
#define OS_MEM_NEXT_NODE(node) \
    ((struct OsMemNodeHead *)(VOID *)((UINT8 *)(node) + OS_MEM_NODE_GET_SIZE((node)->sizeAndFlag)))
#define OS_MEM_FIRST_NODE(pool) \
    (struct OsMemNodeHead *)((UINT8 *)(pool) + sizeof(struct OsMemPoolHead))
#define OS_MEM_END_NODE(pool, size) \
    (struct OsMemNodeHead *)((UINT8 *)(pool) + (size) - OS_MEM_NODE_HEAD_SIZE)
#define OS_MEM_MIDDLE_ADDR_OPEN_END(startAddr, middleAddr, endAddr) \
    (((UINT8 *)(startAddr) <= (UINT8 *)(middleAddr)) && ((UINT8 *)(middleAddr) < (UINT8 *)(endAddr)))
#define OS_MEM_MIDDLE_ADDR(startAddr, middleAddr, endAddr) \
    (((UINT8 *)(startAddr) <= (UINT8 *)(middleAddr)) && ((UINT8 *)(middleAddr) <= (UINT8 *)(endAddr)))
#define OS_MEM_SET_MAGIC(node)      ((node)->magic = OS_MEM_NODE_MAGIC)
#define OS_MEM_MAGIC_VALID(node)    ((node)->magic == OS_MEM_NODE_MAGIC)

STATIC INLINE VOID OsMemFreeNodeAdd(VOID *pool, struct OsMemFreeNodeHead *node);
STATIC INLINE UINT32 OsMemFree(struct OsMemPoolHead *pool, struct OsMemNodeHead *node);
STATIC VOID OsMemInfoPrint(VOID *pool);
#ifdef LOSCFG_BASE_MEM_NODE_INTEGRITY_CHECK
STATIC INLINE UINT32 OsMemAllocCheck(struct OsMemPoolHead *pool, UINT32 intSave);
#endif

#if OS_MEM_FREE_BY_TASKID
STATIC INLINE VOID OsMemNodeSetTaskID(struct OsMemUsedNodeHead *node)
{
    node->taskID = LOS_CurTaskIDGet();
}
#endif

#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
STATIC INLINE VOID OsMemWaterUsedRecord(struct OsMemPoolHead *pool, UINT32 size)
{
    pool->info.curUsedSize += size;
    if (pool->info.curUsedSize > pool->info.waterLine) {
        pool->info.waterLine = pool->info.curUsedSize;
    }
}
#else
STATIC INLINE VOID OsMemWaterUsedRecord(struct OsMemPoolHead *pool, UINT32 size)
{
    (VOID)pool;
    (VOID)size;
}
#endif

#if OS_MEM_EXPAND_ENABLE
STATIC INLINE struct OsMemNodeHead *OsMemLastSentinelNodeGet(const struct OsMemNodeHead *sentinelNode)
{
    struct OsMemNodeHead *node = NULL;
    VOID *ptr = sentinelNode->ptr.next;
    UINT32 size = OS_MEM_NODE_GET_SIZE(sentinelNode->sizeAndFlag);

    while ((ptr != NULL) && (size != 0)) {
        node = OS_MEM_END_NODE(ptr, size);
        ptr = node->ptr.next;
        size = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);
    }

    return node;
}

STATIC INLINE BOOL OsMemSentinelNodeCheck(struct OsMemNodeHead *sentinelNode)
{
    if (!OS_MEM_NODE_GET_USED_FLAG(sentinelNode->sizeAndFlag)) {
        return FALSE;
    }

    if (!OS_MEM_MAGIC_VALID(sentinelNode)) {
        return FALSE;
    }

    return TRUE;
}

STATIC INLINE BOOL OsMemIsLastSentinelNode(struct OsMemNodeHead *sentinelNode)
{
    if (OsMemSentinelNodeCheck(sentinelNode) == FALSE) {
        PRINT_ERR("%s %d, The current sentinel node is invalid\n", __FUNCTION__, __LINE__);
        return TRUE;
    }

    if ((OS_MEM_NODE_GET_SIZE(sentinelNode->sizeAndFlag) == 0) ||
        (sentinelNode->ptr.next == NULL)) {
        return TRUE;
    }

    return FALSE;
}

STATIC INLINE VOID OsMemSentinelNodeSet(struct OsMemNodeHead *sentinelNode, VOID *newNode, UINT32 size)
{
    if (sentinelNode->ptr.next != NULL) {
        sentinelNode = OsMemLastSentinelNodeGet(sentinelNode);
    }

    sentinelNode->sizeAndFlag = size;
    sentinelNode->ptr.next = newNode;
    OS_MEM_NODE_SET_USED_FLAG(sentinelNode->sizeAndFlag);
    OS_MEM_NODE_SET_LAST_FLAG(sentinelNode->sizeAndFlag);
}

STATIC INLINE VOID *OsMemSentinelNodeGet(struct OsMemNodeHead *node)
{
    return node->ptr.next;
}

STATIC INLINE struct OsMemNodeHead *PreSentinelNodeGet(const VOID *pool, const struct OsMemNodeHead *node)
{
    UINT32 nextSize;
    struct OsMemNodeHead *nextNode = NULL;
    struct OsMemNodeHead *sentinelNode = NULL;

    sentinelNode = OS_MEM_END_NODE(pool, ((struct OsMemPoolHead *)pool)->info.totalSize);
    while (sentinelNode != NULL) {
        if (OsMemIsLastSentinelNode(sentinelNode)) {
            PRINT_ERR("PreSentinelNodeGet can not find node %#x\n", node);
            return NULL;
        }
        nextNode = OsMemSentinelNodeGet(sentinelNode);
        if (nextNode == node) {
            return sentinelNode;
        }
        nextSize = OS_MEM_NODE_GET_SIZE(sentinelNode->sizeAndFlag);
        sentinelNode = OS_MEM_END_NODE(nextNode, nextSize);
    }

    return NULL;
}

UINT32 OsMemLargeNodeFree(const VOID *ptr)
{
    LosVmPage *page = OsVmVaddrToPage((VOID *)ptr);
    if ((page == NULL) || (page->nPages == 0)) {
        return LOS_NOK;
    }
    LOS_PhysPagesFreeContiguous((VOID *)ptr, page->nPages);

    return LOS_OK;
}

STATIC INLINE BOOL TryShrinkPool(const VOID *pool, const struct OsMemNodeHead *node)
{
    struct OsMemNodeHead *mySentinel = NULL;
    struct OsMemNodeHead *preSentinel = NULL;
    size_t totalSize = (UINTPTR)node->ptr.prev - (UINTPTR)node;
    size_t nodeSize = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);

    if (nodeSize != totalSize) {
        return FALSE;
    }

    preSentinel = PreSentinelNodeGet(pool, node);
    if (preSentinel == NULL) {
        return FALSE;
    }

    mySentinel = node->ptr.prev;
    if (OsMemIsLastSentinelNode(mySentinel)) { /* prev node becomes sentinel node */
        preSentinel->ptr.next = NULL;
        OsMemSentinelNodeSet(preSentinel, NULL, 0);
    } else {
        preSentinel->sizeAndFlag = mySentinel->sizeAndFlag;
        preSentinel->ptr.next = mySentinel->ptr.next;
    }

    if (OsMemLargeNodeFree(node) != LOS_OK) {
        PRINT_ERR("TryShrinkPool free %#x failed!\n", node);
        return FALSE;
    }

    return TRUE;
}

STATIC INLINE INT32 OsMemPoolExpandSub(VOID *pool, UINT32 size, UINT32 intSave)
{
    UINT32 tryCount = MAX_SHRINK_PAGECACHE_TRY;
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *newNode = NULL;
    struct OsMemNodeHead *endNode = NULL;

    size = ROUNDUP(size + OS_MEM_NODE_HEAD_SIZE, PAGE_SIZE);
    endNode = OS_MEM_END_NODE(pool, poolInfo->info.totalSize);

RETRY:
    newNode = (struct OsMemNodeHead *)LOS_PhysPagesAllocContiguous(size >> PAGE_SHIFT);
    if (newNode == NULL) {
        if (tryCount > 0) {
            tryCount--;
            MEM_UNLOCK(poolInfo, intSave);
            OsTryShrinkMemory(size >> PAGE_SHIFT);
            MEM_LOCK(poolInfo, intSave);
            goto RETRY;
        }

        PRINT_ERR("OsMemPoolExpand alloc failed size = %u\n", size);
        return -1;
    }
    newNode->sizeAndFlag = (size - OS_MEM_NODE_HEAD_SIZE);
    newNode->ptr.prev = OS_MEM_END_NODE(newNode, size);
    OsMemSentinelNodeSet(endNode, newNode, size);
    OsMemFreeNodeAdd(pool, (struct OsMemFreeNodeHead *)newNode);

    endNode = OS_MEM_END_NODE(newNode, size);
    (VOID)memset_s(endNode, sizeof(*endNode), 0, sizeof(*endNode));
    endNode->ptr.next = NULL;
    endNode->magic = OS_MEM_NODE_MAGIC;
    OsMemSentinelNodeSet(endNode, NULL, 0);
    OsMemWaterUsedRecord(poolInfo, OS_MEM_NODE_HEAD_SIZE);

    return 0;
}

STATIC INLINE INT32 OsMemPoolExpand(VOID *pool, UINT32 allocSize, UINT32 intSave)
{
    UINT32 expandDefault = MEM_EXPAND_SIZE(LOS_MemPoolSizeGet(pool));
    UINT32 expandSize = MAX(expandDefault, allocSize);
    UINT32 tryCount = 1;
    UINT32 ret;

    do {
        ret = OsMemPoolExpandSub(pool, expandSize, intSave);
        if (ret == 0) {
            return 0;
        }

        if (allocSize > expandDefault) {
            break;
        }
        expandSize = allocSize;
    } while (tryCount--);

    return -1;
}

VOID LOS_MemExpandEnable(VOID *pool)
{
    if (pool == NULL) {
        return;
    }

    ((struct OsMemPoolHead *)pool)->info.attr |= OS_MEM_POOL_EXPAND_ENABLE;
}
#endif

#ifdef LOSCFG_MEM_LEAKCHECK
STATIC INLINE VOID OsMemLinkRegisterRecord(struct OsMemNodeHead *node)
{
    LOS_RecordLR(node->linkReg, LOS_RECORD_LR_CNT, LOS_RECORD_LR_CNT, LOS_OMIT_LR_CNT);
}

STATIC INLINE VOID OsMemUsedNodePrint(struct OsMemNodeHead *node)
{
    UINT32 count;

    if (OS_MEM_NODE_GET_USED_FLAG(node->sizeAndFlag)) {
#ifdef __LP64__
        PRINTK("0x%018x: ", node);
#else
        PRINTK("0x%010x: ", node);
#endif
        for (count = 0; count < LOS_RECORD_LR_CNT; count++) {
#ifdef __LP64__
            PRINTK(" 0x%018x ", node->linkReg[count]);
#else
            PRINTK(" 0x%010x ", node->linkReg[count]);
#endif
        }
        PRINTK("\n");
    }
}

VOID OsMemUsedNodeShow(VOID *pool)
{
    if (pool == NULL) {
        PRINTK("input param is NULL\n");
        return;
    }
    if (LOS_MemIntegrityCheck(pool)) {
        PRINTK("LOS_MemIntegrityCheck error\n");
        return;
    }
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *tmpNode = NULL;
    struct OsMemNodeHead *endNode = NULL;
    UINT32 size;
    UINT32 intSave;
    UINT32 count;

#ifdef __LP64__
    PRINTK("\n\rnode                ");
#else
    PRINTK("\n\rnode        ");
#endif
    for (count = 0; count < LOS_RECORD_LR_CNT; count++) {
#ifdef __LP64__
        PRINTK("        LR[%u]       ", count);
#else
        PRINTK("    LR[%u]   ", count);
#endif
    }
    PRINTK("\n");

    MEM_LOCK(poolInfo, intSave);
    endNode = OS_MEM_END_NODE(pool, poolInfo->info.totalSize);
#if OS_MEM_EXPAND_ENABLE
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode <= endNode;
         tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        if (tmpNode == endNode) {
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                size = OS_MEM_NODE_GET_SIZE(endNode->sizeAndFlag);
                tmpNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(tmpNode, size);
                continue;
            } else {
                break;
            }
        } else {
            OsMemUsedNodePrint(tmpNode);
        }
    }
#else
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode < endNode;
         tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        OsMemUsedNodePrint(tmpNode);
    }
#endif
    MEM_UNLOCK(poolInfo, intSave);
}

STATIC VOID OsMemNodeBacktraceInfo(const struct OsMemNodeHead *tmpNode,
                                   const struct OsMemNodeHead *preNode)
{
    int i;
    PRINTK("\n broken node head LR info: \n");
    for (i = 0; i < LOS_RECORD_LR_CNT; i++) {
        PRINTK(" LR[%d]:%#x\n", i, tmpNode->linkReg[i]);
    }

    PRINTK("\n pre node head LR info: \n");
    for (i = 0; i < LOS_RECORD_LR_CNT; i++) {
        PRINTK(" LR[%d]:%#x\n", i, preNode->linkReg[i]);
    }
}
#endif

STATIC INLINE UINT32 OsMemFreeListIndexGet(UINT32 size)
{
    UINT32 fl = OsMemFlGet(size);
    if (size < OS_MEM_SMALL_BUCKET_MAX_SIZE) {
        return fl;
    }

    UINT32 sl = OsMemSlGet(size, fl);
    return (OS_MEM_SMALL_BUCKET_COUNT + ((fl - OS_MEM_LARGE_START_BUCKET) << OS_MEM_SLI) + sl);
}

STATIC INLINE struct OsMemFreeNodeHead *OsMemFindCurSuitableBlock(struct OsMemPoolHead *poolHead,
                                        UINT32 index, UINT32 size)
{
    struct OsMemFreeNodeHead *node = NULL;

    for (node = poolHead->freeList[index]; node != NULL; node = node->next) {
        if (node->header.sizeAndFlag >= size) {
            return node;
        }
    }

    return NULL;
}

STATIC INLINE UINT32 OsMemNotEmptyIndexGet(struct OsMemPoolHead *poolHead, UINT32 index)
{
    UINT32 mask = poolHead->freeListBitmap[index >> 5]; /* 5: Divide by 32 to calculate the index of the bitmap array. */
    mask &= ~((1 << (index & OS_MEM_BITMAP_MASK)) - 1);
    if (mask != 0) {
        index = OsMemFFS(mask) + (index & ~OS_MEM_BITMAP_MASK);
        return index;
    }

    return OS_MEM_FREE_LIST_COUNT;
}

STATIC INLINE struct OsMemFreeNodeHead *OsMemFindNextSuitableBlock(VOID *pool, UINT32 size, UINT32 *outIndex)
{
    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    UINT32 fl = OsMemFlGet(size);
    UINT32 sl;
    UINT32 index, tmp;
    UINT32 curIndex = OS_MEM_FREE_LIST_COUNT;
    UINT32 mask;

    do {
        if (size < OS_MEM_SMALL_BUCKET_MAX_SIZE) {
            index = fl;
        } else {
            sl = OsMemSlGet(size, fl);
            curIndex = ((fl - OS_MEM_LARGE_START_BUCKET) << OS_MEM_SLI) + sl + OS_MEM_SMALL_BUCKET_COUNT;
            index = curIndex + 1;
        }

        tmp = OsMemNotEmptyIndexGet(poolHead, index);
        if (tmp != OS_MEM_FREE_LIST_COUNT) {
            index = tmp;
            goto DONE;
        }

        for (index = LOS_Align(index + 1, 32); index < OS_MEM_FREE_LIST_COUNT; index += 32) {
            mask = poolHead->freeListBitmap[index >> 5]; /* 5: Divide by 32 to calculate the index of the bitmap array. */
            if (mask != 0) {
                index = OsMemFFS(mask) + index;
                goto DONE;
            }
        }
    } while (0);

    if (curIndex == OS_MEM_FREE_LIST_COUNT) {
        return NULL;
    }

    *outIndex = curIndex;
    return OsMemFindCurSuitableBlock(poolHead, curIndex, size);
DONE:
    *outIndex = index;
    return poolHead->freeList[index];
}

STATIC INLINE VOID OsMemSetFreeListBit(struct OsMemPoolHead *head, UINT32 index)
{
    head->freeListBitmap[index >> 5] |= 1U << (index & 0x1f); /* 5: Divide by 32 to calculate the index of the bitmap array. */
}

STATIC INLINE VOID OsMemClearFreeListBit(struct OsMemPoolHead *head, UINT32 index)
{
    head->freeListBitmap[index >> 5] &= ~(1U << (index & 0x1f)); /* 5: Divide by 32 to calculate the index of the bitmap array. */
}

STATIC INLINE VOID OsMemListAdd(struct OsMemPoolHead *pool, UINT32 listIndex, struct OsMemFreeNodeHead *node)
{
    struct OsMemFreeNodeHead *firstNode = pool->freeList[listIndex];
    if (firstNode != NULL) {
        firstNode->prev = node;
    }
    node->prev = NULL;
    node->next = firstNode;
    pool->freeList[listIndex] = node;
    OsMemSetFreeListBit(pool, listIndex);
    node->header.magic = OS_MEM_NODE_MAGIC;
}

STATIC INLINE VOID OsMemListDelete(struct OsMemPoolHead *pool, UINT32 listIndex, struct OsMemFreeNodeHead *node)
{
    if (node == pool->freeList[listIndex]) {
        pool->freeList[listIndex] = node->next;
        if (node->next == NULL) {
            OsMemClearFreeListBit(pool, listIndex);
        } else {
            node->next->prev = NULL;
        }
    } else {
        node->prev->next = node->next;
        if (node->next != NULL) {
            node->next->prev = node->prev;
        }
    }
    node->header.magic = OS_MEM_NODE_MAGIC;
}

STATIC INLINE VOID OsMemFreeNodeAdd(VOID *pool, struct OsMemFreeNodeHead *node)
{
    UINT32 index = OsMemFreeListIndexGet(node->header.sizeAndFlag);
    if (index >= OS_MEM_FREE_LIST_COUNT) {
        LOS_Panic("The index of free lists is error, index = %u\n", index);
        return;
    }
    OsMemListAdd(pool, index, node);
}

STATIC INLINE VOID OsMemFreeNodeDelete(VOID *pool, struct OsMemFreeNodeHead *node)
{
    UINT32 index = OsMemFreeListIndexGet(node->header.sizeAndFlag);
    if (index >= OS_MEM_FREE_LIST_COUNT) {
        LOS_Panic("The index of free lists is error, index = %u\n", index);
        return;
    }
    OsMemListDelete(pool, index, node);
}

STATIC INLINE struct OsMemNodeHead *OsMemFreeNodeGet(VOID *pool, UINT32 size)
{
    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    UINT32 index;
    struct OsMemFreeNodeHead *firstNode = OsMemFindNextSuitableBlock(pool, size, &index);
    if (firstNode == NULL) {
        return NULL;
    }

    OsMemListDelete(poolHead, index, firstNode);

    return &firstNode->header;
}

STATIC INLINE VOID OsMemMergeNode(struct OsMemNodeHead *node)
{
    struct OsMemNodeHead *nextNode = NULL;

    node->ptr.prev->sizeAndFlag += node->sizeAndFlag;
    nextNode = (struct OsMemNodeHead *)((UINTPTR)node + node->sizeAndFlag);
    if (!OS_MEM_NODE_GET_LAST_FLAG(nextNode->sizeAndFlag)) {
        nextNode->ptr.prev = node->ptr.prev;
    }
}

STATIC INLINE VOID OsMemSplitNode(VOID *pool, struct OsMemNodeHead *allocNode, UINT32 allocSize)
{
    struct OsMemFreeNodeHead *newFreeNode = NULL;
    struct OsMemNodeHead *nextNode = NULL;

    newFreeNode = (struct OsMemFreeNodeHead *)(VOID *)((UINT8 *)allocNode + allocSize);
    newFreeNode->header.ptr.prev = allocNode;
    newFreeNode->header.sizeAndFlag = allocNode->sizeAndFlag - allocSize;
    allocNode->sizeAndFlag = allocSize;
    nextNode = OS_MEM_NEXT_NODE(&newFreeNode->header);
    if (!OS_MEM_NODE_GET_LAST_FLAG(nextNode->sizeAndFlag)) {
        nextNode->ptr.prev = &newFreeNode->header;
        if (!OS_MEM_NODE_GET_USED_FLAG(nextNode->sizeAndFlag)) {
            OsMemFreeNodeDelete(pool, (struct OsMemFreeNodeHead *)nextNode);
            OsMemMergeNode(nextNode);
        }
    }

    OsMemFreeNodeAdd(pool, newFreeNode);
}

STATIC INLINE VOID *OsMemCreateUsedNode(VOID *addr)
{
    struct OsMemUsedNodeHead *node = (struct OsMemUsedNodeHead *)addr;

#if OS_MEM_FREE_BY_TASKID
    OsMemNodeSetTaskID(node);
#endif

    return node + 1;
}

STATIC UINT32 OsMemPoolInit(VOID *pool, UINT32 size)
{
    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *newNode = NULL;
    struct OsMemNodeHead *endNode = NULL;

    (VOID)memset_s(poolHead, sizeof(struct OsMemPoolHead), 0, sizeof(struct OsMemPoolHead));

    LOS_SpinInit(&poolHead->spinlock);
    poolHead->info.pool = pool;
    poolHead->info.totalSize = size;
    poolHead->info.attr = OS_MEM_POOL_LOCK_ENABLE; /* default attr: lock, not expand. */

    newNode = OS_MEM_FIRST_NODE(pool);
    newNode->sizeAndFlag = (size - sizeof(struct OsMemPoolHead) - OS_MEM_NODE_HEAD_SIZE);
    newNode->ptr.prev = NULL;
    newNode->magic = OS_MEM_NODE_MAGIC;
    OsMemFreeNodeAdd(pool, (struct OsMemFreeNodeHead *)newNode);

    /* The last mem node */
    endNode = OS_MEM_END_NODE(pool, size);
    endNode->magic = OS_MEM_NODE_MAGIC;
#if OS_MEM_EXPAND_ENABLE
    endNode->ptr.next = NULL;
    OsMemSentinelNodeSet(endNode, NULL, 0);
#else
    endNode->sizeAndFlag = 0;
    endNode->ptr.prev = newNode;
    OS_MEM_NODE_SET_USED_FLAG(endNode->sizeAndFlag);
#endif
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    poolHead->info.curUsedSize = sizeof(struct OsMemPoolHead) + OS_MEM_NODE_HEAD_SIZE;
    poolHead->info.waterLine = poolHead->info.curUsedSize;
#endif

    return LOS_OK;
}

#ifdef LOSCFG_MEM_MUL_POOL
STATIC VOID OsMemPoolDeinit(VOID *pool)
{
    (VOID)memset_s(pool, sizeof(struct OsMemPoolHead), 0, sizeof(struct OsMemPoolHead));
}

STATIC UINT32 OsMemPoolAdd(VOID *pool, UINT32 size)
{
    VOID *nextPool = g_poolHead;
    VOID *curPool = g_poolHead;
    UINTPTR poolEnd;
    while (nextPool != NULL) {
        poolEnd = (UINTPTR)nextPool + LOS_MemPoolSizeGet(nextPool);
        if (((pool <= nextPool) && (((UINTPTR)pool + size) > (UINTPTR)nextPool)) ||
            (((UINTPTR)pool < poolEnd) && (((UINTPTR)pool + size) >= poolEnd))) {
            PRINT_ERR("pool [%#x, %#x) conflict with pool [%#x, %#x)\n",
                      pool, (UINTPTR)pool + size,
                      nextPool, (UINTPTR)nextPool + LOS_MemPoolSizeGet(nextPool));
            return LOS_NOK;
        }
        curPool = nextPool;
        nextPool = ((struct OsMemPoolHead *)nextPool)->nextPool;
    }

    if (g_poolHead == NULL) {
        g_poolHead = pool;
    } else {
        ((struct OsMemPoolHead *)curPool)->nextPool = pool;
    }

    ((struct OsMemPoolHead *)pool)->nextPool = NULL;
    return LOS_OK;
}

STATIC UINT32 OsMemPoolDelete(VOID *pool)
{
    UINT32 ret = LOS_NOK;
    VOID *nextPool = NULL;
    VOID *curPool = NULL;

    do {
        if (pool == g_poolHead) {
            g_poolHead = ((struct OsMemPoolHead *)g_poolHead)->nextPool;
            ret = LOS_OK;
            break;
        }

        curPool = g_poolHead;
        nextPool = g_poolHead;
        while (nextPool != NULL) {
            if (pool == nextPool) {
                ((struct OsMemPoolHead *)curPool)->nextPool = ((struct OsMemPoolHead *)nextPool)->nextPool;
                ret = LOS_OK;
                break;
            }
            curPool = nextPool;
            nextPool = ((struct OsMemPoolHead *)nextPool)->nextPool;
        }
    } while (0);

    return ret;
}
#endif

UINT32 LOS_MemInit(VOID *pool, UINT32 size)
{
    if ((pool == NULL) || (size <= OS_MEM_MIN_POOL_SIZE)) {
        return OS_ERROR;
    }

    size = OS_MEM_ALIGN(size, OS_MEM_ALIGN_SIZE);
    if (OsMemPoolInit(pool, size)) {
        return OS_ERROR;
    }

#ifdef LOSCFG_MEM_MUL_POOL
    if (OsMemPoolAdd(pool, size)) {
        (VOID)OsMemPoolDeinit(pool);
        return OS_ERROR;
    }
#endif

#ifdef LOSCFG_KERNEL_TRACE
    LOS_TraceReg(LOS_TRACE_MEM_TIME, OsMemTimeTrace, LOS_TRACE_MEM_TIME_NAME, LOS_TRACE_ENABLE);
    LOS_TraceReg(LOS_TRACE_MEM_INFO, OsMemInfoTrace, LOS_TRACE_MEM_INFO_NAME, LOS_TRACE_ENABLE);
#endif

    return LOS_OK;
}

#ifdef LOSCFG_MEM_MUL_POOL
UINT32 LOS_MemDeInit(VOID *pool)
{
    if (pool == NULL) {
        return OS_ERROR;
    }

    if (OsMemPoolDelete(pool)) {
        return OS_ERROR;
    }

    OsMemPoolDeinit(pool);

#ifdef LOSCFG_KERNEL_TRACE
    LOS_TraceUnreg(LOS_TRACE_MEM_TIME);
    LOS_TraceUnreg(LOS_TRACE_MEM_INFO);
#endif

    return LOS_OK;
}

UINT32 LOS_MemPoolList(VOID)
{
    VOID *nextPool = g_poolHead;
    UINT32 index = 0;
    while (nextPool != NULL) {
        PRINTK("pool%u :\n", index);
        index++;
        OsMemInfoPrint(nextPool);
        nextPool = ((struct OsMemPoolHead *)nextPool)->nextPool;
    }
    return index;
}
#endif

STATIC INLINE VOID *OsMemAlloc(struct OsMemPoolHead *pool, UINT32 size, UINT32 intSave)
{
    struct OsMemNodeHead *allocNode = NULL;

#ifdef LOSCFG_BASE_MEM_NODE_INTEGRITY_CHECK
    if (OsMemAllocCheck(pool, intSave) == LOS_NOK) {
        return NULL;
    }
#endif

    UINT32 allocSize = OS_MEM_ALIGN(size + OS_MEM_NODE_HEAD_SIZE, OS_MEM_ALIGN_SIZE);
    if (allocSize == 0) {
        return NULL;
    }

#if OS_MEM_EXPAND_ENABLE
retry:
#endif
    allocNode = OsMemFreeNodeGet(pool, allocSize);
    if (allocNode == NULL) {
#if OS_MEM_EXPAND_ENABLE
        if (pool->info.attr & OS_MEM_POOL_EXPAND_ENABLE) {
            INT32 ret = OsMemPoolExpand(pool, allocSize, intSave);
            if (ret == 0) {
                goto retry;
            }
        }
#endif
        MEM_UNLOCK(pool, intSave);
        PRINT_ERR("---------------------------------------------------"
                  "--------------------------------------------------------\n");
        OsMemInfoPrint(pool);
        PRINT_ERR("[%s] No suitable free block, require free node size: 0x%x\n", __FUNCTION__, allocSize);
        PRINT_ERR("----------------------------------------------------"
                  "-------------------------------------------------------\n");
        MEM_LOCK(pool, intSave);
        return NULL;
    }

    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_MIN_ALLOC_SIZE) <= allocNode->sizeAndFlag) {
        OsMemSplitNode(pool, allocNode, allocSize);
    }

    OS_MEM_NODE_SET_USED_FLAG(allocNode->sizeAndFlag);
    OsMemWaterUsedRecord(pool, OS_MEM_NODE_GET_SIZE(allocNode->sizeAndFlag));

#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(allocNode);
#endif
    return OsMemCreateUsedNode((VOID *)allocNode);
}

VOID *LOS_MemAlloc(VOID *pool, UINT32 size)
{
#ifdef LOSCFG_KERNEL_TRACE
    UINT64 start = HalClockGetCycles();
#endif

    if ((pool == NULL) || (size == 0)) {
        return (size > 0) ? OsVmBootMemAlloc(size) : NULL;
    }

    if (size < OS_MEM_MIN_ALLOC_SIZE) {
        size = OS_MEM_MIN_ALLOC_SIZE;
    }

    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    VOID *ptr = NULL;
    UINT32 intSave;

    do {
        if (OS_MEM_NODE_GET_USED_FLAG(size) || OS_MEM_NODE_GET_ALIGNED_FLAG(size)) {
            break;
        }
        MEM_LOCK(poolHead, intSave);
        ptr = OsMemAlloc(poolHead, size, intSave);
        MEM_UNLOCK(poolHead, intSave);
    } while (0);

#ifdef LOSCFG_KERNEL_TRACE
    UINT64 end = HalClockGetCycles();
    UINT32 timeUsed = MEM_TRACE_CYCLE_TO_US(end - start);
    LOS_Trace(LOS_TRACE_MEM_TIME, (UINTPTR)pool & MEM_POOL_ADDR_MASK, MEM_TRACE_MALLOC, timeUsed);

    LOS_MEM_POOL_STATUS poolStatus = {0};
    (VOID)LOS_MemInfoGet(pool, &poolStatus);
    UINT8 fragment = 100 - poolStatus.maxFreeNodeSize * 100 / poolStatus.totalFreeSize; /* 100: percent denominator. */
    UINT8 usage = LOS_MemTotalUsedGet(pool) * 100 / LOS_MemPoolSizeGet(pool); /* 100: percent denominator. */
    LOS_Trace(LOS_TRACE_MEM_INFO, (UINTPTR)pool & MEM_POOL_ADDR_MASK, fragment, usage, poolStatus.totalFreeSize,
              poolStatus.maxFreeNodeSize, poolStatus.usedNodeNum, poolStatus.freeNodeNum);
#endif

    return ptr;
}

VOID *LOS_MemAllocAlign(VOID *pool, UINT32 size, UINT32 boundary)
{
#ifdef LOSCFG_KERNEL_TRACE
    UINT64 start = HalClockGetCycles();
#endif

    UINT32 gapSize;

    if ((pool == NULL) || (size == 0) || (boundary == 0) || !OS_MEM_IS_POW_TWO(boundary) ||
        !OS_MEM_IS_ALIGNED(boundary, sizeof(VOID *))) {
        return NULL;
    }

    if (size < OS_MEM_MIN_ALLOC_SIZE) {
        size = OS_MEM_MIN_ALLOC_SIZE;
    }

    /*
     * sizeof(gapSize) bytes stores offset between alignedPtr and ptr,
     * the ptr has been OS_MEM_ALIGN_SIZE(4 or 8) aligned, so maximum
     * offset between alignedPtr and ptr is boundary - OS_MEM_ALIGN_SIZE
     */
    if ((boundary - sizeof(gapSize)) > ((UINT32)(-1) - size)) {
        return NULL;
    }

    UINT32 useSize = (size + boundary) - sizeof(gapSize);
    if (OS_MEM_NODE_GET_USED_FLAG(useSize) || OS_MEM_NODE_GET_ALIGNED_FLAG(useSize)) {
        return NULL;
    }

    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    UINT32 intSave;
    VOID *ptr = NULL;
    VOID *alignedPtr = NULL;

    do {
        MEM_LOCK(poolHead, intSave);
        ptr = OsMemAlloc(pool, useSize, intSave);
        MEM_UNLOCK(poolHead, intSave);
        alignedPtr = (VOID *)OS_MEM_ALIGN(ptr, boundary);
        if (ptr == alignedPtr) {
            break;
        }

        /* store gapSize in address (ptr - 4), it will be checked while free */
        gapSize = (UINT32)((UINTPTR)alignedPtr - (UINTPTR)ptr);
        struct OsMemUsedNodeHead *allocNode = (struct OsMemUsedNodeHead *)ptr - 1;
        OS_MEM_NODE_SET_ALIGNED_FLAG(allocNode->header.sizeAndFlag);
        OS_MEM_NODE_SET_ALIGNED_FLAG(gapSize);
        *(UINT32 *)((UINTPTR)alignedPtr - sizeof(gapSize)) = gapSize;
        ptr = alignedPtr;
    } while (0);

#ifdef LOSCFG_KERNEL_TRACE
    UINT64 end = HalClockGetCycles();
    UINT32 timeUsed = MEM_TRACE_CYCLE_TO_US(end - start);
    LOS_Trace(LOS_TRACE_MEM_TIME, (UINTPTR)pool & MEM_POOL_ADDR_MASK, MEM_TRACE_MEMALIGN, timeUsed);
#endif

    return ptr;
}

STATIC INLINE BOOL OsMemAddrValidCheck(const struct OsMemPoolHead *pool, const VOID *addr)
{
    UINT32 size;

    /* First node prev is NULL */
    if (addr == NULL) {
        return TRUE;
    }

    size = pool->info.totalSize;
    if (OS_MEM_MIDDLE_ADDR_OPEN_END(pool + 1, addr, (UINTPTR)pool + size)) {
        return TRUE;
    }
#if OS_MEM_EXPAND_ENABLE
    struct OsMemNodeHead *node = NULL;
    struct OsMemNodeHead *sentinel = OS_MEM_END_NODE(pool, size);
    while (OsMemIsLastSentinelNode(sentinel) == FALSE) {
        size = OS_MEM_NODE_GET_SIZE(sentinel->sizeAndFlag);
        node = OsMemSentinelNodeGet(sentinel);
        sentinel = OS_MEM_END_NODE(node, size);
        if (OS_MEM_MIDDLE_ADDR_OPEN_END(node, addr, (UINTPTR)node + size)) {
            return TRUE;
        }
    }
#endif
    return FALSE;
}

STATIC INLINE BOOL OsMemIsNodeValid(const struct OsMemNodeHead *node, const struct OsMemNodeHead *startNode,
                                    const struct OsMemNodeHead *endNode,
                                    const struct OsMemPoolHead *poolInfo)
{
    if (!OS_MEM_MIDDLE_ADDR(startNode, node, endNode)) {
        return FALSE;
    }

    if (OS_MEM_NODE_GET_USED_FLAG(node->sizeAndFlag)) {
        if (!OS_MEM_MAGIC_VALID(node)) {
            return FALSE;
        }
        return TRUE;
    }

    if (!OsMemAddrValidCheck(poolInfo, node->ptr.prev)) {
        return FALSE;
    }

    return TRUE;
}

STATIC UINT32 OsMemCheckUsedNode(const struct OsMemPoolHead *pool, const struct OsMemNodeHead *node)
{
    struct OsMemNodeHead *startNode = (struct OsMemNodeHead *)OS_MEM_FIRST_NODE(pool);
    struct OsMemNodeHead *endNode = (struct OsMemNodeHead *)OS_MEM_END_NODE(pool, pool->info.totalSize);
    struct OsMemNodeHead *nextNode = NULL;
    BOOL doneFlag = FALSE;

    do {
        do {
            if (!OsMemIsNodeValid(node, startNode, endNode, pool)) {
                break;
            }

            if (!OS_MEM_NODE_GET_USED_FLAG(node->sizeAndFlag)) {
                break;
            }

            nextNode = OS_MEM_NEXT_NODE(node);
            if (!OsMemIsNodeValid(nextNode, startNode, endNode, pool)) {
                break;
            }

            if (!OS_MEM_NODE_GET_LAST_FLAG(nextNode->sizeAndFlag)) {
                if (nextNode->ptr.prev != node) {
                    break;
                }
            }

            if ((node != startNode) &&
                ((!OsMemIsNodeValid(node->ptr.prev, startNode, endNode, pool)) ||
                (OS_MEM_NEXT_NODE(node->ptr.prev) != node))) {
                break;
            }
            doneFlag = TRUE;
        } while (0);

        if (!doneFlag) {
#if OS_MEM_EXPAND_ENABLE
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                startNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(startNode, OS_MEM_NODE_GET_SIZE(endNode->sizeAndFlag));
                continue;
            }
#endif
            return LOS_NOK;
        }
    } while (!doneFlag);

    return LOS_OK;
}

STATIC INLINE UINT32 OsMemFree(struct OsMemPoolHead *pool, struct OsMemNodeHead *node)
{
    UINT32 ret = OsMemCheckUsedNode(pool, node);
    if (ret != LOS_OK) {
        PRINT_ERR("OsMemFree check error!\n");
        return ret;
    }

#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    pool->info.curUsedSize -= OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);
#endif

    node->sizeAndFlag = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(node);
#endif
    struct OsMemNodeHead *preNode = node->ptr.prev; /* merage preNode */
    if ((preNode != NULL) && !OS_MEM_NODE_GET_USED_FLAG(preNode->sizeAndFlag)) {
        OsMemFreeNodeDelete(pool, (struct OsMemFreeNodeHead *)preNode);
        OsMemMergeNode(node);
        node = preNode;
    }

    struct OsMemNodeHead *nextNode = OS_MEM_NEXT_NODE(node); /* merage nextNode */
    if ((nextNode != NULL) && !OS_MEM_NODE_GET_USED_FLAG(nextNode->sizeAndFlag)) {
        OsMemFreeNodeDelete(pool, (struct OsMemFreeNodeHead *)nextNode);
        OsMemMergeNode(nextNode);
    }

#if OS_MEM_EXPAND_ENABLE
    if (pool->info.attr & OS_MEM_POOL_EXPAND_ENABLE) {
        /* if this is a expand head node, and all unused, free it to pmm */
        if ((node->ptr.prev != NULL) && (node->ptr.prev > node)) {
            if (TryShrinkPool(pool, node)) {
                return LOS_OK;
            }
        }
    }
#endif
    OsMemFreeNodeAdd(pool, (struct OsMemFreeNodeHead *)node);

    return ret;
}

UINT32 LOS_MemFree(VOID *pool, VOID *ptr)
{
#ifdef LOSCFG_KERNEL_TRACE
    UINT64 start = HalClockGetCycles();
#endif

    if ((pool == NULL) || (ptr == NULL) || !OS_MEM_IS_ALIGNED(pool, sizeof(VOID *)) ||
        !OS_MEM_IS_ALIGNED(ptr, sizeof(VOID *))) {
        return LOS_NOK;
    }

    UINT32 ret = LOS_NOK;
    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *node = NULL;
    UINT32 intSave;

    do {
        UINT32 gapSize = *(UINT32 *)((UINTPTR)ptr - sizeof(UINT32));
        if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize) && OS_MEM_NODE_GET_USED_FLAG(gapSize)) {
            PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
            break;
        }

        node = (struct OsMemNodeHead *)((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE);

        if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize)) {
            gapSize = OS_MEM_NODE_GET_ALIGNED_GAPSIZE(gapSize);
            if ((gapSize & (OS_MEM_ALIGN_SIZE - 1)) || (gapSize > ((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE))) {
                PRINT_ERR("illegal gapSize: 0x%x\n", gapSize);
                break;
            }
            node = (struct OsMemNodeHead *)((UINTPTR)ptr - gapSize - OS_MEM_NODE_HEAD_SIZE);
        }
        MEM_LOCK(poolHead, intSave);
        ret = OsMemFree(poolHead, node);
        MEM_UNLOCK(poolHead, intSave);
    } while (0);

#ifdef LOSCFG_KERNEL_TRACE
    UINT64 end = HalClockGetCycles();
    UINT32 timeUsed = MEM_TRACE_CYCLE_TO_US(end - start);
    LOS_Trace(LOS_TRACE_MEM_TIME, (UINTPTR)pool & MEM_POOL_ADDR_MASK, MEM_TRACE_FREE, timeUsed);
#endif

    return ret;
}

STATIC INLINE VOID OsMemReAllocSmaller(VOID *pool, UINT32 allocSize, struct OsMemNodeHead *node, UINT32 nodeSize)
{
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;
#endif
    node->sizeAndFlag = nodeSize;
    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_MIN_ALLOC_SIZE) <= nodeSize) {
        OsMemSplitNode(pool, node, allocSize);
        OS_MEM_NODE_SET_USED_FLAG(node->sizeAndFlag);
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
        poolInfo->info.curUsedSize -= nodeSize - allocSize;
#endif
    }
    OS_MEM_NODE_SET_USED_FLAG(node->sizeAndFlag);
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(node);
#endif
}

STATIC INLINE VOID OsMemMergeNodeForReAllocBigger(VOID *pool, UINT32 allocSize, struct OsMemNodeHead *node,
                                                  UINT32 nodeSize, struct OsMemNodeHead *nextNode)
{
    node->sizeAndFlag = nodeSize;
    OsMemFreeNodeDelete(pool, (struct OsMemFreeNodeHead *)nextNode);
    OsMemMergeNode(nextNode);
    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_MIN_ALLOC_SIZE) <= node->sizeAndFlag) {
        OsMemSplitNode(pool, node, allocSize);
    }
    OS_MEM_NODE_SET_USED_FLAG(node->sizeAndFlag);
    OsMemWaterUsedRecord((struct OsMemPoolHead *)pool, node->sizeAndFlag - nodeSize);
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(node);
#endif
}

STATIC INLINE VOID *OsGetRealPtr(const VOID *pool, VOID *ptr)
{
    VOID *realPtr = ptr;
    UINT32 gapSize = *((UINT32 *)((UINTPTR)ptr - sizeof(UINT32)));

    if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize) && OS_MEM_NODE_GET_USED_FLAG(gapSize)) {
        PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
        return NULL;
    }
    if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize)) {
        gapSize = OS_MEM_NODE_GET_ALIGNED_GAPSIZE(gapSize);
        if ((gapSize & (OS_MEM_ALIGN_SIZE - 1)) ||
            (gapSize > ((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE - (UINTPTR)pool))) {
            PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
            return NULL;
        }
        realPtr = (VOID *)((UINTPTR)ptr - (UINTPTR)gapSize);
    }
    return realPtr;
}

STATIC INLINE VOID *OsMemRealloc(struct OsMemPoolHead *pool, const VOID *ptr,
                struct OsMemNodeHead *node, UINT32 size, UINT32 intSave)
{
    struct OsMemNodeHead *nextNode = NULL;
    UINT32 allocSize = OS_MEM_ALIGN(size + OS_MEM_NODE_HEAD_SIZE, OS_MEM_ALIGN_SIZE);
    UINT32 nodeSize = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);
    VOID *tmpPtr = NULL;

    if (nodeSize >= allocSize) {
        OsMemReAllocSmaller(pool, allocSize, node, nodeSize);
        return (VOID *)ptr;
    }

    nextNode = OS_MEM_NEXT_NODE(node);
    if (!OS_MEM_NODE_GET_USED_FLAG(nextNode->sizeAndFlag) &&
        ((nextNode->sizeAndFlag + nodeSize) >= allocSize)) {
        OsMemMergeNodeForReAllocBigger(pool, allocSize, node, nodeSize, nextNode);
        return (VOID *)ptr;
    }

    tmpPtr = OsMemAlloc(pool, size, intSave);
    if (tmpPtr != NULL) {
        if (memcpy_s(tmpPtr, size, ptr, (nodeSize - OS_MEM_NODE_HEAD_SIZE)) != EOK) {
            MEM_UNLOCK(pool, intSave);
            (VOID)LOS_MemFree((VOID *)pool, (VOID *)tmpPtr);
            MEM_LOCK(pool, intSave);
            return NULL;
        }
        (VOID)OsMemFree(pool, node);
    }
    return tmpPtr;
}

VOID *LOS_MemRealloc(VOID *pool, VOID *ptr, UINT32 size)
{
#ifdef LOSCFG_KERNEL_TRACE
    UINT64 start = HalClockGetCycles();
#endif

    if ((pool == NULL) || OS_MEM_NODE_GET_USED_FLAG(size) || OS_MEM_NODE_GET_ALIGNED_FLAG(size)) {
        return NULL;
    }

    if (size < OS_MEM_MIN_ALLOC_SIZE) {
        size = OS_MEM_MIN_ALLOC_SIZE;
    }

    if (ptr == NULL) {
        return LOS_MemAlloc(pool, size);
    }

    if (size == 0) {
        (VOID)LOS_MemFree(pool, ptr);
        return NULL;
    }

    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *node = NULL;
    VOID *newPtr = NULL;
    UINT32 intSave;

    MEM_LOCK(poolHead, intSave);
    do {
        ptr = OsGetRealPtr(pool, ptr);
        if (ptr == NULL) {
            break;
        }

        node = (struct OsMemNodeHead *)((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE);
        if (OsMemCheckUsedNode(pool, node) != LOS_OK) {
            break;
        }

        newPtr = OsMemRealloc(pool, ptr, node, size, intSave);
    } while (0);
    MEM_UNLOCK(poolHead, intSave);

#ifdef LOSCFG_KERNEL_TRACE
    UINT64 end = HalClockGetCycles();
    UINT32 timeUsed = MEM_TRACE_CYCLE_TO_US(end - start);
    LOS_Trace(LOS_TRACE_MEM_TIME, (UINTPTR)pool & MEM_POOL_ADDR_MASK, MEM_TRACE_REALLOC, timeUsed);
#endif

    return newPtr;
}

#if OS_MEM_FREE_BY_TASKID
UINT32 LOS_MemFreeByTaskID(VOID *pool, UINT32 taskID)
{
    if (pool == NULL) {
        return OS_ERROR;
    }

    if (taskID >= LOSCFG_BASE_CORE_TSK_LIMIT) {
        return OS_ERROR;
    }

    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *tmpNode = NULL;
    struct OsMemUsedNodeHead *node = NULL;
    struct OsMemNodeHead *endNode = NULL;
    UINT32 size;
    UINT32 intSave;

    MEM_LOCK(poolHead, intSave);
    endNode = OS_MEM_END_NODE(pool, poolHead->info.totalSize);
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode <= endNode;) {
        if (tmpNode == endNode) {
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                size = OS_MEM_NODE_GET_SIZE(endNode->sizeAndFlag);
                tmpNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(tmpNode, size);
                continue;
            } else {
                break;
            }
        } else {
            if (!OS_MEM_NODE_GET_USED_FLAG(tmpNode->sizeAndFlag)) {
                tmpNode = OS_MEM_NEXT_NODE(tmpNode);
                continue;
            }

            node = (struct OsMemUsedNodeHead *)tmpNode;
            tmpNode = OS_MEM_NEXT_NODE(tmpNode);

            if (node->taskID == taskID) {
                OsMemFree(poolHead, &node->header);
            }
        }
    }
    MEM_UNLOCK(poolHead, intSave);

    return LOS_OK;
}
#endif

UINT32 LOS_MemPoolSizeGet(const VOID *pool)
{
    UINT32 count = 0;

    if (pool == NULL) {
        return LOS_NOK;
    }

    count += ((struct OsMemPoolHead *)pool)->info.totalSize;

#if OS_MEM_EXPAND_ENABLE
    UINT32 size;
    struct OsMemNodeHead *node = NULL;
    struct OsMemNodeHead *sentinel = OS_MEM_END_NODE(pool, count);

    while (OsMemIsLastSentinelNode(sentinel) == FALSE) {
        size = OS_MEM_NODE_GET_SIZE(sentinel->sizeAndFlag);
        node = OsMemSentinelNodeGet(sentinel);
        sentinel = OS_MEM_END_NODE(node, size);
        count += size;
    }
#endif
    return count;
}

UINT32 LOS_MemTotalUsedGet(VOID *pool)
{
    struct OsMemNodeHead *tmpNode = NULL;
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *endNode = NULL;
    UINT32 memUsed = 0;
    UINT32 intSave;

    if (pool == NULL) {
        return LOS_NOK;
    }

    MEM_LOCK(poolInfo, intSave);
    endNode = OS_MEM_END_NODE(pool, poolInfo->info.totalSize);
#if OS_MEM_EXPAND_ENABLE
    UINT32 size;
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode <= endNode;) {
        if (tmpNode == endNode) {
            memUsed += OS_MEM_NODE_HEAD_SIZE;
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                size = OS_MEM_NODE_GET_SIZE(endNode->sizeAndFlag);
                tmpNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(tmpNode, size);
                continue;
            } else {
                break;
            }
        } else {
            if (OS_MEM_NODE_GET_USED_FLAG(tmpNode->sizeAndFlag)) {
                memUsed += OS_MEM_NODE_GET_SIZE(tmpNode->sizeAndFlag);
            }
            tmpNode = OS_MEM_NEXT_NODE(tmpNode);
        }
    }
#else
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode < endNode;) {
        if (OS_MEM_NODE_GET_USED_FLAG(tmpNode->sizeAndFlag)) {
            memUsed += OS_MEM_NODE_GET_SIZE(tmpNode->sizeAndFlag);
        }
        tmpNode = OS_MEM_NEXT_NODE(tmpNode);
    }
#endif
    MEM_UNLOCK(poolInfo, intSave);

    return memUsed;
}

STATIC INLINE VOID OsMemMagicCheckPrint(struct OsMemNodeHead **tmpNode)
{
    PRINT_ERR("[%s], %d, memory check error!\n"
              "memory used but magic num wrong, magic num = %#x\n",
              __FUNCTION__, __LINE__, (*tmpNode)->magic);
}

STATIC UINT32 OsMemAddrValidCheckPrint(const VOID *pool, struct OsMemFreeNodeHead **tmpNode)
{
    if (!OsMemAddrValidCheck(pool, (*tmpNode)->prev)) {
        PRINT_ERR("[%s], %d, memory check error!\n"
                  " freeNode.prev:%#x is out of legal mem range\n",
                  __FUNCTION__, __LINE__, (*tmpNode)->prev);
        return LOS_NOK;
    }
    if (!OsMemAddrValidCheck(pool, (*tmpNode)->next)) {
        PRINT_ERR("[%s], %d, memory check error!\n"
                  " freeNode.next:%#x is out of legal mem range\n",
                  __FUNCTION__, __LINE__, (*tmpNode)->next);
        return LOS_NOK;
    }
    return LOS_OK;
}

STATIC UINT32 OsMemIntegrityCheckSub(struct OsMemNodeHead **tmpNode, const VOID *pool,
                const struct OsMemNodeHead *endNode)
{
    if (!OS_MEM_MAGIC_VALID(*tmpNode)) {
        OsMemMagicCheckPrint(tmpNode);
        return LOS_NOK;
    }

    if (!OS_MEM_NODE_GET_USED_FLAG((*tmpNode)->sizeAndFlag)) { /* is free node, check free node range */
        if (OsMemAddrValidCheckPrint(pool, (struct OsMemFreeNodeHead **)tmpNode)) {
            return LOS_NOK;
        }
    }
    return LOS_OK;
}

STATIC UINT32 OsMemFreeListNodeCheck(const struct OsMemPoolHead *pool,
                const struct OsMemFreeNodeHead *node)
{
    if (!OsMemAddrValidCheck(pool, node) ||
        !OsMemAddrValidCheck(pool, node->prev) ||
        !OsMemAddrValidCheck(pool, node->next) ||
        !OsMemAddrValidCheck(pool, node->header.ptr.prev)) {
        return LOS_NOK;
    }

    if (!OS_MEM_IS_ALIGNED(node, sizeof(VOID *)) ||
        !OS_MEM_IS_ALIGNED(node->prev, sizeof(VOID *)) ||
        !OS_MEM_IS_ALIGNED(node->next, sizeof(VOID *)) ||
        !OS_MEM_IS_ALIGNED(node->header.ptr.prev, sizeof(VOID *))) {
        return LOS_NOK;
    }

    return LOS_OK;
}

STATIC VOID OsMemPoolHeadCheck(const struct OsMemPoolHead *pool)
{
    struct OsMemFreeNodeHead *tmpNode = NULL;
    UINT32 index;
    UINT32 flag = 0;

    if ((pool->info.pool != pool) || !OS_MEM_IS_ALIGNED(pool, sizeof(VOID *))) {
        PRINT_ERR("wrong mem pool addr: %#x, func:%s, line:%d\n", pool, __FUNCTION__, __LINE__);
        return;
    }

    for (index = 0; index < OS_MEM_FREE_LIST_COUNT; index++) {
        for (tmpNode = pool->freeList[index]; tmpNode != NULL; tmpNode = tmpNode->next) {
            if (OsMemFreeListNodeCheck(pool, tmpNode)) {
                flag = 1;
                PRINT_ERR("FreeListIndex: %u, node: %#x, bNode: %#x, prev: %#x, next: %#x\n",
                          index, tmpNode, tmpNode->header.ptr.prev, tmpNode->prev, tmpNode->next);
            }
        }
    }

    if (flag) {
        PRINTK("mem pool info: poolAddr: %#x, poolSize: 0x%x\n", pool, pool->info.totalSize);
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
        PRINTK("mem pool info: poolWaterLine: 0x%x, poolCurUsedSize: 0x%x\n", pool->info.waterLine,
               pool->info.curUsedSize);
#endif
#if OS_MEM_EXPAND_ENABLE
        UINT32 size;
        struct OsMemNodeHead *node = NULL;
        struct OsMemNodeHead *sentinel = OS_MEM_END_NODE(pool, pool->info.totalSize);
        while (OsMemIsLastSentinelNode(sentinel) == FALSE) {
            size = OS_MEM_NODE_GET_SIZE(sentinel->sizeAndFlag);
            node = OsMemSentinelNodeGet(sentinel);
            sentinel = OS_MEM_END_NODE(node, size);
            PRINTK("expand node info: nodeAddr: %#x, nodeSize: 0x%x\n", node, size);
        }
#endif
    }
}

STATIC UINT32 OsMemIntegrityCheck(const struct OsMemPoolHead *pool, struct OsMemNodeHead **tmpNode,
                struct OsMemNodeHead **preNode)
{
    struct OsMemNodeHead *endNode = OS_MEM_END_NODE(pool, pool->info.totalSize);

    OsMemPoolHeadCheck(pool);

    *preNode = OS_MEM_FIRST_NODE(pool);
    do {
        for (*tmpNode = *preNode; *tmpNode < endNode; *tmpNode = OS_MEM_NEXT_NODE(*tmpNode)) {
            if (OsMemIntegrityCheckSub(tmpNode, pool, endNode) == LOS_NOK) {
                return LOS_NOK;
            }
            *preNode = *tmpNode;
        }
#if OS_MEM_EXPAND_ENABLE
        if (OsMemIsLastSentinelNode(*tmpNode) == FALSE) {
            *preNode = OsMemSentinelNodeGet(*tmpNode);
            endNode = OS_MEM_END_NODE(*preNode, OS_MEM_NODE_GET_SIZE((*tmpNode)->sizeAndFlag));
        } else
#endif
        {
            break;
        }
    } while (1);
    return LOS_OK;
}

STATIC VOID OsMemNodeInfo(const struct OsMemNodeHead *tmpNode,
                          const struct OsMemNodeHead *preNode)
{
    struct OsMemUsedNodeHead *usedNode = NULL;
    struct OsMemFreeNodeHead *freeNode = NULL;

    if (tmpNode == preNode) {
        PRINTK("\n the broken node is the first node\n");
    }

    if (OS_MEM_NODE_GET_USED_FLAG(tmpNode->sizeAndFlag)) {
        usedNode = (struct OsMemUsedNodeHead *)tmpNode;
        PRINTK("\n broken node head: %#x  %#x  %#x, ",
               usedNode->header.ptr.prev, usedNode->header.magic, usedNode->header.sizeAndFlag);
    } else {
        freeNode = (struct OsMemFreeNodeHead *)tmpNode;
        PRINTK("\n broken node head: %#x  %#x  %#x  %#x, ",
               freeNode->header.ptr.prev, freeNode->next, freeNode->prev, freeNode->header.magic,
               freeNode->header.sizeAndFlag);
    }

    if (OS_MEM_NODE_GET_USED_FLAG(preNode->sizeAndFlag)) {
        usedNode = (struct OsMemUsedNodeHead *)preNode;
        PRINTK("prev node head: %#x  %#x  %#x\n",
               usedNode->header.ptr.prev, usedNode->header.magic, usedNode->header.sizeAndFlag);
    } else {
        freeNode = (struct OsMemFreeNodeHead *)preNode;
        PRINTK("prev node head: %#x  %#x  %#x  %#x, ",
               freeNode->header.ptr.prev, freeNode->next, freeNode->prev, freeNode->header.magic,
               freeNode->header.sizeAndFlag);
    }

#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemNodeBacktraceInfo(tmpNode, preNode);
#endif

    PRINTK("\n---------------------------------------------\n");
    PRINTK(" dump mem tmpNode:%#x ~ %#x\n", tmpNode, ((UINTPTR)tmpNode + OS_MEM_NODE_DUMP_SIZE));
    OsDumpMemByte(OS_MEM_NODE_DUMP_SIZE, (UINTPTR)tmpNode);
    PRINTK("\n---------------------------------------------\n");
    if (preNode != tmpNode) {
        PRINTK(" dump mem :%#x ~ tmpNode:%#x\n", ((UINTPTR)tmpNode - OS_MEM_NODE_DUMP_SIZE), tmpNode);
        OsDumpMemByte(OS_MEM_NODE_DUMP_SIZE, ((UINTPTR)tmpNode - OS_MEM_NODE_DUMP_SIZE));
        PRINTK("\n---------------------------------------------\n");
    }
}

STATIC VOID OsMemIntegrityCheckError(struct OsMemPoolHead *pool,
                                     const struct OsMemNodeHead *tmpNode,
                                     const struct OsMemNodeHead *preNode,
                                     UINT32 intSave)
{
    OsMemNodeInfo(tmpNode, preNode);

#if OS_MEM_FREE_BY_TASKID
    LosTaskCB *taskCB = NULL;
    if (OS_MEM_NODE_GET_USED_FLAG(preNode->sizeAndFlag)) {
        struct OsMemUsedNodeHead *usedNode = (struct OsMemUsedNodeHead *)preNode;
        UINT32 taskID = usedNode->taskID;
        if (OS_TID_CHECK_INVALID(taskID)) {
            MEM_UNLOCK(pool, intSave);
            LOS_Panic("Task ID %u in pre node is invalid!\n", taskID);
            return;
        }

        taskCB = OS_TCB_FROM_TID(taskID);
        if (OsTaskIsUnused(taskCB) || (taskCB->taskEntry == NULL)) {
            MEM_UNLOCK(pool, intSave);
            LOS_Panic("\r\nTask ID %u in pre node is not created!\n", taskID);
            return;
        }
    } else {
        PRINTK("The prev node is free\n");
    }
    MEM_UNLOCK(pool, intSave);
    LOS_Panic("cur node: %#x\npre node: %#x\npre node was allocated by task:%s\n",
              tmpNode, preNode, taskCB->taskName);
#else
    MEM_UNLOCK(pool, intSave);
    LOS_Panic("Memory interity check error, cur node: %#x, pre node: %#x\n", tmpNode, preNode);
#endif
}

#ifdef LOSCFG_BASE_MEM_NODE_INTEGRITY_CHECK
STATIC INLINE UINT32 OsMemAllocCheck(struct OsMemPoolHead *pool, UINT32 intSave)
{
    struct OsMemNodeHead *tmpNode = NULL;
    struct OsMemNodeHead *preNode = NULL;

    if (OsMemIntegrityCheck(pool, &tmpNode, &preNode)) {
        OsMemIntegrityCheckError(pool, tmpNode, preNode, intSave);
        return LOS_NOK;
    }
    return LOS_OK;
}
#endif

UINT32 LOS_MemIntegrityCheck(const VOID *pool)
{
    if (pool == NULL) {
        return LOS_NOK;
    }

    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *tmpNode = NULL;
    struct OsMemNodeHead *preNode = NULL;
    UINT32 intSave = 0;

    MEM_LOCK(poolHead, intSave);
    if (OsMemIntegrityCheck(poolHead, &tmpNode, &preNode)) {
        goto ERROR_OUT;
    }
    MEM_UNLOCK(poolHead, intSave);
    return LOS_OK;

ERROR_OUT:
    OsMemIntegrityCheckError(poolHead, tmpNode, preNode, intSave);
    return LOS_NOK;
}

STATIC INLINE VOID OsMemInfoGet(struct OsMemPoolHead *poolInfo, struct OsMemNodeHead *node,
                LOS_MEM_POOL_STATUS *poolStatus)
{
    UINT32 totalUsedSize = 0;
    UINT32 totalFreeSize = 0;
    UINT32 usedNodeNum = 0;
    UINT32 freeNodeNum = 0;
    UINT32 maxFreeSize = 0;
    UINT32 size;

    if (!OS_MEM_NODE_GET_USED_FLAG(node->sizeAndFlag)) {
        size = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);
        ++freeNodeNum;
        totalFreeSize += size;
        if (maxFreeSize < size) {
            maxFreeSize = size;
        }
    } else {
        size = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);
        ++usedNodeNum;
        totalUsedSize += size;
    }

    poolStatus->totalUsedSize += totalUsedSize;
    poolStatus->totalFreeSize += totalFreeSize;
    poolStatus->maxFreeNodeSize = MAX(poolStatus->maxFreeNodeSize, maxFreeSize);
    poolStatus->usedNodeNum += usedNodeNum;
    poolStatus->freeNodeNum += freeNodeNum;
}

UINT32 LOS_MemInfoGet(VOID *pool, LOS_MEM_POOL_STATUS *poolStatus)
{
    struct OsMemPoolHead *poolInfo = pool;

    if (poolStatus == NULL) {
        PRINT_ERR("can't use NULL addr to save info\n");
        return LOS_NOK;
    }

    if ((pool == NULL) || (poolInfo->info.pool != pool)) {
        PRINT_ERR("wrong mem pool addr: %#x, line:%d\n", poolInfo, __LINE__);
        return LOS_NOK;
    }

    (VOID)memset_s(poolStatus, sizeof(LOS_MEM_POOL_STATUS), 0, sizeof(LOS_MEM_POOL_STATUS));

    struct OsMemNodeHead *tmpNode = NULL;
    struct OsMemNodeHead *endNode = NULL;
    UINT32 intSave;

    MEM_LOCK(poolInfo, intSave);
    endNode = OS_MEM_END_NODE(pool, poolInfo->info.totalSize);
#if OS_MEM_EXPAND_ENABLE
    UINT32 size;
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode <= endNode; tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        if (tmpNode == endNode) {
            poolStatus->totalUsedSize += OS_MEM_NODE_HEAD_SIZE;
            poolStatus->usedNodeNum++;
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                size = OS_MEM_NODE_GET_SIZE(endNode->sizeAndFlag);
                tmpNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(tmpNode, size);
                continue;
            } else {
                break;
            }
        } else {
            OsMemInfoGet(poolInfo, tmpNode, poolStatus);
        }
    }
#else
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode < endNode; tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        OsMemInfoGet(poolInfo, tmpNode, poolStatus);
    }
#endif
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    poolStatus->usageWaterLine = poolInfo->info.waterLine;
#endif
    MEM_UNLOCK(poolInfo, intSave);

    return LOS_OK;
}

STATIC VOID OsMemInfoPrint(VOID *pool)
{
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;
    LOS_MEM_POOL_STATUS status = {0};

    if (LOS_MemInfoGet(pool, &status) == LOS_NOK) {
        return;
    }

#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    PRINTK("pool addr          pool size    used size     free size    "
           "max free node size   used node num     free node num      UsageWaterLine\n");
    PRINTK("---------------    --------     -------       --------     "
           "--------------       -------------      ------------      ------------\n");
    PRINTK("%-16#x   0x%-8x   0x%-8x    0x%-8x   0x%-16x   0x%-13x    0x%-13x    0x%-13x\n",
           poolInfo->info.pool, LOS_MemPoolSizeGet(pool), status.totalUsedSize,
           status.totalFreeSize, status.maxFreeNodeSize, status.usedNodeNum,
           status.freeNodeNum, status.usageWaterLine);
#else
    PRINTK("pool addr          pool size    used size     free size    "
           "max free node size   used node num     free node num\n");
    PRINTK("---------------    --------     -------       --------     "
           "--------------       -------------      ------------\n");
    PRINTK("%-16#x   0x%-8x   0x%-8x    0x%-8x   0x%-16x   0x%-13x    0x%-13x\n",
           poolInfo->info.pool, LOS_MemPoolSizeGet(pool), status.totalUsedSize,
           status.totalFreeSize, status.maxFreeNodeSize, status.usedNodeNum,
           status.freeNodeNum);
#endif
}

UINT32 LOS_MemFreeNodeShow(VOID *pool)
{
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;

    if ((poolInfo == NULL) || ((UINTPTR)pool != (UINTPTR)poolInfo->info.pool)) {
        PRINT_ERR("wrong mem pool addr: %#x, line:%d\n", poolInfo, __LINE__);
        return LOS_NOK;
    }

    struct OsMemFreeNodeHead *node = NULL;
    UINT32 countNum[OS_MEM_FREE_LIST_COUNT] = {0};
    UINT32 index;
    UINT32 intSave;

    MEM_LOCK(poolInfo, intSave);
    for (index = 0; index < OS_MEM_FREE_LIST_COUNT; index++) {
        node = poolInfo->freeList[index];
        while (node) {
            node = node->next;
            countNum[index]++;
        }
    }
    MEM_UNLOCK(poolInfo, intSave);

    PRINTK("\n   ************************ left free node number**********************\n");
    for (index = 0; index < OS_MEM_FREE_LIST_COUNT; index++) {
        if (countNum[index] == 0) {
            continue;
        }

        PRINTK("free index: %03u, ", index);
        if (index < OS_MEM_SMALL_BUCKET_COUNT) {
            PRINTK("size: [%#x], num: %u\n", (index + 1) << 2, countNum[index]); /* 2: setup is 4. */
        } else {
            UINT32 val = 1 << (((index - OS_MEM_SMALL_BUCKET_COUNT) >> OS_MEM_SLI) + OS_MEM_LARGE_START_BUCKET);
            UINT32 offset = val >> OS_MEM_SLI;
            PRINTK("size: [%#x, %#x], num: %u\n", (offset * ((index - OS_MEM_SMALL_BUCKET_COUNT) % (1 << OS_MEM_SLI))) + val,
                    ((offset * (((index - OS_MEM_SMALL_BUCKET_COUNT) % (1 << OS_MEM_SLI)) + 1)) + val - 1), countNum[index]);
        }
    }
    PRINTK("\n   ********************************************************************\n\n");

    return LOS_OK;
}

STATUS_T OsKHeapInit(size_t size)
{
    STATUS_T ret;
    VOID *ptr = NULL;
    /*
     * roundup to MB aligned in order to set kernel attributes. kernel text/code/data attributes
     * should page mapping, remaining region should section mapping. so the boundary should be
     * MB aligned.
     */
    UINTPTR end = ROUNDUP(g_vmBootMemBase + size, MB);
    size = end - g_vmBootMemBase;

    ptr = OsVmBootMemAlloc(size);
    if (!ptr) {
        PRINT_ERR("vmm_kheap_init boot_alloc_mem failed! %d\n", size);
        return -1;
    }

    m_aucSysMem0 = m_aucSysMem1 = ptr;
    ret = LOS_MemInit(m_aucSysMem0, size);
    if (ret != LOS_OK) {
        PRINT_ERR("vmm_kheap_init LOS_MemInit failed!\n");
        g_vmBootMemBase -= size;
        return ret;
    }
#if OS_MEM_EXPAND_ENABLE
    LOS_MemExpandEnable(OS_SYS_MEM_ADDR);
#endif
    return LOS_OK;
}

BOOL OsMemIsHeapNode(const VOID *ptr)
{
    struct OsMemPoolHead *pool = (struct OsMemPoolHead *)m_aucSysMem1;
    struct OsMemNodeHead *firstNode = OS_MEM_FIRST_NODE(pool);
    struct OsMemNodeHead *endNode = OS_MEM_END_NODE(pool, pool->info.totalSize);

    if (OS_MEM_MIDDLE_ADDR(firstNode, ptr, endNode)) {
        return TRUE;
    }

#if OS_MEM_EXPAND_ENABLE
    UINT32 intSave;
    UINT32 size;
    MEM_LOCK(pool, intSave);
    while (OsMemIsLastSentinelNode(endNode) == FALSE) {
        size = OS_MEM_NODE_GET_SIZE(endNode->sizeAndFlag);
        firstNode = OsMemSentinelNodeGet(endNode);
        endNode = OS_MEM_END_NODE(firstNode, size);
        if (OS_MEM_MIDDLE_ADDR(firstNode, ptr, endNode)) {
            MEM_UNLOCK(pool, intSave);
            return TRUE;
        }
    }
    MEM_UNLOCK(pool, intSave);
#endif
    return FALSE;
}


