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

LITE_OS_SEC_BSS SWTMR_CTRL_S    *g_swtmrCBArray = NULL;     /**< First address in Timer memory space  \n 定时器池 */
LITE_OS_SEC_BSS UINT8           *g_swtmrHandlerPool = NULL; /**< Pool of Swtmr Handler \n 用于注册软时钟的回调函数 */
LITE_OS_SEC_BSS LOS_DL_LIST     g_swtmrFreeList;            /**< Free list of Software Timer \n 空闲定时器链表 */

/* spinlock for swtmr module, only available on SMP mode */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_swtmrSpin);///< 初始化软时钟自旋锁,只有SMP情况才需要,只要是自旋锁都是用于CPU多核的同步
#define SWTMR_LOCK(state)       LOS_SpinLockSave(&g_swtmrSpin, &(state))///< 持有软时钟自旋锁
#define SWTMR_UNLOCK(state)     LOS_SpinUnlockRestore(&g_swtmrSpin, (state))///< 释放软时钟自旋锁

/**
 * @brief 软时钟的入口函数,拥有任务的最高优先级 0 级!
 * 
 * @return LITE_OS_SEC_TEXT 
 */
LITE_OS_SEC_TEXT VOID OsSwtmrTask(VOID)
{
    SwtmrHandlerItemPtr swtmrHandlePtr = NULL;
    SwtmrHandlerItem swtmrHandle;
    UINT32 ret, swtmrHandlerQueue;

    swtmrHandlerQueue = OsPercpuGet()->swtmrHandlerQueue;//获取定时器超时队列
    for (;;) {//死循环获取队列item,一直读干净为止
        ret = LOS_QueueRead(swtmrHandlerQueue, &swtmrHandlePtr, sizeof(CHAR *), LOS_WAIT_FOREVER);//一个一个读队列
        if ((ret == LOS_OK) && (swtmrHandlePtr != NULL)) {
            swtmrHandle.handler = swtmrHandlePtr->handler;//超时中断处理函数,也称回调函数
            swtmrHandle.arg = swtmrHandlePtr->arg;//回调函数的参数
            (VOID)LOS_MemboxFree(g_swtmrHandlerPool, swtmrHandlePtr);//静态释放内存,注意在鸿蒙内核只有软时钟注册用到了静态内存
            if (swtmrHandle.handler != NULL) {
                swtmrHandle.handler(swtmrHandle.arg);//回调函数处理函数
            }
        }
    }
}
///创建软时钟任务,每个cpu core都可以拥有自己的软时钟任务
LITE_OS_SEC_TEXT_INIT UINT32 OsSwtmrTaskCreate(VOID)
{
    UINT32 ret, swtmrTaskID;
    TSK_INIT_PARAM_S swtmrTask;
    UINT32 cpuid = ArchCurrCpuid();//获取当前CPU id

    (VOID)memset_s(&swtmrTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));//清0
    swtmrTask.pfnTaskEntry = (TSK_ENTRY_FUNC)OsSwtmrTask;//入口函数
    swtmrTask.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;//16K默认内核任务栈
    swtmrTask.pcName = "Swt_Task";//任务名称
    swtmrTask.usTaskPrio = 0;//哇塞! 逮到一个最高优先级的任务 @note_thinking 这里应该用 OS_TASK_PRIORITY_HIGHEST 表示
    swtmrTask.uwResved = LOS_TASK_STATUS_DETACHED;//分离模式
#ifdef LOSCFG_KERNEL_SMP
    swtmrTask.usCpuAffiMask   = CPUID_TO_AFFI_MASK(cpuid);//交给当前CPU执行这个任务
