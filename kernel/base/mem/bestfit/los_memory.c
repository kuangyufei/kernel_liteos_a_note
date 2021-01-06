/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "los_memory_pri.h"
#include "los_vm_phys.h"
#include "los_vm_boot.h"
#include "los_vm_common.h"
#include "los_vm_filemap.h"
#include "asm/page.h"
#include "los_multipledlinkhead_pri.h"
#include "los_memstat_pri.h"
#include "los_memrecord_pri.h"
#include "los_task_pri.h"
#include "los_exc.h"
#include "los_spinlock.h"

#ifdef LOSCFG_SHELL_EXCINFO
#include "los_excinfo_pri.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/******************************************************************************
基本概念
	内存管理模块管理系统的内存资源，它是操作系统的核心模块之一，主要包括内存的初始化、分配以及释放。
	在系统运行过程中，内存管理模块通过对内存的申请/释放来管理用户和OS对内存的使用，
	使内存的利用率和使用效率达到最优，同时最大限度地解决系统的内存碎片问题。

内存管理分为静态内存管理和动态内存管理，提供内存初始化、分配、释放等功能。
	动态内存：在动态内存池中分配用户指定大小的内存块。
		优点：按需分配。
		缺点：内存池中可能出现碎片。
	
	静态内存：在静态内存池中分配用户初始化时预设（固定）大小的内存块。
		优点：分配和释放效率高，静态内存池中无碎片。
		缺点：只能申请到初始化预设大小的内存块，不能按需申请。

动态内存运作机制
	动态内存管理，即在内存资源充足的情况下，根据用户需求，从系统配置的一块比较大的连续内存
	（内存池，也是堆内存）中分配任意大小的内存块。当用户不需要该内存块时，又可以释放回系统供下一次使用。
	与静态内存相比，动态内存管理的优点是按需分配，缺点是内存池中容易出现碎片。
	LiteOS动态内存支持bestfit（也称为dlink）和bestfit_little两种内存管理算法。

使用场景
	动态内存管理的主要工作是动态分配并管理用户申请到的内存区间。
	动态内存管理主要用于用户需要使用大小不等的内存块的场景。当用户需要使用内存时，
	可以通过操作系统的动态内存申请函数索取指定大小的内存块，一旦使用完毕，
	通过动态内存释放函数归还所占用内存，使之可以重复使用。

	对于bestfit_little算法，只支持宏开关LOSCFG_MEM_MUL_POOL控制的多内存池相关接口和
	宏开关LOSCFG_BASE_MEM_NODE_INTEGRITY_CHECK控制的内存合法性检查接口，不支持其他内存调测功能。
	通过LOS_MemAllocAlign/LOS_MemMallocAlign申请的内存进行LOS_MemRealloc/LOS_MemMrealloc操作后，
	不能保障新的内存首地址保持对齐。
	对于bestfit_little算法，不支持对LOS_MemAllocAlign申请的内存进行LOS_MemRealloc操作，否则将返回失败。

注意事项
	由于动态内存管理需要管理控制块数据结构来管理内存，这些数据结构会额外消耗内存，
	故实际用户可使用内存总量小于配置项OS_SYS_MEM_SIZE的大小。
	
	对齐分配内存接口LOS_MemAllocAlign/LOS_MemMallocAlign因为要进行地址对齐，可能会额外
	消耗部分内存，故存在一些遗失内存，当系统释放该对齐内存时，同时回收由于对齐导致的遗失内存。
	
	重新分配内存接口LOS_MemRealloc/LOS_MemMrealloc如果分配成功，系统会自己判定是否需要
	释放原来申请的内存，并返回重新分配的内存地址。如果重新分配失败，原来的内存保持不变，
	并返回NULL。禁止使用pPtr = LOS_MemRealloc(pool, pPtr, uwSize)，
	即：不能使用原来的旧内存地址pPtr变量来接收返回值。
	
	对同一块内存多次调用LOS_MemFree/LOS_MemMfree时，第一次会返回成功，但对同一块内存多次
	重复释放会导致非法指针操作，结果不可预知。
	
	由于动态内存管理的内存节点控制块结构体LosMemDynNode中，成员sizeAndFlag的数据类型为UINT32，
	高两位为标志位，余下的30位表示内存结点大小，因此用户初始化内存池的大小不能超过1G，否则会出现不可预知的结果。
	
参考
	https://gitee.com/LiteOS/LiteOS/blob/master/doc/Huawei_LiteOS_Kernel_Developer_Guide_zh.md
******************************************************************************/

#define NODEDUMPSIZE  64  /* the dump size of current broken node when memcheck error */
#define COLUMN_NUM    8   /* column num of the output info of mem node */

#define MEM_POOL_EXPAND_ENABLE  1 //内存池扩展使能
#define MEM_POOL_EXPAND_DISABLE 0 //内存池禁止扩展

/* Memory pool information structure */
typedef struct { //内存池信息结构体,bestfit动态内存管理第一部分
    VOID *pool;      /* Starting address of a memory pool */			//内存池开始地址,必须放在结构体首位
    UINT32 poolSize; /* Memory pool size */								//内存池大小
    UINT32 flag;     /* Whether the memory pool supports expansion */	//内存池是否支持扩展
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)	//警戒线					
    UINT32 poolWaterLine;   /* Maximum usage size in a memory pool */	//内存池中的最大使用大小
    UINT32 poolCurUsedSize; /* Current usage size in a memory pool */	//当前已使用内存池大小
#endif
#ifdef LOSCFG_MEM_MUL_POOL
    VOID *nextPool;
#endif
} LosMemPoolInfo;

/* Memory linked list control node structure */
typedef struct {//内存链表节点实体结构体体
    LOS_DL_LIST freeNodeInfo;         /* Free memory node */	//空闲链表,将通过它挂到 LosMultipleDlinkHead[n]上
    struct tagLosMemDynNode *preNode; /* Pointer to the previous memory node */	//指向前一个节点

#ifdef LOSCFG_MEM_HEAD_BACKUP
    UINT32 gapSize;
    UINTPTR checksum; /* magic = xor checksum */
#endif

#ifdef LOSCFG_MEM_RECORDINFO
    UINT32 originSize;
#ifdef LOSCFG_AARCH64
    UINT32 reserve1; /* 64-bit alignment */
#endif
#endif

#ifdef LOSCFG_MEM_LEAKCHECK
    UINTPTR linkReg[LOS_RECORD_LR_CNT];
#endif

#ifdef LOSCFG_AARCH64
    UINT32 reserve2; /* 64-bit alignment */
#endif
    /* Size and flag of the current node (the high two bits represent a flag,and the rest bits specify the size) */
    UINT32 sizeAndFlag;//当前节点的大小和标志（高两位表示标志，其余位指定大小）
} LosMemCtlNode;

/* Memory linked list node structure */
typedef struct tagLosMemDynNode {//内存链表节点结构体,bestfit动态内存管理第三部分
#ifdef LOSCFG_MEM_HEAD_BACKUP
    LosMemCtlNode backupNode;
#endif
    LosMemCtlNode selfNode;
} LosMemDynNode;

#ifdef LOSCFG_MEM_MUL_POOL
VOID *g_poolHead = NULL;
#endif

/* spinlock for mem module, only available on SMP mode */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_memSpin);

#ifdef LOSCFG_MEM_HEAD_BACKUP
/*
 * 0xffff0000U, 0xffffU
 * the taskID and moduleID multiplex the node->selfNode.freeNodeInfo.pstNext
 * the low 16 bits is the taskID and the high 16bits is the moduleID
 */
