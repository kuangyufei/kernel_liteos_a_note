/*!
 * @file    los_memory.c
 * @brief
 * @link tlsf算法论文 http://www.gii.upv.es/tlsf/files/ecrts04_tlsf.pdf @endlink
   @verbatim
   https://www.codenong.com/cs106845116/ TLSF算法（一）分配中的位图计算
   基本概念
		内存管理模块管理系统的内存资源，它是操作系统的核心模块之一，主要包括内存的初始化、分配以及释放。
		OpenHarmony LiteOS-A的堆内存管理提供内存初始化、分配、释放等功能。在系统运行过程中，堆内存管理
		模块通过对内存的申请/释放来管理用户和OS对内存的使用，使内存的利用率和使用效率达到最优，同时最大限度地解决系统的内存碎片问题。

   运行机制
		堆内存管理，即在内存资源充足的情况下，根据用户需求，从系统配置的一块比较大的连续内存
		（内存池，也是堆内存）中分配任意大小的内存块。当用户不需要该内存块时，又可以释放回系统供下一次使用。
		与静态内存相比，动态内存管理的优点是按需分配，缺点是内存池中容易出现碎片。
		OpenHarmony LiteOS-A堆内存在TLSF算法的基础上，对区间的划分进行了优化，获得更优的性能，降低了碎片率。
		动态内存核心算法框图如下：
   @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/11/1.png  
   @verbatim
	根据空闲内存块的大小，使用多个空闲链表来管理。根据内存空闲块大小分为两个部分：[4, 127]和[27, 231]，如上图size class所示：

	1. 对[4,127]区间的内存进行等分，如上图下半部分所示，分为31个小区间，每个小区间对应内存块大小为4字节的倍数。
		每个小区间对应一个空闲内存链表和用于标记对应空闲内存链表是否为空的一个比特位，值为1时，空闲链表非空。
		[4,127]区间的31个小区间内存对应31个比特位进行标记链表是否为空。
	2. 大于127字节的空闲内存块，按照2的次幂区间大小进行空闲链表管理。总共分为24个小区间，每个小区间又等分为8个二级小区间，
		见上图上半部分的Size Class和Size SubClass部分。每个二级小区间对应一个空闲链表和用于标记对应空闲内存链表是否为空的一个比特位。
		总共24*8=192个二级小区间，对应192个空闲链表和192个比特位进行标记链表是否为空。
	例如，当有40字节的空闲内存需要插入空闲链表时，对应小区间[40,43]，第10个空闲链表，位图标记的第10比特位。
	把40字节的空闲内存挂载第10个空闲链表上，并判断是否需要更新位图标记。当需要申请40字节的内存时，
	根据位图标记获取存在满足申请大小的内存块的空闲链表，从空闲链表上获取空闲内存节点。如果分配的节点大于需要申请的内存大小，
	进行分割节点操作，剩余的节点重新挂载到相应的空闲链表上。当有580字节的空闲内存需要插入空闲链表时，对应二级小区间[29,29+2^6]，
	第31+2*8=47个空闲链表，并使用位图的第47个比特位来标记链表是否为空。把580字节的空闲内存挂载第47个空闲链表上，并判断是否需要更新位图标记。
	当需要申请580字节的内存时，根据位图标记获取存在满足申请大小的内存块的空闲链表，从空闲链表上获取空闲内存节点。
	如果分配的节点大于需要申请的内存大小，进行分割节点操作，剩余的节点重新挂载到相应的空闲链表上。如果对应的空闲链表为空，
	则向更大的内存区间去查询是否有满足条件的空闲链表，实际计算时，会一次性查找到满足申请大小的空闲链表。
	
    内存信息包括内存池大小、内存使用量、剩余内存大小、最大空闲内存、内存水线、内存节点数统计、碎片率等。
    内存水线：即内存池的最大使用量，每次申请和释放时，都会更新水线值，实际业务可根据该值，优化内存池大小；
    碎片率：衡量内存池的碎片化程度，碎片率高表现为内存池剩余内存很多，但是最大空闲内存块很小，可以用公式（fragment=100-最大空闲内存块大小/剩余内存大小）来度量；

	内存管理结构如下图所示：
   @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/11/2.png  
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-19
 */
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
#include "los_hook.h"

#ifdef LOSCFG_KERNEL_LMS
#include "los_lms_pri.h"
#endif

/* Used to cut non-essential functions. | 用于削减非必要功能 */
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

UINT8 *m_aucSysMem0 = NULL;	///< 异常交互动态内存池地址的起始地址，当不支持异常交互特性时，m_aucSysMem0等于m_aucSysMem1。
UINT8 *m_aucSysMem1 = NULL;	///< 系统动态内存池地址的起始地址

#ifdef LOSCFG_MEM_MUL_POOL
VOID *g_poolHead = NULL;	///内存池头,由它牵引多个内存池
#endif

/* The following is the macro definition and interface implementation related to the TLSF. */

/* Supposing a Second Level Index: SLI = 3. */
#define OS_MEM_SLI                      3 ///< 二级小区间级数,
/* Giving 1 free list for each small bucket: 4, 8, 12, up to 124. */
#define OS_MEM_SMALL_BUCKET_COUNT       31 ///< 小桶的偏移单位 从 4 ~ 124 ,共32级
#define OS_MEM_SMALL_BUCKET_MAX_SIZE    128 ///< 小桶的最大数量 
/* Giving OS_MEM_FREE_LIST_NUM free lists for each large bucket.  */
#define OS_MEM_LARGE_BUCKET_COUNT       24	/// 为每个大存储桶空闲列表数量 大桶范围: [2^7, 2^31] ,每个小区间有分为 2^3个小区间
#define OS_MEM_FREE_LIST_NUM            (1 << OS_MEM_SLI) ///<  2^3 = 8 个,即大桶的每个区间又分为8个小区间
/* OS_MEM_SMALL_BUCKET_MAX_SIZE to the power of 2 is 7. */
#define OS_MEM_LARGE_START_BUCKET       7 /// 大桶的开始下标