#endif
    ret = LOS_TaskCreate(&swtmrTaskID, &swtmrTask);//创建任务并申请调度
    if (ret == LOS_OK) {
        g_percpu[cpuid].swtmrTaskID = swtmrTaskID;//全局变量记录 软时钟任务ID
        OS_TCB_FROM_TID(swtmrTaskID)->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK;//告知这是一个系统任务
    }

    return ret;
}
///回收指定进程的软时钟
LITE_OS_SEC_TEXT_INIT VOID OsSwtmrRecycle(UINT32 processID)
{
    for (UINT16 index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++) {//一个进程往往会有多个定时器
        if (g_swtmrCBArray[index].uwOwnerPid == processID) {//找到一个
            LOS_SwtmrDelete(index);//删除定时器
        }
    }
}
///软时钟初始化 ,注意函数在多CPU情况下会执行多次
LITE_OS_SEC_TEXT_INIT UINT32 OsSwtmrInit(VOID)
{
    UINT32 size;
    UINT16 index;
    UINT32 ret;
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 swtmrHandlePoolSize;
    UINT32 cpuid = ArchCurrCpuid();
    if (cpuid == 0) {//确保以下代码块由一个CPU执行,g_swtmrCBArray和g_swtmrHandlerPool 是所有CPU共用的
        size = sizeof(SWTMR_CTRL_S) * LOSCFG_BASE_CORE_SWTMR_LIMIT;//申请软时钟内存大小 
        swtmr = (SWTMR_CTRL_S *)LOS_MemAlloc(m_aucSysMem0, size); /* system resident resource */ //常驻内存
        if (swtmr == NULL) {
            ret = LOS_ERRNO_SWTMR_NO_MEMORY;
            goto ERROR;
        }

        (VOID)memset_s(swtmr, size, 0, size);//清0
        g_swtmrCBArray = swtmr;//软时钟
        LOS_ListInit(&g_swtmrFreeList);//初始化空闲链表
        for (index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr++) {
            swtmr->usTimerID = index;//按顺序赋值
            LOS_ListTailInsert(&g_swtmrFreeList, &swtmr->stSortList.sortLinkNode);//通过sortLinkNode将节点挂到空闲链表 
        }
		//想要用静态内存池管理,就必须要使用LOS_MEMBOX_SIZE来计算申请的内存大小,因为需要点前缀内存承载头部信息.
        swtmrHandlePoolSize = LOS_MEMBOX_SIZE(sizeof(SwtmrHandlerItem), OS_SWTMR_HANDLE_QUEUE_SIZE);//计算所有注册函数内存大小
		//规划一片内存区域作为软时钟处理函数的静态内存池。
        g_swtmrHandlerPool = (UINT8 *)LOS_MemAlloc(m_aucSysMem1, swtmrHandlePoolSize); /* system resident resource *///常驻内存
        if (g_swtmrHandlerPool == NULL) {
            ret = LOS_ERRNO_SWTMR_NO_MEMORY;
            goto ERROR;
        }

        ret = LOS_MemboxInit(g_swtmrHandlerPool, swtmrHandlePoolSize, sizeof(SwtmrHandlerItem));//初始化软时钟注册池
        if (ret != LOS_OK) {
            ret = LOS_ERRNO_SWTMR_HANDLER_POOL_NO_MEM;
            goto ERROR;
        }
        ret = OsSchedSwtmrScanRegister((SchedScan)OsSwtmrScan);
        if (ret != LOS_OK) {
            goto ERROR;
        }
    }
	//每个CPU都会创建一个属于自己的 OS_SWTMR_HANDLE_QUEUE_SIZE 的队列
    ret = LOS_QueueCreate(NULL, OS_SWTMR_HANDLE_QUEUE_SIZE, &g_percpu[cpuid].swtmrHandlerQueue, 0, sizeof(CHAR *));//为当前CPU core 创建软时钟队列 maxMsgSize:sizeof(CHAR *)
    if (ret != LOS_OK) {
        ret = LOS_ERRNO_SWTMR_QUEUE_CREATE_FAILED;
        goto ERROR;
    }

    ret = OsSwtmrTaskCreate();//每个CPU独自创建属于自己的软时钟任务,统一处理队列
    if (ret != LOS_OK) {
        ret = LOS_ERRNO_SWTMR_TASK_CREATE_FAILED;
        goto ERROR;
    }

    ret = OsSortLinkInit(&g_percpu[cpuid].swtmrSortLink);//每个CPU独自对自己软时钟链表排序初始化,为啥要排序因为每个定时器的时间不一样,鸿蒙把用时短的排在前面
    if (ret != LOS_OK) {
        ret = LOS_ERRNO_SWTMR_SORTLINK_CREATE_FAILED;
        goto ERROR;
    }

    return LOS_OK;
ERROR:
    PRINT_ERR("OsSwtmrInit error! ret = %u\n", ret);
    return ret;
}

