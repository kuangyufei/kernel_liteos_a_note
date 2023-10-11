/*!
* @file  los_swtmr.c
* @brief  软定时器主文件
* @details  
* @attention @verbatim 
基本概念 
	软件定时器，是基于系统Tick时钟中断且由软件来模拟的定时器。当经过设定的Tick数后，会触发用户自定义的回调函数。
	硬件定时器受硬件的限制，数量上不足以满足用户的实际需求。因此为了满足用户需求，提供更多的定时器，
	软件定时器功能，支持如下特性：
		创建软件定时器。
		启动软件定时器。
		停止软件定时器。
		删除软件定时器。
		获取软件定时器剩余Tick数。
		可配置支持的软件定时器个数。
 		
    运作机制 
        软件定时器是系统资源，在模块初始化的时候已经分配了一块连续内存。
        软件定时器使用了系统的一个队列和一个任务资源，软件定时器的触发遵循队列规则，
        先进先出。定时时间短的定时器总是比定时时间长的靠近队列头，满足优先触发的准则。
        软件定时器以Tick为基本计时单位，当创建并启动一个软件定时器时，Huawei LiteOS会根据 
        当前系统Tick时间及设置的定时时长确定该定时器的到期Tick时间，并将该定时器控制结构挂入计时全局链表。
        当Tick中断到来时，在Tick中断处理函数中扫描软件定时器的计时全局链表，检查是否有定时器超时，
        若有则将超时的定时器记录下来。Tick中断处理函数结束后，软件定时器任务（优先级为最高）
        被唤醒，在该任务中调用已经记录下来的定时器的回调函数。
 
    定时器状态 
        OS_SWTMR_STATUS_UNUSED（定时器未使用）
        系统在定时器模块初始化时，会将系统中所有定时器资源初始化成该状态。
 
        OS_SWTMR_STATUS_TICKING（定时器处于计数状态）
        在定时器创建后调用LOS_SwtmrStart接口启动，定时器将变成该状态，是定时器运行时的状态。
 
        OS_SWTMR_STATUS_CREATED（定时器创建后未启动，或已停止）
        定时器创建后，不处于计数状态时，定时器将变成该状态。
 
    软件定时器提供了三类模式：
        单次触发定时器，这类定时器在启动后只会触发一次定时器事件，然后定时器自动删除。
        周期触发定时器，这类定时器会周期性的触发定时器事件，直到用户手动停止定时器，否则将永远持续执行下去。
        单次触发定时器，但这类定时器超时触发后不会自动删除，需要调用定时器删除接口删除定时器。
 
    使用场景 
        创建一个单次触发的定时器，超时后执行用户自定义的回调函数。
        创建一个周期性触发的定时器，超时后执行用户自定义的回调函数。
 
     软件定时器的典型开发流程 
         通过make menuconfig配置软件定时器 
         创建定时器LOS_SwtmrCreate，设置定时器的定时时长、定时器模式、超时后的回调函数。
         启动定时器LOS_SwtmrStart。
         获得软件定时器剩余Tick数LOS_SwtmrTimeGet。
         停止定时器LOS_SwtmrStop。
         删除定时器LOS_SwtmrDelete。
 
     注意事项 
         软件定时器的回调函数中不应执行过多操作，不建议使用可能引起任务挂起或者阻塞的接口或操作， 
         如果使用会导致软件定时器响应不及时，造成的影响无法确定。
         软件定时器使用了系统的一个队列和一个任务资源。软件定时器任务的优先级设定为0，且不允许修改 。
         系统可配置的软件定时器个数是指：整个系统可使用的软件定时器总个数，并非用户可使用的软件定时器个数。
         例如：系统多占用一个软件定时器，那么用户能使用的软件定时器资源就会减少一个。
         创建单次不自删除属性的定时器，用户需要自行调用定时器删除接口删除定时器，回收定时器资源，避免资源泄露。
         软件定时器的定时精度与系统Tick时钟的周期有关。
    @endverbatim 
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

#include "los_swtmr_pri.h"
#include "los_init.h"
#include "los_process_pri.h"
#include "los_queue_pri.h"
#include "los_sched_pri.h"
#include "los_sortlink_pri.h"
#include "los_task_pri.h"
#include "los_hook.h"

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
#if (LOSCFG_BASE_CORE_SWTMR_LIMIT <= 0)
#error "swtmr maxnum cannot be zero"
#endif /* LOSCFG_BASE_CORE_SWTMR_LIMIT <= 0 */