STATIC VOID OsMemNodeSave(LosMemDynNode *node);
#define OS_MEM_TASKID_SET(node, ID) do {                                                  \
    UINTPTR tmp_ = (UINTPTR)(((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext);   \
    tmp_ &= 0xffff0000U;                                                                  \
    tmp_ |= (ID);                                                                         \
    ((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext = (LOS_DL_LIST *)tmp_;       \
    OsMemNodeSave((LosMemDynNode *)(node));                                               \
} while (0)
#else
#define OS_MEM_TASKID_SET(node, ID) do {                                                  \
    UINTPTR tmp_ = (UINTPTR)(((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext);   \
    tmp_ &= 0xffff0000U;                                                                  \
    tmp_ |= (ID);                                                                         \
    ((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext = (LOS_DL_LIST *)tmp_;       \
} while (0)
#endif
#define OS_MEM_TASKID_GET(node) ((UINTPTR)(((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext) & 0xffffU)

#ifdef LOSCFG_MEM_MUL_MODULE
#define BITS_NUM_OF_TYPE_SHORT    16
#ifdef LOSCFG_MEM_HEAD_BACKUP
#define OS_MEM_MODID_SET(node, ID) do {                                                   \
    UINTPTR tmp_ = (UINTPTR)(((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext);   \
    tmp_ &= 0xffffU;                                                                      \
    tmp_ |= (ID) << BITS_NUM_OF_TYPE_SHORT;                                               \
    ((LosMemDynNode *)node)->selfNode.freeNodeInfo.pstNext = (LOS_DL_LIST *)tmp_;         \
    OsMemNodeSave((LosMemDynNode *)(node));                                               \
} while (0)
#else
#define OS_MEM_MODID_SET(node, ID) do {                                                   \
    UINTPTR tmp_ = (UINTPTR)(((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext);   \
    tmp_ &= 0xffffU;                                                                      \
    tmp_ |= (ID) << BITS_NUM_OF_TYPE_SHORT;                                               \
    ((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext = (LOS_DL_LIST *)tmp_;       \
} while (0)
#endif
#define OS_MEM_MODID_GET(node) \
    (((UINTPTR)(((LosMemDynNode *)(node))->selfNode.freeNodeInfo.pstNext) >> BITS_NUM_OF_TYPE_SHORT) & 0xffffU)
#endif

#define OS_MEM_ALIGN(p, alignSize) (((UINTPTR)(p) + (alignSize) - 1) & ~((UINTPTR)((alignSize) - 1)))
#define OS_MEM_NODE_HEAD_SIZE      sizeof(LosMemDynNode)	//内存池第三部分 一个节点的大小
/******************************************************************************
一个内存池最低内存大小是多少呢? 可以算出固定值,
第一部分: sizeof(LosMemPoolInfo)
第二部分: OS_DLNK_HEAD_SIZE
第三部分: 至少需要头尾两个节点 (2 * OS_MEM_NODE_HEAD_SIZE)
******************************************************************************/
#define OS_MEM_MIN_POOL_SIZE       (OS_DLNK_HEAD_SIZE + (2 * OS_MEM_NODE_HEAD_SIZE) + sizeof(LosMemPoolInfo))//成为一个内存池最低配置
#define IS_POW_TWO(value)          ((((UINTPTR)(value)) & ((UINTPTR)(value) - 1)) == 0)
#define POOL_ADDR_ALIGNSIZE        64
#ifdef LOSCFG_AARCH64
#define OS_MEM_ALIGN_SIZE 8	//8字节对齐方式 8*8 = 64
#else
#define OS_MEM_ALIGN_SIZE 4	//4字节对齐方式 4*8 = 64
#endif
#define OS_MEM_NODE_USED_FLAG             0x80000000U
#define OS_MEM_NODE_ALIGNED_FLAG          0x40000000U
#define OS_MEM_NODE_ALIGNED_AND_USED_FLAG (OS_MEM_NODE_USED_FLAG | OS_MEM_NODE_ALIGNED_FLAG) //0xB0000000U

#define OS_MEM_NODE_GET_ALIGNED_FLAG(sizeAndFlag) \
    ((sizeAndFlag) & OS_MEM_NODE_ALIGNED_FLAG) //获取对齐标签
#define OS_MEM_NODE_SET_ALIGNED_FLAG(sizeAndFlag) \
    ((sizeAndFlag) = ((sizeAndFlag) | OS_MEM_NODE_ALIGNED_FLAG)) //设置对齐标签
#define OS_MEM_NODE_GET_ALIGNED_GAPSIZE(sizeAndFlag) \
    ((sizeAndFlag) & ~OS_MEM_NODE_ALIGNED_FLAG) //通过增加Gap域确保返回的指针符合对齐要求
#define OS_MEM_NODE_GET_USED_FLAG(sizeAndFlag) \
    ((sizeAndFlag) & OS_MEM_NODE_USED_FLAG) //获取使用标签
#define OS_MEM_NODE_SET_USED_FLAG(sizeAndFlag) \
    ((sizeAndFlag) = ((sizeAndFlag) | OS_MEM_NODE_USED_FLAG)) //设置使用标签
#define OS_MEM_NODE_GET_SIZE(sizeAndFlag) \
    ((sizeAndFlag) & ~OS_MEM_NODE_ALIGNED_AND_USED_FLAG) //获得大小
#define OS_MEM_HEAD(pool, size) \
    OsDLnkMultiHead(OS_MEM_HEAD_ADDR(pool), size) //通过size找到链表头节点
#define OS_MEM_HEAD_ADDR(pool) \
    ((VOID *)((UINTPTR)(pool) + sizeof(LosMemPoolInfo))) //指向内存池的第二部分
#define OS_MEM_NEXT_NODE(node) \
    ((LosMemDynNode *)(VOID *)((UINT8 *)(node) + OS_MEM_NODE_GET_SIZE((node)->selfNode.sizeAndFlag)))//获取节点的下一个节点
#define OS_MEM_FIRST_NODE(pool) \
    ((LosMemDynNode *)(VOID *)((UINT8 *)OS_MEM_HEAD_ADDR(pool) + OS_DLNK_HEAD_SIZE)) //指向内存池的第三部分的头节点
#define OS_MEM_END_NODE(pool, size) \
    ((LosMemDynNode *)(VOID *)(((UINT8 *)(pool) + (size)) - OS_MEM_NODE_HEAD_SIZE)) //指向内存池的第三部分的最后一个节点
#define OS_MEM_MIDDLE_ADDR_OPEN_END(startAddr, middleAddr, endAddr) \
    (((UINT8 *)(startAddr) <= (UINT8 *)(middleAddr)) && ((UINT8 *)(middleAddr) < (UINT8 *)(endAddr))) //判断是否为中间地址,只包括等于开始地址
#define OS_MEM_MIDDLE_ADDR(startAddr, middleAddr, endAddr) \
    (((UINT8 *)(startAddr) <= (UINT8 *)(middleAddr)) && ((UINT8 *)(middleAddr) <= (UINT8 *)(endAddr))) //判断是否为中间地址,包括等于开始和末尾地址
#define OS_MEM_SET_MAGIC(value) \
    (value) = (LOS_DL_LIST *)(((UINTPTR)&(value)) ^ (UINTPTR)(-1)) //设置魔法数字
#define OS_MEM_MAGIC_VALID(value) \
    (((UINTPTR)(value) ^ ((UINTPTR)&(value))) == (UINTPTR)(-1))	//魔法数字是否有效,被修改过即无效
//变量前缀 uc:UINT8  us:UINT16 uw:UINT32 的意思
//auc是什么意思? 为何要用0,1来命名,就不能整个好听的名字吗？@note_thinking
UINT8 *m_aucSysMem0 = NULL; //异常接管内存池
UINT8 *m_aucSysMem1 = NULL;	//系统动态内存池 
#ifdef LOSCFG_BASE_MEM_NODE_SIZE_CHECK
STATIC UINT8 g_memCheckLevel = LOS_MEM_CHECK_LEVEL_DEFAULT;
#endif

#ifdef LOSCFG_MEM_MUL_MODULE
UINT32 g_moduleMemUsedSize[MEM_MODULE_MAX + 1] = { 0 };
#endif

VOID OsMemInfoPrint(VOID *pool);
#ifdef LOSCFG_BASE_MEM_NODE_SIZE_CHECK
const VOID *OsMemFindNodeCtrl(const VOID *pool, const VOID *ptr);
#endif
#ifdef LOSCFG_MEM_HEAD_BACKUP
#define CHECKSUM_MAGICNUM    0xDEADBEEF
#define OS_MEM_NODE_CHECKSUN_CALCULATE(ctlNode)    \
    (((UINTPTR)(ctlNode)->freeNodeInfo.pstPrev) ^  \
    ((UINTPTR)(ctlNode)->freeNodeInfo.pstNext) ^   \
    ((UINTPTR)(ctlNode)->preNode) ^                \
    (ctlNode)->gapSize ^                           \
    (ctlNode)->sizeAndFlag ^                       \
    CHECKSUM_MAGICNUM)
//打印节点信息
STATIC INLINE VOID OsMemDispCtlNode(const LosMemCtlNode *ctlNode)
{
    UINTPTR checksum;

    checksum = OS_MEM_NODE_CHECKSUN_CALCULATE(ctlNode);

    PRINTK("node:%p checksum=%p[%p] freeNodeInfo.pstPrev=%p "
           "freeNodeInfo.pstNext=%p preNode=%p gapSize=0x%x sizeAndFlag=0x%x\n",
           ctlNode,
           ctlNode->checksum,
           checksum,
           ctlNode->freeNodeInfo.pstPrev,
           ctlNode->freeNodeInfo.pstNext,
           ctlNode->preNode,
           ctlNode->gapSize,
           ctlNode->sizeAndFlag);
}

STATIC INLINE VOID OsMemDispMoreDetails(const LosMemDynNode *node)
{
    UINT32 taskID;
    LosTaskCB *taskCB = NULL;

    PRINT_ERR("************************************************\n");
    OsMemDispCtlNode(&node->selfNode);
    PRINT_ERR("the address of node :%p\n", node);

    if (!OS_MEM_NODE_GET_USED_FLAG(node->selfNode.sizeAndFlag)) {
        PRINT_ERR("this is a FREE node\n");
        PRINT_ERR("************************************************\n\n");
        return;
    }

    taskID = OS_MEM_TASKID_GET(node);
    if (OS_TID_CHECK_INVALID(taskID)) {
        PRINT_ERR("The task [ID: 0x%x] is ILLEGAL\n", taskID);
        if (taskID == g_taskMaxNum) {
            PRINT_ERR("PROBABLY alloc by SYSTEM INIT, NOT IN ANY TASK\n");
        }
        PRINT_ERR("************************************************\n\n");
        return;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    if (OsTaskIsUnused(taskCB) || (taskCB->taskEntry == NULL)) {
        PRINT_ERR("The task [ID: 0x%x] is NOT CREATED(ILLEGAL)\n", taskID);
        PRINT_ERR("************************************************\n\n");
        return;
    }

    PRINT_ERR("allocated by task: %s [ID = 0x%x]\n", taskCB->taskName, taskID);
#ifdef LOSCFG_MEM_MUL_MODULE
    PRINT_ERR("allocated by moduleID: %lu\n", OS_MEM_MODID_GET(node));
#endif

    PRINT_ERR("************************************************\n\n");
}

STATIC INLINE VOID OsMemDispWildPointerMsg(const LosMemDynNode *node, const VOID *ptr)
{
    PRINT_ERR("*****************************************************\n");
    PRINT_ERR("find an control block at: %p, gap size: 0x%x, sizeof(LosMemDynNode): 0x%x\n", node,
              node->selfNode.gapSize, sizeof(LosMemDynNode));
    PRINT_ERR("the pointer should be: %p\n",
              ((UINTPTR)node + node->selfNode.gapSize + sizeof(LosMemDynNode)));
    PRINT_ERR("the pointer given is: %p\n", ptr);
    PRINT_ERR("PROBABLY A WILD POINTER\n");
    OsBackTrace();
    PRINT_ERR("*****************************************************\n\n");
}

STATIC INLINE VOID OsMemChecksumSet(LosMemCtlNode *ctlNode)
{
    ctlNode->checksum = OS_MEM_NODE_CHECKSUN_CALCULATE(ctlNode);
}

STATIC INLINE BOOL OsMemChecksumVerify(const LosMemCtlNode *ctlNode)
{
    return ctlNode->checksum == OS_MEM_NODE_CHECKSUN_CALCULATE(ctlNode);
}

STATIC INLINE VOID OsMemBackupSetup(const LosMemDynNode *node)
{
    LosMemDynNode *nodePre = node->selfNode.preNode;
    if (nodePre != NULL) {
        nodePre->backupNode.freeNodeInfo.pstNext = node->selfNode.freeNodeInfo.pstNext;
        nodePre->backupNode.freeNodeInfo.pstPrev = node->selfNode.freeNodeInfo.pstPrev;
        nodePre->backupNode.preNode = node->selfNode.preNode;
        nodePre->backupNode.checksum = node->selfNode.checksum;
        nodePre->backupNode.gapSize = node->selfNode.gapSize;
#ifdef LOSCFG_MEM_RECORDINFO
        nodePre->backupNode.originSize = node->selfNode.originSize;
#endif
        nodePre->backupNode.sizeAndFlag = node->selfNode.sizeAndFlag;
    }
}

LosMemDynNode *OsMemNodeNextGet(const VOID *pool, const LosMemDynNode *node)
{
    const LosMemPoolInfo *poolInfo = (const LosMemPoolInfo *)pool;

    if (node == OS_MEM_END_NODE(pool, poolInfo->poolSize)) {
        return OS_MEM_FIRST_NODE(pool);
    } else {
        return OS_MEM_NEXT_NODE(node);
    }
}

STATIC INLINE UINT32 OsMemBackupSetup4Next(const VOID *pool, LosMemDynNode *node)
{
    LosMemDynNode *nodeNext = OsMemNodeNextGet(pool, node);

    if (!OsMemChecksumVerify(&nodeNext->selfNode)) {
        PRINT_ERR("[%s]the next node is broken!!\n", __FUNCTION__);
        OsMemDispCtlNode(&(nodeNext->selfNode));
        PRINT_ERR("Current node details:\n");
        OsMemDispMoreDetails(node);

        return LOS_NOK;
    }

    if (!OsMemChecksumVerify(&node->backupNode)) {
        node->backupNode.freeNodeInfo.pstNext = nodeNext->selfNode.freeNodeInfo.pstNext;
        node->backupNode.freeNodeInfo.pstPrev = nodeNext->selfNode.freeNodeInfo.pstPrev;
        node->backupNode.preNode = nodeNext->selfNode.preNode;
        node->backupNode.checksum = nodeNext->selfNode.checksum;
        node->backupNode.gapSize = nodeNext->selfNode.gapSize;
#ifdef LOSCFG_MEM_RECORDINFO
        node->backupNode.originSize = nodeNext->selfNode.originSize;
#endif
        node->backupNode.sizeAndFlag = nodeNext->selfNode.sizeAndFlag;
    }
    return LOS_OK;
}

UINT32 OsMemBackupDoRestore(VOID *pool, const LosMemDynNode *nodePre, LosMemDynNode *node)
{
    if (node == NULL) {
        PRINT_ERR("the node is NULL.\n");
        return LOS_NOK;
    }
    PRINT_ERR("the backup node information of current node in previous node:\n");
    OsMemDispCtlNode(&nodePre->backupNode);
    PRINT_ERR("the detailed information of previous node:\n");
    OsMemDispMoreDetails(nodePre);

    node->selfNode.freeNodeInfo.pstNext = nodePre->backupNode.freeNodeInfo.pstNext;
    node->selfNode.freeNodeInfo.pstPrev = nodePre->backupNode.freeNodeInfo.pstPrev;
    node->selfNode.preNode = nodePre->backupNode.preNode;
    node->selfNode.checksum = nodePre->backupNode.checksum;
    node->selfNode.gapSize = nodePre->backupNode.gapSize;
#ifdef LOSCFG_MEM_RECORDINFO
    node->selfNode.originSize = nodePre->backupNode.originSize;
#endif
    node->selfNode.sizeAndFlag = nodePre->backupNode.sizeAndFlag;

    /* we should re-setup next node's backup on current node */
    return OsMemBackupSetup4Next(pool, node);
}

STATIC LosMemDynNode *OsMemFirstNodePrevGet(const LosMemPoolInfo *poolInfo)
{
    LosMemDynNode *nodePre = NULL;

    nodePre = OS_MEM_END_NODE(poolInfo, poolInfo->poolSize);
    if (!OsMemChecksumVerify(&(nodePre->selfNode))) {
        PRINT_ERR("the current node is THE FIRST NODE !\n");
        PRINT_ERR("[%s]: the node information of previous node is bad !!\n", __FUNCTION__);
        OsMemDispCtlNode(&(nodePre->selfNode));
        return nodePre;
    }
    if (!OsMemChecksumVerify(&(nodePre->backupNode))) {
        PRINT_ERR("the current node is THE FIRST NODE !\n");
        PRINT_ERR("[%s]: the backup node information of current node in previous Node is bad !!\n", __FUNCTION__);
        OsMemDispCtlNode(&(nodePre->backupNode));
        return nodePre;
    }

    return NULL;
}

LosMemDynNode *OsMemNodePrevGet(VOID *pool, const LosMemDynNode *node)
{
    LosMemDynNode *nodeCur = NULL;
    LosMemDynNode *nodePre = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;

    if (node == OS_MEM_FIRST_NODE(pool)) {
        return OsMemFirstNodePrevGet(poolInfo);
    }

    for (nodeCur = OS_MEM_FIRST_NODE(pool);
         nodeCur < OS_MEM_END_NODE(pool, poolInfo->poolSize);
         nodeCur = OS_MEM_NEXT_NODE(nodeCur)) {
        if (!OsMemChecksumVerify(&(nodeCur->selfNode))) {
            PRINT_ERR("[%s]: the node information of current node is bad !!\n", __FUNCTION__);
            OsMemDispCtlNode(&(nodeCur->selfNode));

            if (nodePre == NULL) {
                return NULL;
            }

            PRINT_ERR("the detailed information of previous node:\n");
            OsMemDispMoreDetails(nodePre);

            /* due to the every step's checksum verify, nodePre is trustful */
            if (OsMemBackupDoRestore(pool, nodePre, nodeCur) != LOS_OK) {
                return NULL;
            }
        }

        if (!OsMemChecksumVerify(&(nodeCur->backupNode))) {
            PRINT_ERR("[%s]: the backup node information in current node is bad !!\n", __FUNCTION__);
            OsMemDispCtlNode(&(nodeCur->backupNode));

            if (nodePre != NULL) {
                PRINT_ERR("the detailed information of previous node:\n");
                OsMemDispMoreDetails(nodePre);
            }

            if (OsMemBackupSetup4Next(pool, nodeCur) != LOS_OK) {
                return NULL;
            }
        }

        if (OS_MEM_NEXT_NODE(nodeCur) == node) {
            return nodeCur;
        }

        if (OS_MEM_NEXT_NODE(nodeCur) > node) {
            break;
        }

        nodePre = nodeCur;
    }

    return NULL;
}

LosMemDynNode *OsMemNodePrevTryGet(VOID *pool, LosMemDynNode **node, const VOID *ptr)
{
    UINTPTR nodeShoudBe = 0;
    LosMemDynNode *nodeCur = NULL;
    LosMemDynNode *nodePre = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;

    if (ptr == OS_MEM_FIRST_NODE(pool)) {
        return OsMemFirstNodePrevGet(poolInfo);
    }

    for (nodeCur = OS_MEM_FIRST_NODE(pool);
         nodeCur < OS_MEM_END_NODE(pool, poolInfo->poolSize);
         nodeCur = OS_MEM_NEXT_NODE(nodeCur)) {
        if (!OsMemChecksumVerify(&(nodeCur->selfNode))) {
            PRINT_ERR("[%s]: the node information of current node is bad !!\n", __FUNCTION__);
            OsMemDispCtlNode(&(nodeCur->selfNode));

            if (nodePre == NULL) {
                return NULL;
            }

            PRINT_ERR("the detailed information of previous node:\n");
            OsMemDispMoreDetails(nodePre);

            /* due to the every step's checksum verify, nodePre is trustful */
            if (OsMemBackupDoRestore(pool, nodePre, nodeCur) != LOS_OK) {
                return NULL;
            }
        }

        if (!OsMemChecksumVerify(&(nodeCur->backupNode))) {
            PRINT_ERR("[%s]: the backup node information in current node is bad !!\n", __FUNCTION__);
            OsMemDispCtlNode(&(nodeCur->backupNode));

            if (nodePre != NULL) {
                PRINT_ERR("the detailed information of previous node:\n");
                OsMemDispMoreDetails(nodePre);
            }

            if (OsMemBackupSetup4Next(pool, nodeCur) != LOS_OK) {
                return NULL;
            }
        }

        nodeShoudBe = (UINTPTR)nodeCur + nodeCur->selfNode.gapSize + sizeof(LosMemDynNode);
        if (nodeShoudBe == (UINTPTR)ptr) {
            *node = nodeCur;
            return nodePre;
        }

        if (OS_MEM_NEXT_NODE(nodeCur) > (LosMemDynNode *)ptr) {
            OsMemDispWildPointerMsg(nodeCur, ptr);
            break;
        }

        nodePre = nodeCur;
    }

    return NULL;
}

STATIC INLINE UINT32 OsMemBackupTryRestore(VOID *pool, LosMemDynNode **node, const VOID *ptr)
{
    LosMemDynNode *nodeHead = NULL;
    LosMemDynNode *nodePre = OsMemNodePrevTryGet(pool, &nodeHead, ptr);
    if (nodePre == NULL) {
        return LOS_NOK;
    }

    *node = nodeHead;
    return OsMemBackupDoRestore(pool, nodePre, *node);
}

STATIC INLINE UINT32 OsMemBackupRestore(VOID *pool, LosMemDynNode *node)
{
    LosMemDynNode *nodePre = OsMemNodePrevGet(pool, node);
    if (nodePre == NULL) {
        return LOS_NOK;
    }

    return OsMemBackupDoRestore(pool, nodePre, node);
}

STATIC INLINE VOID OsMemSetGapSize(LosMemCtlNode *ctlNode, UINT32 gapSize)
{
    ctlNode->gapSize = gapSize;
}
//保存一个动态节点
STATIC VOID OsMemNodeSave(LosMemDynNode *node)
{
    OsMemSetGapSize(&node->selfNode, 0);
    OsMemChecksumSet(&node->selfNode);
    OsMemBackupSetup(node);
}

STATIC VOID OsMemNodeSaveWithGapSize(LosMemDynNode *node, UINT32 gapSize)
{
    OsMemSetGapSize(&node->selfNode, gapSize);
    OsMemChecksumSet(&node->selfNode);
    OsMemBackupSetup(node);
}

STATIC VOID OsMemListDelete(LOS_DL_LIST *node, const VOID *firstNode)
{
    LosMemDynNode *dynNode = NULL;

    node->pstNext->pstPrev = node->pstPrev;
    node->pstPrev->pstNext = node->pstNext;

    if ((VOID *)(node->pstNext) >= firstNode) {
        dynNode = LOS_DL_LIST_ENTRY(node->pstNext, LosMemDynNode, selfNode.freeNodeInfo);
        OsMemNodeSave(dynNode);
    }

    if ((VOID *)(node->pstPrev) >= firstNode) {
        dynNode = LOS_DL_LIST_ENTRY(node->pstPrev, LosMemDynNode, selfNode.freeNodeInfo);
        OsMemNodeSave(dynNode);
    }

    node->pstNext = NULL;
    node->pstPrev = NULL;

    dynNode = LOS_DL_LIST_ENTRY(node, LosMemDynNode, selfNode.freeNodeInfo);
    OsMemNodeSave(dynNode);
}

STATIC VOID OsMemListAdd(LOS_DL_LIST *listNode, LOS_DL_LIST *node, const VOID *firstNode)
{
    LosMemDynNode *dynNode = NULL;

    node->pstNext = listNode->pstNext;
    node->pstPrev = listNode;

    dynNode = LOS_DL_LIST_ENTRY(node, LosMemDynNode, selfNode.freeNodeInfo);
    OsMemNodeSave(dynNode);

    listNode->pstNext->pstPrev = node;
    if ((VOID *)(listNode->pstNext) >= firstNode) {
        dynNode = LOS_DL_LIST_ENTRY(listNode->pstNext, LosMemDynNode, selfNode.freeNodeInfo);
        OsMemNodeSave(dynNode);
    }

    listNode->pstNext = node;
}

VOID LOS_MemBadNodeShow(VOID *pool)
{
    LosMemDynNode *nodePre = NULL;
    LosMemDynNode *tmpNode = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    UINT32 intSave;

    if (pool == NULL) {
        return;
    }

    MEM_LOCK(intSave);

    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode < OS_MEM_END_NODE(pool, poolInfo->poolSize);
         tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        OsMemDispCtlNode(&tmpNode->selfNode);

        if (OsMemChecksumVerify(&tmpNode->selfNode)) {
            continue;
        }

        nodePre = OsMemNodePrevGet(pool, tmpNode);
        if (nodePre == NULL) {
            PRINT_ERR("the current node is invalid, but cannot find its previous Node\n");
            continue;
        }

        PRINT_ERR("the detailed information of previous node:\n");
        OsMemDispMoreDetails(nodePre);
    }

    MEM_UNLOCK(intSave);
    PRINTK("check finish\n");
}

#else  /* without LOSCFG_MEM_HEAD_BACKUP */
//删除一个节点
STATIC VOID OsMemListDelete(LOS_DL_LIST *node, const VOID *firstNode)
{
    (VOID)firstNode;
    LOS_ListDelete(node);
}
//增加一个节点
STATIC VOID OsMemListAdd(LOS_DL_LIST *listNode, LOS_DL_LIST *node, const VOID *firstNode)
{
    (VOID)firstNode;
    LOS_ListHeadInsert(listNode, node);
}

#endif

#ifdef LOSCFG_EXC_INTERACTION
LITE_OS_SEC_TEXT_INIT UINT32 OsMemExcInteractionInit(UINTPTR memStart)
{
    UINT32 ret;
    UINT32 poolSize;
    m_aucSysMem0 = (UINT8 *)((memStart + (POOL_ADDR_ALIGNSIZE - 1)) & ~((UINTPTR)(POOL_ADDR_ALIGNSIZE - 1)));
    poolSize = OS_EXC_INTERACTMEM_SIZE;
    ret = LOS_MemInit(m_aucSysMem0, poolSize);
    PRINT_INFO("LiteOS kernel exc interaction memory address:%p,size:0x%x\n", m_aucSysMem0, poolSize);
    return ret;
}
#endif

LITE_OS_SEC_TEXT_INIT UINT32 OsMemSystemInit(UINTPTR memStart)
{
    UINT32 ret;
    UINT32 poolSize;

    m_aucSysMem1 = (UINT8 *)((memStart + (POOL_ADDR_ALIGNSIZE - 1)) & ~((UINTPTR)(POOL_ADDR_ALIGNSIZE - 1)));
    poolSize = OS_SYS_MEM_SIZE;
    ret = LOS_MemInit(m_aucSysMem1, poolSize);////初始化 m_aucSysMem1 内存池
    PRINT_INFO("LiteOS system heap memory address:%p,size:0x%x\n", m_aucSysMem1, poolSize);
#ifndef LOSCFG_EXC_INTERACTION
    m_aucSysMem0 = m_aucSysMem1;
#endif
    return ret;
}

#ifdef LOSCFG_MEM_LEAKCHECK //内存泄漏开关
STATIC INLINE VOID OsMemLinkRegisterRecord(LosMemDynNode *node)
{
    UINT32 count = 0;
    UINT32 index = 0;
    UINTPTR framePtr, tmpFramePtr, linkReg;

    (VOID)memset_s(node->selfNode.linkReg, (LOS_RECORD_LR_CNT * sizeof(UINTPTR)), 0,
        (LOS_RECORD_LR_CNT * sizeof(UINTPTR)));
    framePtr = Get_Fp();
    while ((framePtr > OS_SYS_FUNC_ADDR_START) && (framePtr < OS_SYS_FUNC_ADDR_END)) {
        tmpFramePtr = framePtr;
#ifdef __LP64__
        framePtr = *(UINTPTR *)framePtr;
        linkReg = *(UINTPTR *)(tmpFramePtr + sizeof(UINTPTR));
#else
        linkReg = *(UINTPTR *)framePtr;
        framePtr = *(UINTPTR *)(tmpFramePtr - sizeof(UINTPTR));
#endif
        if (index >= LOS_OMIT_LR_CNT) {
            node->selfNode.linkReg[count++] = linkReg;
            if (count == LOS_RECORD_LR_CNT) {
                break;
            }
        }
        index++;
    }
}
#endif

#define OS_CHECK_NULL_RETURN(param) do {              \
    if ((param) == NULL) {                            \
        PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__); \
        return;                                       \
    }                                                 \
} while (0);

/*
 * Description : find suitable free block use "best fit" algorithm
 * Input       : pool      --- Pointer to memory pool
 *               allocSize --- Size of memory in bytes which note need allocate
 * Return      : NULL      --- no suitable block found
 *               tmpNode   --- pointer a suitable free block
 */	//使用最佳适应算法从内存池中找到空闲节点
STATIC INLINE LosMemDynNode *OsMemFindSuitableFreeBlock(VOID *pool, UINT32 allocSize)
{
    LOS_DL_LIST *listNodeHead = NULL;
    LosMemDynNode *tmpNode = NULL;
    UINT32 maxCount = (LOS_MemPoolSizeGet(pool) / allocSize) << 1;//循环退出条件
    UINT32 count;
#ifdef LOSCFG_MEM_HEAD_BACKUP
    UINT32 ret = LOS_OK;
#endif
    for (listNodeHead = OS_MEM_HEAD(pool, allocSize); listNodeHead != NULL;//从最匹配自身链表一直开始找,自身链表不满足时找下一级链表
         listNodeHead = OsDLnkNextMultiHead(OS_MEM_HEAD_ADDR(pool), listNodeHead)) {//因为下一级的内存块比匹配自身链表要大
        count = 0;
        LOS_DL_LIST_FOR_EACH_ENTRY(tmpNode, listNodeHead, LosMemDynNode, selfNode.freeNodeInfo) {//遍历链表
            if (count++ >= maxCount) {
                PRINT_ERR("[%s:%d]node: execute too much time\n", __FUNCTION__, __LINE__);
                break;
            }

#ifdef LOSCFG_MEM_HEAD_BACKUP
            if (!OsMemChecksumVerify(&tmpNode->selfNode)) {
                PRINT_ERR("[%s]: the node information of current node is bad !!\n", __FUNCTION__);
                OsMemDispCtlNode(&tmpNode->selfNode);
                ret = OsMemBackupRestore(pool, tmpNode);
            }
            if (ret != LOS_OK) {
                break;
            }
#endif

            if (((UINTPTR)tmpNode & (OS_MEM_ALIGN_SIZE - 1)) != 0) {
                LOS_Panic("[%s:%d]Mem node data error:OS_MEM_HEAD_ADDR(pool)=%p, listNodeHead:%p,"
                          "allocSize=%u, tmpNode=%p\n",
                          __FUNCTION__, __LINE__, OS_MEM_HEAD_ADDR(pool), listNodeHead, allocSize, tmpNode);
                break;
            }
            if (tmpNode->selfNode.sizeAndFlag >= allocSize) {//找到节点
                return tmpNode;
            }
        }
    }

    return NULL;
}

/*
 * Description : clear a mem node, set every member to NULL
 * Input       : node    --- Pointer to the mem node which will be cleared up
 */ //清空指定动态内存节点
STATIC INLINE VOID OsMemClearNode(LosMemDynNode *node)
{
    (VOID)memset_s((VOID *)node, sizeof(LosMemDynNode), 0, sizeof(LosMemDynNode));
}

/*
 * Description : merge this node and pre node, then clear this node info
 * Input       : node    --- Pointer to node which will be merged
 */ //将指定动态内存节点和前节点合并
STATIC INLINE VOID OsMemMergeNode(LosMemDynNode *node)
{
    LosMemDynNode *nextNode = NULL;

    node->selfNode.preNode->selfNode.sizeAndFlag += node->selfNode.sizeAndFlag;//前节点大小变大
    nextNode = (LosMemDynNode *)((UINTPTR)node + node->selfNode.sizeAndFlag);//找到node的后节点
    nextNode->selfNode.preNode = node->selfNode.preNode;//后节点指向前节点,如此完成了合并
#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(node->selfNode.preNode);
    OsMemNodeSave(nextNode);
#endif
    OsMemClearNode(node);//清空节点
}
//是否是一个扩展内存池
STATIC INLINE BOOL IsExpandPoolNode(VOID *pool, LosMemDynNode *node)
{
    UINTPTR start = (UINTPTR)pool;
    UINTPTR end = start + ((LosMemPoolInfo *)pool)->poolSize;
    return ((UINTPTR)node < start) || ((UINTPTR)node > end);
}

/*
 * Description : split new node from allocNode, and merge remainder mem if necessary
 * Input       : pool      -- Pointer to memory pool
 *               allocNode -- the source node which new node be spit from to.
 *                            After pick up it's node info, change to point the new node
 *               allocSize -- the size of new node
 * Output      : allocNode -- save new node addr
 */	//从allocNode拆分新节点，必要时合并剩余的内存
STATIC INLINE VOID OsMemSplitNode(VOID *pool,
                                  LosMemDynNode *allocNode, UINT32 allocSize)
{
    LosMemDynNode *newFreeNode = NULL;
    LosMemDynNode *nextNode = NULL;
    LOS_DL_LIST *listNodeHead = NULL;
    const VOID *firstNode = (const VOID *)OS_MEM_FIRST_NODE(pool);

    newFreeNode = (LosMemDynNode *)(VOID *)((UINT8 *)allocNode + allocSize);
    newFreeNode->selfNode.preNode = allocNode;
    newFreeNode->selfNode.sizeAndFlag = allocNode->selfNode.sizeAndFlag - allocSize;
    allocNode->selfNode.sizeAndFlag = allocSize;
    nextNode = OS_MEM_NEXT_NODE(newFreeNode);
    nextNode->selfNode.preNode = newFreeNode;
    if (!OS_MEM_NODE_GET_USED_FLAG(nextNode->selfNode.sizeAndFlag)) {
        OsMemListDelete(&nextNode->selfNode.freeNodeInfo, firstNode);
        OsMemMergeNode(nextNode);
#ifdef LOSCFG_MEM_HEAD_BACKUP
    } else {
        OsMemNodeSave(nextNode);
#endif
    }
    listNodeHead = OS_MEM_HEAD(pool, newFreeNode->selfNode.sizeAndFlag);
    OS_CHECK_NULL_RETURN(listNodeHead);
    /* add expand node to tail to make sure origin pool used first */
    if (IsExpandPoolNode(pool, newFreeNode)) {
        OsMemListAdd(listNodeHead->pstPrev, &newFreeNode->selfNode.freeNodeInfo, firstNode);
    } else {
        OsMemListAdd(listNodeHead, &newFreeNode->selfNode.freeNodeInfo, firstNode);
    }

#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(newFreeNode);
#endif
}

STATIC INLINE LosMemDynNode *PreSentinelNodeGet(const VOID *pool, const LosMemDynNode *node);
STATIC INLINE BOOL OsMemIsLastSentinelNode(LosMemDynNode *node);
//释放大节点内存块,方法是直接释放物理内存
UINT32 OsMemLargeNodeFree(const VOID *ptr)
{
    LosVmPage *page = OsVmVaddrToPage((VOID *)ptr);
    if ((page == NULL) || (page->nPages == 0)) {
        return LOS_NOK;
    }
    LOS_PhysPagesFreeContiguous((VOID *)ptr, page->nPages);

    return LOS_OK;
}
//尝试缩小内存池
STATIC INLINE BOOL TryShrinkPool(const VOID *pool, const LosMemDynNode *node)
{
    LosMemDynNode *mySentinel = NULL;
    LosMemDynNode *preSentinel = NULL;
    size_t totalSize = (UINTPTR)node->selfNode.preNode - (UINTPTR)node;
    size_t nodeSize = OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag);

    if (nodeSize != totalSize) {
        return FALSE;
    }

    preSentinel = PreSentinelNodeGet(pool, node);
    if (preSentinel == NULL) {
        return FALSE;
    }

    mySentinel = node->selfNode.preNode;
    if (OsMemIsLastSentinelNode(mySentinel)) {
        preSentinel->selfNode.sizeAndFlag = OS_MEM_NODE_USED_FLAG;
        preSentinel->selfNode.freeNodeInfo.pstNext = NULL;
    } else {
        preSentinel->selfNode.sizeAndFlag = mySentinel->selfNode.sizeAndFlag;
        preSentinel->selfNode.freeNodeInfo.pstNext = mySentinel->selfNode.freeNodeInfo.pstNext;
    }
    if (OsMemLargeNodeFree(node) != LOS_OK) {
        PRINT_ERR("TryShrinkPool free %p failed!\n", node);
        return FALSE;
    }

    return TRUE;
}

/*
 * Description : free the node from memory & if there are free node beside, merger them.
 *               at last update "listNodeHead' which saved all free node control head
 * Input       : node -- the node which need be freed
 *               pool -- Pointer to memory pool
 */	//从内存中释放节点,如果旁边有空闲节点，则合并它们,最后更新listNodeHead，它保存了所有空闲节点控制头
STATIC INLINE VOID OsMemFreeNode(LosMemDynNode *node, VOID *pool)
{
    LosMemDynNode *preNode = NULL;
    LosMemDynNode *nextNode = NULL;
    LOS_DL_LIST *listNodeHead = NULL;
    const VOID *firstNode = (const VOID *)OS_MEM_FIRST_NODE(pool);
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;

#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    poolInfo->poolCurUsedSize -= OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag);
#endif
    if ((pool == (VOID *)OS_SYS_MEM_ADDR) || (pool == (VOID *)m_aucSysMem0)) {
        OS_MEM_REDUCE_USED(OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag), OS_MEM_TASKID_GET(node));
    }

    node->selfNode.sizeAndFlag = OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag);
#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(node);
#endif
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(node);
#endif
    preNode = node->selfNode.preNode; /* merage preNode */
    if ((preNode != NULL) && !OS_MEM_NODE_GET_USED_FLAG(preNode->selfNode.sizeAndFlag)) {
        OsMemListDelete(&(preNode->selfNode.freeNodeInfo), firstNode);
        OsMemMergeNode(node);
        node  = preNode;
    }

    nextNode = OS_MEM_NEXT_NODE(node); /* merage nextNode */
    if ((nextNode != NULL) && !OS_MEM_NODE_GET_USED_FLAG(nextNode->selfNode.sizeAndFlag)) {
        OsMemListDelete(&nextNode->selfNode.freeNodeInfo, firstNode);
        OsMemMergeNode(nextNode);
    }

    if (poolInfo->flag & MEM_POOL_EXPAND_ENABLE) {
        /* if this is a expand head node, and all unused, free it to pmm */
        if ((node->selfNode.preNode > node) && (node != firstNode)) {
            if (TryShrinkPool(pool, node)) {
                return;
            }
        }
    }

    listNodeHead = OS_MEM_HEAD(pool, node->selfNode.sizeAndFlag);
    OS_CHECK_NULL_RETURN(listNodeHead);
    /* add expand node to tail to make sure origin pool used first */
    if (IsExpandPoolNode(pool, node)) {
        OsMemListAdd(listNodeHead->pstPrev, &node->selfNode.freeNodeInfo, firstNode);
    } else {
        OsMemListAdd(listNodeHead, &node->selfNode.freeNodeInfo, firstNode);
    }
}

/*
 * Description : check the result if pointer memory node belongs to pointer memory pool
 * Input       : pool -- Pointer to memory pool
 *               node -- the node which need be checked
 * Return      : LOS_OK or LOS_NOK
 */
#ifdef LOS_DLNK_SAFE_CHECK
STATIC INLINE UINT32 OsMemCheckUsedNode(const VOID *pool, const LosMemDynNode *node)
{
    LosMemDynNode *tmpNode = OS_MEM_FIRST_NODE(pool);
    const LosMemPoolInfo *poolInfo = (const LosMemPoolInfo *)pool;
    LosMemDynNode *endNode = (const LosMemDynNode *)OS_MEM_END_NODE(pool, poolInfo->poolSize);

    do {
        for (; tmpNode < endNode; tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
            if ((tmpNode == node) &&
                OS_MEM_NODE_GET_USED_FLAG(tmpNode->selfNode.sizeAndFlag)) {
                return LOS_OK;
            }
        }

        if (OsMemIsLastSentinelNode(endNode) == FALSE) {
            tmpNode = OsMemSentinelNodeGet(endNode);
            endNode = OS_MEM_END_NODE(tmpNode, OS_MEM_NODE_GET_SIZE(endNode->selfNode.sizeAndFlag));
        } else {
            break;
        }
    } while (1);

    return LOS_NOK;
}

#elif defined(LOS_DLNK_SIMPLE_CHECK)
STATIC INLINE UINT32 OsMemCheckUsedNode(const VOID *pool, const LosMemDynNode *node)
{
    const LosMemPoolInfo *poolInfo = (const LosMemPoolInfo *)pool;
    const LosMemDynNode *startNode = (const LosMemDynNode *)OS_MEM_FIRST_NODE(pool);
    const LosMemDynNode *endNode = (const LosMemDynNode *)OS_MEM_END_NODE(pool, poolInfo->poolSize);
    if (!OS_MEM_MIDDLE_ADDR_OPEN_END(startNode, node, endNode)) {
        return LOS_NOK;
    }

    if (!OS_MEM_NODE_GET_USED_FLAG(node->selfNode.sizeAndFlag)) {
        return LOS_NOK;
    }

    if (!OS_MEM_MAGIC_VALID(node->selfNode.freeNodeInfo.pstPrev)) {
        return LOS_NOK;
    }

    return LOS_OK;
}

#else
STATIC LosMemDynNode *OsMemLastSentinelNodeGet(LosMemDynNode *sentinelNode)
{
    LosMemDynNode *node = NULL;
    VOID *ptr = sentinelNode->selfNode.freeNodeInfo.pstNext;
    UINT32 size = OS_MEM_NODE_GET_SIZE(sentinelNode->selfNode.sizeAndFlag);

    while ((ptr != NULL) && (size != 0)) {
        node = OS_MEM_END_NODE(ptr, size);
        ptr = node->selfNode.freeNodeInfo.pstNext;
        size = OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag);
    }

    return node;
}

STATIC INLINE BOOL OsMemSentinelNodeCheck(LosMemDynNode *node)
{
    if (!OS_MEM_NODE_GET_USED_FLAG(node->selfNode.sizeAndFlag)) {
        return FALSE;
    }

    if (!OS_MEM_MAGIC_VALID(node->selfNode.freeNodeInfo.pstPrev)) {
        return FALSE;
    }

    return TRUE;
}

STATIC INLINE BOOL OsMemIsLastSentinelNode(LosMemDynNode *node)
{
    if (OsMemSentinelNodeCheck(node) == FALSE) {
        PRINT_ERR("%s %d, The current sentinel node is invalid\n", __FUNCTION__, __LINE__);
        return TRUE;
    }

    if ((OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag) == 0) ||
        (node->selfNode.freeNodeInfo.pstNext == NULL)) {
        return TRUE;
    } else {
        return FALSE;
    }
}
//将指定节点设置有哨兵节点
STATIC INLINE VOID OsMemSentinelNodeSet(LosMemDynNode *sentinelNode, VOID *newNode, UINT32 size)
{
    if (sentinelNode->selfNode.freeNodeInfo.pstNext != NULL) {
        sentinelNode = OsMemLastSentinelNodeGet(sentinelNode);
    }

    OS_MEM_SET_MAGIC(sentinelNode->selfNode.freeNodeInfo.pstPrev);
    sentinelNode->selfNode.sizeAndFlag = size;
    OS_MEM_NODE_SET_USED_FLAG(sentinelNode->selfNode.sizeAndFlag);
    sentinelNode->selfNode.freeNodeInfo.pstNext = newNode;
}

STATIC INLINE VOID *OsMemSentinelNodeGet(LosMemDynNode *node)
{
    if (OsMemSentinelNodeCheck(node) == FALSE) {
        return NULL;
    }

    return node->selfNode.freeNodeInfo.pstNext;
}

STATIC INLINE LosMemDynNode *PreSentinelNodeGet(const VOID *pool, const LosMemDynNode *node)
{
    UINT32 nextSize;
    LosMemDynNode *nextNode = NULL;
    LosMemDynNode *sentinelNode = NULL;

    sentinelNode = OS_MEM_END_NODE(pool, ((LosMemPoolInfo *)pool)->poolSize);
    while (sentinelNode != NULL) {
        if (OsMemIsLastSentinelNode(sentinelNode)) {
            PRINT_ERR("PreSentinelNodeGet can not find node %p\n", node);
            return NULL;
        }
        nextNode = OsMemSentinelNodeGet(sentinelNode);
        if (nextNode == node) {
            return sentinelNode;
        }
        nextSize = OS_MEM_NODE_GET_SIZE(sentinelNode->selfNode.sizeAndFlag);
        sentinelNode = OS_MEM_END_NODE(nextNode, nextSize);
    }

    return NULL;
}

BOOL OsMemIsHeapNode(const VOID *ptr)
{
    LosMemPoolInfo *pool = (LosMemPoolInfo *)m_aucSysMem1;
    LosMemDynNode *firstNode = OS_MEM_FIRST_NODE(pool);
    LosMemDynNode *endNode = OS_MEM_END_NODE(pool, pool->poolSize);
    UINT32 intSave;
    UINT32 size;

    if (OS_MEM_MIDDLE_ADDR(firstNode, ptr, endNode)) {
        return TRUE;
    }

    MEM_LOCK(intSave);
    while (OsMemIsLastSentinelNode(endNode) == FALSE) {
        size = OS_MEM_NODE_GET_SIZE(endNode->selfNode.sizeAndFlag);
        firstNode = OsMemSentinelNodeGet(endNode);
        endNode = OS_MEM_END_NODE(firstNode, size);
        if (OS_MEM_MIDDLE_ADDR(firstNode, ptr, endNode)) {
            MEM_UNLOCK(intSave);
            return TRUE;
        }
    }
    MEM_UNLOCK(intSave);

    return FALSE;
}

STATIC BOOL OsMemAddrValidCheck(const LosMemPoolInfo *pool, const VOID *addr)
{
    UINT32 size;
    LosMemDynNode *node = NULL;
    LosMemDynNode *sentinel = NULL;

    size = ((LosMemPoolInfo *)pool)->poolSize;
    if (OS_MEM_MIDDLE_ADDR_OPEN_END(pool + 1, addr, (UINTPTR)pool + size)) {
        return TRUE;
    }

    sentinel = OS_MEM_END_NODE(pool, size);
    while (OsMemIsLastSentinelNode(sentinel) == FALSE) {
        size = OS_MEM_NODE_GET_SIZE(sentinel->selfNode.sizeAndFlag);
        node = OsMemSentinelNodeGet(sentinel);
        sentinel = OS_MEM_END_NODE(node, size);
        if (OS_MEM_MIDDLE_ADDR_OPEN_END(node, addr, (UINTPTR)node + size)) {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC INLINE BOOL OsMemIsNodeValid(const LosMemDynNode *node, const LosMemDynNode *startNode,
                                    const LosMemDynNode *endNode,
                                    const LosMemPoolInfo *poolInfo)
{
    if (!OS_MEM_MIDDLE_ADDR(startNode, node, endNode)) {
        return FALSE;
    }

    if (OS_MEM_NODE_GET_USED_FLAG(node->selfNode.sizeAndFlag)) {
        if (!OS_MEM_MAGIC_VALID(node->selfNode.freeNodeInfo.pstPrev)) {
            return FALSE;
        }
        return TRUE;
    }

    if (!OsMemAddrValidCheck(poolInfo, node->selfNode.freeNodeInfo.pstPrev)) {
        return FALSE;
    }

    return TRUE;
}

STATIC INLINE UINT32 OsMemCheckUsedNode(const VOID *pool, const LosMemDynNode *node)
{
    const LosMemPoolInfo *poolInfo = (const LosMemPoolInfo *)pool;
    LosMemDynNode *startNode = (LosMemDynNode *)OS_MEM_FIRST_NODE(pool);
    LosMemDynNode *endNode = (LosMemDynNode *)OS_MEM_END_NODE(pool, poolInfo->poolSize);
    LosMemDynNode *nextNode = NULL;
    BOOL doneFlag = FALSE;

    do {
        do {
            if (!OsMemIsNodeValid(node, startNode, endNode, poolInfo)) {
                break;
            }

            if (!OS_MEM_NODE_GET_USED_FLAG(node->selfNode.sizeAndFlag)) {
                break;
            }

            nextNode = OS_MEM_NEXT_NODE(node);
            if (!OsMemIsNodeValid(nextNode, startNode, endNode, poolInfo)) {
                break;
            }

            if (nextNode->selfNode.preNode != node) {
                break;
            }

            if ((node != startNode) &&
                ((!OsMemIsNodeValid(node->selfNode.preNode, startNode, endNode, poolInfo)) ||
                (OS_MEM_NEXT_NODE(node->selfNode.preNode) != node))) {
                break;
            }
            doneFlag = TRUE;
        } while (0);

        if (!doneFlag) {
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                startNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(startNode, OS_MEM_NODE_GET_SIZE(endNode->selfNode.sizeAndFlag));
                continue;
            }
            return LOS_NOK;
        }
    } while (!doneFlag);

    return LOS_OK;
}

#endif

/*
 * Description : set magic & taskid
 * Input       : node -- the node which will be set magic & taskid
 */	//给指定节点设置魔法数字和任务ID
STATIC INLINE VOID OsMemSetMagicNumAndTaskID(LosMemDynNode *node)
{
    LosTaskCB *runTask = OsCurrTaskGet();

    OS_MEM_SET_MAGIC(node->selfNode.freeNodeInfo.pstPrev);//通过pstPrev生成魔法数字也是挺赞的! @note_good

    /*
     * If the operation occured before task initialization(runTask was not assigned)
     * or in interrupt, make the value of taskid of node to 0xffffffff
     */
    if ((runTask != NULL) && OS_INT_INACTIVE) {//当前没有中断发生时
        OS_MEM_TASKID_SET(node, runTask->taskID);//设置节点的使用任务
    } else {//发生中断的情况 @note_why 为何中断发生时不能设置任务?
        /* If the task mode does not initialize, the field is the 0xffffffff */
        node->selfNode.freeNodeInfo.pstNext = (LOS_DL_LIST *)OS_NULL_INT;//如果任务模式没有初始化,设置为 0xffffffff
    }
}

LITE_OS_SEC_TEXT_MINOR STATIC INLINE UINT32 OsMemPoolDlinkcheck(const LosMemPoolInfo *pool, LOS_DL_LIST listHead)
{
    if (!OsMemAddrValidCheck(pool, listHead.pstPrev) ||
        !OsMemAddrValidCheck(pool, listHead.pstNext)) {
        return LOS_NOK;
    }

    if (!IS_ALIGNED(listHead.pstPrev, sizeof(VOID *)) ||
        !IS_ALIGNED(listHead.pstNext, sizeof(VOID *))) {
        return LOS_NOK;
    }

    return LOS_OK;
}

/*
 * Description : show mem pool header info
 * Input       : pool --Pointer to memory pool
 */
LITE_OS_SEC_TEXT_MINOR VOID OsMemPoolHeadInfoPrint(const VOID *pool)
{
    const LosMemPoolInfo *poolInfo = (const LosMemPoolInfo *)pool;
    UINT32 dlinkNum;
    UINT32 flag = 0;
    const LosMultipleDlinkHead *dlinkHead = NULL;

    if (!IS_ALIGNED(poolInfo, sizeof(VOID *))) {
        PRINT_ERR("wrong mem pool addr: %p, func:%s,line:%d\n", poolInfo, __FUNCTION__, __LINE__);
#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("wrong mem pool addr: %p, func:%s,line:%d\n", poolInfo, __FUNCTION__, __LINE__);
#endif
        return;
    }

    dlinkHead = (const LosMultipleDlinkHead *)(VOID *)(poolInfo + 1);
    for (dlinkNum = 0; dlinkNum < OS_MULTI_DLNK_NUM; dlinkNum++) {
        if (OsMemPoolDlinkcheck(pool, dlinkHead->listHead[dlinkNum])) {
            flag = 1;
            PRINT_ERR("DlinkHead[%u]: pstPrev:%p, pstNext:%p\n",
                      dlinkNum, dlinkHead->listHead[dlinkNum].pstPrev, dlinkHead->listHead[dlinkNum].pstNext);
#ifdef LOSCFG_SHELL_EXCINFO
            WriteExcInfoToBuf("DlinkHead[%u]: pstPrev:%p, pstNext:%p\n",
                              dlinkNum, dlinkHead->listHead[dlinkNum].pstPrev, dlinkHead->listHead[dlinkNum].pstNext);
#endif
        }
    }
    if (flag) {
        PRINTK("mem pool info: poolAddr:%p, poolSize:0x%x\n", poolInfo->pool, poolInfo->poolSize);
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
        PRINTK("mem pool info: poolWaterLine:0x%x, poolCurUsedSize:0x%x\n", poolInfo->poolWaterLine,
               poolInfo->poolCurUsedSize);
#endif

#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("mem pool info: poolAddr:%p, poolSize:0x%x\n", poolInfo->pool, poolInfo->poolSize);
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
        WriteExcInfoToBuf("mem pool info: poolWaterLine:0x%x, poolCurUsedSize:0x%x\n",
                          poolInfo->poolWaterLine, poolInfo->poolCurUsedSize);
#endif
#endif
    }
}

STATIC VOID OsMemMagicCheckPrint(LosMemDynNode **tmpNode)
{
    PRINT_ERR("[%s], %d, memory check error!\n"
              "memory used but magic num wrong, freeNodeInfo.pstPrev(magic num):%p \n",
              __FUNCTION__, __LINE__, (*tmpNode)->selfNode.freeNodeInfo.pstPrev);
#ifdef LOSCFG_SHELL_EXCINFO
    WriteExcInfoToBuf("[%s], %d, memory check error!\n"
                      "memory used but magic num wrong, freeNodeInfo.pstPrev(magic num):%p \n",
                      __FUNCTION__, __LINE__, (*tmpNode)->selfNode.freeNodeInfo.pstPrev);
#endif
}

STATIC UINT32 OsMemAddrValidCheckPrint(const VOID *pool, const LosMemDynNode *endPool, LosMemDynNode **tmpNode)
{
    if (!OsMemAddrValidCheck(pool, (*tmpNode)->selfNode.freeNodeInfo.pstPrev)) {
        PRINT_ERR("[%s], %d, memory check error!\n"
                  " freeNodeInfo.pstPrev:%p is out of legal mem range[%p, %p]\n",
                  __FUNCTION__, __LINE__, (*tmpNode)->selfNode.freeNodeInfo.pstPrev, pool, endPool);
#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("[%s], %d, memory check error!\n"
                          " freeNodeInfo.pstPrev:%p is out of legal mem range[%p, %p]\n",
                          __FUNCTION__, __LINE__, (*tmpNode)->selfNode.freeNodeInfo.pstPrev, pool, endPool);
#endif
        return LOS_NOK;
    }
    if (!OsMemAddrValidCheck(pool, (*tmpNode)->selfNode.freeNodeInfo.pstNext)) {
        PRINT_ERR("[%s], %d, memory check error!\n"
                  " freeNodeInfo.pstNext:%p is out of legal mem range[%p, %p]\n",
                  __FUNCTION__, __LINE__, (*tmpNode)->selfNode.freeNodeInfo.pstNext, pool, endPool);
#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("[%s], %d, memory check error!\n"
                          " freeNodeInfo.pstNext:%p is out of legal mem range[%p, %p]\n",
                          __FUNCTION__, __LINE__, (*tmpNode)->selfNode.freeNodeInfo.pstNext, pool, endPool);
#endif
        return LOS_NOK;
    }
    return LOS_OK;
}

STATIC UINT32 DoOsMemIntegrityCheck(LosMemDynNode **tmpNode, const VOID *pool, const LosMemDynNode *endPool)
{
    if (OS_MEM_NODE_GET_USED_FLAG((*tmpNode)->selfNode.sizeAndFlag)) {
        if (!OS_MEM_MAGIC_VALID((*tmpNode)->selfNode.freeNodeInfo.pstPrev)) {
            OsMemMagicCheckPrint(tmpNode);
            return LOS_NOK;
        }
    } else { /* is free node, check free node range */
        if (OsMemAddrValidCheckPrint(pool, endPool, tmpNode) == LOS_NOK) {
            return LOS_NOK;
        }
    }
    return LOS_OK;
}
//内存池完整性检查
STATIC UINT32 OsMemIntegrityCheck(const VOID *pool, LosMemDynNode **tmpNode, LosMemDynNode **preNode)
{
    const LosMemPoolInfo *poolInfo = (const LosMemPoolInfo *)pool;
    LosMemDynNode *endPool = (LosMemDynNode *)((UINT8 *)pool + poolInfo->poolSize - OS_MEM_NODE_HEAD_SIZE);

    OsMemPoolHeadInfoPrint(pool);
    *preNode = OS_MEM_FIRST_NODE(pool);

    do {
        for (*tmpNode = *preNode; *tmpNode < endPool; *tmpNode = OS_MEM_NEXT_NODE(*tmpNode)) {
            if (DoOsMemIntegrityCheck(tmpNode, pool, endPool) == LOS_NOK) {
                return LOS_NOK;
            }
            *preNode = *tmpNode;
        }

        if (OsMemIsLastSentinelNode(*tmpNode) == FALSE) {
            *preNode = OsMemSentinelNodeGet(*tmpNode);
            endPool = OS_MEM_END_NODE(*preNode, OS_MEM_NODE_GET_SIZE((*tmpNode)->selfNode.sizeAndFlag));
        } else {
            break;
        }
    } while (1);

    return LOS_OK;
}

#ifdef LOSCFG_MEM_LEAKCHECK //内存泄漏检查开关
STATIC VOID OsMemNodeBacktraceInfo(const LosMemDynNode *tmpNode,
                                   const LosMemDynNode *preNode)
{
    int i;
    PRINTK("\n broken node head LR info: \n");
    for (i = 0; i < LOS_RECORD_LR_CNT; i++) {
        PRINTK(" LR[%d]:%p\n", i, tmpNode->selfNode.linkReg[i]);
    }
    PRINTK("\n pre node head LR info: \n");
    for (i = 0; i < LOS_RECORD_LR_CNT; i++) {
        PRINTK(" LR[%d]:%p\n", i, preNode->selfNode.linkReg[i]);
    }

#ifdef LOSCFG_SHELL_EXCINFO
    WriteExcInfoToBuf("\n broken node head LR info: \n");
    for (i = 0; i < LOS_RECORD_LR_CNT; i++) {
        WriteExcInfoToBuf("LR[%d]:%p\n", i, tmpNode->selfNode.linkReg[i]);
    }
    WriteExcInfoToBuf("\n pre node head LR info: \n");
    for (i = 0; i < LOS_RECORD_LR_CNT; i++) {
        WriteExcInfoToBuf("LR[%d]:%p\n", i, preNode->selfNode.linkReg[i]);
    }
#endif
}
#endif
//打印节点信息
STATIC VOID OsMemNodeInfo(const LosMemDynNode *tmpNode,
                          const LosMemDynNode *preNode)
{
    if (tmpNode == preNode) {
        PRINTK("\n the broken node is the first node\n");
#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("\n the broken node is the first node\n");
#endif
    }
    PRINTK("\n broken node head: %p  %p  %p  0x%x, pre node head: %p  %p  %p  0x%x\n",
           tmpNode->selfNode.freeNodeInfo.pstPrev, tmpNode->selfNode.freeNodeInfo.pstNext,
           tmpNode->selfNode.preNode, tmpNode->selfNode.sizeAndFlag,
           preNode->selfNode.freeNodeInfo.pstPrev, preNode->selfNode.freeNodeInfo.pstNext,
           preNode->selfNode.preNode, preNode->selfNode.sizeAndFlag);

#ifdef LOSCFG_SHELL_EXCINFO
    WriteExcInfoToBuf("\n broken node head: %p  %p  %p  0x%x, pre node head: %p  %p  %p  0x%x\n",
                      tmpNode->selfNode.freeNodeInfo.pstPrev, tmpNode->selfNode.freeNodeInfo.pstNext,
                      tmpNode->selfNode.preNode, tmpNode->selfNode.sizeAndFlag,
                      preNode->selfNode.freeNodeInfo.pstPrev, preNode->selfNode.freeNodeInfo.pstNext,
                      preNode->selfNode.preNode, preNode->selfNode.sizeAndFlag);
#endif
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemNodeBacktraceInfo(tmpNode, preNode);
#endif

    PRINTK("\n---------------------------------------------\n");
    PRINTK(" dump mem tmpNode:%p ~ %p\n", tmpNode, ((UINTPTR)tmpNode + NODEDUMPSIZE));
    OsDumpMemByte(NODEDUMPSIZE, (UINTPTR)tmpNode);
    PRINTK("\n---------------------------------------------\n");
    if (preNode != tmpNode) {
        PRINTK(" dump mem :%p ~ tmpNode:%p\n", ((UINTPTR)tmpNode - NODEDUMPSIZE), tmpNode);
        OsDumpMemByte(NODEDUMPSIZE, ((UINTPTR)tmpNode - NODEDUMPSIZE));
        PRINTK("\n---------------------------------------------\n");
    }
}
//内存池完整性检查错误
STATIC VOID OsMemIntegrityCheckError(const LosMemDynNode *tmpNode,
                                     const LosMemDynNode *preNode,
                                     UINT32 intSave)
{
    LosTaskCB *taskCB = NULL;
    UINT32 taskID;

    OsMemNodeInfo(tmpNode, preNode);

    taskID = OS_MEM_TASKID_GET(preNode);
    if (OS_TID_CHECK_INVALID(taskID)) {
#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("Task ID %u in pre node is invalid!\n", taskID);
#endif
        MEM_UNLOCK(intSave);
        LOS_Panic("Task ID %u in pre node is invalid!\n", taskID);
        return;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    if (OsTaskIsUnused(taskCB) || (taskCB->taskEntry == NULL)) {
#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("\r\nTask ID %u in pre node is not created or deleted!\n", taskID);
#endif
        MEM_UNLOCK(intSave);
        LOS_Panic("\r\nTask ID %u in pre node is not created!\n", taskID);
        return;
    }
#ifdef LOSCFG_SHELL_EXCINFO
    WriteExcInfoToBuf("cur node: %p\npre node: %p\npre node was allocated by task:%s\n",
                      tmpNode, preNode, taskCB->taskName);
#endif
    MEM_UNLOCK(intSave);
    LOS_Panic("cur node: %p\npre node: %p\npre node was allocated by task:%s\n",
              tmpNode, preNode, taskCB->taskName);
    return;
}

/*
 * Description : memory pool integrity checking 
 * Input       : pool --Pointer to memory pool
 * Return      : LOS_OK --memory pool integrate or LOS_NOK--memory pool impaired
 */ //对指定内存池做完整性检查，仅打开LOSCFG_BASE_MEM_NODE_INTEGRITY_CHECK时有效
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemIntegrityCheck(const VOID *pool)
{
    LosMemDynNode *tmpNode = NULL;
    LosMemDynNode *preNode = NULL;
    UINT32 intSave;

    if (pool == NULL) {
        return LOS_NOK;
    }

    MEM_LOCK(intSave);
    if (OsMemIntegrityCheck(pool, &tmpNode, &preNode)) {
        goto ERROR_OUT;
    }
    MEM_UNLOCK(intSave);
    return LOS_OK;

ERROR_OUT:
    OsMemIntegrityCheckError(tmpNode, preNode, intSave);
    return LOS_NOK;
}

STATIC INLINE VOID OsMemNodeDebugOperate(VOID *pool, LosMemDynNode *allocNode, UINT32 size)
{
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    poolInfo->poolCurUsedSize += OS_MEM_NODE_GET_SIZE(allocNode->selfNode.sizeAndFlag);
    if (poolInfo->poolCurUsedSize > poolInfo->poolWaterLine) {
        poolInfo->poolWaterLine = poolInfo->poolCurUsedSize;
    }
#endif

#ifdef LOSCFG_MEM_RECORDINFO
    allocNode->selfNode.originSize = size;
#endif

#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(allocNode);
#endif

#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(allocNode);
#endif
}
//扩展内存池
STATIC INLINE INT32 OsMemPoolExpand(VOID *pool, UINT32 size, UINT32 intSave)
{
    UINT32 tryCount = MAX_SHRINK_PAGECACHE_TRY;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    LosMemDynNode *newNode = NULL;
    LosMemDynNode *endNode = NULL;
    LOS_DL_LIST *listNodeHead = NULL;

    size = ROUNDUP(size + OS_MEM_NODE_HEAD_SIZE, PAGE_SIZE);
    endNode = (LosMemDynNode *)OS_MEM_END_NODE(pool, poolInfo->poolSize);//获取内存池的尾部节点

RETRY:
    newNode = (LosMemDynNode *)LOS_PhysPagesAllocContiguous(size >> PAGE_SHIFT);//分配连续的物理内存
    if (newNode == NULL) {
        if (tryCount > 0) {
            tryCount--;
            MEM_UNLOCK(intSave);
            OsTryShrinkMemory(size >> PAGE_SHIFT);
            MEM_LOCK(intSave);
            goto RETRY;
        }

        PRINT_ERR("OsMemPoolExpand alloc failed size = %u\n", size);
        return -1;
    }
    newNode->selfNode.sizeAndFlag = (size - OS_MEM_NODE_HEAD_SIZE);
    newNode->selfNode.preNode = (LosMemDynNode *)OS_MEM_END_NODE(newNode, size);
    listNodeHead = OS_MEM_HEAD(pool, newNode->selfNode.sizeAndFlag);
    if (listNodeHead == NULL) {
        return -1;
    }
    OsMemSentinelNodeSet(endNode, newNode, size);
    LOS_ListTailInsert(listNodeHead, &(newNode->selfNode.freeNodeInfo));

    endNode = (LosMemDynNode *)OS_MEM_END_NODE(newNode, size);
    (VOID)memset_s(endNode, sizeof(*endNode), 0, sizeof(*endNode));

    endNode->selfNode.preNode = newNode;
    OsMemSentinelNodeSet(endNode, NULL, 0);
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    poolInfo->poolCurUsedSize = sizeof(LosMemPoolInfo) + OS_MULTI_DLNK_HEAD_SIZE +
                                OS_MEM_NODE_GET_SIZE(endNode->selfNode.sizeAndFlag);
    poolInfo->poolWaterLine = poolInfo->poolCurUsedSize;
#endif
#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(newNode);
    OsMemNodeSave(endNode);
#endif
    return 0;
}
//允许内存池扩展
VOID LOS_MemExpandEnable(VOID *pool)
{
    if (pool == NULL) {
        return;
    }

    ((LosMemPoolInfo *)pool)->flag = MEM_POOL_EXPAND_ENABLE;
}

#ifdef LOSCFG_BASE_MEM_NODE_INTEGRITY_CHECK //内存节点完整性检查开关

STATIC INLINE UINT32 OsMemAllocCheck(VOID *pool, UINT32 intSave)
{
    LosMemDynNode *tmpNode = NULL;
    LosMemDynNode *preNode = NULL;

    if (OsMemIntegrityCheck(pool, &tmpNode, &preNode)) {
        OsMemIntegrityCheckError(tmpNode, preNode, intSave);
        return LOS_NOK;
    }
    return LOS_OK;
}
#else
STATIC INLINE UINT32 OsMemAllocCheck(VOID *pool, UINT32 intSave)
{
    return LOS_OK;
}

#endif


/*
 * Description : Allocate node from Memory pool
 * Input       : pool  --- Pointer to memory pool
 *               size  --- Size of memory in bytes to allocate
 * Return      : Pointer to allocated memory
 */	//从内存池分配节点
STATIC INLINE VOID *OsMemAllocWithCheck(VOID *pool, UINT32 size, UINT32 intSave)
{
    LosMemDynNode *allocNode = NULL;
    UINT32 allocSize;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    const VOID *firstNode = (const VOID *)((UINT8 *)OS_MEM_HEAD_ADDR(pool) + OS_DLNK_HEAD_SIZE);
    INT32 ret;

    if (OsMemAllocCheck(pool, intSave) == LOS_NOK) {//检查内存池的完整性
        return NULL;
    }

    allocSize = OS_MEM_ALIGN(size + OS_MEM_NODE_HEAD_SIZE, OS_MEM_ALIGN_SIZE);
    if (allocSize == 0) {//检查对齐
        return NULL;
    }
retry:

    allocNode = OsMemFindSuitableFreeBlock(pool, allocSize);//从内存池中找到合适的节点
    if (allocNode == NULL) {//内存池中没有找到合适的节点
        if (poolInfo->flag & MEM_POOL_EXPAND_ENABLE) {//内存池是否允许扩展
            ret = OsMemPoolExpand(pool, allocSize, intSave);//扩大内存池
            if (ret == 0) {//扩大成功了
                goto retry; // 注意这个goto语句,回去再找合适的节点
            }
        }
        PRINT_ERR("---------------------------------------------------"
                  "--------------------------------------------------------\n");
        MEM_UNLOCK(intSave);
        OsMemInfoPrint(pool);//打印内存池的信息
        MEM_LOCK(intSave);
        PRINT_ERR("[%s] No suitable free block, require free node size: 0x%x\n", __FUNCTION__, allocSize);
        PRINT_ERR("----------------------------------------------------"
                  "-------------------------------------------------------\n");
        return NULL;
    }
    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_ALIGN_SIZE) <= allocNode->selfNode.sizeAndFlag) {
        OsMemSplitNode(pool, allocNode, allocSize);//用不了那么多内存的情况劈开node
    }
    OsMemListDelete(&allocNode->selfNode.freeNodeInfo, firstNode);//从空闲链表中删除该节点
    OsMemSetMagicNumAndTaskID(allocNode);//设置魔法数字和任务ID
    OS_MEM_NODE_SET_USED_FLAG(allocNode->selfNode.sizeAndFlag);//贴上节点已使用的标签
    if ((pool == (VOID *)OS_SYS_MEM_ADDR) || (pool == (VOID *)m_aucSysMem0)) {
        OS_MEM_ADD_USED(OS_MEM_NODE_GET_SIZE(allocNode->selfNode.sizeAndFlag), OS_MEM_TASKID_GET(allocNode));
    }
    OsMemNodeDebugOperate(pool, allocNode, size);
    return (allocNode + 1);
}

/*
 * Description : reAlloc a smaller memory node
 * Input       : pool      --- Pointer to memory pool
 *               allocSize --- the size of new node which will be alloced
 *               node      --- the node which wille be realloced
 *               nodeSize  --- the size of old node
 * Output      : node      --- pointer to the new node after realloc
 */
STATIC INLINE VOID OsMemReAllocSmaller(VOID *pool, UINT32 allocSize, LosMemDynNode *node, UINT32 nodeSize)
{
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
#endif
    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_ALIGN_SIZE) <= nodeSize) {
        node->selfNode.sizeAndFlag = nodeSize;
        OsMemSplitNode(pool, node, allocSize);
        OS_MEM_NODE_SET_USED_FLAG(node->selfNode.sizeAndFlag);
#ifdef LOSCFG_MEM_HEAD_BACKUP
        OsMemNodeSave(node);
#endif
        if ((pool == (VOID *)OS_SYS_MEM_ADDR) || (pool == (VOID *)m_aucSysMem0)) {
            OS_MEM_REDUCE_USED(nodeSize - allocSize, OS_MEM_TASKID_GET(node));
        }
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
        poolInfo->poolCurUsedSize -= nodeSize - allocSize;
#endif
    }
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(node);
#endif
}

