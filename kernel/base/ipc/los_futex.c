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

#define OS_FUTEX_FROM_FUTEXLIST(ptr) LOS_DL_LIST_ENTRY(ptr, FutexNode, futexList)
#define OS_FUTEX_FROM_QUEUELIST(ptr) LOS_DL_LIST_ENTRY(ptr, FutexNode, queueList)
#define OS_FUTEX_KEY_BASE USER_ASPACE_BASE	///< 进程用户空间基址
#define OS_FUTEX_KEY_MAX (USER_ASPACE_BASE + USER_ASPACE_SIZE) ///< 进程用户空间尾址

/* private: 0~63    hash index_num
 * shared:  64~79   hash index_num */
#define FUTEX_INDEX_PRIVATE_MAX     64	///< 0~63号桶用于存放私有锁（以虚拟地址进行哈希）,同一进程不同线程共享futex变量，表明变量在进程地址空间中的位置
///< 它告诉内核，这个futex是进程专有的，不可以与其他进程共享。它仅仅用作同一进程的线程间同步。
#define FUTEX_INDEX_SHARED_MAX      16	///< 64~79号桶用于存放共享锁（以物理地址进行哈希）,不同进程间通过文件共享futex变量，表明该变量在文件中的位置
#define FUTEX_INDEX_MAX             (FUTEX_INDEX_PRIVATE_MAX + FUTEX_INDEX_SHARED_MAX) ///< 80个哈希桶

#define FUTEX_INDEX_SHARED_POS      FUTEX_INDEX_PRIVATE_MAX ///< 共享锁开始位置
#define FUTEX_HASH_PRIVATE_MASK     (FUTEX_INDEX_PRIVATE_MAX - 1)
#define FUTEX_HASH_SHARED_MASK      (FUTEX_INDEX_SHARED_MAX - 1)
/// 单独哈希桶,上面挂了一个个 FutexNode
typedef struct {
    LosMux      listLock;///< 内核操作lockList的互斥锁
    LOS_DL_LIST lockList;///< 用于挂载 FutexNode (Fast userspace mutex，用户态快速互斥锁)
} FutexHash;

FutexHash g_futexHash[FUTEX_INDEX_MAX];///< 80个哈希桶

/// 对互斥锁封装
STATIC INT32 OsFutexLock(LosMux *lock)
{
    UINT32 ret = LOS_MuxLock(lock, LOS_WAIT_FOREVER);
    if (ret != LOS_OK) {
        PRINT_ERR("Futex lock failed! ERROR: 0x%x!\n", ret);
        return LOS_EINVAL;
    }
    return LOS_OK;
}

STATIC INT32 OsFutexUnlock(LosMux *lock)
{
    UINT32 ret = LOS_MuxUnlock(lock);
    if (ret != LOS_OK) {
        PRINT_ERR("Futex unlock failed! ERROR: 0x%x!\n", ret);
        return LOS_EINVAL;
    }
    return LOS_OK;
}
///< 初始化Futex(Fast userspace mutex，用户态快速互斥锁)模块
UINT32 OsFutexInit(VOID)
{
    INT32 count;
    UINT32 ret;
	// 初始化 80个哈希桶 
    for (count = 0; count < FUTEX_INDEX_MAX; count++) {
        LOS_ListInit(&g_futexHash[count].lockList); // 初始化双向链表,上面挂 FutexNode
        ret = LOS_MuxInit(&(g_futexHash[count].listLock), NULL);//初始化互斥锁
        if (ret) {
            return ret;
        }
    }

    return LOS_OK;
}

LOS_MODULE_INIT(OsFutexInit, LOS_INIT_LEVEL_KMOD_EXTENDED);///< 注册Futex模块

#ifdef LOS_FUTEX_DEBUG
STATIC VOID OsFutexShowTaskNodeAttr(const LOS_DL_LIST *futexList)
{
    FutexNode *tempNode = NULL;
    FutexNode *lastNode = NULL;
    LosTaskCB *taskCB = NULL;
    LOS_DL_LIST *queueList = NULL;

    tempNode = OS_FUTEX_FROM_FUTEXLIST(futexList);
    PRINTK("key(pid)           : 0x%x(%d) : ->", tempNode->key, tempNode->pid);

    for (queueList = &tempNode->queueList; ;) {
        lastNode = OS_FUTEX_FROM_QUEUELIST(queueList);
        if (!LOS_ListEmpty(&(lastNode->pendList))) {
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(lastNode->pendList)));
            PRINTK(" %d(%d) ->", taskCB->taskID, taskCB->priority);
        } else {
            taskCB = LOS_DL_LIST_ENTRY(lastNode, LosTaskCB, futex);
            PRINTK(" %d(%d) ->", taskCB->taskID, -1);
        }
        queueList = queueList->pstNext;
        if (queueList == &tempNode->queueList) {
            break;
        }
    }
    PRINTK("\n");
}