/*
 * Description: Start Software Timer \n 开始定时器
 * Input      : swtmr --- Need to start software timer
 */
LITE_OS_SEC_TEXT VOID OsSwtmrStart(UINT64 currTime, SWTMR_CTRL_S *swtmr)
{
    UINT32 ticks;

    if ((swtmr->uwOverrun == 0) && ((swtmr->ucMode == LOS_SWTMR_MODE_ONCE) ||
        (swtmr->ucMode == LOS_SWTMR_MODE_OPP) ||
        (swtmr->ucMode == LOS_SWTMR_MODE_NO_SELFDELETE))) {
        ticks = swtmr->uwExpiry;
    } else {
        ticks = swtmr->uwInterval;
    }
    swtmr->ucState = OS_SWTMR_STATUS_TICKING;

    OsAdd2SortLink(&swtmr->stSortList, swtmr->startTime, ticks, OS_SORT_LINK_SWTMR);
    OsSchedUpdateExpireTime(currTime);
    return;
}

/*
 * Description: Delete Software Timer
 * Input      : swtmr --- Need to delete software timer, When using, Ensure that it can't be NULL.
 */
STATIC INLINE VOID OsSwtmrDelete(SWTMR_CTRL_S *swtmr)
{
    /* insert to free list */
    LOS_ListTailInsert(&g_swtmrFreeList, &swtmr->stSortList.sortLinkNode);//直接插入空闲链表中,回收再利用
    swtmr->ucState = OS_SWTMR_STATUS_UNUSED;//又干净着呢
    swtmr->uwOwnerPid = 0;//谁拥有这个定时器? 是 0号进程
}

STATIC INLINE VOID OsWakePendTimeSwtmr(Percpu *cpu, UINT64 currTime, SWTMR_CTRL_S *swtmr)
{
    LOS_SpinLock(&g_swtmrSpin);
    SwtmrHandlerItemPtr swtmrHandler = (SwtmrHandlerItemPtr)LOS_MemboxAlloc(g_swtmrHandlerPool);
    if (swtmrHandler != NULL) {
        swtmrHandler->handler = swtmr->pfnHandler;
        swtmrHandler->arg = swtmr->uwArg;

        if (LOS_QueueWrite(cpu->swtmrHandlerQueue, swtmrHandler, sizeof(CHAR *), LOS_NO_WAIT)) {
            (VOID)LOS_MemboxFree(g_swtmrHandlerPool, swtmrHandler);
        }
    }

    if (swtmr->ucMode == LOS_SWTMR_MODE_ONCE) {
        OsSwtmrDelete(swtmr);

        if (swtmr->usTimerID < (OS_SWTMR_MAX_TIMERID - LOSCFG_BASE_CORE_SWTMR_LIMIT)) {
            swtmr->usTimerID += LOSCFG_BASE_CORE_SWTMR_LIMIT;
        } else {
            swtmr->usTimerID %= LOSCFG_BASE_CORE_SWTMR_LIMIT;
        }
    } else if (swtmr->ucMode == LOS_SWTMR_MODE_NO_SELFDELETE) {
        swtmr->ucState = OS_SWTMR_STATUS_CREATED;
    } else {
        swtmr->uwOverrun++;
        OsSwtmrStart(currTime, swtmr);
    }

    LOS_SpinUnlock(&g_swtmrSpin);
}
/*
 * Description: Tick interrupt interface module of software timer
 * Return     : LOS_OK on success or error code on failure
 */