/*
 * Description : reAlloc a Bigger memory node after merge node and nextNode
 * Input       : pool      --- Pointer to memory pool
 *               allocSize --- the size of new node which will be alloced
 *               node      --- the node which wille be realloced
 *               nodeSize  --- the size of old node
 *               nextNode  --- pointer next node which will be merged
 * Output      : node      --- pointer to the new node after realloc
 */
STATIC INLINE VOID OsMemMergeNodeForReAllocBigger(VOID *pool, UINT32 allocSize, LosMemDynNode *node,
                                                  UINT32 nodeSize, LosMemDynNode *nextNode)
{
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
#endif
    const VOID *firstNode = (const VOID *)((UINT8 *)OS_MEM_HEAD_ADDR(pool) + OS_DLNK_HEAD_SIZE);

    node->selfNode.sizeAndFlag = nodeSize;
    OsMemListDelete(&nextNode->selfNode.freeNodeInfo, firstNode);
    OsMemMergeNode(nextNode);
    if ((allocSize + OS_MEM_NODE_HEAD_SIZE + OS_MEM_ALIGN_SIZE) <= node->selfNode.sizeAndFlag) {
        OsMemSplitNode(pool, node, allocSize);
    }
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    poolInfo->poolCurUsedSize += (node->selfNode.sizeAndFlag - nodeSize);
    if (poolInfo->poolCurUsedSize > poolInfo->poolWaterLine) {
        poolInfo->poolWaterLine = poolInfo->poolCurUsedSize;
    }
#endif
    if ((pool == (VOID *)OS_SYS_MEM_ADDR) || (pool == (VOID *)m_aucSysMem0)) {
        OS_MEM_ADD_USED(node->selfNode.sizeAndFlag - nodeSize, OS_MEM_TASKID_GET(node));
    }
    OS_MEM_NODE_SET_USED_FLAG(node->selfNode.sizeAndFlag);
#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(node);
#endif
#ifdef LOSCFG_MEM_LEAKCHECK
    OsMemLinkRegisterRecord(node);
#endif
}