VOID OsFutexHashShow(VOID)
{
    LOS_DL_LIST *futexList = NULL;
    INT32 count;
    /* The maximum number of barrels of a hash table */
    INT32 hashNodeMax = FUTEX_INDEX_MAX;
    PRINTK("#################### los_futex_pri.hash ####################\n");
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
/// 通过用户空间地址获取哈希key
STATIC INLINE UINTPTR OsFutexFlagsToKey(const UINT32 *userVaddr, const UINT32 flags)
{
    UINTPTR futexKey;

    if (flags & FUTEX_PRIVATE) {
        futexKey = (UINTPTR)userVaddr;//私有锁（以虚拟地址进行哈希）
    } else {
        futexKey = (UINTPTR)LOS_PaddrQuery((UINT32 *)userVaddr);//共享锁（以物理地址进行哈希）
    }

    return futexKey;
}
/// 通过哈希key获取索引
STATIC INLINE UINT32 OsFutexKeyToIndex(const UINTPTR futexKey, const UINT32 flags)
{
    UINT32 index = LOS_HashFNV32aBuf(&futexKey, sizeof(UINTPTR), FNV1_32A_INIT);//获取哈希桶索引

    if (flags & FUTEX_PRIVATE) {
        index &= FUTEX_HASH_PRIVATE_MASK;
    } else {
        index &= FUTEX_HASH_SHARED_MASK;
        index += FUTEX_INDEX_SHARED_POS;//共享锁索引
    }

    return index;
}
/// 设置快锁哈希key
STATIC INLINE VOID OsFutexSetKey(UINTPTR futexKey, UINT32 flags, FutexNode *node)
{
    node->key = futexKey;//哈希key
    node->index = OsFutexKeyToIndex(futexKey, flags);//哈希桶索引
    node->pid = (flags & FUTEX_PRIVATE) ? LOS_GetCurrProcessID() : OS_INVALID;//获取进程ID,共享快锁时 快锁节点没有进程ID
}

STATIC INLINE VOID OsFutexDeinitFutexNode(FutexNode *node)
{
    node->index = OS_INVALID_VALUE;
    node->pid = 0;
    LOS_ListDelete(&node->queueList);
}

STATIC INLINE VOID OsFutexReplaceQueueListHeadNode(FutexNode *oldHeadNode, FutexNode *newHeadNode)
{
    LOS_DL_LIST *futexList = oldHeadNode->futexList.pstPrev;
    LOS_ListDelete(&oldHeadNode->futexList);
    LOS_ListHeadInsert(futexList, &newHeadNode->futexList);
    if ((newHeadNode->queueList.pstNext == NULL) || (newHeadNode->queueList.pstPrev == NULL)) {
        LOS_ListInit(&newHeadNode->queueList);
    }
}

STATIC INLINE VOID OsFutexDeleteKeyFromFutexList(FutexNode *node)
{
    LOS_ListDelete(&node->futexList);
}

STATIC VOID OsFutexDeleteKeyNodeFromHash(FutexNode *node, BOOL isDeleteHead, FutexNode **headNode, BOOL *queueFlags)
{
    FutexNode *nextNode = NULL;

    if (node->index >= FUTEX_INDEX_MAX) {
        return;
    }

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
/// 从哈希桶上删除快锁
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

STATIC FutexNode *OsFutexDeleteAlreadyWakeTaskAndGetNext(const FutexNode *node, FutexNode **headNode, BOOL isDeleteHead)
{
    FutexNode *tempNode = (FutexNode *)node;
    FutexNode *nextNode = NULL;
    BOOL queueFlag = FALSE;

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
/// 插入一把新Futex锁进哈希
STATIC VOID OsFutexInsertNewFutexKeyToHash(FutexNode *node)
{
    FutexNode *headNode = NULL;
    FutexNode *tailNode = NULL;
    LOS_DL_LIST *futexList = NULL;
    FutexHash *hashNode = &g_futexHash[node->index];

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
///< 
STATIC INT32 OsFutexInsertFindFormBackToFront(LOS_DL_LIST *queueList, const LosTaskCB *runTask, FutexNode *node)
{
    LOS_DL_LIST *listHead = queueList;
    LOS_DL_LIST *listTail = queueList->pstPrev;
    FutexNode *tempNode = NULL;
    LosTaskCB *taskTail = NULL;

    for (; listHead != listTail; listTail = listTail->pstPrev) {
        tempNode = OS_FUTEX_FROM_QUEUELIST(listTail);
        tempNode = OsFutexDeleteAlreadyWakeTaskAndGetNext(tempNode, NULL, FALSE);
        if (tempNode == NULL) {
            return LOS_NOK;
        }
        taskTail = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(tempNode->pendList)));
        if (runTask->priority >= taskTail->priority) {
            LOS_ListHeadInsert(&(tempNode->queueList), &(node->queueList));
            return LOS_OK;
        } else if (runTask->priority < taskTail->priority) {
            if (listTail->pstPrev == listHead) {
                LOS_ListTailInsert(&(tempNode->queueList), &(node->queueList));
                return LOS_OK;
            }
        }
    }

    return LOS_NOK;
}

STATIC INT32 OsFutexInsertFindFromFrontToBack(LOS_DL_LIST *queueList, const LosTaskCB *runTask, FutexNode *node)
{
    LOS_DL_LIST *listHead = queueList;
    LOS_DL_LIST *listTail = queueList->pstPrev;
    FutexNode *tempNode = NULL;
    LosTaskCB *taskHead = NULL;

    for (; listHead != listTail; listHead = listHead->pstNext) {
        tempNode = OS_FUTEX_FROM_QUEUELIST(listHead);
        tempNode = OsFutexDeleteAlreadyWakeTaskAndGetNext(tempNode, NULL, FALSE);
        if (tempNode == NULL) {
            return LOS_NOK;
        }
        taskHead = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(tempNode->pendList)));
        /* High priority comes before low priority,
         * in the case of the same priority, after the current node
         */
        if (runTask->priority >= taskHead->priority) {
            if (listHead->pstNext == listTail) {
                LOS_ListHeadInsert(&(tempNode->queueList), &(node->queueList));
                return LOS_OK;
            }
            continue;
        } else if (runTask->priority < taskHead->priority) {
            LOS_ListTailInsert(&(tempNode->queueList), &(node->queueList));
            return LOS_OK;
        }
    }

    return LOS_NOK;
}

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
///< 将快锁挂到任务的阻塞链表上
STATIC INT32 OsFutexInsertTasktoPendList(FutexNode **firstNode, FutexNode *node, const LosTaskCB *run)
{
    LosTaskCB *taskHead = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&((*firstNode)->pendList)));
    LOS_DL_LIST *queueList = &((*firstNode)->queueList);
    FutexNode *tailNode = NULL;
    LosTaskCB *taskTail = NULL;

    if (run->priority < taskHead->priority) {
        /* The one with the highest priority is inserted at the top of the queue */
        LOS_ListTailInsert(queueList, &(node->queueList));
        OsFutexReplaceQueueListHeadNode(*firstNode, node);
        *firstNode = node;
        return LOS_OK;
    }

    if (LOS_ListEmpty(queueList) && (run->priority >= taskHead->priority)) {
        /* Insert the next position in the queue with equal priority */
        LOS_ListHeadInsert(queueList, &(node->queueList));
        return LOS_OK;
    }

    tailNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_LAST(queueList));
    taskTail = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(tailNode->pendList)));
    if ((run->priority >= taskTail->priority) ||
        ((run->priority - taskHead->priority) > (taskTail->priority - run->priority))) {
        return OsFutexInsertFindFormBackToFront(queueList, run, node);
    }

    return OsFutexInsertFindFromFrontToBack(queueList, run, node);
}
/// 由指定快锁找到对应哈希桶
STATIC FutexNode *OsFindFutexNode(const FutexNode *node)
{
    FutexHash *hashNode = &g_futexHash[node->index];//先找到所在哈希桶
    LOS_DL_LIST *futexList = &(hashNode->lockList);
    FutexNode *headNode = NULL;

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
///< 查找快锁并插入哈希桶中
STATIC INT32 OsFindAndInsertToHash(FutexNode *node)
{
    FutexNode *headNode = NULL;
    FutexNode *firstNode = NULL;
    UINT32 intSave;
    INT32 ret;

    headNode = OsFindFutexNode(node);
    if (headNode == NULL) {//没有找到
        OsFutexInsertNewFutexKeyToHash(node);
        LOS_ListInit(&(node->queueList));
        return LOS_OK;
    }

    ret = OsFutexRecycleAndFindHeadNode(headNode, node, &firstNode);
    if (ret != LOS_OK) {
        return ret;
    } else if (firstNode == NULL) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsFutexInsertTasktoPendList(&firstNode, node, OsCurrTaskGet());
    SCHEDULER_UNLOCK(intSave);

    return ret;
}
/// 共享内存检查
STATIC INT32 OsFutexKeyShmPermCheck(const UINT32 *userVaddr, const UINT32 flags)
{
    PADDR_T paddr;

    /* Check whether the futexKey is a shared lock */
    if (!(flags & FUTEX_PRIVATE)) {//非私有快锁
        paddr = (UINTPTR)LOS_PaddrQuery((UINT32 *)userVaddr);//能否查询到物理地址
        if (paddr == 0) return LOS_NOK;
    }

    return LOS_OK;
}

STATIC INT32 OsFutexWaitParamCheck(const UINT32 *userVaddr, UINT32 flags, UINT32 absTime)
{
    VADDR_T vaddr = (VADDR_T)(UINTPTR)userVaddr;

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    if (flags & (~FUTEX_PRIVATE)) {
        PRINT_ERR("Futex wait param check failed! error flags: 0x%x\n", flags);
        return LOS_EINVAL;
    }

    if ((vaddr % sizeof(INT32)) || (vaddr < OS_FUTEX_KEY_BASE) || (vaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex wait param check failed! error userVaddr: 0x%x\n", vaddr);
        return LOS_EINVAL;
    }

    if (flags && (OsFutexKeyShmPermCheck(userVaddr, flags) != LOS_OK)) {
        PRINT_ERR("Futex wait param check failed! error shared memory perm userVaddr: 0x%x\n", userVaddr);
        return LOS_EINVAL;
    }

    if (!absTime) {
        return LOS_EINVAL;
    }

    return LOS_OK;
}

STATIC INT32 OsFutexDeleteTimeoutTaskNode(FutexHash *hashNode, FutexNode *node)
{
    UINT32 intSave;
    if (OsFutexLock(&hashNode->listLock)) {
        return LOS_EINVAL;
    }

    if (node->index < FUTEX_INDEX_MAX) {
        SCHEDULER_LOCK(intSave);
        (VOID)OsFutexDeleteAlreadyWakeTaskAndGetNext(node, NULL, TRUE);
        SCHEDULER_UNLOCK(intSave);
    }

#ifdef LOS_FUTEX_DEBUG
    OsFutexHashShow();
#endif

    if (OsFutexUnlock(&hashNode->listLock)) {
        return LOS_EINVAL;
    }
    return LOS_ETIMEDOUT;
}

STATIC INT32 OsFutexInsertTaskToHash(LosTaskCB **taskCB, FutexNode **node, const UINTPTR futexKey, const UINT32 flags)
{
    INT32 ret;
    *taskCB = OsCurrTaskGet();
    *node = &((*taskCB)->futex);
    OsFutexSetKey(futexKey, flags, *node);

    ret = OsFindAndInsertToHash(*node);
    if (ret) {
        return LOS_NOK;
    }

    LOS_ListInit(&((*node)->pendList));
    return LOS_OK;
}
//将当前任务挂入等待链表中
STATIC INT32 OsFutexWaitTask(const UINT32 *userVaddr, const UINT32 flags, const UINT32 val, const UINT32 timeOut)
{
    INT32 futexRet;
    UINT32 intSave, lockVal;
    LosTaskCB *taskCB = NULL;
    FutexNode *node = NULL;
    UINTPTR futexKey = OsFutexFlagsToKey(userVaddr, flags);
    UINT32 index = OsFutexKeyToIndex(futexKey, flags);
    FutexHash *hashNode = &g_futexHash[index];

    if (OsFutexLock(&hashNode->listLock)) {
        return LOS_EINVAL;
    }

    if (LOS_ArchCopyFromUser(&lockVal, userVaddr, sizeof(UINT32))) {
        PRINT_ERR("Futex wait param check failed! copy from user failed!\n");
        futexRet = LOS_EINVAL;
        goto EXIT_ERR;
    }

    if (lockVal != val) {
        futexRet = LOS_EBADF;
        goto EXIT_ERR;
    }

    if (OsFutexInsertTaskToHash(&taskCB, &node, futexKey, flags)) {
        futexRet = LOS_NOK;
        goto EXIT_ERR;
    }

    SCHEDULER_LOCK(intSave);
    OsTaskWaitSetPendMask(OS_TASK_WAIT_FUTEX, futexKey, timeOut);
    OsSchedTaskWait(&(node->pendList), timeOut, FALSE);
    OsSchedLock();
    LOS_SpinUnlock(&g_taskSpin);

    futexRet = OsFutexUnlock(&hashNode->listLock);
    if (futexRet) {
        OsSchedUnlock();
        LOS_IntRestore(intSave);
        goto EXIT_UNLOCK_ERR;
    }

    LOS_SpinLock(&g_taskSpin);
    OsSchedUnlock();

    /*
     * it will immediately do the scheduling, so there's no need to release the
     * task spinlock. when this task's been rescheduled, it will be holding the spinlock.
     */
    OsSchedResched();

    if (taskCB->taskStatus & OS_TASK_STATUS_TIMEOUT) {
        taskCB->taskStatus &= ~OS_TASK_STATUS_TIMEOUT;
        SCHEDULER_UNLOCK(intSave);
        return OsFutexDeleteTimeoutTaskNode(hashNode, node);
    }

    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;

EXIT_ERR:
    (VOID)OsFutexUnlock(&hashNode->listLock);
EXIT_UNLOCK_ERR:
    return futexRet;
}
/// 设置线程等待 | 向Futex表中插入代表被阻塞的线程的node
INT32 OsFutexWait(const UINT32 *userVaddr, UINT32 flags, UINT32 val, UINT32 absTime)
{
    INT32 ret;
    UINT32 timeOut = LOS_WAIT_FOREVER;

    ret = OsFutexWaitParamCheck(userVaddr, flags, absTime);//参数检查
    if (ret) {
        return ret;
    }
    if (absTime != LOS_WAIT_FOREVER) {//转换时间 , 内核的时间单位是 tick
        timeOut = OsNS2Tick((UINT64)absTime * OS_SYS_NS_PER_US); //转成 tick
    }

    return OsFutexWaitTask(userVaddr, flags, val, timeOut);//将任务挂起 timeOut 时长
}

STATIC INT32 OsFutexWakeParamCheck(const UINT32 *userVaddr, UINT32 flags)
{
    VADDR_T vaddr = (VADDR_T)(UINTPTR)userVaddr;

    if ((flags & (~FUTEX_PRIVATE)) != FUTEX_WAKE) {
        PRINT_ERR("Futex wake param check failed! error flags: 0x%x\n", flags);
        return LOS_EINVAL;
    }
	//地址必须在用户空间
    if ((vaddr % sizeof(INT32)) || (vaddr < OS_FUTEX_KEY_BASE) || (vaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex wake param check failed! error userVaddr: 0x%x\n", userVaddr);
        return LOS_EINVAL;
    }
	//必须得是个共享内存地址
    if (flags && (OsFutexKeyShmPermCheck(userVaddr, flags) != LOS_OK)) {
        PRINT_ERR("Futex wake param check failed! error shared memory perm userVaddr: 0x%x\n", userVaddr);
        return LOS_EINVAL;
    }

    return LOS_OK;
}

/* Check to see if the task to be awakened has timed out
 * if time out, to weak next pend task. 
 * | 查看要唤醒的任务是否超时，如果超时，就唤醒,并查看下一个挂起的任务。
 */
STATIC VOID OsFutexCheckAndWakePendTask(FutexNode *headNode, const INT32 wakeNumber,
                                        FutexHash *hashNode, FutexNode **nextNode, BOOL *wakeAny)
{
    INT32 count;
    LosTaskCB *taskCB = NULL;
    FutexNode *node = headNode;
    for (count = 0; count < wakeNumber; count++) {
        /* Ensure the integrity of the head */
        *nextNode = OsFutexDeleteAlreadyWakeTaskAndGetNext(node, NULL, FALSE);
        if (*nextNode == NULL) {
            /* The last node in queuelist is invalid or the entire list is invalid */
            return;
        }
        node = *nextNode;
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(node->pendList)));
        OsTaskWakeClearPendMask(taskCB);
        OsSchedTaskWake(taskCB);
        *wakeAny = TRUE;
        *nextNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_FIRST(&(node->queueList)));
        if (node != headNode) {
            OsFutexDeinitFutexNode(node);
        }

        if (LOS_ListEmpty(&headNode->queueList)) {
            /* Wakes up the entire linked list node */
            *nextNode = NULL;
            return;
        }

        node = *nextNode;
    }
    return;
}
 
