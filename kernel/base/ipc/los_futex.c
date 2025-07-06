/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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
/*!
 * @file    los_futex.c
 * @brief
 * @link mutex http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-basic-trans-user-mutex.html @endlink
 * @link d17a6152740c https://www.jianshu.com/p/d17a6152740c @endlink
   @verbatim
   Futex 由一块能够被多个进程共享的内存空间（一个对齐后的整型变量）组成；这个整型变量的值能够通过汇编语言调用CPU提供的原子操作指令来增加或减少，
   并且一个进程可以等待直到那个值变成正数。Futex 的操作几乎全部在用户空间完成；只有当操作结果不一致从而需要仲裁时，才需要进入操作系统内核空间执行。
   这种机制允许使用 futex 的锁定原语有非常高的执行效率：由于绝大多数的操作并不需要在多个进程之间进行仲裁，所以绝大多数操作都可以在应用程序空间执行，
   而不需要使用（相对高代价的）内核系统调用。
   
   基本概念
	   Futex(Fast userspace mutex，用户态快速互斥锁)是内核提供的一种系统调用能力，通常作为基础组件与用户态的相关
	   锁逻辑结合组成用户态锁，是一种用户态与内核态共同作用的锁，例如用户态mutex锁、barrier与cond同步锁、读写锁。
	   其用户态部分负责锁逻辑，内核态部分负责锁调度。
	   
	   当用户态线程请求锁时，先在用户态进行锁状态的判断维护，若此时不产生锁的竞争，则直接在用户态进行上锁返回；
	   反之，则需要进行线程的挂起操作，通过Futex系统调用请求内核介入来挂起线程，并维护阻塞队列。
	   
	   当用户态线程释放锁时，先在用户态进行锁状态的判断维护，若此时没有其他线程被该锁阻塞，则直接在用户态进行解锁返回；
	   反之，则需要进行阻塞线程的唤醒操作，通过Futex系统调用请求内核介入来唤醒阻塞队列中的线程。
   历史
	   futex (fast userspace mutex) 是Linux的一个基础组件，可以用来构建各种更高级别的同步机制，比如锁或者信号量等等，
	   POSIX信号量就是基于futex构建的。大多数时候编写应用程序并不需要直接使用futex，一般用基于它所实现的系统库就够了。

 	   传统的SystemV IPC(inter process communication)进程间同步机制都是通过内核对象来实现的，以 semaphore 为例，
 	   当进程间要同步的时候，必须通过系统调用semop(2)进入内核进行PV操作。系统调用的缺点是开销很大，需要从user mode
 	   切换到kernel mode、保存寄存器状态、从user stack切换到kernel stack、等等，通常要消耗上百条指令。事实上，
 	   有一部分系统调用是可以避免的，因为现实中很多同步操作进行的时候根本不存在竞争，即某个进程从持有semaphore直至
 	   释放semaphore的这段时间内，常常没有其它进程对同一semaphore有需求，在这种情况下，内核的参与本来是不必要的，
 	   可是在传统机制下，持有semaphore必须先调用semop(2)进入内核去看看有没有人和它竞争，释放semaphore也必须调用semop(2)
 	   进入内核去看看有没有人在等待同一semaphore，这些不必要的系统调用造成了大量的性能损耗。  
   设计思想
   	   futex的解决思路是：在无竞争的情况下操作完全在user space进行，不需要系统调用，仅在发生竞争的时候进入内核去完成
   	   相应的处理(wait 或者 wake up)。所以说，futex是一种user mode和kernel mode混合的同步机制，需要两种模式合作才能完成，
   	   futex变量必须位于user space，而不是内核对象，futex的代码也分为user mode和kernel mode两部分，无竞争的情况下在user mode，
   	   发生竞争时则通过sys_futex系统调用进入kernel mode进行处理
   运行机制
   	   当用户态产生锁的竞争或释放需要进行相关线程的调度操作时，会触发Futex系统调用进入内核，此时会将用户态锁的地址
   	   传入内核，并在内核的Futex中以锁地址来区分用户态的每一把锁，因为用户态可用虚拟地址空间为1GiB，为了便于查找、
   	   管理，内核Futex采用哈希桶来存放用户态传入的锁。

	   当前哈希桶共有80个，0~63号桶用于存放私有锁（以虚拟地址进行哈希），64~79号桶用于存放共享锁（以物理地址进行哈希），
	   私有/共享属性通过用户态锁的初始化以及Futex系统调用入参确定。

	   如下图: 每个futex哈希桶中存放被futex_list串联起来的哈希值相同的futex node，每个futex node对应一个被挂起的task，
	   node中key值唯一标识一把用户态锁，具有相同key值的node被queue_list串联起来表示被同一把锁阻塞的task队列。
   @endverbatim
   @image html https://gitee.com/weharmonyos/resources/raw/master/81/futex.png
 * @attention Futex系统调用通常与用户态逻辑共同组成用户态锁，故推荐使用用户态POSIX接口的锁  
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-23
 */
#include "los_futex_pri.h"
#include "los_exc.h"
#include "los_hash.h"
#include "los_init.h"
#include "los_process_pri.h"
#include "los_sched_pri.h"
#include "los_sys_pri.h"
#include "los_mp.h"
#include "los_mux_pri.h"
#include "user_copy.h"

#ifdef LOSCFG_KERNEL_VM

/**
 * @brief 从futexList链表项获取FutexNode指针
 * @param ptr LOS_DL_LIST链表项指针
 * @return FutexNode* - 指向FutexNode结构体的指针
 */
#define OS_FUTEX_FROM_FUTEXLIST(ptr) LOS_DL_LIST_ENTRY(ptr, FutexNode, futexList)

/**
 * @brief 从queueList链表项获取FutexNode指针
 * @param ptr LOS_DL_LIST链表项指针
 * @return FutexNode* - 指向FutexNode结构体的指针
 */
#define OS_FUTEX_FROM_QUEUELIST(ptr) LOS_DL_LIST_ENTRY(ptr, FutexNode, queueList)

/**
 * @brief 用户地址空间基地址
 */
#define OS_FUTEX_KEY_BASE USER_ASPACE_BASE

/**
 * @brief 用户地址空间最大地址
 */
#define OS_FUTEX_KEY_MAX (USER_ASPACE_BASE + USER_ASPACE_SIZE)

/* private: 0~63    hash index_num
 * shared:  64~79   hash index_num */
/**
 * @brief 私有futex哈希表索引最大值
 */
#define FUTEX_INDEX_PRIVATE_MAX     64

/**
 * @brief 共享futex哈希表索引最大值
 */
#define FUTEX_INDEX_SHARED_MAX      16

/**
 * @brief 哈希表索引最大值
 */
#define FUTEX_INDEX_MAX             (FUTEX_INDEX_PRIVATE_MAX + FUTEX_INDEX_SHARED_MAX)

/**
 * @brief 共享futex哈希表起始索引位置
 */
#define FUTEX_INDEX_SHARED_POS      FUTEX_INDEX_PRIVATE_MAX

/**
 * @brief 私有futex哈希掩码
 */
#define FUTEX_HASH_PRIVATE_MASK     (FUTEX_INDEX_PRIVATE_MAX - 1)

/**
 * @brief 共享futex哈希掩码
 */