#ifdef LOSCFG_MEM_MUL_POOL
STATIC UINT32 OsMemPoolAdd(VOID *pool, UINT32 size)
{
    VOID *nextPool = g_poolHead;
    VOID *curPool = g_poolHead;
    UINTPTR poolEnd;
    while (nextPool != NULL) {
        poolEnd = (UINTPTR)nextPool + LOS_MemPoolSizeGet(nextPool);
        if (((pool <= nextPool) && (((UINTPTR)pool + size) > (UINTPTR)nextPool)) ||
            (((UINTPTR)pool < poolEnd) && (((UINTPTR)pool + size) >= poolEnd))) {
            PRINT_ERR("pool [%p, %p) conflict with pool [%p, %p)\n",
                      pool, (UINTPTR)pool + size,
                      nextPool, (UINTPTR)nextPool + LOS_MemPoolSizeGet(nextPool));
            return LOS_NOK;
        }
        curPool = nextPool;
        nextPool = ((LosMemPoolInfo *)nextPool)->nextPool;
    }

    if (g_poolHead == NULL) {
        g_poolHead = pool;
    } else {
        ((LosMemPoolInfo *)curPool)->nextPool = pool;
    }

    ((LosMemPoolInfo *)pool)->nextPool = NULL;
    return LOS_OK;
}
#endif
//初始化内存池
STATIC UINT32 OsMemInit(VOID *pool, UINT32 size)
{
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;//内存池的头部信息
    LosMemDynNode *newNode = NULL;
    LosMemDynNode *endNode = NULL;
    LOS_DL_LIST *listNodeHead = NULL;

    poolInfo->pool = pool;	//保存内存池开始地址,这里决定了 pool 变量一定要放在结构体的首位
    poolInfo->poolSize = size;	//保证内存池大小
    poolInfo->flag = MEM_POOL_EXPAND_DISABLE;//内存池默认不能扩展
    OsDLnkInitMultiHead(OS_MEM_HEAD_ADDR(pool));//初始化内存池的第二部分(双向链表部分)
    newNode = OS_MEM_FIRST_NODE(pool);//跳到内存池第三部分(动态节点部分)
    newNode->selfNode.sizeAndFlag = (size - (UINT32)((UINTPTR)newNode - (UINTPTR)pool) - OS_MEM_NODE_HEAD_SIZE);//得到内存池剩余的大小
    newNode->selfNode.preNode = (LosMemDynNode *)OS_MEM_END_NODE(pool, size);//首个节点指向最后一个节点
    listNodeHead = OS_MEM_HEAD(pool, newNode->selfNode.sizeAndFlag);//通过节点大小,找到节点在内存池第二部分对应的链表头
    if (listNodeHead == NULL) {//只有一种情况,给的内存池太大了,大于 2^30, 无法管理
        return LOS_NOK;
    }

    LOS_ListTailInsert(listNodeHead, &(newNode->selfNode.freeNodeInfo));//将首个节点挂入对应的链表
    endNode = (LosMemDynNode *)OS_MEM_END_NODE(pool, size);//取出尾部节点
    (VOID)memset_s(endNode, sizeof(*endNode), 0, sizeof(*endNode));//清空尾部节点

    endNode->selfNode.preNode = newNode;//尾部节点的前一个节点为首个节点
    OsMemSentinelNodeSet(endNode, NULL, 0);//将尾部节点设置为哨兵节点
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    poolInfo->poolCurUsedSize = sizeof(LosMemPoolInfo) + OS_MULTI_DLNK_HEAD_SIZE +
                                OS_MEM_NODE_GET_SIZE(endNode->selfNode.sizeAndFlag);
    poolInfo->poolWaterLine = poolInfo->poolCurUsedSize;
#endif
#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(newNode);
    OsMemNodeSave(endNode);
#endif

    return LOS_OK;
}
/******************************************************************************
初始化一块指定的动态内存池，大小为size,被以下函数调用
LiteIpcMmap
OsKHeapInit
OsMemSystemInit
OsMemExcInteractionInit
******************************************************************************/
LITE_OS_SEC_TEXT_INIT UINT32 LOS_MemInit(VOID *pool, UINT32 size)
{
    UINT32 intSave;

    if ((pool == NULL) || (size < OS_MEM_MIN_POOL_SIZE)) {//不能小于最低内存池大小
        return OS_ERROR;
    }

    if (!IS_ALIGNED(size, OS_MEM_ALIGN_SIZE)) {
        PRINT_WARN("pool [%p, %p) size 0x%x sholud be aligned with OS_MEM_ALIGN_SIZE\n",
                   pool, (UINTPTR)pool + size, size);
        size = OS_MEM_ALIGN(size, OS_MEM_ALIGN_SIZE) - OS_MEM_ALIGN_SIZE;
    }

    MEM_LOCK(intSave);
#ifdef LOSCFG_MEM_MUL_POOL
    if (OsMemPoolAdd(pool, size)) {
        MEM_UNLOCK(intSave);
        return OS_ERROR;
    }
#endif

    if (OsMemInit(pool, size)) {//主体函数,初始化内存池
#ifdef LOSCFG_MEM_MUL_POOL
        (VOID)LOS_MemDeInit(pool);
#endif
        MEM_UNLOCK(intSave);
        return OS_ERROR;
    }

    MEM_UNLOCK(intSave);
    return LOS_OK;
}