/*!
 * @brief OsFutexWakeTask	唤醒任务
 *
 * @param flags	
 * @param futexKey	
 * @param newHeadNode	
 * @param wakeAny	
 * @param wakeNumber 唤醒数量	
 * @return	
 *
 * @see
 */
STATIC INT32 OsFutexWakeTask(UINTPTR futexKey, UINT32 flags, INT32 wakeNumber, FutexNode **newHeadNode, BOOL *wakeAny)
{
    UINT32 intSave;
    FutexNode *node = NULL;
    FutexNode *headNode = NULL;
    UINT32 index = OsFutexKeyToIndex(futexKey, flags);
    FutexHash *hashNode = &g_futexHash[index];
    FutexNode tempNode = { //先组成一个临时快锁节点,目的是为了找到哈希桶中是否有这个节点
        .key = futexKey,
        .index = index,
        .pid = (flags & FUTEX_PRIVATE) ? LOS_GetCurrProcessID() : OS_INVALID,
    };

    node = OsFindFutexNode(&tempNode);//找快锁节点
    if (node == NULL) {
        return LOS_EBADF;
    }

    headNode = node;

    SCHEDULER_LOCK(intSave);
    OsFutexCheckAndWakePendTask(headNode, wakeNumber, hashNode, newHeadNode, wakeAny);//再找到等这把锁的唤醒指向数量的任务
    if ((*newHeadNode) != NULL) {
        OsFutexReplaceQueueListHeadNode(headNode, *newHeadNode);
        OsFutexDeinitFutexNode(headNode);
    } else if (headNode->index < FUTEX_INDEX_MAX) {
        OsFutexDeleteKeyFromFutexList(headNode);
        OsFutexDeinitFutexNode(headNode);
    }
    SCHEDULER_UNLOCK(intSave);

    return LOS_OK;
}
/// 唤醒一个被指定锁阻塞的线程
INT32 OsFutexWake(const UINT32 *userVaddr, UINT32 flags, INT32 wakeNumber)
{
    INT32 ret, futexRet;
    UINTPTR futexKey;
    UINT32 index;
    FutexHash *hashNode = NULL;
    FutexNode *headNode = NULL;
    BOOL wakeAny = FALSE;
	//1.检查参数
    if (OsFutexWakeParamCheck(userVaddr, flags)) {
        return LOS_EINVAL;
    }
	//2.找到指定用户空间地址对应的桶
    futexKey = OsFutexFlagsToKey(userVaddr, flags);
    index = OsFutexKeyToIndex(futexKey, flags);

    hashNode = &g_futexHash[index];
    if (OsFutexLock(&hashNode->listLock)) {
        return LOS_EINVAL;
    }
	//3.换起等待该锁的进程
    ret = OsFutexWakeTask(futexKey, flags, wakeNumber, &headNode, &wakeAny);
    if (ret) {
        goto EXIT_ERR;
    }

#ifdef LOS_FUTEX_DEBUG
    OsFutexHashShow();
#endif

    futexRet = OsFutexUnlock(&hashNode->listLock);
    if (futexRet) {
        goto EXIT_UNLOCK_ERR;
    }
	//4.根据指定参数决定是否发起调度
    if (wakeAny == TRUE) {
        LOS_MpSchedule(OS_MP_CPU_ALL);
        LOS_Schedule();
    }

    return LOS_OK;

EXIT_ERR:
    futexRet = OsFutexUnlock(&hashNode->listLock);
EXIT_UNLOCK_ERR:
    if (futexRet) {
        return futexRet;
    }
    return ret;
}