#define FUTEX_HASH_SHARED_MASK      (FUTEX_INDEX_SHARED_MAX - 1)

/**
 * @brief Futex哈希表节点结构体
 */
typedef struct {
    LosMux      listLock;         /**< 哈希表项链表锁 */
    LOS_DL_LIST lockList;         /**< futex节点链表 */
} FutexHash;

/**
 * @brief Futex哈希表全局实例
 */
FutexHash g_futexHash[FUTEX_INDEX_MAX];

/**
 * @brief 加锁futex哈希表锁
 * @param lock 指向LosMux的指针
 * @return INT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsFutexLock(LosMux *lock)
{
    UINT32 ret = LOS_MuxLock(lock, LOS_WAIT_FOREVER);
    if (ret != LOS_OK) {
        PRINT_ERR("Futex lock failed! ERROR: 0x%x!\n", ret);
        return LOS_EINVAL;
    }
    return LOS_OK;
}

/**
 * @brief 解锁futex哈希表锁
 * @param lock 指向LosMux的指针
 * @return INT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsFutexUnlock(LosMux *lock)
{
    UINT32 ret = LOS_MuxUnlock(lock);
    if (ret != LOS_OK) {
        PRINT_ERR("Futex unlock failed! ERROR: 0x%x!\n", ret);
        return LOS_EINVAL;
    }
    return LOS_OK;
}

/**
 * @brief 初始化futex哈希表
 * @details 初始化所有哈希表项的链表和互斥锁
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsFutexInit(VOID)
{
    INT32 count;
    UINT32 ret;

    // 遍历所有哈希表项，初始化链表和互斥锁
    for (count = 0; count < FUTEX_INDEX_MAX; count++) {
        LOS_ListInit(&g_futexHash[count].lockList);
        ret = LOS_MuxInit(&(g_futexHash[count].listLock), NULL);
        if (ret) {
            return ret;
        }
    }

    return LOS_OK;
}

// 将OsFutexInit注册为内核扩展模块初始化函数
LOS_MODULE_INIT(OsFutexInit, LOS_INIT_LEVEL_KMOD_EXTENDED);

#ifdef LOS_FUTEX_DEBUG
/**
 * @brief 显示futex节点的任务属性
 * @param futexList 指向futex链表的指针
 */
STATIC VOID OsFutexShowTaskNodeAttr(const LOS_DL_LIST *futexList)
{
    FutexNode *tempNode = NULL;
    FutexNode *lastNode = NULL;
    LosTaskCB *taskCB = NULL;
    LOS_DL_LIST *queueList = NULL;

    tempNode = OS_FUTEX_FROM_FUTEXLIST(futexList);
    PRINTK("key(pid)           : 0x%x(%u) : ->", tempNode->key, tempNode->pid);

    // 遍历队列链表，打印任务ID
    for (queueList = &tempNode->queueList; ;) {
        lastNode = OS_FUTEX_FROM_QUEUELIST(queueList);
        if (!LOS_ListEmpty(&(lastNode->pendList))) {
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(lastNode->pendList)));
            PRINTK(" %u ->", taskCB->taskID);
        } else {
            taskCB = LOS_DL_LIST_ENTRY(lastNode, LosTaskCB, futex);
            PRINTK(" %u ->", taskCB->taskID);
        }
        queueList = queueList->pstNext;
        if (queueList == &tempNode->queueList) {
            break;
        }
    }
    PRINTK("\n");
}

/**
 * @brief 显示futex哈希表内容
 */
VOID OsFutexHashShow(VOID)
{
    LOS_DL_LIST *futexList = NULL;
    INT32 count;
    /* The maximum number of barrels of a hash table */
    INT32 hashNodeMax = FUTEX_INDEX_MAX;
    PRINTK("#################### los_futex_pri.hash ####################\n");
    // 遍历所有哈希表项，显示非空链表的内容
    for (count = 0; count < hashNodeMax; count++) {
        futexList = &(g_futexHash[count].lockList);
        if (LOS_ListEmpty(futexList)) {
            continue;
        }
        PRINTK("hash -> index : %d\n", count);
        for (futexList = futexList->pstNext;
             futexList != &(g_futexHash[count].lockList);
             futexList = futexList->pstNext) {
                OsFutexShowTaskNodeAttr(futexList);
        }
    }
}
#endif

/**
 * @brief 根据标志生成futex键
 * @param userVaddr 用户空间地址
 * @param flags futex标志
 * @return UINTPTR - 生成的futex键
 */
STATIC INLINE UINTPTR OsFutexFlagsToKey(const UINT32 *userVaddr, const UINT32 flags)
{
    UINTPTR futexKey;

    // 私有futex使用虚拟地址作为键，共享futex使用物理地址作为键
    if (flags & FUTEX_PRIVATE) {
        futexKey = (UINTPTR)userVaddr;
    } else {
        futexKey = (UINTPTR)LOS_PaddrQuery((UINT32 *)userVaddr);
    }

    return futexKey;
}

/**
 * @brief 将futex键转换为哈希表索引
 * @param futexKey futex键
 * @param flags futex标志
 * @return UINT32 - 哈希表索引
 */
STATIC INLINE UINT32 OsFutexKeyToIndex(const UINTPTR futexKey, const UINT32 flags)
{
    UINT32 index = LOS_HashFNV32aBuf(&futexKey, sizeof(UINTPTR), FNV1_32A_INIT);

    // 根据私有/共享标志计算不同的哈希索引
    if (flags & FUTEX_PRIVATE) {
        index &= FUTEX_HASH_PRIVATE_MASK;
    } else {
        index &= FUTEX_HASH_SHARED_MASK;
        index += FUTEX_INDEX_SHARED_POS;
    }

    return index;
}

/**
 * @brief 设置futex节点的键信息
 * @param futexKey futex键
 * @param flags futex标志
 * @param node 指向FutexNode的指针
 */
STATIC INLINE VOID OsFutexSetKey(UINTPTR futexKey, UINT32 flags, FutexNode *node)
{
    node->key = futexKey;
    node->index = OsFutexKeyToIndex(futexKey, flags);
    // 私有futex设置当前进程ID，共享futex设置为无效值
    node->pid = (flags & FUTEX_PRIVATE) ? LOS_GetCurrProcessID() : OS_INVALID;
}

/**
 * @brief 反初始化futex节点
 * @param node 指向FutexNode的指针
 */
STATIC INLINE VOID OsFutexDeinitFutexNode(FutexNode *node)
{
    node->index = OS_INVALID_VALUE;
    node->pid = 0;
    LOS_ListDelete(&node->queueList);
}

/**
 * @brief 替换队列链表的头节点
 * @param oldHeadNode 旧头节点
 * @param newHeadNode 新头节点
 */
STATIC INLINE VOID OsFutexReplaceQueueListHeadNode(FutexNode *oldHeadNode, FutexNode *newHeadNode)
{
    LOS_DL_LIST *futexList = oldHeadNode->futexList.pstPrev;
    LOS_ListDelete(&oldHeadNode->futexList);
    LOS_ListHeadInsert(futexList, &newHeadNode->futexList);
    // 如果新头节点的队列链表未初始化，则进行初始化
    if ((newHeadNode->queueList.pstNext == NULL) || (newHeadNode->queueList.pstPrev == NULL)) {
        LOS_ListInit(&newHeadNode->queueList);
    }
}