#ifdef LOSCFG_MEM_MUL_POOL //删除指定内存池，仅打开LOSCFG_MEM_MUL_POOL时有效
LITE_OS_SEC_TEXT_INIT UINT32 LOS_MemDeInit(VOID *pool)
{
    UINT32 intSave;
    UINT32 ret = LOS_NOK;
    VOID *nextPool = NULL;
    VOID *curPool = NULL;

    MEM_LOCK(intSave);
    do {
        if (pool == NULL) {
            break;
        }

        if (pool == g_poolHead) {
            g_poolHead = ((LosMemPoolInfo *)g_poolHead)->nextPool;
            ret = LOS_OK;
            break;
        }

        curPool = g_poolHead;
        nextPool = g_poolHead;
        while (nextPool != NULL) {
            if (pool == nextPool) {
                ((LosMemPoolInfo *)curPool)->nextPool = ((LosMemPoolInfo *)nextPool)->nextPool;
                ret = LOS_OK;
                break;
            }
            curPool = nextPool;
            nextPool = ((LosMemPoolInfo *)nextPool)->nextPool;
        }
    } while (0);

    MEM_UNLOCK(intSave);
    return ret;
}
//打印系统中已初始化的所有内存池，包括内存池的起始地址、内存池大小、空闲内存总大小、已使用内存总大小、
//最大的空闲内存块大小、空闲内存块数量、已使用的内存块数量。仅打开LOSCFG_MEM_MUL_POOL时有效
LITE_OS_SEC_TEXT_INIT UINT32 LOS_MemPoolList(VOID)
{
    VOID *nextPool = g_poolHead;
    UINT32 index = 0;
    while (nextPool != NULL) {
        PRINTK("pool%u :\n", index);
        index++;
        OsMemInfoPrint(nextPool);
        nextPool = ((LosMemPoolInfo *)nextPool)->nextPool;
    }
    return index;
}
#endif
//从指定动态内存池中申请size长度的内存
LITE_OS_SEC_TEXT VOID *LOS_MemAlloc(VOID *pool, UINT32 size)
{
    VOID *ptr = NULL;
    UINT32 intSave;

    if ((pool == NULL) || (size == 0)) {//不指定内存池和大小时,通常开机时
        return (size > 0) ? OsVmBootMemAlloc(size) : NULL;//引导分配器分配，粗暴！
    }

    MEM_LOCK(intSave);//内存自旋锁
    do {
        if (OS_MEM_NODE_GET_USED_FLAG(size) || OS_MEM_NODE_GET_ALIGNED_FLAG(size)) {//最大不超过2G,对齐不超过1G
            break;//要的太大,申请失败
        }

        ptr = OsMemAllocWithCheck(pool, size, intSave);//从内存池分配的主体函数
    } while (0);//只执行一次为什么要这么写，这是面试经常考的地方！

#ifdef LOSCFG_MEM_RECORDINFO
    OsMemRecordMalloc(ptr, size);
#endif
    MEM_UNLOCK(intSave);

    return ptr;
}
//从指定动态内存池中申请长度为size且地址按boundary字节对齐的内存
LITE_OS_SEC_TEXT VOID *LOS_MemAllocAlign(VOID *pool, UINT32 size, UINT32 boundary)
{
    UINT32 useSize;
    UINT32 gapSize;
    VOID *ptr = NULL;
    VOID *alignedPtr = NULL;
    LosMemDynNode *allocNode = NULL;
    UINT32 intSave;

    if ((pool == NULL) || (size == 0) || (boundary == 0) || !IS_POW_TWO(boundary) ||
        !IS_ALIGNED(boundary, sizeof(VOID *))) {
        return NULL;
    }

    MEM_LOCK(intSave);
    /*
     * sizeof(gapSize) bytes stores offset between alignedPtr and ptr,
     * the ptr has been OS_MEM_ALIGN_SIZE(4 or 8) aligned, so maximum
     * offset between alignedPtr and ptr is boundary - OS_MEM_ALIGN_SIZE
     */
    if ((boundary - sizeof(gapSize)) > ((UINT32)(-1) - size)) {
        goto out;
    }

    useSize = (size + boundary) - sizeof(gapSize);
    if (OS_MEM_NODE_GET_USED_FLAG(useSize) || OS_MEM_NODE_GET_ALIGNED_FLAG(useSize)) {
        goto out;
    }

    ptr = OsMemAllocWithCheck(pool, useSize, intSave);

    alignedPtr = (VOID *)OS_MEM_ALIGN(ptr, boundary);
    if (ptr == alignedPtr) {
        goto out;
    }

    /* store gapSize in address (ptr -4), it will be checked while free */
    gapSize = (UINT32)((UINTPTR)alignedPtr - (UINTPTR)ptr);
    allocNode = (LosMemDynNode *)ptr - 1;
    OS_MEM_NODE_SET_ALIGNED_FLAG(allocNode->selfNode.sizeAndFlag);
#ifdef LOSCFG_MEM_RECORDINFO
    allocNode->selfNode.originSize = size;
#endif
#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSaveWithGapSize(allocNode, gapSize);
#endif
    OS_MEM_NODE_SET_ALIGNED_FLAG(gapSize);
    *(UINT32 *)((UINTPTR)alignedPtr - sizeof(gapSize)) = gapSize;
    ptr = alignedPtr;
out:
#ifdef LOSCFG_MEM_RECORDINFO
    OsMemRecordMalloc(ptr, size);
#endif
    MEM_UNLOCK(intSave);

    return ptr;
}