/*!
 * @brief OsSwtmrScan 由系统时钟中断处理函数调用	
 * 扫描定时器,如果碰到超时的,就放入超时队列
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT VOID OsSwtmrScan(VOID)
{
    Percpu *cpu = OsPercpuGet();//获取当前CPU
    SortLinkAttribute* swtmrSortLink = &OsPercpuGet()->swtmrSortLink;//获取需由CPU处理的软件定时器总信息
    LOS_DL_LIST *listObject = &swtmrSortLink->sortLink;//获取定时器链表,上面挂的是等待时间到触发的定时器

    /*
     * it needs to be carefully coped with, since the swtmr is in specific sortlink
     * while other cores still has the chance to process it, like stop the timer.
     */
    LOS_SpinLock(&cpu->swtmrSortLinkSpin);

    if (LOS_ListEmpty(listObject)) {
        LOS_SpinUnlock(&cpu->swtmrSortLinkSpin);
        return;
    }
    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//获取节点

    UINT64 currTime = OsGetCurrSchedTimeCycle();//获取当前时间,用于比较所有定时器的时间是否到了
    while (sortList->responseTime <= currTime) {//说明有定时器的时间到了,需要去触发定时器了
        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//这行代码多余了,可以删除 @note_why 
        SWTMR_CTRL_S *swtmr = LOS_DL_LIST_ENTRY(sortList, SWTMR_CTRL_S, stSortList);//获取软件定时器控制块
        swtmr->startTime = GET_SORTLIST_VALUE(sortList);//获取该定时器的响应时间
        OsDeleteNodeSortLink(swtmrSortLink, sortList);//将其从链表上摘除
        LOS_SpinUnlock(&cpu->swtmrSortLinkSpin);

        OsHookCall(LOS_HOOK_TYPE_SWTMR_EXPIRED, swtmr);//回调钩子函数 将调用 LOS_TraceSwtmrExpired
        OsWakePendTimeSwtmr(cpu, currTime, swtmr);//触发定时器

        LOS_SpinLock(&cpu->swtmrSortLinkSpin);
        if (LOS_ListEmpty(listObject)) {//链表为空就退出
            break;
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//继续下一个节点
    }

    LOS_SpinUnlock(&cpu->swtmrSortLinkSpin);
}

/*
 * Description: Get next timeout
 * Return     : Count of the Timer list
 */
LITE_OS_SEC_TEXT UINT32 OsSwtmrGetNextTimeout(VOID)//获取下一个timeout
{
    return OsSortLinkGetNextExpireTime(&OsPercpuGet()->swtmrSortLink);
}

/*
 * Description: Stop of Software Timer interface
 * Input      : swtmr --- the software timer contrl handler
 */
LITE_OS_SEC_TEXT STATIC VOID OsSwtmrStop(SWTMR_CTRL_S *swtmr)
{
    OsDeleteSortLink(&swtmr->stSortList, OS_SORT_LINK_SWTMR);

    swtmr->ucState = OS_SWTMR_STATUS_CREATED;
    swtmr->uwOverrun = 0;

    OsSchedUpdateExpireTime(OsGetCurrSchedTimeCycle());
}

/*
 * Description: Get next software timer expiretime
 * Input      : swtmr --- the software timer contrl handler
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsSwtmrTimeGet(const SWTMR_CTRL_S *swtmr)
{
    return OsSortLinkGetTargetExpireTime(&swtmr->stSortList);
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

    swtmr->uwOwnerPid = OsCurrProcessGet()->processID;//定时器进程归属设定
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
            OsSwtmrStop(swtmr);//先停止定时器,注意这里没有break;,在OsSwtmrStop中状态将会回到了OS_SWTMR_STATUS_CREATED 接下来就是执行启动了
            /* fall-through */
        case OS_SWTMR_STATUS_CREATED://已经创建好了
            swtmr->startTime = OsGetCurrSchedTimeCycle();
            OsSwtmrStart(swtmr->startTime, swtmr);
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
            OsSwtmrStop(swtmr);//执行正在停止定时器操作
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
            OsSwtmrStop(swtmr);
            /* fall-through */
        case OS_SWTMR_STATUS_CREATED://再删除定时器
            OsSwtmrDelete(swtmr);
            break;
        default:
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }

    SWTMR_UNLOCK(intSave);
    OsHookCall(LOS_HOOK_TYPE_SWTMR_DELETE, swtmr);
    return ret;
}

#endif /* (LOSCFG_BASE_CORE_SWTMR == YES) */