/**
 * @brief 从futex链表中删除节点
 * @param node 指向FutexNode的指针
 */
STATIC INLINE VOID OsFutexDeleteKeyFromFutexList(FutexNode *node)
{
    LOS_ListDelete(&node->futexList);
}

/**
 * @brief 从哈希表中删除futex键节点
 * @param node 指向FutexNode的指针
 * @param isDeleteHead 是否删除头节点
 * @param headNode 头节点指针的指针
 * @param queueFlags 队列标志的指针
 */
STATIC VOID OsFutexDeleteKeyNodeFromHash(FutexNode *node, BOOL isDeleteHead, FutexNode **headNode, BOOL *queueFlags)
{
    FutexNode *nextNode = NULL;

    // 检查索引是否有效
    if (node->index >= FUTEX_INDEX_MAX) {
        return;
    }

    // 如果队列为空，直接从哈希表中删除该节点
    if (LOS_ListEmpty(&node->queueList)) {
        OsFutexDeleteKeyFromFutexList(node);
        if (queueFlags != NULL) {
            *queueFlags = TRUE;
        }
        goto EXIT;
    }

    /* FutexList is not NULL, but the header node of queueList */
    if (node->futexList.pstNext != NULL) {
        if (isDeleteHead == TRUE) {
            nextNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_FIRST(&node->queueList));
            OsFutexReplaceQueueListHeadNode(node, nextNode);
            if (headNode != NULL) {
                *headNode = nextNode;
            }
        } else {
            return;
        }
    }

EXIT:
    OsFutexDeinitFutexNode(node);
    return;
}

/**
 * @brief 从futex哈希表中删除节点
 * @param node 指向FutexNode的指针
 * @param isDeleteHead 是否删除头节点
 * @param headNode 头节点指针的指针
 * @param queueFlags 队列标志的指针
 */
VOID OsFutexNodeDeleteFromFutexHash(FutexNode *node, BOOL isDeleteHead, FutexNode **headNode, BOOL *queueFlags)
{
    FutexHash *hashNode = NULL;

    UINT32 index = OsFutexKeyToIndex(node->key, (node->pid == OS_INVALID) ? 0 : FUTEX_PRIVATE);
    if (index >= FUTEX_INDEX_MAX) {
        return;
    }

    hashNode = &g_futexHash[index];
    if (OsMuxLockUnsafe(&hashNode->listLock, LOS_WAIT_FOREVER)) {
        return;
    }

    // 验证节点索引是否匹配当前哈希表索引
    if (node->index != index) {
        goto EXIT;
    }

    OsFutexDeleteKeyNodeFromHash(node, isDeleteHead, headNode, queueFlags);

EXIT:
    if (OsMuxUnlockUnsafe(OsCurrTaskGet(), &hashNode->listLock, NULL)) {
        return;
    }

    return;
}

/**
 * @brief 删除已唤醒的任务并获取下一个节点
 * @param node 指向FutexNode的指针
 * @param headNode 头节点指针的指针
 * @param isDeleteHead 是否删除头节点
 * @return FutexNode* - 下一个有效的FutexNode指针
 */
STATIC FutexNode *OsFutexDeleteAlreadyWakeTaskAndGetNext(const FutexNode *node, FutexNode **headNode, BOOL isDeleteHead)
{
    FutexNode *tempNode = (FutexNode *)node;
    FutexNode *nextNode = NULL;
    BOOL queueFlag = FALSE;

    // 循环删除已唤醒的任务节点（pendList为空表示已唤醒）
    while (LOS_ListEmpty(&(tempNode->pendList))) {     /* already weak */
        if (!LOS_ListEmpty(&(tempNode->queueList))) { /* It's not a head node */
            nextNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_FIRST(&(tempNode->queueList)));
        }

        OsFutexDeleteKeyNodeFromHash(tempNode, isDeleteHead, headNode, &queueFlag);
        if (queueFlag) {
            return NULL;
        }

        tempNode = nextNode;
    }

    return tempNode;
}

/**
 * @brief 将新的futex键插入哈希表
 * @param node 指向FutexNode的指针
 */
STATIC VOID OsFutexInsertNewFutexKeyToHash(FutexNode *node)
{
    FutexNode *headNode = NULL;
    FutexNode *tailNode = NULL;
    LOS_DL_LIST *futexList = NULL;
    FutexHash *hashNode = &g_futexHash[node->index];

    // 如果哈希表项为空，直接插入头节点
    if (LOS_ListEmpty(&hashNode->lockList)) {
        LOS_ListHeadInsert(&(hashNode->lockList), &(node->futexList));
        goto EXIT;
    }

    headNode = OS_FUTEX_FROM_FUTEXLIST(LOS_DL_LIST_FIRST(&(hashNode->lockList)));
    /* The small key is at the front of the queue */
    if (node->key < headNode->key) {
        LOS_ListHeadInsert(&(hashNode->lockList), &(node->futexList));
        goto EXIT;
    }

    tailNode = OS_FUTEX_FROM_FUTEXLIST(LOS_DL_LIST_LAST(&(hashNode->lockList)));
    if (node->key > tailNode->key) {
        LOS_ListTailInsert(&(hashNode->lockList), &(node->futexList));
        goto EXIT;
    }

    // 按key值大小插入到合适位置
    for (futexList = hashNode->lockList.pstNext;
         futexList != &(hashNode->lockList);
         futexList = futexList->pstNext) {
        headNode = OS_FUTEX_FROM_FUTEXLIST(futexList);
        if (node->key <= headNode->key) {
            LOS_ListTailInsert(&(headNode->futexList), &(node->futexList));
            break;
        }
    }

EXIT:
    return;
}

/**
 * @brief 从后向前查找并插入任务到队列
 * @param queueList 队列链表
 * @param runTask 当前运行任务的TCB
 * @param node 要插入的FutexNode
 * @return INT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsFutexInsertFindFormBackToFront(LOS_DL_LIST *queueList, const LosTaskCB *runTask, FutexNode *node)
{
    LOS_DL_LIST *listHead = queueList;
    LOS_DL_LIST *listTail = queueList->pstPrev;

    // 从后向前遍历队列，找到合适位置插入
    for (; listHead != listTail; listTail = listTail->pstPrev) {
        FutexNode *tempNode = OS_FUTEX_FROM_QUEUELIST(listTail);
        tempNode = OsFutexDeleteAlreadyWakeTaskAndGetNext(tempNode, NULL, FALSE);
        if (tempNode == NULL) {
            return LOS_NOK;
        }
        LosTaskCB *taskTail = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(tempNode->pendList)));
        INT32 ret = OsSchedParamCompare(runTask, taskTail);
        if (ret >= 0) {
            LOS_ListHeadInsert(&(tempNode->queueList), &(node->queueList));
            return LOS_OK;
        } else {
            if (listTail->pstPrev == listHead) {
                LOS_ListTailInsert(&(tempNode->queueList), &(node->queueList));
                return LOS_OK;
            }
        }
    }

    return LOS_NOK;
}

/**
 * @brief 从前向后查找并插入任务到队列
 * @param queueList 队列链表
 * @param runTask 当前运行任务的TCB
 * @param node 要插入的FutexNode
 * @return INT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsFutexInsertFindFromFrontToBack(LOS_DL_LIST *queueList, const LosTaskCB *runTask, FutexNode *node)
{
    LOS_DL_LIST *listHead = queueList;
    LOS_DL_LIST *listTail = queueList->pstPrev;

    // 从前向后遍历队列，找到合适位置插入
    for (; listHead != listTail; listHead = listHead->pstNext) {
        FutexNode *tempNode = OS_FUTEX_FROM_QUEUELIST(listHead);
        tempNode = OsFutexDeleteAlreadyWakeTaskAndGetNext(tempNode, NULL, FALSE);
        if (tempNode == NULL) {
            return LOS_NOK;
        }
        LosTaskCB *taskHead = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(tempNode->pendList)));
        /* High priority comes before low priority,
         * in the case of the same priority, after the current node
         */
        INT32 ret = OsSchedParamCompare(runTask, taskHead);
        if (ret >= 0) {
            if (listHead->pstNext == listTail) {
                LOS_ListHeadInsert(&(tempNode->queueList), &(node->queueList));
                return LOS_OK;
            }
            continue;
        } else {
            LOS_ListTailInsert(&(tempNode->queueList), &(node->queueList));
            return LOS_OK;
        }
    }

    return LOS_NOK;
}