LITE_OS_SEC_TEXT STATIC INLINE UINT32 OsDoMemFree(VOID *pool, const VOID *ptr, LosMemDynNode *node)
{
    UINT32 ret = OsMemCheckUsedNode(pool, node);
    if (ret == LOS_OK) {
#ifdef LOSCFG_MEM_RECORDINFO
        OsMemRecordFree(ptr, node->selfNode.originSize);
#endif
        OsMemFreeNode(node, pool);
    }
    return ret;
}

#ifdef LOSCFG_MEM_HEAD_BACKUP
LITE_OS_SEC_TEXT STATIC INLINE UINT32 OsMemBackupCheckAndRetore(VOID *pool, VOID *ptr, LosMemDynNode *node)
{
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    LosMemDynNode *startNode = OS_MEM_FIRST_NODE(pool);
    LosMemDynNode *endNode   = OS_MEM_END_NODE(pool, poolInfo->poolSize);

    if (OS_MEM_MIDDLE_ADDR(startNode, node, endNode)) {
        /* GapSize is bad or node is broken, we need to verify & try to restore */
        if (!OsMemChecksumVerify(&(node->selfNode))) {
            node = (LosMemDynNode *)((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE);
            return OsMemBackupTryRestore(pool, &node, ptr);
        }
    }
    return LOS_OK;
}
#endif
//释放已申请的内存
LITE_OS_SEC_TEXT UINT32 LOS_MemFree(VOID *pool, VOID *ptr)
{
    UINT32 ret = LOS_NOK;
    UINT32 gapSize;
    UINT32 intSave;
    LosMemDynNode *node = NULL;

    if ((pool == NULL) || (ptr == NULL) || !IS_ALIGNED(pool, sizeof(VOID *)) || !IS_ALIGNED(ptr, sizeof(VOID *))) {
        return LOS_NOK;
    }

    MEM_LOCK(intSave);
    do {
        gapSize = *(UINT32 *)((UINTPTR)ptr - sizeof(UINT32));
        if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize) && OS_MEM_NODE_GET_USED_FLAG(gapSize)) {
            PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
            goto OUT;
        }

        node = (LosMemDynNode *)((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE);

        if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize)) {
            gapSize = OS_MEM_NODE_GET_ALIGNED_GAPSIZE(gapSize);
            if ((gapSize & (OS_MEM_ALIGN_SIZE - 1)) || (gapSize > ((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE))) {
                PRINT_ERR("illegal gapSize: 0x%x\n", gapSize);
                break;
            }
            node = (LosMemDynNode *)((UINTPTR)ptr - gapSize - OS_MEM_NODE_HEAD_SIZE);
        }
#ifndef LOSCFG_MEM_HEAD_BACKUP
        ret = OsDoMemFree(pool, ptr, node);
#endif
    } while (0);
#ifdef LOSCFG_MEM_HEAD_BACKUP
    ret = OsMemBackupCheckAndRetore(pool, ptr, node);
    if (!ret) {
        ret = OsDoMemFree(pool, ptr, node);
    }
#endif
OUT:
    if (ret == LOS_NOK) {
        OsMemRecordFree(ptr, 0);
    }
    MEM_UNLOCK(intSave);
    return ret;
}

STATIC VOID *OsGetRealPtr(const VOID *pool, VOID *ptr)
{
    VOID *realPtr = ptr;
    UINT32 gapSize = *((UINT32 *)((UINTPTR)ptr - sizeof(UINT32)));

    if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize) && OS_MEM_NODE_GET_USED_FLAG(gapSize)) {
#ifdef LOSCFG_MEM_RECORDINFO
        OsMemRecordFree(ptr, 0);
#endif
        PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
        return NULL;
    }
    if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize)) {
        gapSize = OS_MEM_NODE_GET_ALIGNED_GAPSIZE(gapSize);
        if ((gapSize & (OS_MEM_ALIGN_SIZE - 1)) ||
            (gapSize > ((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE - (UINTPTR)pool))) {
            PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
#ifdef LOSCFG_MEM_RECORDINFO
            OsMemRecordFree(ptr, 0);
#endif
            return NULL;
        }
        realPtr = (VOID *)((UINTPTR)ptr - (UINTPTR)gapSize);
    }
    return realPtr;
}

#ifdef LOSCFG_MEM_RECORDINFO
STATIC INLINE VOID OsMemReallocNodeRecord(LosMemDynNode *node, UINT32 size, const VOID *ptr)
{
    node->selfNode.originSize = size;
#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(node);
#endif
    OsMemRecordMalloc(ptr, size);
}
#else
STATIC INLINE VOID OsMemReallocNodeRecord(LosMemDynNode *node, UINT32 size, const VOID *ptr)
{
    return;
}
#endif

STATIC VOID *OsMemRealloc(VOID *pool, const VOID *ptr, LosMemDynNode *node, UINT32 size, UINT32 intSave)
{
    LosMemDynNode *nextNode = NULL;
    UINT32 allocSize = OS_MEM_ALIGN(size + OS_MEM_NODE_HEAD_SIZE, OS_MEM_ALIGN_SIZE);
    UINT32 nodeSize = OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag);
    VOID *tmpPtr = NULL;
    const VOID *originPtr = ptr;
#ifdef LOSCFG_MEM_RECORDINFO
    UINT32 originSize = node->selfNode.originSize;
#else
    UINT32 originSize = 0;