STATIC INT32 OsFutexRequeueInsertNewKey(UINTPTR newFutexKey, INT32 newIndex, FutexNode *oldHeadNode)
{
    BOOL queueListIsEmpty = FALSE;
    INT32 ret;
    UINT32 intSave;
    LosTaskCB *task = NULL;
    FutexNode *nextNode = NULL;
    FutexNode newTempNode = {
        .key = newFutexKey,
        .index = newIndex,
        .pid = (newIndex < FUTEX_INDEX_SHARED_POS) ? LOS_GetCurrProcessID() : OS_INVALID,
    };
    LOS_DL_LIST *queueList = &oldHeadNode->queueList;
    FutexNode *newHeadNode = OsFindFutexNode(&newTempNode);
    if (newHeadNode == NULL) {
        OsFutexInsertNewFutexKeyToHash(oldHeadNode);
        return LOS_OK;
    }

    do {
        nextNode = OS_FUTEX_FROM_QUEUELIST(queueList);
        SCHEDULER_LOCK(intSave);
        if (LOS_ListEmpty(&nextNode->pendList)) {
            if (LOS_ListEmpty(queueList)) {
                queueListIsEmpty = TRUE;
            } else {
                queueList = queueList->pstNext;
            }
            OsFutexDeinitFutexNode(nextNode);
            SCHEDULER_UNLOCK(intSave);
            if (queueListIsEmpty) {
                return LOS_OK;
            }

            continue;
        }

        task = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(nextNode->pendList)));
        if (LOS_ListEmpty(queueList)) {
            queueListIsEmpty = TRUE;
        } else {
            queueList = queueList->pstNext;
        }
        LOS_ListDelete(&nextNode->queueList);
        ret = OsFutexInsertTasktoPendList(&newHeadNode, nextNode, task);
        SCHEDULER_UNLOCK(intSave);
        if (ret != LOS_OK) {
            PRINT_ERR("Futex requeue insert new key failed!\n");
        }
    } while (!queueListIsEmpty);

    return LOS_OK;
}