STATIC INLINE VOID SwtmrDelete(SWTMR_CTRL_S *swtmr);
STATIC INLINE UINT64 SwtmrToStart(SWTMR_CTRL_S *swtmr, UINT16 cpuid);
LITE_OS_SEC_BSS SWTMR_CTRL_S    *g_swtmrCBArray = NULL;     /**< First address in Timer memory space  \n 定时器池 */
LITE_OS_SEC_BSS UINT8           *g_swtmrHandlerPool = NULL; /**< Pool of Swtmr Handler \n 用于注册软时钟的回调函数 */
LITE_OS_SEC_BSS LOS_DL_LIST     g_swtmrFreeList;            /**< Free list of Software Timer \n 空闲定时器链表 */

/* spinlock for swtmr module, only available on SMP mode */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_swtmrSpin);///< 初始化软时钟自旋锁,只有SMP情况才需要,只要是自旋锁都是用于CPU多核的同步
#define SWTMR_LOCK(state)       LOS_SpinLockSave(&g_swtmrSpin, &(state))///< 持有软时钟自旋锁
#define SWTMR_UNLOCK(state)     LOS_SpinUnlockRestore(&g_swtmrSpin, (state))///< 释放软时钟自旋锁

typedef struct {
    SortLinkAttribute swtmrSortLink;
    LosTaskCB         *swtmrTask;           /* software timer task id | 定时器任务ID */
    LOS_DL_LIST       swtmrHandlerQueue;     /* software timer timeout queue id | 定时器超时队列*/
} SwtmrRunqueue;

STATIC SwtmrRunqueue g_swtmrRunqueue[LOSCFG_KERNEL_CORE_NUM];

#ifdef LOSCFG_SWTMR_DEBUG
#define OS_SWTMR_PERIOD_TO_CYCLE(period) (((UINT64)(period) * OS_NS_PER_TICK) / OS_NS_PER_CYCLE)
STATIC SwtmrDebugData *g_swtmrDebugData = NULL;

BOOL OsSwtmrDebugDataUsed(UINT32 swtmrID)
{
    if (swtmrID > LOSCFG_BASE_CORE_SWTMR_LIMIT) {
        return FALSE;
    }

    return g_swtmrDebugData[swtmrID].swtmrUsed;
}

UINT32 OsSwtmrDebugDataGet(UINT32 swtmrID, SwtmrDebugData *data, UINT32 len, UINT8 *mode)
{
    UINT32 intSave;
    errno_t ret;

    if ((swtmrID > LOSCFG_BASE_CORE_SWTMR_LIMIT) || (data == NULL) ||
        (mode == NULL) || (len < sizeof(SwtmrDebugData))) {
        return LOS_NOK;
    }

    SWTMR_CTRL_S *swtmr = &g_swtmrCBArray[swtmrID];
    SWTMR_LOCK(intSave);
    ret = memcpy_s(data, len, &g_swtmrDebugData[swtmrID], sizeof(SwtmrDebugData));
    *mode = swtmr->ucMode;
    SWTMR_UNLOCK(intSave);
    if (ret != EOK) {
        return LOS_NOK;
    }
    return LOS_OK;
}
#endif

STATIC VOID SwtmrDebugDataInit(VOID)
{
#ifdef LOSCFG_SWTMR_DEBUG
    UINT32 size = sizeof(SwtmrDebugData) * LOSCFG_BASE_CORE_SWTMR_LIMIT;
    g_swtmrDebugData = (SwtmrDebugData *)LOS_MemAlloc(m_aucSysMem1, size);
    if (g_swtmrDebugData == NULL) {
        PRINT_ERR("SwtmrDebugDataInit malloc failed!\n");
        return;
    }
    (VOID)memset_s(g_swtmrDebugData, size, 0, size);
#endif
}

STATIC INLINE VOID SwtmrDebugDataUpdate(SWTMR_CTRL_S *swtmr, UINT32 ticks, UINT32 times)
{
#ifdef LOSCFG_SWTMR_DEBUG
    SwtmrDebugData *data = &g_swtmrDebugData[swtmr->usTimerID];
    if (data->period != ticks) {
        (VOID)memset_s(&data->base, sizeof(SwtmrDebugBase), 0, sizeof(SwtmrDebugBase));
        data->period = ticks;
    }
    data->base.startTime = swtmr->startTime;
    data->base.times += times;
#endif
}

STATIC INLINE VOID SwtmrDebugDataStart(SWTMR_CTRL_S *swtmr, UINT16 cpuid)
{
#ifdef LOSCFG_SWTMR_DEBUG
    SwtmrDebugData *data = &g_swtmrDebugData[swtmr->usTimerID];
    data->swtmrUsed = TRUE;
    data->handler = swtmr->pfnHandler;
    data->cpuid = cpuid;
#endif
}