/* The count of free list. */
#define OS_MEM_FREE_LIST_COUNT  (OS_MEM_SMALL_BUCKET_COUNT + (OS_MEM_LARGE_BUCKET_COUNT << OS_MEM_SLI)) ///< 总链表的数量 32 + 24 * 8 = 224  
/* The bitmap is used to indicate whether the free list is empty, 1: not empty, 0: empty. */
#define OS_MEM_BITMAP_WORDS     ((OS_MEM_FREE_LIST_COUNT >> 5) + 1) ///< 224 >> 5 + 1 = 7 ,为什么要右移 5 因为 2^5 = 32 是一个32位整型的大小 
								///< 而 32 * 7 = 224 ,也就是说用 int[7]当位图就能表示完 224个链表 ,此处,一定要理解好,因为这是理解 TLSF 算法的关键. 
#define OS_MEM_BITMAP_MASK 0x1FU ///< 因为一个int型为 32位, 2^5 = 32,所以此处 0x1FU = 5个1 足以. 

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

/* Get the first level: f = log2(size). | 获取第一级*/
STATIC INLINE UINT32 OsMemFlGet(UINT32 size)
{
    if (size < OS_MEM_SMALL_BUCKET_MAX_SIZE) {
        return ((size >> 2) - 1); /* 2: The small bucket setup is 4. */
    }
    return OsMemLog2(size);
}

/* Get the second level: s = (size - 2^f) * 2^SLI / 2^f. | 获取第二级 */
STATIC INLINE UINT32 OsMemSlGet(UINT32 size, UINT32 fl)
{
    return (((size << OS_MEM_SLI) >> fl) - OS_MEM_FREE_LIST_NUM);
}

/* The following is the memory algorithm related macro definition and interface implementation. */
/// 内存池节点
struct OsMemNodeHead {
    UINT32 magic;	///< 魔法数字 0xABCDDCBA
    union {//注意这里的前后指向的是连续的地址节点,用于分割和合并
        struct OsMemNodeHead *prev; /* The prev is used for current node points to the previous node | prev 用于当前节点指向前一个节点*/
        struct OsMemNodeHead *next; /* The next is used for last node points to the expand node | next 用于最后一个节点指向展开节点*/
    } ptr;
#ifdef LOSCFG_MEM_LEAKCHECK //内存泄漏检测
    UINTPTR linkReg[LOS_RECORD_LR_CNT];///< 存放左右节点地址,用于检测
#endif
    UINT32 sizeAndFlag;	///< 节点总大小+标签
};
/// 已使用内存池节点
struct OsMemUsedNodeHead {
    struct OsMemNodeHead header;///< 已被使用节点
#if OS_MEM_FREE_BY_TASKID
    UINT32 taskID; ///< 使用节点的任务ID
#endif
};
/// 内存池空闲节点
struct OsMemFreeNodeHead {
    struct OsMemNodeHead header;	///< 内存池节点
    struct OsMemFreeNodeHead *prev;	///< 前一个空闲前驱节点
    struct OsMemFreeNodeHead *next;	///< 后一个空闲后继节点
};
/// 内存池信息
struct OsMemPoolInfo {
    VOID *pool;			///< 指向内存块基地址,仅做记录而已,真正的分配内存跟它没啥关系
    UINT32 totalSize;	///< 总大小,确定了内存池的边界
    UINT32 attr;		///< 属性 default attr: lock, not expand.
#ifdef LOSCFG_MEM_WATERLINE
    UINT32 waterLine;   /* Maximum usage size in a memory pool | 内存吃水线*/
    UINT32 curUsedSize; /* Current usage size in a memory pool | 当前已使用大小*/
#endif
};
/// 内存池头信息
struct OsMemPoolHead {
    struct OsMemPoolInfo info; ///< 记录内存池的信息
    UINT32 freeListBitmap[OS_MEM_BITMAP_WORDS]; ///< 空闲位图 int[7] = 32 * 7 = 224 > 223 
    struct OsMemFreeNodeHead *freeList[OS_MEM_FREE_LIST_COUNT];///< 空闲节点链表 32 + 24 * 8 = 224  
    SPIN_LOCK_S spinlock;	///< 操作本池的自旋锁,涉及CPU多核竞争,所以必须得是自旋锁
#ifdef LOSCFG_MEM_MUL_POOL
    VOID *nextPool;	///< 指向下一个内存池 OsMemPoolHead 类型
#endif
};

/* Spinlock for mem module, only available on SMP mode */
#define MEM_LOCK(pool, state)       LOS_SpinLockSave(&(pool)->spinlock, &(state))
#define MEM_UNLOCK(pool, state)     LOS_SpinUnlockRestore(&(pool)->spinlock, (state))

/* The memory pool support expand. */
#define OS_MEM_POOL_EXPAND_ENABLE  0x01	///< 支持扩展
/* The memory pool support no lock. */
#define OS_MEM_POOL_LOCK_ENABLE    0x02	///< 加锁

#define OS_MEM_NODE_MAGIC        0xABCDDCBA ///< 内存节点的魔法数字
#define OS_MEM_MIN_ALLOC_SIZE    (sizeof(struct OsMemFreeNodeHead) - sizeof(struct OsMemUsedNodeHead)) //最小分配空间
// 必须给指向空闲块的指针留位置
#define OS_MEM_NODE_USED_FLAG      0x80000000U ///< 已使用标签
#define OS_MEM_NODE_ALIGNED_FLAG   0x40000000U ///< 对齐标签
#define OS_MEM_NODE_LAST_FLAG      0x20000000U  /* Sentinel Node | 哨兵节点标签*/
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
    node->taskID = LOS_CurTaskIDGet();//将当前任务ID绑定到内存池节点上
}
#endif

#ifdef LOSCFG_MEM_WATERLINE
STATIC INLINE VOID OsMemWaterUsedRecord(struct OsMemPoolHead *pool, UINT32 size)
{
    pool->info.curUsedSize += size; //延长可使用空间
    if (pool->info.curUsedSize > pool->info.waterLine) {
        pool->info.waterLine = pool->info.curUsedSize; //警戒线加高
    }
}
#else
STATIC INLINE VOID OsMemWaterUsedRecord(struct OsMemPoolHead *pool, UINT32 size)
{
    (VOID)pool; // @note_thinking 为何要这么写 ,因为格式规范吗 ? 直接啥也不写不行吗 ? 
    (VOID)size; // 编译器会优化掉这种代码
}
#endif