#endif
    if (nodeSize >= allocSize) {
        OsMemRecordFree(originPtr, originSize);
        OsMemReAllocSmaller(pool, allocSize, node, nodeSize);
        OsMemReallocNodeRecord(node, size, ptr);
        return (VOID *)ptr;
    }

    nextNode = OS_MEM_NEXT_NODE(node);
    if (!OS_MEM_NODE_GET_USED_FLAG(nextNode->selfNode.sizeAndFlag) &&
        ((nextNode->selfNode.sizeAndFlag + nodeSize) >= allocSize)) {
        OsMemRecordFree(originPtr, originSize);
        OsMemMergeNodeForReAllocBigger(pool, allocSize, node, nodeSize, nextNode);
        OsMemReallocNodeRecord(node, size, ptr);
        return (VOID *)ptr;
    }

    tmpPtr = OsMemAllocWithCheck(pool, size, intSave);
    if (tmpPtr != NULL) {
        OsMemRecordMalloc(tmpPtr, size);
        if (memcpy_s(tmpPtr, size, ptr, (nodeSize - OS_MEM_NODE_HEAD_SIZE)) != EOK) {
            MEM_UNLOCK(intSave);
            (VOID)LOS_MemFree((VOID *)pool, (VOID *)tmpPtr);
            MEM_LOCK(intSave);
            return NULL;
        }
        OsMemRecordFree(originPtr, originSize);
        OsMemFreeNode(node, pool);
    }
    return tmpPtr;
}
//按size大小重新分配内存块，并将原内存块内容拷贝到新内存块。如果新内存块申请成功，则释放原内存块
LITE_OS_SEC_TEXT_MINOR VOID *LOS_MemRealloc(VOID *pool, VOID *ptr, UINT32 size)
{
    UINT32 intSave;
    VOID *newPtr = NULL;
    LosMemDynNode *node = NULL;
#ifdef LOSCFG_MEM_RECORDINFO
    VOID *originPtr = ptr;
#endif

    if (OS_MEM_NODE_GET_USED_FLAG(size) || OS_MEM_NODE_GET_ALIGNED_FLAG(size) || (pool == NULL)) {
        return NULL;
    }

    if (ptr == NULL) {
        newPtr = LOS_MemAlloc(pool, size);
        goto OUT;
    }

    if (size == 0) {
        (VOID)LOS_MemFree(pool, ptr);
        goto OUT;
    }

    MEM_LOCK(intSave);

    ptr = OsGetRealPtr(pool, ptr);
    if (ptr == NULL) {
        goto OUT_UNLOCK;
    }

    node = (LosMemDynNode *)((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE);
    if (OsMemCheckUsedNode(pool, node) != LOS_OK) {
#ifdef LOSCFG_MEM_RECORDINFO
        OsMemRecordFree(originPtr, 0);
#endif
        goto OUT_UNLOCK;
    }

    newPtr = OsMemRealloc(pool, ptr, node, size, intSave);

OUT_UNLOCK:
    MEM_UNLOCK(intSave);
OUT:
    return newPtr;
}
//获取指定动态内存池的总使用量大小
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemTotalUsedGet(VOID *pool)
{
    LosMemDynNode *tmpNode = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    LosMemDynNode *endNode = NULL;
    UINT32 memUsed = 0;
    UINT32 size;
    UINT32 intSave;

    if (pool == NULL) {
        return LOS_NOK;
    }

    MEM_LOCK(intSave);

    endNode = OS_MEM_END_NODE(pool, poolInfo->poolSize);
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode <= endNode;) {
        if (tmpNode == endNode) {
            memUsed += OS_MEM_NODE_HEAD_SIZE;
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                size = OS_MEM_NODE_GET_SIZE(endNode->selfNode.sizeAndFlag);
                tmpNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(tmpNode, size);
                continue;
            } else {
                break;
            }
        } else {
            if (OS_MEM_NODE_GET_USED_FLAG(tmpNode->selfNode.sizeAndFlag)) {
                memUsed += OS_MEM_NODE_GET_SIZE(tmpNode->selfNode.sizeAndFlag);
            }
            tmpNode = OS_MEM_NEXT_NODE(tmpNode);
        }
    }

    MEM_UNLOCK(intSave);

    return memUsed;
}
//获取指定内存池已使用的内存块数量
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemUsedBlksGet(VOID *pool)
{
    LosMemDynNode *tmpNode = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    UINT32 blkNums = 0;
    UINT32 intSave;

    if (pool == NULL) {
        return LOS_NOK;
    }

    MEM_LOCK(intSave);

    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode < OS_MEM_END_NODE(pool, poolInfo->poolSize);
         tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        if (OS_MEM_NODE_GET_USED_FLAG(tmpNode->selfNode.sizeAndFlag)) {
            blkNums++;
        }
    }

    MEM_UNLOCK(intSave);

    return blkNums;
}
//获取申请了指定内存块的任务ID
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemTaskIdGet(VOID *ptr)
{
    LosMemDynNode *tmpNode = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)(VOID *)m_aucSysMem1;
    UINT32 intSave;
#ifdef LOSCFG_EXC_INTERACTION
    if (ptr < (VOID *)m_aucSysMem1) {
        poolInfo = (LosMemPoolInfo *)(VOID *)m_aucSysMem0;
    }
#endif
    if ((ptr == NULL) ||
        (ptr < (VOID *)OS_MEM_FIRST_NODE(poolInfo)) ||
        (ptr > (VOID *)OS_MEM_END_NODE(poolInfo, poolInfo->poolSize))) {
        PRINT_ERR("input ptr %p is out of system memory range[%p, %p]\n", ptr, OS_MEM_FIRST_NODE(poolInfo),
                  OS_MEM_END_NODE(poolInfo, poolInfo->poolSize));
        return OS_INVALID;
    }

    MEM_LOCK(intSave);

    for (tmpNode = OS_MEM_FIRST_NODE(poolInfo); tmpNode < OS_MEM_END_NODE(poolInfo, poolInfo->poolSize);
         tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        if ((UINTPTR)ptr < (UINTPTR)tmpNode) {
            if (OS_MEM_NODE_GET_USED_FLAG(tmpNode->selfNode.preNode->selfNode.sizeAndFlag)) {
                MEM_UNLOCK(intSave);
                return (UINT32)((UINTPTR)(tmpNode->selfNode.preNode->selfNode.freeNodeInfo.pstNext));
            } else {
                MEM_UNLOCK(intSave);
                PRINT_ERR("input ptr %p is belong to a free mem node\n", ptr);
                return OS_INVALID;
            }
        }
    }

    MEM_UNLOCK(intSave);
    return OS_INVALID;
}
//获取指定内存池的空闲内存块数量
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemFreeBlksGet(VOID *pool)
{
    LosMemDynNode *tmpNode = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    UINT32 blkNums = 0;
    UINT32 intSave;

    if (pool == NULL) {
        return LOS_NOK;
    }

    MEM_LOCK(intSave);

    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode < OS_MEM_END_NODE(pool, poolInfo->poolSize);
         tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        if (!OS_MEM_NODE_GET_USED_FLAG(tmpNode->selfNode.sizeAndFlag)) {
            blkNums++;
        }
    }

    MEM_UNLOCK(intSave);

    return blkNums;
}
//获取内存池最后一个已使用内存块的结束地址
LITE_OS_SEC_TEXT_MINOR UINTPTR LOS_MemLastUsedGet(VOID *pool)
{
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    LosMemDynNode *node = NULL;

    if (pool == NULL) {
        return LOS_NOK;
    }

    node = OS_MEM_END_NODE(pool, poolInfo->poolSize)->selfNode.preNode;
    if (OS_MEM_NODE_GET_USED_FLAG(node->selfNode.sizeAndFlag)) {
        return (UINTPTR)((CHAR *)node + OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag) + sizeof(LosMemDynNode));
    } else {
        return (UINTPTR)((CHAR *)node + sizeof(LosMemDynNode));
    }
}

/*
 * Description : reset "end node"
 * Input       : pool    --- Pointer to memory pool
 *               preAddr --- Pointer to the pre Pointer of end node
 * Output      : endNode --- pointer to "end node"
 * Return      : the number of free node
 */
LITE_OS_SEC_TEXT_MINOR VOID OsMemResetEndNode(VOID *pool, UINTPTR preAddr)
{
    LosMemDynNode *endNode = (LosMemDynNode *)OS_MEM_END_NODE(pool, ((LosMemPoolInfo *)pool)->poolSize);
    endNode->selfNode.sizeAndFlag = OS_MEM_NODE_HEAD_SIZE;
    if (preAddr != 0) {
        endNode->selfNode.preNode = (LosMemDynNode *)(preAddr - sizeof(LosMemDynNode));
    }
    OS_MEM_NODE_SET_USED_FLAG(endNode->selfNode.sizeAndFlag);
    OsMemSetMagicNumAndTaskID(endNode);

#ifdef LOSCFG_MEM_HEAD_BACKUP
    OsMemNodeSave(endNode);
#endif
}
//获取指定动态内存池的总大小
UINT32 LOS_MemPoolSizeGet(const VOID *pool)
{
    UINT32 count = 0;
    UINT32 size;
    LosMemDynNode *node = NULL;
    LosMemDynNode *sentinel = NULL;

    if (pool == NULL) {
        return LOS_NOK;
    }

    count += ((LosMemPoolInfo *)pool)->poolSize;
    sentinel = OS_MEM_END_NODE(pool, count);

    while (OsMemIsLastSentinelNode(sentinel) == FALSE) {
        size = OS_MEM_NODE_GET_SIZE(sentinel->selfNode.sizeAndFlag);
        node = OsMemSentinelNodeGet(sentinel);
        sentinel = OS_MEM_END_NODE(node, size);
        count += size;
    }

    return count;
}

LITE_OS_SEC_TEXT_MINOR VOID OsMemInfoPrint(VOID *pool)
{
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    LOS_MEM_POOL_STATUS status = {0};

    if (LOS_MemInfoGet(pool, &status) == LOS_NOK) {
        return;
    }

#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    PRINTK("pool addr          pool size    used size     free size    "
           "max free node size   used node num     free node num      UsageWaterLine\n");
    PRINTK("---------------    --------     -------       --------     "
           "--------------       -------------      ------------      ------------\n");
    PRINTK("%-16p   0x%-8x   0x%-8x    0x%-8x   0x%-16x   0x%-13x    0x%-13x    0x%-13x\n",
           poolInfo->pool, LOS_MemPoolSizeGet(pool), status.uwTotalUsedSize,
           status.uwTotalFreeSize, status.uwMaxFreeNodeSize, status.uwUsedNodeNum,
           status.uwFreeNodeNum, status.uwUsageWaterLine);

#else
    PRINTK("pool addr          pool size    used size     free size    "
           "max free node size   used node num     free node num\n");
    PRINTK("---------------    --------     -------       --------     "
           "--------------       -------------      ------------\n");
    PRINTK("%-16p   0x%-8x   0x%-8x    0x%-8x   0x%-16x   0x%-13x    0x%-13x\n",
           poolInfo->pool, LOS_MemPoolSizeGet(pool), status.uwTotalUsedSize,
           status.uwTotalFreeSize, status.uwMaxFreeNodeSize, status.uwUsedNodeNum,
           status.uwFreeNodeNum);
#endif
}
//获取指定内存池的内存结构信息，包括空闲内存大小、已使用内存大小、空闲内存块数量、已使用的内存块数量、最大的空闲内存块大小
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemInfoGet(VOID *pool, LOS_MEM_POOL_STATUS *poolStatus)
{
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    LosMemDynNode *tmpNode = NULL;
    UINT32 totalUsedSize = 0;
    UINT32 totalFreeSize = 0;
    UINT32 maxFreeNodeSize = 0;
    UINT32 usedNodeNum = 0;
    UINT32 freeNodeNum = 0;
    UINT32 intSave;

    if (poolStatus == NULL) {
        PRINT_ERR("can't use NULL addr to save info\n");
        return LOS_NOK;
    }

    if ((poolInfo == NULL) || ((UINTPTR)pool != (UINTPTR)poolInfo->pool)) {
        PRINT_ERR("wrong mem pool addr: %p, line:%d\n", poolInfo, __LINE__);
        return LOS_NOK;
    }

    tmpNode = (LosMemDynNode *)OS_MEM_END_NODE(pool, poolInfo->poolSize);
    tmpNode = (LosMemDynNode *)OS_MEM_ALIGN(tmpNode, OS_MEM_ALIGN_SIZE);

    if (!OS_MEM_MAGIC_VALID(tmpNode->selfNode.freeNodeInfo.pstPrev)) {
        PRINT_ERR("wrong mem pool addr: %p\n, line:%d", poolInfo, __LINE__);
        return LOS_NOK;
    }

    MEM_LOCK(intSave);

    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode < OS_MEM_END_NODE(pool, poolInfo->poolSize);
         tmpNode = OS_MEM_NEXT_NODE(tmpNode)) {
        if (!OS_MEM_NODE_GET_USED_FLAG(tmpNode->selfNode.sizeAndFlag)) {
            ++freeNodeNum;
            totalFreeSize += OS_MEM_NODE_GET_SIZE(tmpNode->selfNode.sizeAndFlag);
            if (maxFreeNodeSize < OS_MEM_NODE_GET_SIZE(tmpNode->selfNode.sizeAndFlag)) {
                maxFreeNodeSize = OS_MEM_NODE_GET_SIZE(tmpNode->selfNode.sizeAndFlag);
            }
        } else {
            ++usedNodeNum;
            totalUsedSize += OS_MEM_NODE_GET_SIZE(tmpNode->selfNode.sizeAndFlag);
        }
    }

    MEM_UNLOCK(intSave);

    poolStatus->uwTotalUsedSize = totalUsedSize;
    poolStatus->uwTotalFreeSize = totalFreeSize;
    poolStatus->uwMaxFreeNodeSize = maxFreeNodeSize;
    poolStatus->uwUsedNodeNum = usedNodeNum;
    poolStatus->uwFreeNodeNum = freeNodeNum;
#if defined(OS_MEM_WATERLINE) && (OS_MEM_WATERLINE == YES)
    poolStatus->uwUsageWaterLine = poolInfo->poolWaterLine;
#endif
    return LOS_OK;
}

STATIC INLINE VOID OsShowFreeNode(UINT32 index, UINT32 length, const UINT32 *countNum)
{
    UINT32 count = 0;
    PRINTK("\n    block size:  ");
    while (count < length) {
        PRINTK("2^%-5u", (index + OS_MIN_MULTI_DLNK_LOG2 + count));
        count++;
    }
    PRINTK("\n    node number: ");
    count = 0;
    while (count < length) {
        PRINTK("  %-5u", countNum[count + index]);
        count++;
    }
}
//打印指定内存池的空闲内存块的大小及数量
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemFreeNodeShow(VOID *pool)
{
    LOS_DL_LIST *listNodeHead = NULL;
    LosMultipleDlinkHead *headAddr = (LosMultipleDlinkHead *)((UINTPTR)pool + sizeof(LosMemPoolInfo));
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    UINT32 linkHeadIndex;
    UINT32 countNum[OS_MULTI_DLNK_NUM] = { 0 };
    UINT32 intSave;

    if ((poolInfo == NULL) || ((UINTPTR)pool != (UINTPTR)poolInfo->pool)) {
        PRINT_ERR("wrong mem pool addr: %p, line:%d\n", poolInfo, __LINE__);
        return LOS_NOK;
    }

    PRINTK("\n   ************************ left free node number**********************");
    MEM_LOCK(intSave);

    for (linkHeadIndex = 0; linkHeadIndex <= (OS_MULTI_DLNK_NUM - 1);
         linkHeadIndex++) {
        listNodeHead = headAddr->listHead[linkHeadIndex].pstNext;
        while (listNodeHead != &(headAddr->listHead[linkHeadIndex])) {
            listNodeHead = listNodeHead->pstNext;
            countNum[linkHeadIndex]++;
        }
    }

    linkHeadIndex = 0;
    while (linkHeadIndex < OS_MULTI_DLNK_NUM) {
        if (linkHeadIndex + COLUMN_NUM < OS_MULTI_DLNK_NUM) {
            OsShowFreeNode(linkHeadIndex, COLUMN_NUM, countNum);
            linkHeadIndex += COLUMN_NUM;
        } else {
            OsShowFreeNode(linkHeadIndex, (OS_MULTI_DLNK_NUM - 1 - linkHeadIndex), countNum);
            break;
        }
    }

    MEM_UNLOCK(intSave);
    PRINTK("\n   ********************************************************************\n\n");

    return LOS_OK;
}
#ifdef LOSCFG_MEM_LEAKCHECK
LITE_OS_SEC_TEXT_MINOR VOID OsMemUsedNodeShow(VOID *pool)
{
    LosMemDynNode *tmpNode = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    LosMemDynNode *endNode = NULL;
    UINT32 size;
    UINT32 intSave;
    UINT32 count;

    if (pool == NULL) {
        PRINTK("input param is NULL\n");
        return;
    }
    if (LOS_MemIntegrityCheck(pool)) {
        PRINTK("LOS_MemIntegrityCheck error\n");
        return;
    }
    MEM_LOCK(intSave);
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

    endNode = OS_MEM_END_NODE(pool, poolInfo->poolSize);
    for (tmpNode = OS_MEM_FIRST_NODE(pool); tmpNode <= endNode;) {
        if (tmpNode == endNode) {
            if (OsMemIsLastSentinelNode(endNode) == FALSE) {
                size = OS_MEM_NODE_GET_SIZE(endNode->selfNode.sizeAndFlag);
                tmpNode = OsMemSentinelNodeGet(endNode);
                endNode = OS_MEM_END_NODE(tmpNode, size);
                continue;
            } else {
                break;
            }
        } else {
            if (OS_MEM_NODE_GET_USED_FLAG(tmpNode->selfNode.sizeAndFlag)) {
#ifdef __LP64__
                PRINTK("%018p: ", tmpNode);
#else
                PRINTK("%010p: ", tmpNode);
#endif
                for (count = 0; count < LOS_RECORD_LR_CNT; count++) {
#ifdef __LP64__
                    PRINTK(" %018p ", tmpNode->selfNode.linkReg[count]);
#else
                    PRINTK(" %010p ", tmpNode->selfNode.linkReg[count]);
#endif
                }
                PRINTK("\n");
            }
            tmpNode = OS_MEM_NEXT_NODE(tmpNode);
        }
    }
    MEM_UNLOCK(intSave);
    return;
}
#endif