STATIC INLINE VOID SwtmrDebugWaitTimeCalculate(UINT32 swtmrID, SwtmrHandlerItemPtr swtmrHandler)
{
#ifdef LOSCFG_SWTMR_DEBUG
    SwtmrDebugBase *data = &g_swtmrDebugData[swtmrID].base;
    swtmrHandler->swtmrID = swtmrID;
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    UINT64 waitTime = currTime - data->startTime;
    data->waitTime += waitTime;
    if (waitTime > data->waitTimeMax) {
        data->waitTimeMax = waitTime;
    }
    data->readyStartTime = currTime;
    data->waitCount++;
#endif
}

STATIC INLINE VOID SwtmrDebugDataClear(UINT32 swtmrID)
{
#ifdef LOSCFG_SWTMR_DEBUG
    (VOID)memset_s(&g_swtmrDebugData[swtmrID], sizeof(SwtmrDebugData), 0, sizeof(SwtmrDebugData));
#endif
}

STATIC INLINE VOID SwtmrHandler(SwtmrHandlerItemPtr swtmrHandle)
{
#ifdef LOSCFG_SWTMR_DEBUG
    UINT32 intSave;
    SwtmrDebugBase *data = &g_swtmrDebugData[swtmrHandle->swtmrID].base;
    UINT64 startTime = OsGetCurrSchedTimeCycle();
#endif
    swtmrHandle->handler(swtmrHandle->arg);
#ifdef LOSCFG_SWTMR_DEBUG
    UINT64 runTime = OsGetCurrSchedTimeCycle() - startTime;
    SWTMR_LOCK(intSave);
    data->runTime += runTime;
    if (runTime > data->runTimeMax) {
        data->runTimeMax = runTime;
    }
    runTime = startTime - data->readyStartTime;
    data->readyTime += runTime;
    if (runTime > data->readyTimeMax) {
        data->readyTimeMax = runTime;
    }
    data->runCount++;
    SWTMR_UNLOCK(intSave);
#endif
}

STATIC INLINE VOID SwtmrWake(SwtmrRunqueue *srq, UINT64 startTime, SortLinkList *sortList)
{
    UINT32 intSave;
    SWTMR_CTRL_S *swtmr = LOS_DL_LIST_ENTRY(sortList, SWTMR_CTRL_S, stSortList);
    SwtmrHandlerItemPtr swtmrHandler = (SwtmrHandlerItemPtr)LOS_MemboxAlloc(g_swtmrHandlerPool);
    LOS_ASSERT(swtmrHandler != NULL);

    OsHookCall(LOS_HOOK_TYPE_SWTMR_EXPIRED, swtmr);

    SWTMR_LOCK(intSave);
    swtmrHandler->handler = swtmr->pfnHandler;
    swtmrHandler->arg = swtmr->uwArg;
    LOS_ListTailInsert(&srq->swtmrHandlerQueue, &swtmrHandler->node);
    SwtmrDebugWaitTimeCalculate(swtmr->usTimerID, swtmrHandler);

    if (swtmr->ucMode == LOS_SWTMR_MODE_ONCE) {
        SwtmrDelete(swtmr);

        if (swtmr->usTimerID < (OS_SWTMR_MAX_TIMERID - LOSCFG_BASE_CORE_SWTMR_LIMIT)) {
            swtmr->usTimerID += LOSCFG_BASE_CORE_SWTMR_LIMIT;
        } else {
            swtmr->usTimerID %= LOSCFG_BASE_CORE_SWTMR_LIMIT;
        }
    } else if (swtmr->ucMode == LOS_SWTMR_MODE_NO_SELFDELETE) {
        swtmr->ucState = OS_SWTMR_STATUS_CREATED;
    } else {
        swtmr->uwOverrun++;
        swtmr->startTime = startTime;
        (VOID)SwtmrToStart(swtmr, ArchCurrCpuid());
    }

    SWTMR_UNLOCK(intSave);
}

STATIC INLINE VOID ScanSwtmrTimeList(SwtmrRunqueue *srq)
{
    UINT32 intSave;
    SortLinkAttribute *swtmrSortLink = &srq->swtmrSortLink;
    LOS_DL_LIST *listObject = &swtmrSortLink->sortLink;

    /*
     * it needs to be carefully coped with, since the swtmr is in specific sortlink
     * while other cores still has the chance to process it, like stop the timer.
     */
    LOS_SpinLockSave(&swtmrSortLink->spinLock, &intSave);

    if (LOS_ListEmpty(listObject)) {
        LOS_SpinUnlockRestore(&swtmrSortLink->spinLock, intSave);
        return;
    }
    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);

    UINT64 currTime = OsGetCurrSchedTimeCycle();
    while (sortList->responseTime <= currTime) {
        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
        UINT64 startTime = GET_SORTLIST_VALUE(sortList);
        OsDeleteNodeSortLink(swtmrSortLink, sortList);
        LOS_SpinUnlockRestore(&swtmrSortLink->spinLock, intSave);

        SwtmrWake(srq, startTime, sortList);

        LOS_SpinLockSave(&swtmrSortLink->spinLock, &intSave);
        if (LOS_ListEmpty(listObject)) {
            break;
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    }

    LOS_SpinUnlockRestore(&swtmrSortLink->spinLock, intSave);
    return;
}
/**
 * @brief 软时钟的入口函数,拥有任务的最高优先级 0 级!
 * 
 * @return LITE_OS_SEC_TEXT 
 */