/**
 * @brief 回收并查找头节点
 * @param headNode 头节点指针
 * @param node 新节点指针
 * @param firstNode 输出参数，指向找到的第一个有效节点
 * @return INT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsFutexRecycleAndFindHeadNode(FutexNode *headNode, FutexNode *node, FutexNode **firstNode)
{
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    *firstNode = OsFutexDeleteAlreadyWakeTaskAndGetNext(headNode, NULL, TRUE);
    SCHEDULER_UNLOCK(intSave);

    /* The head node is removed and there was originally only one node under the key */
    if (*firstNode == NULL) {
        OsFutexInsertNewFutexKeyToHash(node);
        LOS_ListInit(&(node->queueList));
        return LOS_OK;
    }

    return LOS_OK;
}

/**
 * @brief 将任务插入到等待队列
 * @param firstNode 第一个有效节点指针的指针
 * @param node 要插入的节点
 * @param run 当前运行任务的TCB
 * @return INT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsFutexInsertTasktoPendList(FutexNode **firstNode, FutexNode *node, const LosTaskCB *run)
{
    LosTaskCB *taskHead = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&((*firstNode)->pendList)));
    LOS_DL_LIST *queueList = &((*firstNode)->queueList);

    INT32 ret1 = OsSchedParamCompare(run, taskHead);
    if (ret1 < 0) {
        /* The one with the highest priority is inserted at the top of the queue */
        LOS_ListTailInsert(queueList, &(node->queueList));
        OsFutexReplaceQueueListHeadNode(*firstNode, node);
        *firstNode = node;
        return LOS_OK;
    }

    if (LOS_ListEmpty(queueList) && (ret1 >= 0)) {
        /* Insert the next position in the queue with equal priority */
        LOS_ListHeadInsert(queueList, &(node->queueList));
        return LOS_OK;
    }

    FutexNode *tailNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_LAST(queueList));
    LosTaskCB *taskTail = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(tailNode->pendList)));
    INT32 ret2 = OsSchedParamCompare(taskTail, run);
    if ((ret2 <= 0) || (ret1 > ret2)) {
        return OsFutexInsertFindFormBackToFront(queueList, run, node);
    }

    return OsFutexInsertFindFromFrontToBack(queueList, run, node);
}

/**
 * @brief 查找futex节点
 * @param node 要查找的节点信息
 * @return FutexNode* - 找到的节点指针，NULL表示未找到
 */
STATIC FutexNode *OsFindFutexNode(const FutexNode *node)
{
    FutexHash *hashNode = &g_futexHash[node->index];
    LOS_DL_LIST *futexList = &(hashNode->lockList);
    FutexNode *headNode = NULL;

    // 遍历哈希表项的链表，查找key和pid匹配的节点
    for (futexList = futexList->pstNext;
         futexList != &(hashNode->lockList);
         futexList = futexList->pstNext) {
        headNode = OS_FUTEX_FROM_FUTEXLIST(futexList);
        if ((headNode->key == node->key) && (headNode->pid == node->pid)) {
            return headNode;
        }
    }

    return NULL;
}

/**
 * @brief 查找并插入节点到哈希表
 * @param node 要插入的节点
 * @return INT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsFindAndInsertToHash(FutexNode *node)
{
    FutexNode *headNode = NULL;
    FutexNode *firstNode = NULL;
    UINT32 intSave;
    INT32 ret;

    headNode = OsFindFutexNode(node);
    if (headNode == NULL) {
        // 未找到节点，插入新节点
        OsFutexInsertNewFutexKeyToHash(node);
        LOS_ListInit(&(node->queueList));
        return LOS_OK;
    }

    // 回收并查找头节点
    ret = OsFutexRecycleAndFindHeadNode(headNode, node, &firstNode);
    if (ret != LOS_OK) {
        return ret;
    } else if (firstNode == NULL) {
        return ret;
    }

    // 加调度锁，插入任务到等待队列
    SCHEDULER_LOCK(intSave);
    ret = OsFutexInsertTasktoPendList(&firstNode, node, OsCurrTaskGet());
    SCHEDULER_UNLOCK(intSave);

    return ret;
}
/**
 * @brief 检查共享内存互斥锁权限
 * @details 验证共享内存区域的互斥锁键是否具有正确的访问权限
 * @param userVaddr 用户空间地址指针
 * @param flags futex操作标志
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsFutexKeyShmPermCheck(const UINT32 *userVaddr, const UINT32 flags)
{
    PADDR_T paddr; // 物理地址变量

    /* 检查futexKey是否为共享锁 */
    if (!(flags & FUTEX_PRIVATE)) {
        // 查询用户空间地址对应的物理地址
        paddr = (UINTPTR)LOS_PaddrQuery((UINT32 *)userVaddr);
        if (paddr == 0) return LOS_NOK; // 物理地址无效，返回错误
    }

    return LOS_OK; // 权限检查通过
}

/**
 * @brief 检查Futex等待操作的参数有效性
 * @details 验证futex等待操作的输入参数，包括地址对齐、权限和标志位
 * @param userVaddr 用户空间地址指针
 * @param flags futex操作标志
 * @param absTime 绝对超时时间
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
STATIC INT32 OsFutexWaitParamCheck(const UINT32 *userVaddr, UINT32 flags, UINT32 absTime)
{
    VADDR_T vaddr = (VADDR_T)(UINTPTR)userVaddr; // 转换为虚拟地址

    if (OS_INT_ACTIVE) { // 检查是否在中断上下文中
        return LOS_EINTR; // 中断上下文中不允许等待，返回中断错误
    }

    // 检查标志位是否包含无效标志
    if (flags & (~FUTEX_PRIVATE)) {
        PRINT_ERR("Futex wait param check failed! error flags: 0x%x\n", flags);
        return LOS_EINVAL; // 返回无效参数错误
    }

    // 检查地址是否按INT32对齐且在有效范围内
    if ((vaddr % sizeof(INT32)) || (vaddr < OS_FUTEX_KEY_BASE) || (vaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex wait param check failed! error userVaddr: 0x%x\n", vaddr);
        return LOS_EINVAL; // 返回无效地址错误
    }

    // 检查共享内存权限
    if (flags && (OsFutexKeyShmPermCheck(userVaddr, flags) != LOS_OK)) {
        PRINT_ERR("Futex wait param check failed! error shared memory perm userVaddr: 0x%x\n", userVaddr);
        return LOS_EINVAL; // 返回权限错误
    }

    if (!absTime) { // 检查超时时间是否有效
        return LOS_ETIMEDOUT; // 返回超时错误
    }

    return LOS_OK; // 参数检查通过
}

/**
 * @brief 删除超时的Futex任务节点
 * @details 从Futex哈希表中移除已超时的任务节点并清理资源
 * @param hashNode 哈希表节点指针
 * @param node 要删除的Futex节点
 * @return 始终返回LOS_ETIMEDOUT表示超时
 */