#ifdef LOSCFG_BASE_MEM_NODE_SIZE_CHECK
//获取指定内存块的总大小和可用大小，仅打开LOSCFG_BASE_MEM_NODE_SIZE_CHECK时有效
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemNodeSizeCheck(VOID *pool, VOID *ptr, UINT32 *totalSize, UINT32 *availSize)
{
    const VOID *head = NULL;
    LosMemPoolInfo *poolInfo = (LosMemPoolInfo *)pool;
    UINT8 *endPool = NULL;

    if (g_memCheckLevel == LOS_MEM_CHECK_LEVEL_DISABLE) {
        return LOS_ERRNO_MEMCHECK_DISABLED;
    }

    if ((pool == NULL) || (ptr == NULL) || (totalSize == NULL) || (availSize == NULL)) {
        return LOS_ERRNO_MEMCHECK_PARA_NULL;
    }

    endPool = (UINT8 *)pool + poolInfo->poolSize;
    if (!(OS_MEM_MIDDLE_ADDR_OPEN_END(pool, ptr, endPool))) {
        return LOS_ERRNO_MEMCHECK_OUTSIDE;
    }

    if (g_memCheckLevel == LOS_MEM_CHECK_LEVEL_HIGH) {
        head = OsMemFindNodeCtrl(pool, ptr);
        if ((head == NULL) ||
            (OS_MEM_NODE_GET_SIZE(((LosMemDynNode *)head)->selfNode.sizeAndFlag) < ((UINTPTR)ptr - (UINTPTR)head))) {
            return LOS_ERRNO_MEMCHECK_NO_HEAD;
        }
        *totalSize = OS_MEM_NODE_GET_SIZE(((LosMemDynNode *)head)->selfNode.sizeAndFlag - sizeof(LosMemDynNode));
        *availSize = OS_MEM_NODE_GET_SIZE(((LosMemDynNode *)head)->selfNode.sizeAndFlag - ((UINTPTR)ptr -
                                          (UINTPTR)head));
        return LOS_OK;
    }
    if (g_memCheckLevel == LOS_MEM_CHECK_LEVEL_LOW) {
        if (ptr != (VOID *)OS_MEM_ALIGN(ptr, OS_MEM_ALIGN_SIZE)) {
            return LOS_ERRNO_MEMCHECK_NO_HEAD;
        }
        head = (const VOID *)((UINTPTR)ptr - sizeof(LosMemDynNode));
        if (OS_MEM_MAGIC_VALID(((LosMemDynNode *)head)->selfNode.freeNodeInfo.pstPrev)) {
            *totalSize = OS_MEM_NODE_GET_SIZE(((LosMemDynNode *)head)->selfNode.sizeAndFlag - sizeof(LosMemDynNode));
            *availSize = OS_MEM_NODE_GET_SIZE(((LosMemDynNode *)head)->selfNode.sizeAndFlag - sizeof(LosMemDynNode));
            return LOS_OK;
        } else {
            return LOS_ERRNO_MEMCHECK_NO_HEAD;
        }
    }

    return LOS_ERRNO_MEMCHECK_WRONG_LEVEL;
}

/*
 * Description : get a pool's memCtrl
 * Input       : ptr -- point to source ptr
 * Return      : search forward for ptr's memCtrl or "NULL"
 * attention : this func couldn't ensure the return memCtrl belongs to ptr it just find forward the most nearly one
 */
LITE_OS_SEC_TEXT_MINOR const VOID *OsMemFindNodeCtrl(const VOID *pool, const VOID *ptr)
{
    const VOID *head = ptr;

    if (ptr == NULL) {
        return NULL;
    }

    head = (const VOID *)OS_MEM_ALIGN(head, OS_MEM_ALIGN_SIZE);
    while (!OS_MEM_MAGIC_VALID(((LosMemDynNode *)head)->selfNode.freeNodeInfo.pstPrev)) {
        head = (const VOID *)((UINT8 *)head - sizeof(CHAR *));
        if (head <= pool) {
            return NULL;
        }
    }
    return head;
}
//设置内存检查级别
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemCheckLevelSet(UINT8 checkLevel)
{
    if (checkLevel == LOS_MEM_CHECK_LEVEL_LOW) {
        PRINTK("%s: LOS_MEM_CHECK_LEVEL_LOW \n", __FUNCTION__);
    } else if (checkLevel == LOS_MEM_CHECK_LEVEL_HIGH) {
        PRINTK("%s: LOS_MEM_CHECK_LEVEL_HIGH \n", __FUNCTION__);
    } else if (checkLevel == LOS_MEM_CHECK_LEVEL_DISABLE) {
        PRINTK("%s: LOS_MEM_CHECK_LEVEL_DISABLE \n", __FUNCTION__);
    } else {
        PRINTK("%s: wrong param, setting failed !! \n", __FUNCTION__);
        return LOS_ERRNO_MEMCHECK_WRONG_LEVEL;
    }
    g_memCheckLevel = checkLevel;
    return LOS_OK;
}
//获得内存检查级别
LITE_OS_SEC_TEXT_MINOR UINT8 LOS_MemCheckLevelGet(VOID)
{
    return g_memCheckLevel;
}


UINT32 OsMemSysNodeCheck(VOID *dstAddr, VOID *srcAddr, UINT32 nodeLength, UINT8 pos)
{
    UINT32 ret;
    UINT32 totalSize = 0;
    UINT32 availSize = 0;
    UINT8 *pool = m_aucSysMem1;
#ifdef LOSCFG_EXC_INTERACTION
    if ((UINTPTR)dstAddr < ((UINTPTR)m_aucSysMem0 + OS_EXC_INTERACTMEM_SIZE)) {
        pool = m_aucSysMem0;
    }
#endif
    if (pos == 0) { /* if this func was called by memset */
        ret = LOS_MemNodeSizeCheck(pool, dstAddr, &totalSize, &availSize);
        if ((ret == LOS_OK) && (nodeLength > availSize)) {
            PRINT_ERR("---------------------------------------------\n");
            PRINT_ERR("memset: dst inode availSize is not enough"
                      " availSize = 0x%x, memset length = 0x%x\n", availSize, nodeLength);
            OsBackTrace();
            PRINT_ERR("---------------------------------------------\n");
            return LOS_NOK;
        }
    } else if (pos == 1) { /* if this func was called by memcpy */
        ret = LOS_MemNodeSizeCheck(pool, dstAddr, &totalSize, &availSize);
        if ((ret == LOS_OK) && (nodeLength > availSize)) {
            PRINT_ERR("---------------------------------------------\n");
            PRINT_ERR("memcpy: dst inode availSize is not enough"
                      " availSize = 0x%x, memcpy length = 0x%x\n", availSize, nodeLength);
            OsBackTrace();
            PRINT_ERR("---------------------------------------------\n");
            return LOS_NOK;
        }
#ifdef LOSCFG_EXC_INTERACTION
        if ((UINTPTR)srcAddr < ((UINTPTR)m_aucSysMem0 + OS_EXC_INTERACTMEM_SIZE)) {
            pool = m_aucSysMem0;
        } else {
            pool = m_aucSysMem1;
        }
#endif
        ret = LOS_MemNodeSizeCheck(pool, srcAddr, &totalSize, &availSize);
        if ((ret == LOS_OK) && (nodeLength > availSize)) {
            PRINT_ERR("---------------------------------------------\n");
            PRINT_ERR("memcpy: src inode availSize is not enough"
                      " availSize = 0x%x, memcpy length = 0x%x\n",
                      availSize, nodeLength);
            OsBackTrace();
            PRINT_ERR("---------------------------------------------\n");
            return LOS_NOK;
        }
    }
    return LOS_OK;
}
#endif /* LOSCFG_BASE_MEM_NODE_SIZE_CHECK */

#ifdef LOSCFG_MEM_MUL_MODULE
STATIC INLINE UINT32 OsMemModCheck(UINT32 moduleID)
{
    if (moduleID > MEM_MODULE_MAX) {
        PRINT_ERR("error module ID input!\n");
        return LOS_NOK;
    }
    return LOS_OK;
}

STATIC INLINE VOID *OsMemPtrToNode(VOID *ptr)
{
    UINT32 gapSize;

    if ((UINTPTR)ptr & (OS_MEM_ALIGN_SIZE - 1)) {
        PRINT_ERR("[%s:%d]ptr:%p not align by 4byte\n", __FUNCTION__, __LINE__, ptr);
        return NULL;
    }

    gapSize = *((UINT32 *)((UINTPTR)ptr - sizeof(UINT32)));
    if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize) && OS_MEM_NODE_GET_USED_FLAG(gapSize)) {
        PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
        return NULL;
    }
    if (OS_MEM_NODE_GET_ALIGNED_FLAG(gapSize)) {
        gapSize = OS_MEM_NODE_GET_ALIGNED_GAPSIZE(gapSize);
        if ((gapSize & (OS_MEM_ALIGN_SIZE - 1)) || (gapSize > ((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE))) {
            PRINT_ERR("[%s:%d]gapSize:0x%x error\n", __FUNCTION__, __LINE__, gapSize);
            return NULL;
        }

        ptr = (VOID *)((UINTPTR)ptr - gapSize);
    }

    return (VOID *)((UINTPTR)ptr - OS_MEM_NODE_HEAD_SIZE);
}

STATIC INLINE UINT32 OsMemNodeSizeGet(VOID *ptr)
{
    LosMemDynNode *node = (LosMemDynNode *)OsMemPtrToNode(ptr);
    if (node == NULL) {
        return 0;
    }

    return OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag);
}
//从指定动态内存池分配size长度的内存给指定模块，并纳入模块统计
VOID *LOS_MemMalloc(VOID *pool, UINT32 size, UINT32 moduleID)
{
    UINT32 intSave;
    VOID *ptr = NULL;
    VOID *node = NULL;
    if (OsMemModCheck(moduleID) == LOS_NOK) {
        return NULL;
    }
    ptr = LOS_MemAlloc(pool, size);
    if (ptr != NULL) {
        MEM_LOCK(intSave);
        g_moduleMemUsedSize[moduleID] += OsMemNodeSizeGet(ptr);
        node = OsMemPtrToNode(ptr);
        if (node != NULL) {
            OS_MEM_MODID_SET(node, moduleID);
        }
        MEM_UNLOCK(intSave);
    }
    return ptr;
}
//从指定动态内存池中申请长度为size且地址按boundary字节对齐的内存给指定模块，并纳入模块统计
VOID *LOS_MemMallocAlign(VOID *pool, UINT32 size, UINT32 boundary, UINT32 moduleID)
{
    UINT32 intSave;
    VOID *ptr = NULL;
    VOID *node = NULL;
    if (OsMemModCheck(moduleID) == LOS_NOK) {
        return NULL;
    }
    ptr = LOS_MemAllocAlign(pool, size, boundary);
    if (ptr != NULL) {
        MEM_LOCK(intSave);
        g_moduleMemUsedSize[moduleID] += OsMemNodeSizeGet(ptr);
        node = OsMemPtrToNode(ptr);
        if (node != NULL) {
            OS_MEM_MODID_SET(node, moduleID);
        }
        MEM_UNLOCK(intSave);
    }
    return ptr;
}
//释放已经申请的内存块，并纳入模块统计
UINT32 LOS_MemMfree(VOID *pool, VOID *ptr, UINT32 moduleID)
{
    UINT32 intSave;
    UINT32 ret;
    UINT32 size;
    LosMemDynNode *node = NULL;

    if ((OsMemModCheck(moduleID) == LOS_NOK) || (ptr == NULL) || (pool == NULL)) {
        return LOS_NOK;
    }

    node = (LosMemDynNode *)OsMemPtrToNode(ptr);
    if (node == NULL) {
        return LOS_NOK;
    }

    size = OS_MEM_NODE_GET_SIZE(node->selfNode.sizeAndFlag);

    if (moduleID != OS_MEM_MODID_GET(node)) {
        PRINT_ERR("node[%p] alloced in module %lu, but free in module %u\n node's taskID: 0x%x\n",
                  ptr, OS_MEM_MODID_GET(node), moduleID, OS_MEM_TASKID_GET(node));
        moduleID = OS_MEM_MODID_GET(node);
    }

    ret = LOS_MemFree(pool, ptr);
    if (ret == LOS_OK) {
        MEM_LOCK(intSave);
        g_moduleMemUsedSize[moduleID] -= size;
        MEM_UNLOCK(intSave);
    }
    return ret;
}
//按size大小重新分配内存块给指定模块，并将原内存块内容拷贝到新内存块，同时纳入模块统计。如果新内存块申请成功，则释放原内存块
VOID *LOS_MemMrealloc(VOID *pool, VOID *ptr, UINT32 size, UINT32 moduleID)
{
    VOID *newPtr = NULL;
    UINT32 oldNodeSize;
    UINT32 intSave;
    LosMemDynNode *node = NULL;
    UINT32 oldModuleID = moduleID;

    if ((OsMemModCheck(moduleID) == LOS_NOK) || (pool == NULL)) {
        return NULL;
    }

    if (ptr == NULL) {
        return LOS_MemMalloc(pool, size, moduleID);
    }

    node = (LosMemDynNode *)OsMemPtrToNode(ptr);
    if (node == NULL) {
        return NULL;
    }

    if (moduleID != OS_MEM_MODID_GET(node)) {
        PRINT_ERR("a node[%p] alloced in module %lu, but realloc in module %u\n node's taskID: %lu\n",
                  ptr, OS_MEM_MODID_GET(node), moduleID, OS_MEM_TASKID_GET(node));
        oldModuleID = OS_MEM_MODID_GET(node);
    }

    if (size == 0) {
        (VOID)LOS_MemMfree(pool, ptr, oldModuleID);
        return NULL;
    }

    oldNodeSize = OsMemNodeSizeGet(ptr);
    newPtr = LOS_MemRealloc(pool, ptr, size);
    if (newPtr != NULL) {
        MEM_LOCK(intSave);
        g_moduleMemUsedSize[moduleID] += OsMemNodeSizeGet(newPtr);
        g_moduleMemUsedSize[oldModuleID] -= oldNodeSize;
        node = (LosMemDynNode *)OsMemPtrToNode(newPtr);
        OS_MEM_MODID_SET(node, moduleID);
        MEM_UNLOCK(intSave);
    }
    return newPtr;
}
//获取指定模块的内存使用量，仅打开LOSCFG_MEM_MUL_MODULE时有效
UINT32 LOS_MemMusedGet(UINT32 moduleID)
{
    if (OsMemModCheck(moduleID) == LOS_NOK) {
        return OS_NULL_INT;
    }
    return g_moduleMemUsedSize[moduleID];
}
#endif
//初始化内核堆空间
STATUS_T OsKHeapInit(size_t size)
{
    STATUS_T ret;
    VOID *ptr = NULL;
    /*
     * roundup to MB aligned in order to set kernel attributes. kernel text/code/data attributes
     * should page mapping, remaining region should section mapping. so the boundary should be
     * MB aligned.
     */
     //向上舍入到MB对齐是为了设置内核属性。内核文本/代码/数据属性使用页映射，其余区域使用段映射,所以边界应该对齐。
    UINTPTR end = ROUNDUP(g_vmBootMemBase + size, MB);//用M是因为采用section mapping 鸿蒙内核源码分析(内存映射篇)有阐述
    size = end - g_vmBootMemBase;
    ptr = OsVmBootMemAlloc(size);//因刚开机，使用引导分配器分配
    if (!ptr) {
        PRINT_ERR("vmm_kheap_init boot_alloc_mem failed! %d\n", size);
        return -1;
    }

    m_aucSysMem0 = m_aucSysMem1 = ptr;//内存池基地址，auc 是啥意思没整明白？
    ret = LOS_MemInit(m_aucSysMem0, size);//初始化 m_aucSysMem0 内存池
    if (ret != LOS_OK) {
        PRINT_ERR("vmm_kheap_init LOS_MemInit failed!\n");
        g_vmBootMemBase -= size;//分配失败时需归还size, g_vmBootMemBase是很野蛮粗暴的
        return ret;
    }
    LOS_MemExpandEnable(OS_SYS_MEM_ADDR);//地址可扩展
    return LOS_OK;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