STATIC VOID OsFutexRequeueSplitTwoLists(FutexHash *oldHashNode, FutexNode *oldHeadNode,
                                        UINT32 flags, UINTPTR futexKey, INT32 count)
{
    LOS_DL_LIST *queueList = &oldHeadNode->queueList;
    FutexNode *tailNode = OS_FUTEX_FROM_QUEUELIST(LOS_DL_LIST_LAST(queueList));
    INT32 newIndex = OsFutexKeyToIndex(futexKey, flags);
    FutexNode *nextNode = NULL;
    FutexNode *newHeadNode = NULL;
    LOS_DL_LIST *futexList = NULL;
    BOOL isAll = FALSE;
    INT32 i;

    for (i = 0; i < count; i++) {
        nextNode = OS_FUTEX_FROM_QUEUELIST(queueList);
        nextNode->key = futexKey;
        nextNode->index = newIndex;
        if (queueList->pstNext == &oldHeadNode->queueList) {
            isAll = TRUE;
            break;
        }

        queueList = queueList->pstNext;
    }

    futexList = oldHeadNode->futexList.pstPrev;
    LOS_ListDelete(&oldHeadNode->futexList);
    if (isAll == TRUE) {
        return;
    }

    newHeadNode = OS_FUTEX_FROM_QUEUELIST(queueList);
    LOS_ListHeadInsert(futexList, &newHeadNode->futexList);
    oldHeadNode->queueList.pstPrev = &nextNode->queueList;
    nextNode->queueList.pstNext = &oldHeadNode->queueList;
    newHeadNode->queueList.pstPrev = &tailNode->queueList;
    tailNode->queueList.pstNext = &newHeadNode->queueList;
    return;
}
/// 删除旧key并获取头节点
STATIC FutexNode *OsFutexRequeueRemoveOldKeyAndGetHead(UINTPTR oldFutexKey, UINT32 flags, INT32 wakeNumber,
                                                       UINTPTR newFutexKey, INT32 requeueCount, BOOL *wakeAny)
{
    INT32 ret;
    INT32 oldIndex = OsFutexKeyToIndex(oldFutexKey, flags);
    FutexNode *oldHeadNode = NULL;
    FutexHash *oldHashNode = &g_futexHash[oldIndex];
    FutexNode oldTempNode = {
        .key = oldFutexKey,
        .index = oldIndex,
        .pid = (flags & FUTEX_PRIVATE) ? LOS_GetCurrProcessID() : OS_INVALID,
    };

    if (wakeNumber > 0) {
        ret = OsFutexWakeTask(oldFutexKey, flags, wakeNumber, &oldHeadNode, wakeAny);
        if ((ret != LOS_OK) || (oldHeadNode == NULL)) {
            return NULL;
        }
    }

    if (requeueCount <= 0) {
        return NULL;
    }

    if (oldHeadNode == NULL) {
        oldHeadNode = OsFindFutexNode(&oldTempNode);
        if (oldHeadNode == NULL) {
            return NULL;
        }
    }

    OsFutexRequeueSplitTwoLists(oldHashNode, oldHeadNode, flags, newFutexKey, requeueCount);

    return oldHeadNode;
}
/// 检查锁在Futex表中的状态
STATIC INT32 OsFutexRequeueParamCheck(const UINT32 *oldUserVaddr, UINT32 flags, const UINT32 *newUserVaddr)
{
    VADDR_T oldVaddr = (VADDR_T)(UINTPTR)oldUserVaddr;
    VADDR_T newVaddr = (VADDR_T)(UINTPTR)newUserVaddr;

    if (oldVaddr == newVaddr) {
        return LOS_EINVAL;
    }
	//检查标记
    if ((flags & (~FUTEX_PRIVATE)) != FUTEX_REQUEUE) {
        PRINT_ERR("Futex requeue param check failed! error flags: 0x%x\n", flags);
        return LOS_EINVAL;
    }
	//检查地址范围,必须在用户空间
    if ((oldVaddr % sizeof(INT32)) || (oldVaddr < OS_FUTEX_KEY_BASE) || (oldVaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex requeue param check failed! error old userVaddr: 0x%x\n", oldUserVaddr);
        return LOS_EINVAL;
    }

    if ((newVaddr % sizeof(INT32)) || (newVaddr < OS_FUTEX_KEY_BASE) || (newVaddr >= OS_FUTEX_KEY_MAX)) {
        PRINT_ERR("Futex requeue param check failed! error new userVaddr: 0x%x\n", newUserVaddr);
        return LOS_EINVAL;
    }

    return LOS_OK;
}
/// 调整指定锁在Futex表中的位置
INT32 OsFutexRequeue(const UINT32 *userVaddr, UINT32 flags, INT32 wakeNumber, INT32 count, const UINT32 *newUserVaddr)
{
    INT32 ret;
    UINTPTR oldFutexKey;
    UINTPTR newFutexKey;
    INT32 oldIndex;
    INT32 newIndex;
    FutexHash *oldHashNode = NULL;
    FutexHash *newHashNode = NULL;
    FutexNode *oldHeadNode = NULL;
    BOOL wakeAny = FALSE;

    if (OsFutexRequeueParamCheck(userVaddr, flags, newUserVaddr)) {
        return LOS_EINVAL;
    }

    oldFutexKey = OsFutexFlagsToKey(userVaddr, flags);//先拿key
    newFutexKey = OsFutexFlagsToKey(newUserVaddr, flags);
    oldIndex = OsFutexKeyToIndex(oldFutexKey, flags);//再拿所在哈希桶位置,共有80个哈希桶
    newIndex = OsFutexKeyToIndex(newFutexKey, flags);

    oldHashNode = &g_futexHash[oldIndex];//拿到对应哈希桶实体
    if (OsFutexLock(&oldHashNode->listLock)) {
        return LOS_EINVAL;
    }

    oldHeadNode = OsFutexRequeueRemoveOldKeyAndGetHead(oldFutexKey, flags, wakeNumber, newFutexKey, count, &wakeAny);
    if (oldHeadNode == NULL) {
        (VOID)OsFutexUnlock(&oldHashNode->listLock);
        if (wakeAny == TRUE) {
            ret = LOS_OK;
            goto EXIT;
        }
        return LOS_EBADF;
    }

    newHashNode = &g_futexHash[newIndex];
    if (oldIndex != newIndex) {
        if (OsFutexUnlock(&oldHashNode->listLock)) {
            return LOS_EINVAL;
        }

        if (OsFutexLock(&newHashNode->listLock)) {
            return LOS_EINVAL;
        }
    }

    ret = OsFutexRequeueInsertNewKey(newFutexKey, newIndex, oldHeadNode);

    if (OsFutexUnlock(&newHashNode->listLock)) {
        return LOS_EINVAL;
    }

EXIT:
    if (wakeAny == TRUE) {
        LOS_MpSchedule(OS_MP_CPU_ALL);
        LOS_Schedule();
    }

    return ret;
}
#endif