STATIC INT32 OsFutexDeleteTimeoutTaskNode(FutexHash *hashNode, FutexNode *node)
{
    UINT32 intSave; // 中断状态保存变量
    if (OsFutexLock(&hashNode->listLock)) { // 获取哈希表列表锁
        return LOS_EINVAL; // 加锁失败，返回错误
    }

    if (node->index < FUTEX_INDEX_MAX) { // 检查节点索引是否有效
        SCHEDULER_LOCK(intSave); // 锁定调度器
        // 删除已唤醒的任务并获取下一个节点
        (VOID)OsFutexDeleteAlreadyWakeTaskAndGetNext(node, NULL, TRUE);
        SCHEDULER_UNLOCK(intSave); // 解锁调度器
    }

#ifdef LOS_FUTEX_DEBUG
    OsFutexHashShow(); // 调试模式下显示哈希表状态
#endif

    if (OsFutexUnlock(&hashNode->listLock)) { // 释放哈希表列表锁
        return LOS_EINVAL; // 解锁失败，返回错误
    }
    return LOS_ETIMEDOUT; // 返回超时状态
}

/**
 * @brief 将任务插入Futex哈希表
 * @details 初始化任务的Futex节点并将其插入到对应的哈希表项中
 * @param taskCB 任务控制块指针的指针
 * @param node Futex节点指针的指针
 * @param futexKey Futex键值
 * @param flags futex操作标志
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsFutexInsertTaskToHash(LosTaskCB **taskCB, FutexNode **node, const UINTPTR futexKey, const UINT32 flags)
{
    INT32 ret; // 返回值
    *taskCB = OsCurrTaskGet(); // 获取当前任务控制块
    *node = &((*taskCB)->futex); // 获取任务的Futex节点
    OsFutexSetKey(futexKey, flags, *node); // 设置Futex键值

    ret = OsFindAndInsertToHash(*node); // 查找并插入到哈希表
    if (ret) {
        return LOS_NOK; // 插入失败，返回错误
    }

    LOS_ListInit(&((*node)->pendList)); // 初始化等待列表
    return LOS_OK; // 插入成功
}

/**
 * @brief 执行Futex等待操作
 * @details 实现futex等待逻辑，包括加锁、参数验证、任务阻塞和超时处理
 * @param userVaddr 用户空间地址指针
 * @param flags futex操作标志
 * @param val 预期的futex值
 * @param timeout 超时时间（滴答数）
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
STATIC INT32 OsFutexWaitTask(const UINT32 *userVaddr, const UINT32 flags, const UINT32 val, const UINT32 timeout)
{
    INT32 futexRet; // futex操作返回值
    UINT32 intSave, lockVal; // 中断状态和锁值
    LosTaskCB *taskCB = NULL; // 任务控制块指针
    FutexNode *node = NULL; // Futex节点指针
    UINTPTR futexKey = OsFutexFlagsToKey(userVaddr, flags); // 生成futex键
    UINT32 index = OsFutexKeyToIndex(futexKey, flags); // 计算哈希索引
    FutexHash *hashNode = &g_futexHash[index]; // 获取哈希表节点

    if (OsFutexLock(&hashNode->listLock)) { // 获取哈希表列表锁
        return LOS_EINVAL; // 加锁失败，返回错误
    }

    // 从用户空间复制锁值
    if (LOS_ArchCopyFromUser(&lockVal, userVaddr, sizeof(UINT32))) {
        PRINT_ERR("Futex wait param check failed! copy from user failed!\n");
        futexRet = LOS_EINVAL;
        goto EXIT_ERR; // 复制失败，跳转到错误处理
    }

    if (lockVal != val) { // 检查锁值是否与预期一致
        futexRet = LOS_EBADF;
        goto EXIT_ERR; // 值不匹配，跳转到错误处理
    }

    // 将任务插入哈希表
    if (OsFutexInsertTaskToHash(&taskCB, &node, futexKey, flags)) {
        futexRet = LOS_NOK;
        goto EXIT_ERR; // 插入失败，跳转到错误处理
    }

    SCHEDULER_LOCK(intSave); // 锁定调度器
    OsSchedLock(); // 锁定调度
    // 设置任务等待掩码和超时
    OsTaskWaitSetPendMask(OS_TASK_WAIT_FUTEX, futexKey, timeout);
    // 将任务加入等待队列
    taskCB->ops->wait(taskCB, &(node->pendList), timeout);
    LOS_SpinUnlock(&g_taskSpin); // 解锁任务自旋锁

    futexRet = OsFutexUnlock(&hashNode->listLock); // 释放哈希表列表锁
    if (futexRet) { // 解锁失败处理
        OsSchedUnlock(); // 解锁调度
        LOS_IntRestore(intSave); // 恢复中断
        goto EXIT_UNLOCK_ERR; // 跳转到解锁错误处理
    }

    LOS_SpinLock(&g_taskSpin); // 锁定任务自旋锁
    OsSchedUnlock(); // 解锁调度

    /*
     * 立即进行调度，因此不需要释放任务自旋锁。
     * 当任务被重新调度时，它将持有自旋锁。
     */
    OsSchedResched(); // 触发调度

    // 检查任务是否超时
    if (taskCB->taskStatus & OS_TASK_STATUS_TIMEOUT) {
        taskCB->taskStatus &= ~OS_TASK_STATUS_TIMEOUT; // 清除超时标志
        SCHEDULER_UNLOCK(intSave); // 解锁调度器
        // 删除超时任务节点
        return OsFutexDeleteTimeoutTaskNode(hashNode, node);
    }

    SCHEDULER_UNLOCK(intSave); // 解锁调度器
    return LOS_OK; // 等待成功

EXIT_ERR:
    (VOID)OsFutexUnlock(&hashNode->listLock); // 释放哈希表列表锁
EXIT_UNLOCK_ERR:
    return futexRet; // 返回错误码
}