#if OS_MEM_EXPAND_ENABLE
/// 更新哨兵节点内容
STATIC INLINE struct OsMemNodeHead *OsMemLastSentinelNodeGet(const struct OsMemNodeHead *sentinelNode)
{
    struct OsMemNodeHead *node = NULL;
    VOID *ptr = sentinelNode->ptr.next;//返回不连续的内存块
    UINT32 size = OS_MEM_NODE_GET_SIZE(sentinelNode->sizeAndFlag); // 获取大小

    while ((ptr != NULL) && (size != 0)) {
        node = OS_MEM_END_NODE(ptr, size);
        ptr = node->ptr.next;
        size = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);
    }

    return node;
}
/// 检查哨兵节点
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
/// 是否为最后一个哨兵节点
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
/// 设置哨兵节点内容
STATIC INLINE VOID OsMemSentinelNodeSet(struct OsMemNodeHead *sentinelNode, VOID *newNode, UINT32 size)
{
    if (sentinelNode->ptr.next != NULL) { //哨兵节点有 逻辑地址不连续的衔接内存块
        sentinelNode = OsMemLastSentinelNodeGet(sentinelNode);//更新哨兵节点内容
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
#ifdef LOSCFG_KERNEL_LMS
    LOS_LmsCheckPoolDel(node);
#endif
    return TRUE;
}
/// 内存池扩展实现
STATIC INLINE INT32 OsMemPoolExpandSub(VOID *pool, UINT32 size, UINT32 intSave)
{
    UINT32 tryCount = MAX_SHRINK_PAGECACHE_TRY;
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *newNode = NULL;
    struct OsMemNodeHead *endNode = NULL;

    size = ROUNDUP(size + OS_MEM_NODE_HEAD_SIZE, PAGE_SIZE);//圆整
    endNode = OS_MEM_END_NODE(pool, poolInfo->info.totalSize);//获取哨兵节点

RETRY:
    newNode = (struct OsMemNodeHead *)LOS_PhysPagesAllocContiguous(size >> PAGE_SHIFT);//申请新的内存池 | 物理内存
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
#ifdef LOSCFG_KERNEL_LMS
    UINT32 resize = 0;
    if (g_lms != NULL) {
        /*
         * resize == 0, shadow memory init failed, no shadow memory for this pool, set poolSize as original size.
         * resize != 0, shadow memory init successful, set poolSize as resize.
         */
        resize = g_lms->init(newNode, size);
        size = (resize == 0) ? size : resize;
    }
#endif
    newNode->sizeAndFlag = (size - OS_MEM_NODE_HEAD_SIZE);//设置新节点大小
    newNode->ptr.prev = OS_MEM_END_NODE(newNode, size);//新节点的前节点指向新节点的哨兵节点
    OsMemSentinelNodeSet(endNode, newNode, size);//设置老内存池的哨兵节点信息,其实就是指向新内存块
    OsMemFreeNodeAdd(pool, (struct OsMemFreeNodeHead *)newNode);//将新节点加入空闲链表

    endNode = OS_MEM_END_NODE(newNode, size);//获取新节点的哨兵节点
    (VOID)memset(endNode, 0, sizeof(*endNode));//清空内存
    endNode->ptr.next = NULL;//新哨兵节点没有后续指向,因为它已成为最后
    endNode->magic = OS_MEM_NODE_MAGIC;//设置新哨兵节的魔法数字
    OsMemSentinelNodeSet(endNode, NULL, 0); //设置新哨兵节点内容
    OsMemWaterUsedRecord(poolInfo, OS_MEM_NODE_HEAD_SIZE);//更新内存池警戒线

    return 0;
}
/// 扩展内存池
STATIC INLINE INT32 OsMemPoolExpand(VOID *pool, UINT32 allocSize, UINT32 intSave)
{
    UINT32 expandDefault = MEM_EXPAND_SIZE(LOS_MemPoolSizeGet(pool));//至少要扩展现有内存池的 1/8 大小
    UINT32 expandSize = MAX(expandDefault, allocSize);
    UINT32 tryCount = 1;//尝试次数
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
///< 允许指定内存池扩展
VOID LOS_MemExpandEnable(VOID *pool)
{
    if (pool == NULL) {
        return;
    }

    ((struct OsMemPoolHead *)pool)->info.attr |= OS_MEM_POOL_EXPAND_ENABLE;
}
#endif

#ifdef LOSCFG_KERNEL_LMS
STATIC INLINE VOID OsLmsFirstNodeMark(VOID *pool, struct OsMemNodeHead *node)
{
    if (g_lms == NULL) {
        return;
    }

    g_lms->simpleMark((UINTPTR)pool, (UINTPTR)node, LMS_SHADOW_PAINT_U8);
    g_lms->simpleMark((UINTPTR)node, (UINTPTR)node + OS_MEM_NODE_HEAD_SIZE, LMS_SHADOW_REDZONE_U8);
    g_lms->simpleMark((UINTPTR)OS_MEM_NEXT_NODE(node), (UINTPTR)OS_MEM_NEXT_NODE(node) + OS_MEM_NODE_HEAD_SIZE,
        LMS_SHADOW_REDZONE_U8);
    g_lms->simpleMark((UINTPTR)node + OS_MEM_NODE_HEAD_SIZE, (UINTPTR)OS_MEM_NEXT_NODE(node),
        LMS_SHADOW_AFTERFREE_U8);
}

STATIC INLINE VOID OsLmsAllocAlignMark(VOID *ptr, VOID *alignedPtr, UINT32 size)
{
    struct OsMemNodeHead *allocNode = NULL;

    if ((g_lms == NULL) || (ptr == NULL)) {
        return;
    }
    allocNode = (struct OsMemNodeHead *)((struct OsMemUsedNodeHead *)ptr - 1);
    if (ptr != alignedPtr) {
        g_lms->simpleMark((UINTPTR)ptr, (UINTPTR)ptr + sizeof(UINT32), LMS_SHADOW_PAINT_U8);
        g_lms->simpleMark((UINTPTR)ptr + sizeof(UINT32), (UINTPTR)alignedPtr, LMS_SHADOW_REDZONE_U8);
    }

    /* mark remining as redzone */
    g_lms->simpleMark(LMS_ADDR_ALIGN((UINTPTR)alignedPtr + size), (UINTPTR)OS_MEM_NEXT_NODE(allocNode),
        LMS_SHADOW_REDZONE_U8);
}

STATIC INLINE VOID OsLmsReallocMergeNodeMark(struct OsMemNodeHead *node)
{
    if (g_lms == NULL) {
        return;
    }

    g_lms->simpleMark((UINTPTR)node + OS_MEM_NODE_HEAD_SIZE, (UINTPTR)OS_MEM_NEXT_NODE(node),
        LMS_SHADOW_ACCESSABLE_U8);
}

STATIC INLINE VOID OsLmsReallocSplitNodeMark(struct OsMemNodeHead *node)
{
    if (g_lms == NULL) {
        return;
    }
    /* mark next node */
    g_lms->simpleMark((UINTPTR)OS_MEM_NEXT_NODE(node),
        (UINTPTR)OS_MEM_NEXT_NODE(node) + OS_MEM_NODE_HEAD_SIZE, LMS_SHADOW_REDZONE_U8);
    g_lms->simpleMark((UINTPTR)OS_MEM_NEXT_NODE(node) + OS_MEM_NODE_HEAD_SIZE,
        (UINTPTR)OS_MEM_NEXT_NODE(OS_MEM_NEXT_NODE(node)), LMS_SHADOW_AFTERFREE_U8);
}

STATIC INLINE VOID OsLmsReallocResizeMark(struct OsMemNodeHead *node, UINT32 resize)
{
    if (g_lms == NULL) {
        return;
    }
    /* mark remaining as redzone */
    g_lms->simpleMark((UINTPTR)node + resize, (UINTPTR)OS_MEM_NEXT_NODE(node), LMS_SHADOW_REDZONE_U8);
}
#endif
#ifdef LOSCFG_MEM_LEAKCHECK //内存泄漏检查
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
/// 打印已使用的节点
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
    UINT32 fl = OsMemFlGet(size);//获取一级位图
    if (size < OS_MEM_SMALL_BUCKET_MAX_SIZE) {
        return fl;
    }

    UINT32 sl = OsMemSlGet(size, fl);//获取二级位图
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

#define BITMAP_INDEX(index) ((index) >> 5)
STATIC INLINE UINT32 OsMemNotEmptyIndexGet(struct OsMemPoolHead *poolHead, UINT32 index)
{
    UINT32 mask;

    mask = poolHead->freeListBitmap[BITMAP_INDEX(index)];
    mask &= ~((1 << (index & OS_MEM_BITMAP_MASK)) - 1);
    if (mask != 0) {
        index = OsMemFFS(mask) + (index & ~OS_MEM_BITMAP_MASK);
        return index;
    }

    return OS_MEM_FREE_LIST_COUNT;
}
/// 找到下一个合适的块
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

        for (index = LOS_Align(index + 1, 32); index < OS_MEM_FREE_LIST_COUNT; index += 32) { /* 32: align size */
            mask = poolHead->freeListBitmap[BITMAP_INDEX(index)];
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
    head->freeListBitmap[BITMAP_INDEX(index)] |= 1U << (index & 0x1f);
}

STATIC INLINE VOID OsMemClearFreeListBit(struct OsMemPoolHead *head, UINT32 index)
{
    head->freeListBitmap[BITMAP_INDEX(index)] &= ~(1U << (index & 0x1f));
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
/// 从空闲链表中删除
STATIC INLINE VOID OsMemListDelete(struct OsMemPoolHead *pool, UINT32 listIndex, struct OsMemFreeNodeHead *node)
{
    if (node == pool->freeList[listIndex]) {
        pool->freeList[listIndex] = node->next;
        if (node->next == NULL) {//如果链表空了
            OsMemClearFreeListBit(pool, listIndex);//将位图位 置为 0 
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
/// 添加一个空闲节点
STATIC INLINE VOID OsMemFreeNodeAdd(VOID *pool, struct OsMemFreeNodeHead *node)
{
    UINT32 index = OsMemFreeListIndexGet(node->header.sizeAndFlag);//根据大小定位索引位
    if (index >= OS_MEM_FREE_LIST_COUNT) {
        LOS_Panic("The index of free lists is error, index = %u\n", index);
        return;
    }
    OsMemListAdd(pool, index, node);//挂入链表
}
/// 从空闲链表上摘除节点
STATIC INLINE VOID OsMemFreeNodeDelete(VOID *pool, struct OsMemFreeNodeHead *node)
{
    UINT32 index = OsMemFreeListIndexGet(node->header.sizeAndFlag);//根据大小定位索引位
    if (index >= OS_MEM_FREE_LIST_COUNT) {
        LOS_Panic("The index of free lists is error, index = %u\n", index);
        return;
    }
    OsMemListDelete(pool, index, node);
}
//获取一个空闲的节点
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
/// 合并节点,和前面的节点合并 node 消失
STATIC INLINE VOID OsMemMergeNode(struct OsMemNodeHead *node)
{
    struct OsMemNodeHead *nextNode = NULL;

    node->ptr.prev->sizeAndFlag += node->sizeAndFlag; //前节点长度变长
    nextNode = (struct OsMemNodeHead *)((UINTPTR)node + node->sizeAndFlag); // 下一个节点位置
    if (!OS_MEM_NODE_GET_LAST_FLAG(nextNode->sizeAndFlag)) {//不是哨兵节点
        nextNode->ptr.prev = node->ptr.prev;//后一个节点的前节点变成前前节点
    }
}
/// 切割节点
STATIC INLINE VOID OsMemSplitNode(VOID *pool, struct OsMemNodeHead *allocNode, UINT32 allocSize)
{
    struct OsMemFreeNodeHead *newFreeNode = NULL;
    struct OsMemNodeHead *nextNode = NULL;

    newFreeNode = (struct OsMemFreeNodeHead *)(VOID *)((UINT8 *)allocNode + allocSize);//切割后出现的新空闲节点,在分配节点的右侧
    newFreeNode->header.ptr.prev = allocNode;//新节点指向前节点,说明是从左到右切割
    newFreeNode->header.sizeAndFlag = allocNode->sizeAndFlag - allocSize;//新空闲节点大小
    allocNode->sizeAndFlag = allocSize;//分配节点大小
    nextNode = OS_MEM_NEXT_NODE(&newFreeNode->header);//获取新节点的下一个节点
    if (!OS_MEM_NODE_GET_LAST_FLAG(nextNode->sizeAndFlag)) {//如果下一个节点不是哨兵节点(末尾节点)
        nextNode->ptr.prev = &newFreeNode->header;//下一个节点的前节点为新空闲节点
        if (!OS_MEM_NODE_GET_USED_FLAG(nextNode->sizeAndFlag)) {//如果下一个节点也是空闲的
            OsMemFreeNodeDelete(pool, (struct OsMemFreeNodeHead *)nextNode);//删除下一个节点信息
            OsMemMergeNode(nextNode);//下一个节点和新空闲节点 合并成一个新节点
        }
    }

    OsMemFreeNodeAdd(pool, newFreeNode);//挂入空闲链表
}
//
STATIC INLINE VOID *OsMemCreateUsedNode(VOID *addr)
{
    struct OsMemUsedNodeHead *node = (struct OsMemUsedNodeHead *)addr;//直接将地址转成使用节点,说明节点信息也存在内存池中
    //这种用法是非常巧妙的
#if OS_MEM_FREE_BY_TASKID
    OsMemNodeSetTaskID(node);//设置使用内存节点的任务
#endif

#ifdef LOSCFG_KERNEL_LMS //检测内存泄漏
    struct OsMemNodeHead *newNode = (struct OsMemNodeHead *)node;
    if (g_lms != NULL) {
        g_lms->mallocMark(newNode, OS_MEM_NEXT_NODE(newNode), OS_MEM_NODE_HEAD_SIZE);
    }
#endif
    return node + 1; //@note_good 这个地方挺有意思的,只是将结构体扩展下,留一个 int 位 ,变成了已使用节点,返回的地址正是要分配给应用的地址
}

/*!
 * @brief OsMemPoolInit	内存池初始化
 * 内存池节点部分包含3种类型节点：未使用空闲内存节点(OsMemFreeNodeHead)，已使用内存节点(OsMemUsedNodeHead) 和 尾节点(OsMemNodeHead)。
 * \n 每个内存节点维护一个前序指针，指向内存池中上一个内存节点，还维护内存节点的大小和使用标记。
 * \n 空闲内存节点和已使用内存节点后面的内存区域是数据域
 * @param pool	
 * @param size	
 * @return	
 *
 * @see
 */
STATIC UINT32 OsMemPoolInit(VOID *pool, UINT32 size)
{
    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *newNode = NULL;
    struct OsMemNodeHead *endNode = NULL;
#ifdef LOSCFG_KERNEL_LMS
    UINT32 resize = 0;
    if (g_lms != NULL) {
        /*
         * resize == 0, shadow memory init failed, no shadow memory for this pool, set poolSize as original size.
         * resize != 0, shadow memory init successful, set poolSize as resize.
         */
        resize = g_lms->init(pool, size);
        size = (resize == 0) ? size : resize;
    }
#endif
    (VOID)memset(poolHead, 0, sizeof(struct OsMemPoolHead));

    LOS_SpinInit(&poolHead->spinlock);
    poolHead->info.pool = pool;	//内存池的起始地址,但注意真正的内存并不是从此处分配,它只是用来记录这个内存块的开始位置而已.
    poolHead->info.totalSize = size;//内存池总大小
    poolHead->info.attr = OS_MEM_POOL_LOCK_ENABLE; /* default attr: lock, not expand. | 默认是上锁,不支持扩展,需扩展得另外设置*/

    newNode = OS_MEM_FIRST_NODE(pool);//跳到第一个节点位置,即跳过结构体本身位置,真正的分配内存是从newNode开始的.
    newNode->sizeAndFlag = (size - sizeof(struct OsMemPoolHead) - OS_MEM_NODE_HEAD_SIZE);//这才是可供分配给外界使用的总内存块大小,即数据域
    //OS_MEM_NODE_HEAD_SIZE 叫当前使用节点,即指 newNode占用的空间
    newNode->ptr.prev = NULL;//开始是空指向
    newNode->magic = OS_MEM_NODE_MAGIC;//魔法数字 用于标识这是一个 OsMemNodeHead 节点, 魔法数字不能被覆盖,
    OsMemFreeNodeAdd(pool, (struct OsMemFreeNodeHead *)newNode);//添加一个空闲节点,由此有了首个可供分配的空闲节点

    /* The last mem node */
    endNode = OS_MEM_END_NODE(pool, size);//确定尾节点位置,尾节点没有数据域
    endNode->magic = OS_MEM_NODE_MAGIC; //填入尾节点的魔法数字
#if OS_MEM_EXPAND_ENABLE //支持扩展
    endNode->ptr.next = NULL;//尾节点没有后继节点
    OsMemSentinelNodeSet(endNode, NULL, 0);//将尾节点设置为哨兵节点
#else
    endNode->sizeAndFlag = 0;//0代表没有数据域
    endNode->ptr.prev = newNode;//前驱指针指向第一个节点
    OS_MEM_NODE_SET_USED_FLAG(endNode->sizeAndFlag);
#endif
#ifdef LOSCFG_MEM_WATERLINE //吃水线开关
    poolHead->info.curUsedSize = sizeof(struct OsMemPoolHead) + OS_MEM_NODE_HEAD_SIZE;//内存池已使用了这么多空间,这些都是存内存池自身数据的空间, 
    							//但此处是否还要算是 endNode ? @note_thinking 
    poolHead->info.waterLine = poolHead->info.curUsedSize; //设置吃水线
#endif
#ifdef LOSCFG_KERNEL_LMS
    if (resize != 0) {
        OsLmsFirstNodeMark(pool, newNode);
    }
#endif
    return LOS_OK;
}

#ifdef LOSCFG_MEM_MUL_POOL
STATIC VOID OsMemPoolDeinit(VOID *pool)
{
    (VOID)memset(pool, 0, sizeof(struct OsMemPoolHead));
}
/// 新增内存池
STATIC UINT32 OsMemPoolAdd(VOID *pool, UINT32 size)
{
    VOID *nextPool = g_poolHead;
    VOID *curPool = g_poolHead;
    UINTPTR poolEnd;
    while (nextPool != NULL) {//单链表遍历方式
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
        g_poolHead = pool; //首个内存池
    } else {
        ((struct OsMemPoolHead *)curPool)->nextPool = pool; //两池扯上关系
    }

    ((struct OsMemPoolHead *)pool)->nextPool = NULL; //新池下一个无所指
    return LOS_OK;
}
/// 删除内存池
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

/*!
 * @brief LOS_MemInit	初始化一块指定的动态内存池，大小为size
 * 初始一个内存池后生成一个内存池控制头、尾节点EndNode，剩余的内存被标记为FreeNode内存节点。
 * @param pool	
 * @param size	
 * @return	
 * @attention EndNode作为内存池末尾的节点，size为0。
 * @see
 */
UINT32 LOS_MemInit(VOID *pool, UINT32 size)
{
    if ((pool == NULL) || (size <= OS_MEM_MIN_POOL_SIZE)) {
        return OS_ERROR;
    }

    size = OS_MEM_ALIGN(size, OS_MEM_ALIGN_SIZE);//4个字节对齐
    if (OsMemPoolInit(pool, size)) {
        return OS_ERROR;
    }

#ifdef LOSCFG_MEM_MUL_POOL //多内存池开关
    if (OsMemPoolAdd(pool, size)) {
        (VOID)OsMemPoolDeinit(pool);
        return OS_ERROR;
    }
#endif

    OsHookCall(LOS_HOOK_TYPE_MEM_INIT, pool, size);//打印日志
    return LOS_OK;
}

#ifdef LOSCFG_MEM_MUL_POOL
/// 删除指定内存池
UINT32 LOS_MemDeInit(VOID *pool)
{
    if (pool == NULL) {
        return OS_ERROR;
    }

    if (OsMemPoolDelete(pool)) {
        return OS_ERROR;
    }

    OsMemPoolDeinit(pool);

    OsHookCall(LOS_HOOK_TYPE_MEM_DEINIT, pool);
    return LOS_OK;
}
/// 打印系统中已初始化的所有内存池，包括内存池的起始地址、内存池大小、空闲内存总大小、已使用内存总大小、最大的空闲内存块大小、空闲内存块数量、已使用的内存块数量。
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
/// 从指定动态内存池中申请size长度的内存
STATIC INLINE VOID *OsMemAlloc(struct OsMemPoolHead *pool, UINT32 size, UINT32 intSave)
{
    struct OsMemNodeHead *allocNode = NULL;

#ifdef LOSCFG_BASE_MEM_NODE_INTEGRITY_CHECK
    if (OsMemAllocCheck(pool, intSave) == LOS_NOK) {
        return NULL;
    }
#endif

    UINT32 allocSize = OS_MEM_ALIGN(size + OS_MEM_NODE_HEAD_SIZE, OS_MEM_ALIGN_SIZE);
#if OS_MEM_EXPAND_ENABLE
retry: //这种写法也挺赞的 @note_good
#endif
    allocNode = OsMemFreeNodeGet(pool, allocSize);//获取空闲节点
    if (allocNode == NULL) {//没有内存了,怎搞? 
#if OS_MEM_EXPAND_ENABLE
        if (pool->info.attr & OS_MEM_POOL_EXPAND_ENABLE) {
            INT32 ret = OsMemPoolExpand(pool, allocSize, intSave);//扩展内存池
            if (ret == 0) {
                goto retry;//再来一遍 
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

    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_MIN_ALLOC_SIZE) <= allocNode->sizeAndFlag) {//所需小于内存池可供分配量
        OsMemSplitNode(pool, allocNode, allocSize);//劈开内存池
    }

    OS_MEM_NODE_SET_USED_FLAG(allocNode->sizeAndFlag);//给节点贴上已使用的标签
    OsMemWaterUsedRecord(pool, OS_MEM_NODE_GET_SIZE(allocNode->sizeAndFlag));//更新吃水线

#ifdef LOSCFG_MEM_LEAKCHECK //检测内存泄漏开关
    OsMemLinkRegisterRecord(allocNode);
#endif
    return OsMemCreateUsedNode((VOID *)allocNode);//创建已使用节点
}
/// 从指定内存池中申请size长度的内存,注意这可不是从内核堆空间中申请内存
VOID *LOS_MemAlloc(VOID *pool, UINT32 size)
{
    if ((pool == NULL) || (size == 0)) {//没提供内存池时
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
        ptr = OsMemAlloc(poolHead, size, intSave);//真正的分配内存函数,详细查看 鸿蒙内核源码分析(内存池篇)
        MEM_UNLOCK(poolHead, intSave);
    } while (0);

    OsHookCall(LOS_HOOK_TYPE_MEM_ALLOC, pool, ptr, size);//打印日志,到此一游
    return ptr;
}
/// 从指定内存池中申请size长度的内存且地址按boundary字节对齐的内存
VOID *LOS_MemAllocAlign(VOID *pool, UINT32 size, UINT32 boundary)
{
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
#ifdef LOSCFG_KERNEL_LMS
            OsLmsAllocAlignMark(ptr, alignedPtr, size);
#endif
            break;
        }

        /* store gapSize in address (ptr - 4), it will be checked while free */
        gapSize = (UINT32)((UINTPTR)alignedPtr - (UINTPTR)ptr);
        struct OsMemUsedNodeHead *allocNode = (struct OsMemUsedNodeHead *)ptr - 1;
        OS_MEM_NODE_SET_ALIGNED_FLAG(allocNode->header.sizeAndFlag);
        OS_MEM_NODE_SET_ALIGNED_FLAG(gapSize);
        *(UINT32 *)((UINTPTR)alignedPtr - sizeof(gapSize)) = gapSize;
#ifdef LOSCFG_KERNEL_LMS
        OsLmsAllocAlignMark(ptr, alignedPtr, size);
#endif
        ptr = alignedPtr;
    } while (0);

    OsHookCall(LOS_HOOK_TYPE_MEM_ALLOCALIGN, pool, ptr, size, boundary);//打印对齐日志,表示程序曾临幸过此处
    return ptr;
}
/// 内存池有效性检查
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
#if OS_MEM_EXPAND_ENABLE //如果支持可扩展
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

STATIC  BOOL MemCheckUsedNode(const struct OsMemPoolHead *pool, const struct OsMemNodeHead *node,
                              const struct OsMemNodeHead *startNode, const struct OsMemNodeHead *endNode)
{
    if (!OsMemIsNodeValid(node, startNode, endNode, pool)) {
        return FALSE;
    }

    if (!OS_MEM_NODE_GET_USED_FLAG(node->sizeAndFlag)) {
        return FALSE;
    }

    const struct OsMemNodeHead *nextNode = OS_MEM_NEXT_NODE(node);
    if (!OsMemIsNodeValid(nextNode, startNode, endNode, pool)) {
        return FALSE;
    }

    if (!OS_MEM_NODE_GET_LAST_FLAG(nextNode->sizeAndFlag)) {
        if (nextNode->ptr.prev != node) {
            return FALSE;
        }
    }

    if ((node != startNode) &&
        ((!OsMemIsNodeValid(node->ptr.prev, startNode, endNode, pool)) ||
        (OS_MEM_NEXT_NODE(node->ptr.prev) != node))) {
        return FALSE;
    }

    return TRUE;
}

STATIC UINT32 OsMemCheckUsedNode(const struct OsMemPoolHead *pool, const struct OsMemNodeHead *node)
{
    struct OsMemNodeHead *startNode = (struct OsMemNodeHead *)OS_MEM_FIRST_NODE(pool);
    struct OsMemNodeHead *endNode = (struct OsMemNodeHead *)OS_MEM_END_NODE(pool, pool->info.totalSize);
    BOOL doneFlag = FALSE;

    do {
        doneFlag = MemCheckUsedNode(pool, node, startNode, endNode);
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
/// 释放内存
STATIC INLINE UINT32 OsMemFree(struct OsMemPoolHead *pool, struct OsMemNodeHead *node)
{
    UINT32 ret = OsMemCheckUsedNode(pool, node);
    if (ret != LOS_OK) {
        PRINT_ERR("OsMemFree check error!\n");
        return ret;
    }

#ifdef LOSCFG_MEM_WATERLINE
    pool->info.curUsedSize -= OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);//降低水位线
#endif

    node->sizeAndFlag = OS_MEM_NODE_GET_SIZE(node->sizeAndFlag);//获取大小和标记
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(node);
#endif
#ifdef LOSCFG_KERNEL_LMS
    struct OsMemNodeHead *nextNodeBackup = OS_MEM_NEXT_NODE(node);
    struct OsMemNodeHead *curNodeBackup = node;
    if (g_lms != NULL) {
        g_lms->check((UINTPTR)node + OS_MEM_NODE_HEAD_SIZE, TRUE);
    }
#endif
    struct OsMemNodeHead *preNode = node->ptr.prev; /* merage preNode | 合并前一个节点*/
    if ((preNode != NULL) && !OS_MEM_NODE_GET_USED_FLAG(preNode->sizeAndFlag)) {
        OsMemFreeNodeDelete(pool, (struct OsMemFreeNodeHead *)preNode);//删除前节点的信息
        OsMemMergeNode(node);//向前合并
        node = preNode;
    }

    struct OsMemNodeHead *nextNode = OS_MEM_NEXT_NODE(node); /* merage nextNode  | 计算后一个节点位置*/
    if ((nextNode != NULL) && !OS_MEM_NODE_GET_USED_FLAG(nextNode->sizeAndFlag)) {
        OsMemFreeNodeDelete(pool, (struct OsMemFreeNodeHead *)nextNode);//删除后节点信息
        OsMemMergeNode(nextNode);//合并节点
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
#ifdef LOSCFG_KERNEL_LMS
    if (g_lms != NULL) {
        g_lms->freeMark(curNodeBackup, nextNodeBackup, OS_MEM_NODE_HEAD_SIZE);
    }
#endif
    return ret;
}
/// 释放从指定动态内存中申请的内存
UINT32 LOS_MemFree(VOID *pool, VOID *ptr)
{
    UINT32 intSave;
    UINT32 ret = LOS_NOK;

    if ((pool == NULL) || (ptr == NULL) || !OS_MEM_IS_ALIGNED(pool, sizeof(VOID *)) ||
        !OS_MEM_IS_ALIGNED(ptr, sizeof(VOID *))) {
        return ret;
    }
    OsHookCall(LOS_HOOK_TYPE_MEM_FREE, pool, ptr);

    struct OsMemPoolHead *poolHead = (struct OsMemPoolHead *)pool;
    struct OsMemNodeHead *node = NULL;

    do {
        UINT32 gapSize = *(UINT32 *)((UINTPTR)ptr - sizeof(UINT32));//获取节点大小和标签 即: sizeAndFlag
        if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize) && OS_MEM_NODE_GET_USED_FLAG(gapSize)) {
            PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
            break;
        }

        node = (struct OsMemNodeHead *)((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE);//定位到节点开始位置

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

    return ret;
}

STATIC INLINE VOID OsMemReAllocSmaller(VOID *pool, UINT32 allocSize, struct OsMemNodeHead *node, UINT32 nodeSize)
{
#ifdef LOSCFG_MEM_WATERLINE
    struct OsMemPoolHead *poolInfo = (struct OsMemPoolHead *)pool;
#endif
    node->sizeAndFlag = nodeSize;
    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_MIN_ALLOC_SIZE) <= nodeSize) {
        OsMemSplitNode(pool, node, allocSize);
        OS_MEM_NODE_SET_USED_FLAG(node->sizeAndFlag);
#ifdef LOSCFG_MEM_WATERLINE
        poolInfo->info.curUsedSize -= nodeSize - allocSize;
#endif
#ifdef LOSCFG_KERNEL_LMS
        OsLmsReallocSplitNodeMark(node);
    } else {
        OsLmsReallocResizeMark(node, allocSize);
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
#ifdef LOSCFG_KERNEL_LMS
    OsLmsReallocMergeNodeMark(node);
#endif
    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_MIN_ALLOC_SIZE) <= node->sizeAndFlag) {
        OsMemSplitNode(pool, node, allocSize);
#ifdef LOSCFG_KERNEL_LMS
        OsLmsReallocSplitNodeMark(node);
    } else {
        OsLmsReallocResizeMark(node, allocSize);
#endif
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
/// 按size大小重新分配内存块，并将原内存块内容拷贝到新内存块。如果新内存块申请成功，则释放原内存块
VOID *LOS_MemRealloc(VOID *pool, VOID *ptr, UINT32 size)
{
    if ((pool == NULL) || OS_MEM_NODE_GET_USED_FLAG(size) || OS_MEM_NODE_GET_ALIGNED_FLAG(size)) {
        return NULL;
    }
    OsHookCall(LOS_HOOK_TYPE_MEM_REALLOC, pool, ptr, size);
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
/// 获取指定动态内存池的总大小
UINT32 LOS_MemPoolSizeGet(const VOID *pool)
{
    UINT32 count = 0;

    if (pool == NULL) {
        return LOS_NOK;
    }

    count += ((struct OsMemPoolHead *)pool)->info.totalSize; // 这里的 += 好像没必要吧?, = 就可以了, @note_thinking 

#if OS_MEM_EXPAND_ENABLE //支持扩展
    UINT32 size;
    struct OsMemNodeHead *node = NULL;
    struct OsMemNodeHead *sentinel = OS_MEM_END_NODE(pool, count);//获取哨兵节点

    while (OsMemIsLastSentinelNode(sentinel) == FALSE) {//不是最后一个节点
        size = OS_MEM_NODE_GET_SIZE(sentinel->sizeAndFlag);//数据域大小
        node = OsMemSentinelNodeGet(sentinel);//再获取哨兵节点
        sentinel = OS_MEM_END_NODE(node, size);//获取尾节点
        count += size; //内存池大小变大
    }
#endif
    return count;
}
/// 获取指定动态内存池的总使用量大小
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
    if (((*tmpNode)->prev != NULL) && !OsMemAddrValidCheck(pool, (*tmpNode)->prev)) {
        PRINT_ERR("[%s], %d, memory check error!\n"
                  " freeNode.prev:%#x is out of legal mem range\n",
                  __FUNCTION__, __LINE__, (*tmpNode)->prev);
        return LOS_NOK;
    }
    if (((*tmpNode)->next != NULL) && !OsMemAddrValidCheck(pool, (*tmpNode)->next)) {
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
                goto OUT;
            }
        }
    }

OUT:
    if (flag) {
        PRINTK("mem pool info: poolAddr: %#x, poolSize: 0x%x\n", pool, pool->info.totalSize);
#ifdef LOSCFG_MEM_WATERLINE
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
//对指定内存池做完整性检查,
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
        PRINTK("\n broken node head: %#x  %#x  %#x  %#x, %#x",
               freeNode->header.ptr.prev, freeNode->next, freeNode->prev, freeNode->header.magic,
               freeNode->header.sizeAndFlag);
    }

    if (OS_MEM_NODE_GET_USED_FLAG(preNode->sizeAndFlag)) {
        usedNode = (struct OsMemUsedNodeHead *)preNode;
        PRINTK("prev node head: %#x  %#x  %#x\n",
               usedNode->header.ptr.prev, usedNode->header.magic, usedNode->header.sizeAndFlag);
    } else {
        freeNode = (struct OsMemFreeNodeHead *)preNode;
        PRINTK("prev node head: %#x  %#x  %#x  %#x, %#x",
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
    LOS_Panic("Memory integrity check error, cur node: %#x, pre node: %#x\n", tmpNode, preNode);
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
/// 对指定内存池做完整性检查
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

/*!
 * @brief LOS_MemInfoGet	
 * 获取指定内存池的内存结构信息，包括空闲内存大小、已使用内存大小、空闲内存块数量、已使用的内存块数量、最大的空闲内存块大小
 * @param pool	
 * @param poolStatus	
 * @return	
 *
 * @see
 */
UINT32 LOS_MemInfoGet(VOID *pool, LOS_MEM_POOL_STATUS *poolStatus)
{//内存碎片率计算：同样调用LOS_MemInfoGet接口，可以获取内存池的剩余内存大小和最大空闲内存块大小，然后根据公式（fragment=100-最大空闲内存块大小/剩余内存大小）得出此时的动态内存池碎片率。
    struct OsMemPoolHead *poolInfo = pool;

    if (poolStatus == NULL) {
        PRINT_ERR("can't use NULL addr to save info\n");
        return LOS_NOK;
    }

    if ((pool == NULL) || (poolInfo->info.pool != pool)) {
        PRINT_ERR("wrong mem pool addr: %#x, line:%d\n", poolInfo, __LINE__);
        return LOS_NOK;
    }

    (VOID)memset(poolStatus, 0, sizeof(LOS_MEM_POOL_STATUS));

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
#ifdef LOSCFG_MEM_WATERLINE
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

#ifdef LOSCFG_MEM_WATERLINE
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
/// 打印指定内存池的空闲内存块的大小及数量
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
            PRINTK("size: [%#x, %#x], num: %u\n",
                   (offset * ((index - OS_MEM_SMALL_BUCKET_COUNT) % (1 << OS_MEM_SLI))) + val,
                   ((offset * (((index - OS_MEM_SMALL_BUCKET_COUNT) % (1 << OS_MEM_SLI)) + 1)) + val - 1),
                   countNum[index]);
        }
    }
    PRINTK("\n   ********************************************************************\n\n");

    return LOS_OK;
}
///内核空间动态内存(堆内存)初始化
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
    ret = LOS_MemInit(m_aucSysMem0, size); //初始化内存池
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