STATIC VOID SwtmrTask(VOID)
{
    SwtmrHandlerItem swtmrHandle;
    UINT32 intSave;
    UINT64 waitTime;

    SwtmrRunqueue *srq = &g_swtmrRunqueue[ArchCurrCpuid()];
    LOS_DL_LIST *head = &srq->swtmrHandlerQueue;
    for (;;) {//死循环获取队列item,一直读干净为止
        waitTime = OsSortLinkGetNextExpireTime(OsGetCurrSchedTimeCycle(), &srq->swtmrSortLink);
        if (waitTime != 0) {
            SCHEDULER_LOCK(intSave);
            srq->swtmrTask->ops->delay(srq->swtmrTask, waitTime);
            OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTODELAYEDLIST, srq->swtmrTask);
            SCHEDULER_UNLOCK(intSave);
        }

        ScanSwtmrTimeList(srq);

        while (!LOS_ListEmpty(head)) {
            SwtmrHandlerItemPtr swtmrHandlePtr = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(head), SwtmrHandlerItem, node);
            LOS_ListDelete(&swtmrHandlePtr->node);

            (VOID)memcpy_s(&swtmrHandle, sizeof(SwtmrHandlerItem), swtmrHandlePtr, sizeof(SwtmrHandlerItem));
            (VOID)LOS_MemboxFree(g_swtmrHandlerPool, swtmrHandlePtr);//静态释放内存,注意在鸿蒙内核只有软时钟注册用到了静态内存
            SwtmrHandler(&swtmrHandle);
            }
        }
    }

///创建软时钟任务,每个cpu core都可以拥有自己的软时钟任务
STATIC UINT32 SwtmrTaskCreate(UINT16 cpuid, UINT32 *swtmrTaskID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S swtmrTask;

    (VOID)memset_s(&swtmrTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));//清0
    swtmrTask.pfnTaskEntry = (TSK_ENTRY_FUNC)SwtmrTask;//入口函数
    swtmrTask.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;//16K默认内核任务栈
    swtmrTask.pcName = "Swt_Task";//任务名称
    swtmrTask.usTaskPrio = 0;//哇塞! 逮到一个最高优先级的任务 @note_thinking 这里应该用 OS_TASK_PRIORITY_HIGHEST 表示
    swtmrTask.uwResved = LOS_TASK_STATUS_DETACHED;//分离模式
#ifdef LOSCFG_KERNEL_SMP
    swtmrTask.usCpuAffiMask   = CPUID_TO_AFFI_MASK(cpuid);//交给当前CPU执行这个任务
#endif
    ret = LOS_TaskCreate(swtmrTaskID, &swtmrTask);//创建任务并申请调度
    if (ret == LOS_OK) {
        OS_TCB_FROM_TID(*swtmrTaskID)->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK;//告知这是一个系统任务
    }

    return ret;
}

UINT32 OsSwtmrTaskIDGetByCpuid(UINT16 cpuid)
{
    return g_swtmrRunqueue[cpuid].swtmrTask->taskID;
}