/**
 * @brief Futex等待系统调用实现
 * @details 提供用户空间的futex等待接口，处理超时转换和参数验证
 * @param userVaddr 用户空间地址指针
 * @param flags futex操作标志
 * @param val 预期的futex值
 * @param absTime 绝对超时时间（微秒）
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
INT32 OsFutexWait(const UINT32 *userVaddr, UINT32 flags, UINT32 val, UINT32 absTime)
{
    INT32 ret; // 返回值
    UINT32 timeout = LOS_WAIT_FOREVER; // 超时时间（默认永久等待）

    // 检查等待参数
    ret = OsFutexWaitParamCheck(userVaddr, flags, absTime);
    if (ret) {
        return ret; // 参数错误，返回错误码
    }
    // 转换绝对时间为滴答数
    if (absTime != LOS_WAIT_FOREVER) {
        timeout = OsNS2Tick((UINT64)absTime * OS_SYS_NS_PER_US);
    }

    // 执行等待任务
    return OsFutexWaitTask(userVaddr, flags, val, timeout);
}

/**
 * @brief 检查Futex唤醒操作的参数有效性
 * @details 验证futex唤醒操作的输入参数，包括地址对齐、权限和标志位
 * @param userVaddr 用户空间地址指针
 * @param flags futex操作标志
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
STATIC INT32 OsFutexWakeParamCheck(const UINT32 *userVaddr, UINT32 flags)
{
    VADDR_T vaddr = (VADDR_T)(UINTPTR)userVaddr; // 转换为虚拟地址

    // 检查标志位是否有效
    if ((flags & (~FUTEX_PRIVATE)) != FUTEX_WAKE) {
        PRINT_ERR("Futex wake param check failed! error flags: 0x%x\n", flags);
        return LOS_EINVAL; // 标志无效，返回错误
    }

    // 检查地址是否按INT32对齐且在有效范围内
    if ((vaddr % sizeof(INT32)) || (vaddr < OS_FUTEX_KEY_BASE) || (vaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex wake param check failed! error userVaddr: 0x%x\n", userVaddr);
        return LOS_EINVAL; // 地址无效，返回错误
    }

    // 检查共享内存权限
    if (flags && (OsFutexKeyShmPermCheck(userVaddr, flags) != LOS_OK)) {
        PRINT_ERR("Futex wake param check failed! error shared memory perm userVaddr: 0x%x\n", userVaddr);
        return LOS_EINVAL; // 权限错误，返回错误
    }

    return LOS_OK; // 参数检查通过
}

/**
 * @brief 检查并唤醒等待的Futex任务
 * @details 遍历等待队列，唤醒指定数量的任务，并处理超时情况
 * @param headNode 等待队列头节点
 * @param wakeNumber 要唤醒的任务数量
 * @param hashNode 哈希表节点
 * @param nextNode 下一个节点指针的指针
 * @param wakeAny 是否有任务被唤醒的标志指针
 */
STATIC VOID OsFutexCheckAndWakePendTask(FutexNode *headNode, const INT32 wakeNumber,
                                        FutexHash *hashNode, FutexNode **nextNode, BOOL *wakeAny)
{
    INT32 count; // 计数器
    LosTaskCB *taskCB = NULL; // 任务控制块指针
    FutexNode *node = headNode; // 当前节点
    for (count = 0; count < wakeNumber; count++) {
        /* 确保头部完整性 */
        // 删除已唤醒任务并获取下一个节点
        *nextNode = OsFutexDeleteAlreadyWakeTaskAndGetNext(node, NULL, FALSE);
        if (*nextNode == NULL) {
            /* 队列中的最后一个节点无效或整个列表无效 */
            return;
        }
        node = *nextNode; // 移动到下一个节点
        // 从等待列表获取任务控制块
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(node->pendList)));
        OsTaskWakeClearPendMask(taskCB); // 清除任务等待掩码
        taskCB->ops->wake(taskCB); // 唤醒任务
        *wakeAny = TRUE; // 设置唤醒标志
        // 获取队列列表中的下一个节点
        *nextNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_FIRST(&(node->queueList)));
        if (node != headNode) {
            OsFutexDeinitFutexNode(node); // 释放非头节点资源
        }

        if (LOS_ListEmpty(&headNode->queueList)) {
            /* 唤醒整个链表节点 */
            *nextNode = NULL;
            return;
        }

        node = *nextNode; // 移动到下一个节点
    }
    return;
}

/**
 * @brief 执行Futex唤醒操作
 * @details 根据futex键查找并唤醒指定数量的等待任务
 * @param futexKey Futex键值
 * @param flags futex操作标志
 * @param wakeNumber 要唤醒的任务数量
 * @param newHeadNode 新的头节点指针的指针
 * @param wakeAny 是否有任务被唤醒的标志指针
 * @return 成功返回LOS_OK，失败返回LOS_EBADF
 */
STATIC INT32 OsFutexWakeTask(UINTPTR futexKey, UINT32 flags, INT32 wakeNumber, FutexNode **newHeadNode, BOOL *wakeAny)
{
    UINT32 intSave; // 中断状态保存变量
    FutexNode *node = NULL; // Futex节点指针
    FutexNode *headNode = NULL; // 头节点指针
    UINT32 index = OsFutexKeyToIndex(futexKey, flags); // 计算哈希索引
    FutexHash *hashNode = &g_futexHash[index]; // 获取哈希表节点
    // 初始化临时Futex节点
    FutexNode tempNode = {
        .key = futexKey,
        .index = index,
        .pid = (flags & FUTEX_PRIVATE) ? LOS_GetCurrProcessID() : OS_INVALID,
    };

    node = OsFindFutexNode(&tempNode); // 查找Futex节点
    if (node == NULL) {
        return LOS_EBADF; // 节点不存在，返回错误
    }

    headNode = node; // 设置头节点

    SCHEDULER_LOCK(intSave); // 锁定调度器
    // 检查并唤醒等待任务
    OsFutexCheckAndWakePendTask(headNode, wakeNumber, hashNode, newHeadNode, wakeAny);
    if ((*newHeadNode) != NULL) {
        // 替换队列列表头节点
        OsFutexReplaceQueueListHeadNode(headNode, *newHeadNode);
        OsFutexDeinitFutexNode(headNode); // 释放旧头节点资源
    } else if (headNode->index < FUTEX_INDEX_MAX) {
        // 从Futex列表中删除键
        OsFutexDeleteKeyFromFutexList(headNode);
        OsFutexDeinitFutexNode(headNode); // 释放头节点资源
    }
    SCHEDULER_UNLOCK(intSave); // 解锁调度器

    return LOS_OK; // 唤醒成功
}

/**
 * @brief Futex唤醒系统调用实现
 * @details 提供用户空间的futex唤醒接口，唤醒指定数量的等待任务
 * @param userVaddr 用户空间地址指针
 * @param flags futex操作标志
 * @param wakeNumber 要唤醒的任务数量
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
INT32 OsFutexWake(const UINT32 *userVaddr, UINT32 flags, INT32 wakeNumber)
{
    INT32 ret, futexRet; // 返回值
    UINTPTR futexKey; // Futex键值
    UINT32 index; // 哈希索引
    FutexHash *hashNode = NULL; // 哈希表节点指针
    FutexNode *headNode = NULL; // 头节点指针
    BOOL wakeAny = FALSE; // 是否有任务被唤醒的标志

    if (OsFutexWakeParamCheck(userVaddr, flags)) { // 检查唤醒参数
        return LOS_EINVAL; // 参数错误，返回错误码
    }

    futexKey = OsFutexFlagsToKey(userVaddr, flags); // 生成futex键
    index = OsFutexKeyToIndex(futexKey, flags); // 计算哈希索引

    hashNode = &g_futexHash[index]; // 获取哈希表节点
    if (OsFutexLock(&hashNode->listLock)) { // 获取哈希表列表锁
        return LOS_EINVAL; // 加锁失败，返回错误
    }

    // 执行唤醒任务
    ret = OsFutexWakeTask(futexKey, flags, wakeNumber, &headNode, &wakeAny);
    if (ret) {
        goto EXIT_ERR; // 唤醒失败，跳转到错误处理
    }

#ifdef LOS_FUTEX_DEBUG
    OsFutexHashShow(); // 调试模式下显示哈希表状态
#endif

    futexRet = OsFutexUnlock(&hashNode->listLock); // 释放哈希表列表锁
    if (futexRet) {
        goto EXIT_UNLOCK_ERR; // 解锁失败，跳转到错误处理
    }

    if (wakeAny == TRUE) { // 如果有任务被唤醒
        LOS_MpSchedule(OS_MP_CPU_ALL); // 多处理器调度
        LOS_Schedule(); // 触发调度
    }

    return LOS_OK; // 唤醒成功

EXIT_ERR:
    futexRet = OsFutexUnlock(&hashNode->listLock); // 释放哈希表列表锁
EXIT_UNLOCK_ERR:
    if (futexRet) {
        return futexRet; // 返回解锁错误
    }
    return ret; // 返回错误码
}

/**
 * @brief 将任务重新排队到新的Futex键
 * @details 将任务从旧的Futex键等待队列移动到新的Futex键等待队列
 * @param newFutexKey 新的Futex键值
 * @param newIndex 新的哈希索引
 * @param oldHeadNode 旧的头节点指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsFutexRequeueInsertNewKey(UINTPTR newFutexKey, INT32 newIndex, FutexNode *oldHeadNode)
{
    BOOL queueListIsEmpty = FALSE; // 队列是否为空标志
    INT32 ret; // 返回值
    UINT32 intSave; // 中断状态保存变量
    LosTaskCB *task = NULL; // 任务控制块指针
    FutexNode *nextNode = NULL; // 下一个节点指针
    // 初始化新的临时Futex节点
    FutexNode newTempNode = {
        .key = newFutexKey,
        .index = newIndex,
        .pid = (newIndex < FUTEX_INDEX_SHARED_POS) ? LOS_GetCurrProcessID() : OS_INVALID,
    };
    LOS_DL_LIST *queueList = &oldHeadNode->queueList; // 队列列表指针
    // 查找新的Futex节点
    FutexNode *newHeadNode = OsFindFutexNode(&newTempNode);
    if (newHeadNode == NULL) {
        // 插入新的Futex键到哈希表
        OsFutexInsertNewFutexKeyToHash(oldHeadNode);
        return LOS_OK; // 插入成功
    }

    do {
        nextNode = OS_FUTEX_FROM_QUEUELIST(queueList); // 获取下一个节点
        SCHEDULER_LOCK(intSave); // 锁定调度器
        if (LOS_ListEmpty(&nextNode->pendList)) { // 检查等待列表是否为空
            if (LOS_ListEmpty(queueList)) { // 检查队列是否为空
                queueListIsEmpty = TRUE;
            } else {
                queueList = queueList->pstNext; // 移动到下一个队列元素
            }
            OsFutexDeinitFutexNode(nextNode); // 释放Futex节点
            SCHEDULER_UNLOCK(intSave); // 解锁调度器
            if (queueListIsEmpty) {
                return LOS_OK; // 队列为空，返回成功
            }

            continue; // 继续处理下一个节点
        }

        // 从等待列表获取任务控制块
        task = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(nextNode->pendList)));
        if (LOS_ListEmpty(queueList)) { // 检查队列是否为空
            queueListIsEmpty = TRUE;
        } else {
            queueList = queueList->pstNext; // 移动到下一个队列元素
        }
        LOS_ListDelete(&nextNode->queueList); // 从队列中删除节点
        // 将任务插入到新的等待列表
        ret = OsFutexInsertTasktoPendList(&newHeadNode, nextNode, task);
        SCHEDULER_UNLOCK(intSave); // 解锁调度器
        if (ret != LOS_OK) {
            PRINT_ERR("Futex requeue insert new key failed!\n");
        }
    } while (!queueListIsEmpty); // 直到队列为空

    return LOS_OK; // 重新排队成功
}

/**
 * @brief 将Futex队列分割为两个列表
 * @details 根据指定数量将Futex等待队列分割为两个独立的列表
 * @param oldHashNode 旧的哈希表节点
 * @param oldHeadNode 旧的头节点
 * @param flags futex操作标志
 * @param futexKey Futex键值
 * @param count 分割数量
 */
STATIC VOID OsFutexRequeueSplitTwoLists(FutexHash *oldHashNode, FutexNode *oldHeadNode,
                                        UINT32 flags, UINTPTR futexKey, INT32 count)
{
    LOS_DL_LIST *queueList = &oldHeadNode->queueList; // 队列列表指针
    // 获取队列列表的尾节点
    FutexNode *tailNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_LAST(queueList));
    INT32 newIndex = OsFutexKeyToIndex(futexKey, flags); // 计算新的哈希索引
    FutexNode *nextNode = NULL; // 下一个节点指针
    FutexNode *newHeadNode = NULL; // 新的头节点指针
    LOS_DL_LIST *futexList = NULL; // Futex列表指针
    BOOL isAll = FALSE; // 是否处理所有节点的标志
    INT32 i; // 计数器

    for (i = 0; i < count; i++) { // 遍历指定数量的节点
        nextNode = OS_FUTEX_FROM_QUEUELIST(queueList); // 获取下一个节点
        nextNode->key = futexKey; // 更新节点的键值
        nextNode->index = newIndex; // 更新节点的索引
        if (queueList->pstNext == &oldHeadNode->queueList) { // 检查是否到达队列末尾
            isAll = TRUE; // 已处理所有节点
            break;
        }

        queueList = queueList->pstNext; // 移动到下一个队列元素
    }

    futexList = oldHeadNode->futexList.pstPrev; // 获取Futex列表的前一个节点
    LOS_ListDelete(&oldHeadNode->futexList); // 从Futex列表中删除旧头节点
    if (isAll == TRUE) { // 如果已处理所有节点
        return;
    }

    newHeadNode = OS_FUTEX_FROM_QUEUELIST(queueList); // 获取新的头节点
    // 将新头节点插入到Futex列表
    LOS_ListHeadInsert(futexList, &newHeadNode->futexList);
    // 更新旧头节点的队列指针
    oldHeadNode->queueList.pstPrev = &nextNode->queueList;
    nextNode->queueList.pstNext = &oldHeadNode->queueList;
    // 更新新头节点的队列指针
    newHeadNode->queueList.pstPrev = &tailNode->queueList;
    tailNode->queueList.pstNext = &newHeadNode->queueList;
    return;
}

/**
 * @brief 移除旧的Futex键并获取头节点
 * @details 唤醒指定数量的任务，然后为重新排队准备旧的头节点
 * @param oldFutexKey 旧的Futex键值
 * @param flags futex操作标志
 * @param wakeNumber 要唤醒的任务数量
 * @param newFutexKey 新的Futex键值
 * @param requeueCount 要重新排队的任务数量
 * @param wakeAny 是否有任务被唤醒的标志指针
 * @return 成功返回旧的头节点指针，失败返回NULL
 */