BOOL OsIsSwtmrTask(const LosTaskCB *taskCB)
{
    if (taskCB->taskEntry == (TSK_ENTRY_FUNC)SwtmrTask) {
        return TRUE;
    }
    return FALSE;
}
///回收指定进程的软时钟
LITE_OS_SEC_TEXT_INIT VOID OsSwtmrRecycle(UINTPTR ownerID)
{
    for (UINT16 index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++) {//一个进程往往会有多个定时器
        if (g_swtmrCBArray[index].uwOwnerPid == ownerID) {
            LOS_SwtmrDelete(index);//删除定时器
        }
    }
}
///软时钟初始化 ,注意函数在多CPU情况下会执行多次
STATIC UINT32 SwtmrBaseInit(VOID)
{
    UINT32 ret;
    UINT32 size = sizeof(SWTMR_CTRL_S) * LOSCFG_BASE_CORE_SWTMR_LIMIT;
    SWTMR_CTRL_S *swtmr = (SWTMR_CTRL_S *)LOS_MemAlloc(m_aucSysMem0, size); /* system resident resource */
    if (swtmr == NULL) {
        return LOS_ERRNO_SWTMR_NO_MEMORY;
    }

    (VOID)memset_s(swtmr, size, 0, size);//清0
    g_swtmrCBArray = swtmr;//软时钟
    LOS_ListInit(&g_swtmrFreeList);//初始化空闲链表
    for (UINT16 index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr++) {
            swtmr->usTimerID = index;//按顺序赋值
            LOS_ListTailInsert(&g_swtmrFreeList, &swtmr->stSortList.sortLinkNode);//通过sortLinkNode将节点挂到空闲链表 
    }
	//想要用静态内存池管理,就必须要使用LOS_MEMBOX_SIZE来计算申请的内存大小,因为需要点前缀内存承载头部信息.
    size = LOS_MEMBOX_SIZE(sizeof(SwtmrHandlerItem), OS_SWTMR_HANDLE_QUEUE_SIZE);//规划一片内存区域作为软时钟处理函数的静态内存池。
    g_swtmrHandlerPool = (UINT8 *)LOS_MemAlloc(m_aucSysMem1, size); /* system resident resource */
    if (g_swtmrHandlerPool == NULL) {
        return LOS_ERRNO_SWTMR_NO_MEMORY;
    }

    ret = LOS_MemboxInit(g_swtmrHandlerPool, size, sizeof(SwtmrHandlerItem));
    if (ret != LOS_OK) {
        return LOS_ERRNO_SWTMR_HANDLER_POOL_NO_MEM;
    }

    for (UINT16 index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
        SwtmrRunqueue *srq = &g_swtmrRunqueue[index];
        /* The linked list of all cores must be initialized at core 0 startup for load balancing */
        OsSortLinkInit(&srq->swtmrSortLink);
        LOS_ListInit(&srq->swtmrHandlerQueue);
        srq->swtmrTask = NULL;
    }

    SwtmrDebugDataInit();

    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT UINT32 OsSwtmrInit(VOID)
{
    UINT32 ret;
    UINT32 cpuid = ArchCurrCpuid();
    UINT32 swtmrTaskID;

    if (cpuid == 0) {
        ret = SwtmrBaseInit();
        if (ret != LOS_OK) {
            goto ERROR;
        }
    }

    ret = SwtmrTaskCreate(cpuid, &swtmrTaskID);
    if (ret != LOS_OK) {
        ret = LOS_ERRNO_SWTMR_TASK_CREATE_FAILED;
        goto ERROR;
    }

    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    srq->swtmrTask = OsGetTaskCB(swtmrTaskID);
    return LOS_OK;

ERROR:
    PRINT_ERR("OsSwtmrInit error! ret = %u\n", ret);
    (VOID)LOS_MemFree(m_aucSysMem0, g_swtmrCBArray);
    g_swtmrCBArray = NULL;
    (VOID)LOS_MemFree(m_aucSysMem1, g_swtmrHandlerPool);
    g_swtmrHandlerPool = NULL;
    return ret;
}

#ifdef LOSCFG_KERNEL_SMP
STATIC INLINE VOID FindIdleSwtmrRunqueue(UINT16 *idleCpuid)
{
    SwtmrRunqueue *idleRq = &g_swtmrRunqueue[0];
    UINT32 nodeNum = OsGetSortLinkNodeNum(&idleRq->swtmrSortLink);
    UINT16 cpuid = 1;
    do {
        SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
        UINT32 temp = OsGetSortLinkNodeNum(&srq->swtmrSortLink);
        if (nodeNum > temp) {
            *idleCpuid = cpuid;
            nodeNum = temp;
        }
        cpuid++;
    } while (cpuid < LOSCFG_KERNEL_CORE_NUM);
}
#endif

STATIC INLINE VOID AddSwtmr2TimeList(SortLinkList *node, UINT64 responseTime, UINT16 cpuid)
{
    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    OsAdd2SortLink(&srq->swtmrSortLink, node, responseTime, cpuid);
}

STATIC INLINE VOID DeSwtmrFromTimeList(SortLinkList *node)
{
#ifdef LOSCFG_KERNEL_SMP
    UINT16 cpuid = OsGetSortLinkNodeCpuid(node);
#else
    UINT16 cpuid = 0;
#endif
    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    OsDeleteFromSortLink(&srq->swtmrSortLink, node);
    return;
}

STATIC VOID SwtmrAdjustCheck(UINT16 cpuid, UINT64 responseTime)
{
    UINT32 ret;
    UINT32 intSave;
    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    SCHEDULER_LOCK(intSave);
    if ((srq->swtmrTask == NULL) || !OsTaskIsBlocked(srq->swtmrTask)) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    if (responseTime >= GET_SORTLIST_VALUE(&srq->swtmrTask->sortList)) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    ret = OsSchedTimeoutQueueAdjust(srq->swtmrTask, responseTime);
    SCHEDULER_UNLOCK(intSave);
    if (ret != LOS_OK) {
        return;
    }

    if (cpuid == ArchCurrCpuid()) {
        OsSchedExpireTimeUpdate();
    } else {
        LOS_MpSchedule(CPUID_TO_AFFI_MASK(cpuid));
    }
}

STATIC UINT64 SwtmrToStart(SWTMR_CTRL_S *swtmr, UINT16 cpuid)
{
    UINT32 ticks;
    UINT32 times = 0;

    if ((swtmr->uwOverrun == 0) && ((swtmr->ucMode == LOS_SWTMR_MODE_ONCE) ||
        (swtmr->ucMode == LOS_SWTMR_MODE_OPP) ||
        (swtmr->ucMode == LOS_SWTMR_MODE_NO_SELFDELETE))) {//如果是一次性的定时器
        ticks = swtmr->uwExpiry; //获取时间间隔
    } else {
        ticks = swtmr->uwInterval;//获取周期性定时器时间间隔
    }
    swtmr->ucState = OS_SWTMR_STATUS_TICKING;//计数状态

    UINT64 period = (UINT64)ticks * OS_CYCLE_PER_TICK;
    UINT64 responseTime = swtmr->startTime + period;
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    if (responseTime < currTime) {
        times = (UINT32)((currTime - swtmr->startTime) / period);
        swtmr->startTime += times * period;
        responseTime = swtmr->startTime + period;
        PRINT_WARN("Swtmr already timeout! SwtmrID: %u\n", swtmr->usTimerID);
    }

    AddSwtmr2TimeList(&swtmr->stSortList, responseTime, cpuid);
    SwtmrDebugDataUpdate(swtmr, ticks, times);
    return responseTime;
}

/*
 * Description: Start Software Timer
 * Input      : swtmr --- Need to start software timer
 */
STATIC INLINE VOID SwtmrStart(SWTMR_CTRL_S *swtmr)
{
    UINT64 responseTime;
    UINT16 idleCpu = 0;
#ifdef LOSCFG_KERNEL_SMP
    FindIdleSwtmrRunqueue(&idleCpu);
#endif
    swtmr->startTime = OsGetCurrSchedTimeCycle();
    responseTime = SwtmrToStart(swtmr, idleCpu);

    SwtmrDebugDataStart(swtmr, idleCpu);

    SwtmrAdjustCheck(idleCpu, responseTime);
}

/*
 * Description: Delete Software Timer
 * Input      : swtmr --- Need to delete software timer, When using, Ensure that it can't be NULL.
 */
STATIC INLINE VOID SwtmrDelete(SWTMR_CTRL_S *swtmr)
{
    /* insert to free list */
    LOS_ListTailInsert(&g_swtmrFreeList, &swtmr->stSortList.sortLinkNode);//直接插入空闲链表中,回收再利用
    swtmr->ucState = OS_SWTMR_STATUS_UNUSED;//又干净着呢
    swtmr->uwOwnerPid = OS_INVALID_VALUE;
    SwtmrDebugDataClear(swtmr->usTimerID);
}

STATIC INLINE VOID SwtmrRestart(UINT64 startTime, SortLinkList *sortList, UINT16 cpuid)
{
    UINT32 intSave;

    SWTMR_CTRL_S *swtmr = LOS_DL_LIST_ENTRY(sortList, SWTMR_CTRL_S, stSortList);
    SWTMR_LOCK(intSave);
    swtmr->startTime = startTime;
    (VOID)SwtmrToStart(swtmr, cpuid);
    SWTMR_UNLOCK(intSave);
}

VOID OsSwtmrResponseTimeReset(UINT64 startTime)
{
    UINT16 cpuid = ArchCurrCpuid();
    SortLinkAttribute *swtmrSortLink = &g_swtmrRunqueue[cpuid].swtmrSortLink;
    LOS_DL_LIST *listHead = &swtmrSortLink->sortLink;
    LOS_DL_LIST *listNext = listHead->pstNext;

    LOS_SpinLock(&swtmrSortLink->spinLock);
    while (listNext != listHead) {
        SortLinkList *sortList = LOS_DL_LIST_ENTRY(listNext, SortLinkList, sortLinkNode);
        OsDeleteNodeSortLink(swtmrSortLink, sortList);
        LOS_SpinUnlock(&swtmrSortLink->spinLock);

        SwtmrRestart(startTime, sortList, cpuid);

        LOS_SpinLock(&swtmrSortLink->spinLock);
        listNext = listNext->pstNext;
    }
    LOS_SpinUnlock(&swtmrSortLink->spinLock);
}

STATIC INLINE BOOL SwtmrRunqueueFind(SortLinkAttribute *swtmrSortLink, SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg)
{
    LOS_DL_LIST *listObject = &swtmrSortLink->sortLink;
    LOS_DL_LIST *list = listObject->pstNext;

    LOS_SpinLock(&swtmrSortLink->spinLock);
    while (list != listObject) {
        SortLinkList *listSorted = LOS_DL_LIST_ENTRY(list, SortLinkList, sortLinkNode);
        if (checkFunc((UINTPTR)listSorted, arg)) {
            LOS_SpinUnlock(&swtmrSortLink->spinLock);
            return TRUE;
        }
        list = list->pstNext;
    }

    LOS_SpinUnlock(&swtmrSortLink->spinLock);
    return FALSE;
}

STATIC BOOL SwtmrTimeListFind(SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg)
{
    for (UINT16 cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {
        SortLinkAttribute *swtmrSortLink = &g_swtmrRunqueue[cpuid].swtmrSortLink;
        if (SwtmrRunqueueFind(swtmrSortLink, checkFunc, arg)) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL OsSwtmrWorkQueueFind(SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg)
{
    UINT32 intSave;

    SWTMR_LOCK(intSave);
    BOOL find = SwtmrTimeListFind(checkFunc, arg);
    SWTMR_UNLOCK(intSave);
    return find;
}

/*
 * Description: Get next timeout
 * Return     : Count of the Timer list
 */
LITE_OS_SEC_TEXT UINT32 OsSwtmrGetNextTimeout(VOID)
{
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    SwtmrRunqueue *srq = &g_swtmrRunqueue[ArchCurrCpuid()];
    UINT64 time = (OsSortLinkGetNextExpireTime(currTime, &srq->swtmrSortLink) / OS_CYCLE_PER_TICK);
    if (time > OS_INVALID_VALUE) {
        time = OS_INVALID_VALUE;
    }
    return (UINT32)time;
}

/*
 * Description: Stop of Software Timer interface
 * Input      : swtmr --- the software timer control handler
 */
STATIC VOID SwtmrStop(SWTMR_CTRL_S *swtmr)
{
    swtmr->ucState = OS_SWTMR_STATUS_CREATED;
    swtmr->uwOverrun = 0;

    DeSwtmrFromTimeList(&swtmr->stSortList);
}

/*
 * Description: Get next software timer expiretime
 * Input      : swtmr --- the software timer control handler
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsSwtmrTimeGet(const SWTMR_CTRL_S *swtmr)
{
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    UINT64 time = (OsSortLinkGetTargetExpireTime(currTime, &swtmr->stSortList) / OS_CYCLE_PER_TICK);
    if (time > OS_INVALID_VALUE) {
        time = OS_INVALID_VALUE;
    }
    return (UINT32)time;
}
///创建定时器，设置定时器的定时时长、定时器模式、回调函数，并返回定时器ID
LITE_OS_SEC_TEXT_INIT UINT32 LOS_SwtmrCreate(UINT32 interval,
                                             UINT8 mode,
                                             SWTMR_PROC_FUNC handler,
                                             UINT16 *swtmrID,
                                             UINTPTR arg)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    SortLinkList *sortList = NULL;

    if (interval == 0) {
        return LOS_ERRNO_SWTMR_INTERVAL_NOT_SUITED;
    }

    if ((mode != LOS_SWTMR_MODE_ONCE) && (mode != LOS_SWTMR_MODE_PERIOD) &&
        (mode != LOS_SWTMR_MODE_NO_SELFDELETE)) {
        return LOS_ERRNO_SWTMR_MODE_INVALID;
    }

    if (handler == NULL) {
        return LOS_ERRNO_SWTMR_PTR_NULL;
    }

    if (swtmrID == NULL) {
        return LOS_ERRNO_SWTMR_RET_PTR_NULL;
    }

    SWTMR_LOCK(intSave);
    if (LOS_ListEmpty(&g_swtmrFreeList)) {//空闲链表不能为空
        SWTMR_UNLOCK(intSave);
        return LOS_ERRNO_SWTMR_MAXSIZE;
    }

    sortList = LOS_DL_LIST_ENTRY(g_swtmrFreeList.pstNext, SortLinkList, sortLinkNode);
    swtmr = LOS_DL_LIST_ENTRY(sortList, SWTMR_CTRL_S, stSortList);
    LOS_ListDelete(LOS_DL_LIST_FIRST(&g_swtmrFreeList));//
    SWTMR_UNLOCK(intSave);

    swtmr->uwOwnerPid = (UINTPTR)OsCurrProcessGet();
    swtmr->pfnHandler = handler;//时间到了的回调函数
    swtmr->ucMode = mode;	//定时器模式
    swtmr->uwOverrun = 0;
    swtmr->uwInterval = interval;	//周期性超时间隔
    swtmr->uwExpiry = interval;		//一次性超时间隔
    swtmr->uwArg = arg;				//回调函数的参数
    swtmr->ucState = OS_SWTMR_STATUS_CREATED;	//已创建状态
    SET_SORTLIST_VALUE(&swtmr->stSortList, OS_SORT_LINK_INVALID_TIME);
    *swtmrID = swtmr->usTimerID;
    OsHookCall(LOS_HOOK_TYPE_SWTMR_CREATE, swtmr);
    return LOS_OK;
}
///接口函数 启动定时器       参数定时任务ID
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrStart(UINT16 swtmrID)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;//取模
    swtmr = g_swtmrCBArray + swtmrCBID;//获取定时器控制结构体

    SWTMR_LOCK(intSave);
    if (swtmr->usTimerID != swtmrID) {//ID必须一样
        SWTMR_UNLOCK(intSave);
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    switch (swtmr->ucState) {//判断定时器状态
        case OS_SWTMR_STATUS_UNUSED:
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;
            break;
        /* 如果定时器的状态为启动中,应先停止定时器再重新启动
         * If the status of swtmr is timing, it should stop the swtmr first,
         * then start the swtmr again.
         */
        case OS_SWTMR_STATUS_TICKING://正在计数的定时器
            SwtmrStop(swtmr);//先停止定时器,注意这里没有break;,在OsSwtmrStop中状态将会回到了OS_SWTMR_STATUS_CREATED 接下来就是执行启动了
            /* fall-through */
        case OS_SWTMR_STATUS_CREATED://已经创建好了
            SwtmrStart(swtmr);
            break;
        default:
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }

    SWTMR_UNLOCK(intSave);
    OsHookCall(LOS_HOOK_TYPE_SWTMR_START, swtmr);
    return ret;
}
///接口函数 停止定时器          参数定时任务ID
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrStop(UINT16 swtmrID)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;//取模
    swtmr = g_swtmrCBArray + swtmrCBID;//获取定时器控制结构体
    SWTMR_LOCK(intSave);

    if (swtmr->usTimerID != swtmrID) {//ID必须一样
        SWTMR_UNLOCK(intSave);
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    switch (swtmr->ucState) {//判断定时器状态
        case OS_SWTMR_STATUS_UNUSED:
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;//返回没有创建
            break;
        case OS_SWTMR_STATUS_CREATED:
            ret = LOS_ERRNO_SWTMR_NOT_STARTED;//返回没有开始
            break;
        case OS_SWTMR_STATUS_TICKING://正在计数
            SwtmrStop(swtmr);//执行正在停止定时器操作
            break;
        default:
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }

    SWTMR_UNLOCK(intSave);
    OsHookCall(LOS_HOOK_TYPE_SWTMR_STOP, swtmr);
    return ret;
}
///接口函数 获得软件定时器剩余Tick数 通过 *tick 带走 
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrTimeGet(UINT16 swtmrID, UINT32 *tick)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    if (tick == NULL) {
        return LOS_ERRNO_SWTMR_TICK_PTR_NULL;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;//取模
    swtmr = g_swtmrCBArray + swtmrCBID;//获取定时器控制结构体
    SWTMR_LOCK(intSave);

    if (swtmr->usTimerID != swtmrID) {//ID必须一样
        SWTMR_UNLOCK(intSave);
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }
    switch (swtmr->ucState) {
        case OS_SWTMR_STATUS_UNUSED:
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;
            break;
        case OS_SWTMR_STATUS_CREATED:
            ret = LOS_ERRNO_SWTMR_NOT_STARTED;
            break;
        case OS_SWTMR_STATUS_TICKING://正在计数的定时器
            *tick = OsSwtmrTimeGet(swtmr);//获取
            break;
        default:
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }
    SWTMR_UNLOCK(intSave);
    return ret;
}
///接口函数 删除定时器
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrDelete(UINT16 swtmrID)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;//取模
    swtmr = g_swtmrCBArray + swtmrCBID;//获取定时器控制结构体
    SWTMR_LOCK(intSave);

    if (swtmr->usTimerID != swtmrID) {//ID必须一样
        SWTMR_UNLOCK(intSave);
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    switch (swtmr->ucState) {
        case OS_SWTMR_STATUS_UNUSED:
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;
            break;
        case OS_SWTMR_STATUS_TICKING://正在计数就先停止再删除,这里没有break;
            SwtmrStop(swtmr);
            /* fall-through */
        case OS_SWTMR_STATUS_CREATED://再删除定时器
            SwtmrDelete(swtmr);
            break;
        default:
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }

    SWTMR_UNLOCK(intSave);
    OsHookCall(LOS_HOOK_TYPE_SWTMR_DELETE, swtmr);
    return ret;
}

#endif /* LOSCFG_BASE_CORE_SWTMR_ENABLE */