STATIC FutexNode *OsFutexRequeueRemoveOldKeyAndGetHead(UINTPTR oldFutexKey, UINT32 flags, INT32 wakeNumber,
                                                       UINTPTR newFutexKey, INT32 requeueCount, BOOL *wakeAny)
{
    INT32 ret; // 返回值
    INT32 oldIndex = OsFutexKeyToIndex(oldFutexKey, flags); // 计算旧的哈希索引
    FutexNode *oldHeadNode = NULL; // 旧的头节点指针
    FutexHash *oldHashNode = &g_futexHash[oldIndex]; // 获取旧的哈希表节点
    // 初始化旧的临时Futex节点
    FutexNode oldTempNode = {
        .key = oldFutexKey,
        .index = oldIndex,
        .pid = (flags & FUTEX_PRIVATE) ? LOS_GetCurrProcessID() : OS_INVALID,
    };

    if (wakeNumber > 0) { // 如果需要唤醒任务
        // 唤醒指定数量的任务
        ret = OsFutexWakeTask(oldFutexKey, flags, wakeNumber, &oldHeadNode, wakeAny);
        if ((ret != LOS_OK) || (oldHeadNode == NULL)) {
            return NULL; // 唤醒失败或头节点为空，返回NULL
        }
    }

    if (requeueCount <= 0) { // 如果不需要重新排队
        return NULL;
    }

    if (oldHeadNode == NULL) { // 如果头节点为空
        oldHeadNode = OsFindFutexNode(&oldTempNode); // 查找Futex节点
        if (oldHeadNode == NULL) {
            return NULL; // 节点不存在，返回NULL
        }
    }

    // 分割两个列表准备重新排队
    OsFutexRequeueSplitTwoLists(oldHashNode, oldHeadNode, flags, newFutexKey, requeueCount);

    return oldHeadNode; // 返回旧的头节点
}

/**
 * @brief 检查Futex重新排队操作的参数有效性
 * @details 验证futex重新排队操作的输入参数，包括地址和标志位
 * @param oldUserVaddr 旧的用户空间地址指针
 * @param flags futex操作标志
 * @param newUserVaddr 新的用户空间地址指针
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
STATIC INT32 OsFutexRequeueParamCheck(const UINT32 *oldUserVaddr, UINT32 flags, const UINT32 *newUserVaddr)
{
    VADDR_T oldVaddr = (VADDR_T)(UINTPTR)oldUserVaddr; // 旧地址转换为虚拟地址
    VADDR_T newVaddr = (VADDR_T)(UINTPTR)newUserVaddr; // 新地址转换为虚拟地址

    if (oldVaddr == newVaddr) { // 检查新旧地址是否相同
        return LOS_EINVAL; // 地址相同，返回错误
    }

    // 检查标志位是否有效
    if ((flags & (~FUTEX_PRIVATE)) != FUTEX_REQUEUE) {
        PRINT_ERR("Futex requeue param check failed! error flags: 0x%x\n", flags);
        return LOS_EINVAL; // 标志无效，返回错误
    }

    // 检查旧地址是否按INT32对齐且在有效范围内
    if ((oldVaddr % sizeof(INT32)) || (oldVaddr < OS_FUTEX_KEY_BASE) || (oldVaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex requeue param check failed! error old userVaddr: 0x%x\n", oldUserVaddr);
        return LOS_EINVAL; // 旧地址无效，返回错误
    }

    // 检查新地址是否按INT32对齐且在有效范围内
    if ((newVaddr % sizeof(INT32)) || (newVaddr < OS_FUTEX_KEY_BASE) || (newVaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex requeue param check failed! error new userVaddr: 0x%x\n", newUserVaddr);
        return LOS_EINVAL; // 新地址无效，返回错误
    }

    return LOS_OK; // 参数检查通过
}

/**
 * @brief Futex重新排队系统调用实现
 * @details 将等待任务从一个Futex键重新排队到另一个Futex键
 * @param userVaddr 旧的用户空间地址指针
 * @param flags futex操作标志
 * @param wakeNumber 要唤醒的任务数量
 * @param count 要重新排队的任务数量
 * @param newUserVaddr 新的用户空间地址指针
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
INT32 OsFutexRequeue(const UINT32 *userVaddr, UINT32 flags, INT32 wakeNumber, INT32 count, const UINT32 *newUserVaddr)
{
    INT32 ret; // 返回值
    UINTPTR oldFutexKey; // 旧的Futex键值
    UINTPTR newFutexKey; // 新的Futex键值
    INT32 oldIndex; // 旧的哈希索引
    INT32 newIndex; // 新的哈希索引
    FutexHash *oldHashNode = NULL; // 旧的哈希表节点指针
    FutexHash *newHashNode = NULL; // 新的哈希表节点指针
    FutexNode *oldHeadNode = NULL; // 旧的头节点指针
    BOOL wakeAny = FALSE; // 是否有任务被唤醒的标志

    // 检查重新排队参数
    if (OsFutexRequeueParamCheck(userVaddr, flags, newUserVaddr)) {
        return LOS_EINVAL; // 参数错误，返回错误码
    }

    oldFutexKey = OsFutexFlagsToKey(userVaddr, flags); // 生成旧的futex键
    newFutexKey = OsFutexFlagsToKey(newUserVaddr, flags); // 生成新的futex键
    oldIndex = OsFutexKeyToIndex(oldFutexKey, flags); // 计算旧的哈希索引
    newIndex = OsFutexKeyToIndex(newFutexKey, flags); // 计算新的哈希索引

    oldHashNode = &g_futexHash[oldIndex]; // 获取旧的哈希表节点
    if (OsFutexLock(&oldHashNode->listLock)) { // 获取旧哈希表列表锁
        return LOS_EINVAL; // 加锁失败，返回错误
    }

    // 移除旧键并获取头节点
    oldHeadNode = OsFutexRequeueRemoveOldKeyAndGetHead(oldFutexKey, flags, wakeNumber, newFutexKey, count, &wakeAny);
    if (oldHeadNode == NULL) { // 如果头节点为空
        (VOID)OsFutexUnlock(&oldHashNode->listLock); // 释放旧哈希表列表锁
        if (wakeAny == TRUE) { // 如果有任务被唤醒
            ret = LOS_OK;
            goto EXIT; // 跳转到出口
        }
        return LOS_EBADF; // 返回错误
    }

    newHashNode = &g_futexHash[newIndex]; // 获取新的哈希表节点
    if (oldIndex != newIndex) { // 如果新旧索引不同
        if (OsFutexUnlock(&oldHashNode->listLock)) { // 释放旧哈希表列表锁
            return LOS_EINVAL; // 解锁失败，返回错误
        }

        if (OsFutexLock(&newHashNode->listLock)) { // 获取新哈希表列表锁
            return LOS_EINVAL; // 加锁失败，返回错误
        }
    }

    // 将任务插入新键
    ret = OsFutexRequeueInsertNewKey(newFutexKey, newIndex, oldHeadNode);

    if (OsFutexUnlock(&newHashNode->listLock)) { // 释放新哈希表列表锁
        return LOS_EINVAL; // 解锁失败，返回错误
    }

EXIT:
    if (wakeAny == TRUE) { // 如果有任务被唤醒
        LOS_MpSchedule(OS_MP_CPU_ALL); // 多处理器调度
        LOS_Schedule(); // 触发调度
    }

    return ret; // 返回结果
}
#endif