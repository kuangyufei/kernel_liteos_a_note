/*!
* @file  los_swtmr.c
* @brief  软定时器主文件
* @details  实现软件定时器的创建、启动、停止、删除及回调处理等功能
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
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

/**
 * @brief 删除软件定时器
 * @details 将指定的软件定时器从计时链表中移除并置为未使用状态
 * @param swtmr 软件定时器控制块指针
 * @return 无
 */
STATIC INLINE VOID SwtmrDelete(SWTMR_CTRL_S *swtmr);

/**
 * @brief 启动软件定时器并计算到期时间
 * @details 根据定时器模式和当前时间计算下一次到期时间，并添加到计时链表
 * @param swtmr 软件定时器控制块指针
 * @param cpuid CPU核心ID
 * @return 定时器到期时间（周期数）
 */
STATIC INLINE UINT64 SwtmrToStart(SWTMR_CTRL_S *swtmr, UINT16 cpuid);

LITE_OS_SEC_BSS SWTMR_CTRL_S    *g_swtmrCBArray = NULL;     /* 定时器内存空间首地址 */
LITE_OS_SEC_BSS UINT8           *g_swtmrHandlerPool = NULL; /* 软件定时器处理函数池 */
LITE_OS_SEC_BSS LOS_DL_LIST     g_swtmrFreeList;            /* 软件定时器空闲链表 */

/* SMP模式下使用的自旋锁 */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_swtmrSpin);
#define SWTMR_LOCK(state)       LOS_SpinLockSave(&g_swtmrSpin, &(state))  // 获取软件定时器自旋锁并保存中断状态
#define SWTMR_UNLOCK(state)     LOS_SpinUnlockRestore(&g_swtmrSpin, (state))  // 恢复中断状态并释放软件定时器自旋锁

typedef struct {
    SortLinkAttribute swtmrSortLink;  // 软件定时器排序链表属性
    LosTaskCB         *swtmrTask;     // 软件定时器任务控制块指针
    LOS_DL_LIST       swtmrHandlerQueue;  // 软件定时器超时处理队列
} SwtmrRunqueue;

STATIC SwtmrRunqueue g_swtmrRunqueue[LOSCFG_KERNEL_CORE_NUM];  // 每个CPU核心的软件定时器运行队列

#ifdef LOSCFG_SWTMR_DEBUG
#define OS_SWTMR_PERIOD_TO_CYCLE(period) (((UINT64)(period) * OS_NS_PER_TICK) / OS_NS_PER_CYCLE)  // 将周期(Tick)转换为周期数
STATIC SwtmrDebugData *g_swtmrDebugData = NULL;  // 软件定时器调试数据

/**
 * @brief 检查软件定时器调试数据是否被使用
 * @details 判断指定ID的软件定时器调试数据是否处于使用状态
 * @param swtmrID 软件定时器ID
 * @return TRUE-已使用，FALSE-未使用
 */
BOOL OsSwtmrDebugDataUsed(UINT32 swtmrID)
{
    if (swtmrID > LOSCFG_BASE_CORE_SWTMR_LIMIT) {
        return FALSE;  // 定时器ID超出限制，返回未使用
    }

    return g_swtmrDebugData[swtmrID].swtmrUsed;  // 返回调试数据的使用状态
}

/**
 * @brief 获取软件定时器调试数据
 * @details 将指定ID的软件定时器调试数据复制到输出缓冲区
 * @param swtmrID 软件定时器ID
 * @param data 输出缓冲区指针
 * @param len 输出缓冲区长度
 * @param mode 定时器模式输出指针
 * @return LOS_OK-成功，LOS_NOK-失败
 */
UINT32 OsSwtmrDebugDataGet(UINT32 swtmrID, SwtmrDebugData *data, UINT32 len, UINT8 *mode)
{
    UINT32 intSave;
    errno_t ret;

    if ((swtmrID > LOSCFG_BASE_CORE_SWTMR_LIMIT) || (data == NULL) ||
        (mode == NULL) || (len < sizeof(SwtmrDebugData))) {
        return LOS_NOK;  // 参数无效，返回失败
    }

    SWTMR_CTRL_S *swtmr = &g_swtmrCBArray[swtmrID];
    SWTMR_LOCK(intSave);  // 加锁保护调试数据访问
    ret = memcpy_s(data, len, &g_swtmrDebugData[swtmrID], sizeof(SwtmrDebugData));
    *mode = swtmr->ucMode;  // 获取定时器模式
    SWTMR_UNLOCK(intSave);  // 解锁
    if (ret != EOK) {
        return LOS_NOK;  // 复制失败，返回错误
    }
    return LOS_OK;  // 成功获取调试数据
}
#endif

/**
 * @brief 初始化软件定时器调试数据
 * @details 为软件定时器调试数据分配内存并初始化为0
 * @param 无
 * @return 无
 */
STATIC VOID SwtmrDebugDataInit(VOID)
{
#ifdef LOSCFG_SWTMR_DEBUG
    UINT32 size = sizeof(SwtmrDebugData) * LOSCFG_BASE_CORE_SWTMR_LIMIT;
    g_swtmrDebugData = (SwtmrDebugData *)LOS_MemAlloc(m_aucSysMem1, size);
    if (g_swtmrDebugData == NULL) {
        PRINT_ERR("SwtmrDebugDataInit malloc failed!\n");  // 内存分配失败，打印错误信息
        return;
    }
    (VOID)memset_s(g_swtmrDebugData, size, 0, size);  // 初始化调试数据内存为0
#endif
}

/**
 * @brief 更新软件定时器调试数据
 * @details 更新定时器周期和触发次数等调试信息
 * @param swtmr 软件定时器控制块指针
 * @param ticks 定时器周期(Tick)
 * @param times 触发次数
 * @return 无
 */
STATIC INLINE VOID SwtmrDebugDataUpdate(SWTMR_CTRL_S *swtmr, UINT32 ticks, UINT32 times)
{
#ifdef LOSCFG_SWTMR_DEBUG
    SwtmrDebugData *data = &g_swtmrDebugData[swtmr->usTimerID];
    if (data->period != ticks) {
        (VOID)memset_s(&data->base, sizeof(SwtmrDebugBase), 0, sizeof(SwtmrDebugBase));  // 周期改变时重置基础调试数据
        data->period = ticks;  // 更新周期
    }
    data->base.startTime = swtmr->startTime;  // 更新开始时间
    data->base.times += times;  // 累加触发次数
#endif
}

/**
 * @brief 记录软件定时器启动调试信息
 * @details 标记定时器为已使用并记录处理函数和CPU核心ID
 * @param swtmr 软件定时器控制块指针
 * @param cpuid CPU核心ID
 * @return 无
 */
STATIC INLINE VOID SwtmrDebugDataStart(SWTMR_CTRL_S *swtmr, UINT16 cpuid)
{
#ifdef LOSCFG_SWTMR_DEBUG
    SwtmrDebugData *data = &g_swtmrDebugData[swtmr->usTimerID];
    data->swtmrUsed = TRUE;  // 标记为已使用
    data->handler = swtmr->pfnHandler;  // 记录处理函数
    data->cpuid = cpuid;  // 记录CPU核心ID
#endif
}

/**
 * @brief 计算软件定时器等待时间
 * @details 记录定时器从到期到开始处理的等待时间
 * @param swtmrID 软件定时器ID
 * @param swtmrHandler 定时器处理项指针
 * @return 无
 */
STATIC INLINE VOID SwtmrDebugWaitTimeCalculate(UINT32 swtmrID, SwtmrHandlerItemPtr swtmrHandler)
{
#ifdef LOSCFG_SWTMR_DEBUG
    SwtmrDebugBase *data = &g_swtmrDebugData[swtmrID].base;
    swtmrHandler->swtmrID = swtmrID;  // 设置处理项的定时器ID
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前调度时间(周期数)
    UINT64 waitTime = currTime - data->startTime;  // 计算等待时间
    data->waitTime += waitTime;  // 累加总等待时间
    if (waitTime > data->waitTimeMax) {
        data->waitTimeMax = waitTime;  // 更新最大等待时间
    }
    data->readyStartTime = currTime;  // 记录就绪开始时间
    data->waitCount++;  // 累加等待次数
#endif
}

/**
 * @brief 清除软件定时器调试数据
 * @details 将指定ID的定时器调试数据重置为0
 * @param swtmrID 软件定时器ID
 * @return 无
 */
STATIC INLINE VOID SwtmrDebugDataClear(UINT32 swtmrID)
{
#ifdef LOSCFG_SWTMR_DEBUG
    (VOID)memset_s(&g_swtmrDebugData[swtmrID], sizeof(SwtmrDebugData), 0, sizeof(SwtmrDebugData));  // 清除调试数据
#endif
}

/**
 * @brief 软件定时器处理函数
 * @details 执行定时器回调函数并记录运行时间调试信息
 * @param swtmrHandle 定时器处理项指针
 * @return 无
 */
STATIC INLINE VOID SwtmrHandler(SwtmrHandlerItemPtr swtmrHandle)
{
#ifdef LOSCFG_SWTMR_DEBUG
    UINT32 intSave;
    SwtmrDebugBase *data = &g_swtmrDebugData[swtmrHandle->swtmrID].base;
    UINT64 startTime = OsGetCurrSchedTimeCycle();  // 记录开始执行时间
#endif
    swtmrHandle->handler(swtmrHandle->arg);  // 执行用户回调函数
#ifdef LOSCFG_SWTMR_DEBUG
    UINT64 runTime = OsGetCurrSchedTimeCycle() - startTime;  // 计算运行时间
    SWTMR_LOCK(intSave);  // 加锁保护调试数据
    data->runTime += runTime;  // 累加总运行时间
    if (runTime > data->runTimeMax) {
        data->runTimeMax = runTime;  // 更新最大运行时间
    }
    runTime = startTime - data->readyStartTime;  // 计算就绪到执行的时间
    data->readyTime += runTime;  // 累加总就绪时间
    if (runTime > data->readyTimeMax) {
        data->readyTimeMax = runTime;  // 更新最大就绪时间
    }
    data->runCount++;  // 累加运行次数
    SWTMR_UNLOCK(intSave);  // 解锁
#endif
}

/**
 * @brief 唤醒软件定时器处理
 * @details 将超时的定时器添加到处理队列，并根据模式处理定时器状态
 * @param srq 软件定时器运行队列指针
 * @param startTime 定时器开始时间
 * @param sortList 排序链表节点指针
 * @return 无
 */
STATIC INLINE VOID SwtmrWake(SwtmrRunqueue *srq, UINT64 startTime, SortLinkList *sortList)
{
    UINT32 intSave;
    SWTMR_CTRL_S *swtmr = LOS_DL_LIST_ENTRY(sortList, SWTMR_CTRL_S, stSortList);  // 获取定时器控制块
    SwtmrHandlerItemPtr swtmrHandler = (SwtmrHandlerItemPtr)LOS_MemboxAlloc(g_swtmrHandlerPool);  // 分配处理项
    LOS_ASSERT(swtmrHandler != NULL);  // 断言处理项分配成功

    OsHookCall(LOS_HOOK_TYPE_SWTMR_EXPIRED, swtmr);  // 调用定时器到期钩子函数

    SWTMR_LOCK(intSave);  // 加锁保护队列操作
    swtmrHandler->handler = swtmr->pfnHandler;  // 设置处理函数
    swtmrHandler->arg = swtmr->uwArg;  // 设置参数
    LOS_ListTailInsert(&srq->swtmrHandlerQueue, &swtmrHandler->node);  // 添加到处理队列尾部
    SwtmrDebugWaitTimeCalculate(swtmr->usTimerID, swtmrHandler);  // 计算等待时间

    if (swtmr->ucMode == LOS_SWTMR_MODE_ONCE) {  // 单次触发模式
        SwtmrDelete(swtmr);  // 删除定时器

        if (swtmr->usTimerID < (OS_SWTMR_MAX_TIMERID - LOSCFG_BASE_CORE_SWTMR_LIMIT)) {
            swtmr->usTimerID += LOSCFG_BASE_CORE_SWTMR_LIMIT;  // 调整定时器ID
        } else {
            swtmr->usTimerID %= LOSCFG_BASE_CORE_SWTMR_LIMIT;  // 循环使用ID
        }
    } else if (swtmr->ucMode == LOS_SWTMR_MODE_NO_SELFDELETE) {  // 单次不自动删除模式
        swtmr->ucState = OS_SWTMR_STATUS_CREATED;  // 置为已创建状态
    } else {  // 周期模式
        swtmr->uwOverrun++;  // 溢出计数加1
        swtmr->startTime = startTime;  // 更新开始时间
        (VOID)SwtmrToStart(swtmr, ArchCurrCpuid());  // 重新启动定时器
    }

    SWTMR_UNLOCK(intSave);  // 解锁
}

/**
 * @brief 扫描软件定时器计时链表
 * @details 检查并处理所有已超时的软件定时器
 * @param srq 软件定时器运行队列指针
 * @return 无
 */
STATIC INLINE VOID ScanSwtmrTimeList(SwtmrRunqueue *srq)
{
    UINT32 intSave;
    SortLinkAttribute *swtmrSortLink = &srq->swtmrSortLink;  // 获取排序链表属性
    LOS_DL_LIST *listObject = &swtmrSortLink->sortLink;  // 获取链表头

    /* 需要小心处理，因为定时器可能在特定排序链表中，而其他核心仍可能处理它，如停止定时器 */
    LOS_SpinLockSave(&swtmrSortLink->spinLock, &intSave);  // 加锁保护链表操作

    if (LOS_ListEmpty(listObject)) {  // 链表为空
        LOS_SpinUnlockRestore(&swtmrSortLink->spinLock, intSave);  // 解锁并返回
        return;
    }
    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);  // 获取第一个节点

    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前时间
    while (sortList->responseTime <= currTime) {  // 处理所有已超时的定时器
        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);  // 获取下一个节点
        UINT64 startTime = GET_SORTLIST_VALUE(sortList);  // 获取开始时间
        OsDeleteNodeSortLink(swtmrSortLink, sortList);  // 从排序链表中删除节点
        LOS_SpinUnlockRestore(&swtmrSortLink->spinLock, intSave);  // 解锁

        SwtmrWake(srq, startTime, sortList);  // 唤醒定时器处理

        LOS_SpinLockSave(&swtmrSortLink->spinLock, &intSave);  // 重新加锁
        if (LOS_ListEmpty(listObject)) {  // 链表为空则退出循环
            break;
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);  // 更新当前节点
    }

    LOS_SpinUnlockRestore(&swtmrSortLink->spinLock, intSave);  // 解锁
    return;
}

/**
 * @brief 软件定时器任务函数
 * @details 循环等待并处理超时的软件定时器回调
 * @param 无
 * @return 无
 */
STATIC VOID SwtmrTask(VOID)
{
    SwtmrHandlerItem swtmrHandle;  // 定时器处理项
    UINT32 intSave;
    UINT64 waitTime;  // 等待时间

    SwtmrRunqueue *srq = &g_swtmrRunqueue[ArchCurrCpuid()];  // 获取当前CPU的运行队列
    LOS_DL_LIST *head = &srq->swtmrHandlerQueue;  // 获取处理队列头
    for (;;) {  // 无限循环
        waitTime = OsSortLinkGetNextExpireTime(OsGetCurrSchedTimeCycle(), &srq->swtmrSortLink);  // 获取下一个超时时间
        if (waitTime != 0) {  // 需要等待
            SCHEDULER_LOCK(intSave);  // 调度器加锁
            srq->swtmrTask->ops->delay(srq->swtmrTask, waitTime);  // 延迟等待
            OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTODELAYEDLIST, srq->swtmrTask);  // 调用任务移至延迟链表钩子
            SCHEDULER_UNLOCK(intSave);  // 调度器解锁
        }

        ScanSwtmrTimeList(srq);  // 扫描并处理超时定时器

        while (!LOS_ListEmpty(head)) {  // 处理所有就绪的定时器
            SwtmrHandlerItemPtr swtmrHandlePtr = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(head), SwtmrHandlerItem, node);  // 获取第一个处理项
            LOS_ListDelete(&swtmrHandlePtr->node);  // 从队列中删除

            (VOID)memcpy_s(&swtmrHandle, sizeof(SwtmrHandlerItem), swtmrHandlePtr, sizeof(SwtmrHandlerItem));  // 复制处理项
            (VOID)LOS_MemboxFree(g_swtmrHandlerPool, swtmrHandlePtr);  // 释放处理项内存
            SwtmrHandler(&swtmrHandle);  // 执行处理函数
        }
    }
}

/**
 * @brief 创建软件定时器任务
 * @details 为指定CPU核心创建软件定时器处理任务
 * @param cpuid CPU核心ID
 * @param swtmrTaskID 输出参数，任务ID
 * @return LOS_OK-成功，其他-失败
 */
STATIC UINT32 SwtmrTaskCreate(UINT16 cpuid, UINT32 *swtmrTaskID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S swtmrTask;  // 任务初始化参数

    (VOID)memset_s(&swtmrTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));  // 初始化参数为0
    swtmrTask.pfnTaskEntry = (TSK_ENTRY_FUNC)SwtmrTask;  // 设置任务入口函数
    swtmrTask.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;  // 设置栈大小
    swtmrTask.pcName = "Swt_Task";  // 设置任务名称
    swtmrTask.usTaskPrio = 0;  // 设置优先级为最高
    swtmrTask.uwResved = LOS_TASK_STATUS_DETACHED;  // 设置为分离状态
#ifdef LOSCFG_KERNEL_SMP
    swtmrTask.usCpuAffiMask   = CPUID_TO_AFFI_MASK(cpuid);  // 设置CPU亲和性
#endif
    ret = LOS_TaskCreate(swtmrTaskID, &swtmrTask);  // 创建任务
    if (ret == LOS_OK) {
        OS_TCB_FROM_TID(*swtmrTaskID)->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK;  // 标记为系统任务
    }

    return ret;  // 返回创建结果
}

/**
 * @brief 根据CPU核心ID获取软件定时器任务ID
 * @details 获取指定CPU核心上的软件定时器任务ID
 * @param cpuid CPU核心ID
 * @return 任务ID
 */
UINT32 OsSwtmrTaskIDGetByCpuid(UINT16 cpuid)
{
    return g_swtmrRunqueue[cpuid].swtmrTask->taskID;  // 返回任务ID
}

/**
 * @brief 判断是否为软件定时器任务
 * @details 检查指定任务是否为软件定时器任务
 * @param taskCB 任务控制块指针
 * @return TRUE-是，FALSE-否
 */
BOOL OsIsSwtmrTask(const LosTaskCB *taskCB)
{
    if (taskCB->taskEntry == (TSK_ENTRY_FUNC)SwtmrTask) {
        return TRUE;  // 是软件定时器任务
    }
    return FALSE;  // 不是
}

/**
 * @brief 回收指定进程的软件定时器
 * @details 删除属于指定进程的所有软件定时器
 * @param ownerID 进程ID
 * @return 无
 */
LITE_OS_SEC_TEXT_INIT VOID OsSwtmrRecycle(UINTPTR ownerID)
{
    for (UINT16 index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++) {  // 遍历所有定时器
        if (g_swtmrCBArray[index].uwOwnerPid == ownerID) {  // 属于指定进程
            LOS_SwtmrDelete(index);  // 删除定时器
        }
    }
}

/**
 * @brief 软件定时器基础初始化
 * @details 分配定时器控制块和处理池内存，初始化链表
 * @param 无
 * @return LOS_OK-成功，其他-失败
 */
STATIC UINT32 SwtmrBaseInit(VOID)
{
    UINT32 ret;
    UINT32 size = sizeof(SWTMR_CTRL_S) * LOSCFG_BASE_CORE_SWTMR_LIMIT;  // 计算控制块内存大小
    SWTMR_CTRL_S *swtmr = (SWTMR_CTRL_S *)LOS_MemAlloc(m_aucSysMem0, size);  // 分配控制块内存
    if (swtmr == NULL) {
        return LOS_ERRNO_SWTMR_NO_MEMORY;  // 内存分配失败
    }

    (VOID)memset_s(swtmr, size, 0, size);  // 初始化控制块内存
    g_swtmrCBArray = swtmr;  // 设置全局控制块数组指针
    LOS_ListInit(&g_swtmrFreeList);  // 初始化空闲链表
    for (UINT16 index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr++) {  // 初始化每个控制块
        swtmr->usTimerID = index;  // 设置定时器ID
        LOS_ListTailInsert(&g_swtmrFreeList, &swtmr->stSortList.sortLinkNode);  // 添加到空闲链表
    }

    size = LOS_MEMBOX_SIZE(sizeof(SwtmrHandlerItem), OS_SWTMR_HANDLE_QUEUE_SIZE);  // 计算处理池大小
    g_swtmrHandlerPool = (UINT8 *)LOS_MemAlloc(m_aucSysMem1, size);  // 分配处理池内存
    if (g_swtmrHandlerPool == NULL) {
        return LOS_ERRNO_SWTMR_NO_MEMORY;  // 内存分配失败
    }

    ret = LOS_MemboxInit(g_swtmrHandlerPool, size, sizeof(SwtmrHandlerItem));  // 初始化内存池
    if (ret != LOS_OK) {
        return LOS_ERRNO_SWTMR_HANDLER_POOL_NO_MEM;  // 内存池初始化失败
    }

    for (UINT16 index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {  // 初始化每个CPU的运行队列
        SwtmrRunqueue *srq = &g_swtmrRunqueue[index];
        /* 所有核心的链表必须在核心0启动时初始化以实现负载均衡 */
        OsSortLinkInit(&srq->swtmrSortLink);  // 初始化排序链表
        LOS_ListInit(&srq->swtmrHandlerQueue);  // 初始化处理队列
        srq->swtmrTask = NULL;  // 任务指针初始化为空
    }

    SwtmrDebugDataInit();  // 初始化调试数据

    return LOS_OK;  // 初始化成功
}

/**
 * @brief 软件定时器模块初始化
 * @details 初始化软件定时器基础资源并创建处理任务
 * @param 无
 * @return LOS_OK-成功，其他-失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsSwtmrInit(VOID)
{
    UINT32 ret;
    UINT32 cpuid = ArchCurrCpuid();  // 获取当前CPU核心ID
    UINT32 swtmrTaskID;  // 任务ID

    if (cpuid == 0) {  // 仅在核心0执行基础初始化
        ret = SwtmrBaseInit();
        if (ret != LOS_OK) {
            goto ERROR;  // 初始化失败，跳转错误处理
        }
    }

    ret = SwtmrTaskCreate(cpuid, &swtmrTaskID);  // 创建定时器任务
    if (ret != LOS_OK) {
        ret = LOS_ERRNO_SWTMR_TASK_CREATE_FAILED;  // 任务创建失败
        goto ERROR;
    }

    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    srq->swtmrTask = OsGetTaskCB(swtmrTaskID);  // 设置任务控制块指针
    return LOS_OK;  // 初始化成功

ERROR:
    PRINT_ERR("OsSwtmrInit error! ret = %u\n", ret);  // 打印错误信息
    (VOID)LOS_MemFree(m_aucSysMem0, g_swtmrCBArray);  // 释放控制块内存
    g_swtmrCBArray = NULL;
    (VOID)LOS_MemFree(m_aucSysMem1, g_swtmrHandlerPool);  // 释放处理池内存
    g_swtmrHandlerPool = NULL;
    return ret;  // 返回错误码
}

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief 查找空闲的软件定时器运行队列
 * @details 找到定时器数量最少的CPU核心运行队列
 * @param idleCpuid 输出参数，空闲CPU核心ID
 * @return 无
 */
STATIC INLINE VOID FindIdleSwtmrRunqueue(UINT16 *idleCpuid)
{
    SwtmrRunqueue *idleRq = &g_swtmrRunqueue[0];  // 初始化为核心0
    UINT32 nodeNum = OsGetSortLinkNodeNum(&idleRq->swtmrSortLink);  // 获取节点数量
    UINT16 cpuid = 1;
    do {
        SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
        UINT32 temp = OsGetSortLinkNodeNum(&srq->swtmrSortLink);  // 获取当前核心节点数量
        if (nodeNum > temp) {
            *idleCpuid = cpuid;  // 更新空闲核心ID
            nodeNum = temp;  // 更新最小节点数量
        }
        cpuid++;
    } while (cpuid < LOSCFG_KERNEL_CORE_NUM);  // 遍历所有核心
}
#endif

/**
 * @brief 添加软件定时器到计时链表
 * @details 将定时器节点添加到指定CPU核心的排序计时链表
 * @param node 排序链表节点指针
 * @param responseTime 到期时间
 * @param cpuid CPU核心ID
 * @return 无
 */
STATIC INLINE VOID AddSwtmr2TimeList(SortLinkList *node, UINT64 responseTime, UINT16 cpuid)
{
    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    OsAdd2SortLink(&srq->swtmrSortLink, node, responseTime, cpuid);  // 添加到排序链表
}

/**
 * @brief 从计时链表中移除软件定时器
 * @details 将定时器节点从其所在的计时链表中移除
 * @param node 排序链表节点指针
 * @return 无
 */
STATIC INLINE VOID DeSwtmrFromTimeList(SortLinkList *node)
{
#ifdef LOSCFG_KERNEL_SMP
    UINT16 cpuid = OsGetSortLinkNodeCpuid(node);  // 获取节点所在CPU核心
#else
    UINT16 cpuid = 0;  // 单核心模式
#endif
    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    OsDeleteFromSortLink(&srq->swtmrSortLink, node);  // 从排序链表中删除
    return;
}

/**
 * @brief 调整软件定时器任务等待时间
 * @details 如果新的到期时间更早，则调整定时器任务的等待时间
 * @param cpuid CPU核心ID
 * @param responseTime 新的到期时间
 * @return 无
 */
STATIC VOID SwtmrAdjustCheck(UINT16 cpuid, UINT64 responseTime)
{
    UINT32 ret;
    UINT32 intSave;
    SwtmrRunqueue *srq = &g_swtmrRunqueue[cpuid];
    SCHEDULER_LOCK(intSave);  // 调度器加锁
    if ((srq->swtmrTask == NULL) || !OsTaskIsBlocked(srq->swtmrTask)) {  // 任务不存在或未阻塞
        SCHEDULER_UNLOCK(intSave);  // 解锁
        return;
    }

    if (responseTime >= GET_SORTLIST_VALUE(&srq->swtmrTask->sortList)) {  // 新时间不早于当前等待时间
        SCHEDULER_UNLOCK(intSave);  // 解锁
        return;
    }

    ret = OsSchedTimeoutQueueAdjust(srq->swtmrTask, responseTime);  // 调整任务超时时间
    SCHEDULER_UNLOCK(intSave);  // 解锁
    if (ret != LOS_OK) {
        return;
    }

    if (cpuid == ArchCurrCpuid()) {
        OsSchedExpireTimeUpdate();  // 更新当前核心的到期时间
    } else {
        LOS_MpSchedule(CPUID_TO_AFFI_MASK(cpuid));  // 触发其他核心调度
    }
}

/**
 * @brief 启动软件定时器并计算到期时间
 * @details 根据定时器模式和当前时间计算下一次到期时间，并添加到计时链表
 * @param swtmr 软件定时器控制块指针
 * @param cpuid CPU核心ID
 * @return 定时器到期时间（周期数）
 */
STATIC UINT64 SwtmrToStart(SWTMR_CTRL_S *swtmr, UINT16 cpuid)
{
    UINT32 ticks;
    UINT32 times = 0;  // 触发次数

    if ((swtmr->uwOverrun == 0) && ((swtmr->ucMode == LOS_SWTMR_MODE_ONCE) ||
        (swtmr->ucMode == LOS_SWTMR_MODE_OPP) ||
        (swtmr->ucMode == LOS_SWTMR_MODE_NO_SELFDELETE))) {
        ticks = swtmr->uwExpiry;  // 使用初始到期时间
    } else {
        ticks = swtmr->uwInterval;  // 使用间隔时间
    }
    swtmr->ucState = OS_SWTMR_STATUS_TICKING;  // 设置为计时状态

    UINT64 period = (UINT64)ticks * OS_CYCLE_PER_TICK;  // 转换为周期数
    UINT64 responseTime = swtmr->startTime + period;  // 计算到期时间
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前时间
    if (responseTime < currTime) {  // 已经超时
        times = (UINT32)((currTime - swtmr->startTime) / period);  // 计算超时次数
        swtmr->startTime += times * period;  // 调整开始时间
        responseTime = swtmr->startTime + period;  // 计算新的到期时间
        PRINT_WARN("Swtmr already timeout! SwtmrID: %u\n", swtmr->usTimerID);  // 打印警告
    }

    AddSwtmr2TimeList(&swtmr->stSortList, responseTime, cpuid);  // 添加到计时链表
    SwtmrDebugDataUpdate(swtmr, ticks, times);  // 更新调试数据
    return responseTime;  // 返回到期时间
}

/*
 * Description: Start Software Timer
 * Input      : swtmr --- Need to start software timer
 */
/**
 * @brief 启动软件定时器
 * @details 设置定时器开始时间并添加到计时链表
 * @param swtmr 软件定时器控制块指针
 * @return 无
 */
STATIC INLINE VOID SwtmrStart(SWTMR_CTRL_S *swtmr)
{
    UINT64 responseTime;
    UINT16 idleCpu = 0;
#ifdef LOSCFG_KERNEL_SMP
    FindIdleSwtmrRunqueue(&idleCpu);  // SMP模式下查找空闲CPU
#endif
    swtmr->startTime = OsGetCurrSchedTimeCycle();  // 设置开始时间
    responseTime = SwtmrToStart(swtmr, idleCpu);  // 计算到期时间并添加到链表

    SwtmrDebugDataStart(swtmr, idleCpu);  // 记录启动调试信息

    SwtmrAdjustCheck(idleCpu, responseTime);  // 调整任务等待时间
}

/*
 * Description: Delete Software Timer
 * Input      : swtmr --- Need to delete software timer, When using, Ensure that it can't be NULL.
 */
/**
 * @brief 删除软件定时器
 * @details 将指定的软件定时器添加到空闲链表并重置状态
 * @param swtmr 软件定时器控制块指针（确保非空）
 * @return 无
 */
STATIC INLINE VOID SwtmrDelete(SWTMR_CTRL_S *swtmr)
{
    /* 插入到空闲链表 */
    LOS_ListTailInsert(&g_swtmrFreeList, &swtmr->stSortList.sortLinkNode);  // 添加到空闲链表尾部
    swtmr->ucState = OS_SWTMR_STATUS_UNUSED;  // 设置为未使用状态
    swtmr->uwOwnerPid = OS_INVALID_VALUE;  // 重置所有者进程ID

    SwtmrDebugDataClear(swtmr->usTimerID);  // 清除调试数据
}

/**
 * @brief 重启软件定时器
 * @details 重置定时器开始时间并重新添加到计时链表
 * @param startTime 新的开始时间
 * @param sortList 排序链表节点指针
 * @param cpuid CPU核心ID
 * @return 无
 */
STATIC INLINE VOID SwtmrRestart(UINT64 startTime, SortLinkList *sortList, UINT16 cpuid)
{
    UINT32 intSave;

    SWTMR_CTRL_S *swtmr = LOS_DL_LIST_ENTRY(sortList, SWTMR_CTRL_S, stSortList);  // 获取定时器控制块
    SWTMR_LOCK(intSave);  // 加锁
    swtmr->startTime = startTime;  // 更新开始时间
    (VOID)SwtmrToStart(swtmr, cpuid);  // 重新启动定时器
    SWTMR_UNLOCK(intSave);  // 解锁
}

/**
 * @brief 重置软件定时器响应时间
 * @details 重新计算所有定时器的到期时间（用于系统时间调整）
 * @param startTime 新的开始时间
 * @return 无
 */
VOID OsSwtmrResponseTimeReset(UINT64 startTime)
{
    UINT16 cpuid = ArchCurrCpuid();  // 获取当前CPU核心
    SortLinkAttribute *swtmrSortLink = &g_swtmrRunqueue[cpuid].swtmrSortLink;  // 获取排序链表属性
    LOS_DL_LIST *listHead = &swtmrSortLink->sortLink;  // 获取链表头
    LOS_DL_LIST *listNext = listHead->pstNext;  // 获取第一个节点

    LOS_SpinLock(&swtmrSortLink->spinLock);  // 加锁
    while (listNext != listHead) {  // 遍历所有节点
        SortLinkList *sortList = LOS_DL_LIST_ENTRY(listNext, SortLinkList, sortLinkNode);  // 获取当前节点
        OsDeleteNodeSortLink(swtmrSortLink, sortList);  // 从链表中删除
        LOS_SpinUnlock(&swtmrSortLink->spinLock);  // 解锁

        SwtmrRestart(startTime, sortList, cpuid);  // 重启定时器

        LOS_SpinLock(&swtmrSortLink->spinLock);  // 重新加锁
        listNext = listNext->pstNext;  // 移动到下一个节点
    }
    LOS_SpinUnlock(&swtmrSortLink->spinLock);  // 解锁
}

/**
 * @brief 在软件定时器运行队列中查找节点
 * @details 使用指定的检查函数在定时器链表中查找节点
 * @param swtmrSortLink 排序链表属性指针
 * @param checkFunc 检查函数指针
 * @param arg 检查函数参数
 * @return TRUE-找到，FALSE-未找到
 */
STATIC INLINE BOOL SwtmrRunqueueFind(SortLinkAttribute *swtmrSortLink, SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg)
{
    LOS_DL_LIST *listObject = &swtmrSortLink->sortLink;  // 获取链表头
    LOS_DL_LIST *list = listObject->pstNext;  // 获取第一个节点

    LOS_SpinLock(&swtmrSortLink->spinLock);  // 加锁
    while (list != listObject) {  // 遍历链表
        SortLinkList *listSorted = LOS_DL_LIST_ENTRY(list, SortLinkList, sortLinkNode);  // 获取当前节点
        if (checkFunc((UINTPTR)listSorted, arg)) {  // 调用检查函数
            LOS_SpinUnlock(&swtmrSortLink->spinLock);  // 解锁
            return TRUE;  // 找到节点
        }
        list = list->pstNext;  // 移动到下一个节点
    }

    LOS_SpinUnlock(&swtmrSortLink->spinLock);  // 解锁
    return FALSE;  // 未找到
}

/**
 * @brief 在所有CPU核心的定时器链表中查找节点
 * @details 遍历所有CPU核心的定时器链表，使用检查函数查找节点
 * @param checkFunc 检查函数指针
 * @param arg 检查函数参数
 * @return TRUE-找到，FALSE-未找到
 */
STATIC BOOL SwtmrTimeListFind(SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg)
{
    for (UINT16 cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {  // 遍历所有CPU核心
        SortLinkAttribute *swtmrSortLink = &g_swtmrRunqueue[cpuid].swtmrSortLink;
        if (SwtmrRunqueueFind(swtmrSortLink, checkFunc, arg)) {  // 在当前核心查找
            return TRUE;  // 找到节点
        }
    }
    return FALSE;  // 未找到
}

/**
 * @brief 在软件定时器工作队列中查找节点
 * @details 加锁保护下在所有定时器链表中查找节点
 * @param checkFunc 检查函数指针
 * @param arg 检查函数参数
 * @return TRUE-找到，FALSE-未找到
 */
BOOL OsSwtmrWorkQueueFind(SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg)
{
    UINT32 intSave;

    SWTMR_LOCK(intSave);  // 加锁
    BOOL find = SwtmrTimeListFind(checkFunc, arg);  // 查找节点
    SWTMR_UNLOCK(intSave);  // 解锁
    return find;  // 返回查找结果
}

/*
 * Description: Get next timeout
 * Return     : Count of the Timer list
 */
/**
 * @brief 获取下一个定时器超时时间
 * @details 计算并返回下一个定时器超时的Tick数
 * @param 无
 * @return 超时Tick数，OS_INVALID_VALUE表示无超时定时器
 */
LITE_OS_SEC_TEXT UINT32 OsSwtmrGetNextTimeout(VOID)
{
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前时间
    SwtmrRunqueue *srq = &g_swtmrRunqueue[ArchCurrCpuid()];  // 获取当前CPU运行队列
    UINT64 time = (OsSortLinkGetNextExpireTime(currTime, &srq->swtmrSortLink) / OS_CYCLE_PER_TICK);  // 转换为Tick数
    if (time > OS_INVALID_VALUE) {
        time = OS_INVALID_VALUE;  // 溢出处理
    }
    return (UINT32)time;  // 返回超时Tick数
}

/*
 * Description: Stop of Software Timer interface
 * Input      : swtmr --- the software timer control handler
 */
/**
 * @brief 停止软件定时器接口
 * @details 将指定定时器从计时链表中移除并重置状态
 * @param swtmr 软件定时器控制块指针
 * @return 无
 */
STATIC VOID SwtmrStop(SWTMR_CTRL_S *swtmr)
{
    swtmr->ucState = OS_SWTMR_STATUS_CREATED;  // 设置为已创建状态
    swtmr->uwOverrun = 0;  // 重置溢出计数

    DeSwtmrFromTimeList(&swtmr->stSortList);  // 从计时链表中移除
}

/*
 * Description: Get next software timer expiretime
 * Input      : swtmr --- the software timer control handler
 */
/**
 * @brief 获取软件定时器剩余时间
 * @details 计算并返回指定定时器的剩余超时Tick数
 * @param swtmr 软件定时器控制块指针
 * @return 剩余Tick数
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsSwtmrTimeGet(const SWTMR_CTRL_S *swtmr)
{
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前时间
    UINT64 time = (OsSortLinkGetTargetExpireTime(currTime, &swtmr->stSortList) / OS_CYCLE_PER_TICK);  // 转换为Tick数
    if (time > OS_INVALID_VALUE) {
        time = OS_INVALID_VALUE;  // 溢出处理
    }
    return (UINT32)time;  // 返回剩余Tick数
}

/**
 * @brief 创建软件定时器
 * @details 分配定时器资源并初始化定时器控制块
 * @param interval 定时器间隔(Tick)
 * @param mode 定时器模式：LOS_SWTMR_MODE_ONCE(单次)/LOS_SWTMR_MODE_PERIOD(周期)/LOS_SWTMR_MODE_NO_SELFDELETE(单次不自动删除)
 * @param handler 超时回调函数
 * @param swtmrID 输出参数，定时器ID
 * @param arg 回调函数参数
 * @return LOS_OK-成功，其他-失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_SwtmrCreate(UINT32 interval,
                                             UINT8 mode,
                                             SWTMR_PROC_FUNC handler,
                                             UINT16 *swtmrID,
                                             UINTPTR arg)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    SortLinkList *sortList = NULL;

    if (interval == 0) {  // 间隔不能为0
        return LOS_ERRNO_SWTMR_INTERVAL_NOT_SUITED;
    }

    if ((mode != LOS_SWTMR_MODE_ONCE) && (mode != LOS_SWTMR_MODE_PERIOD) &&
        (mode != LOS_SWTMR_MODE_NO_SELFDELETE)) {  // 模式无效
        return LOS_ERRNO_SWTMR_MODE_INVALID;
    }

    if (handler == NULL) {  // 回调函数为空
        return LOS_ERRNO_SWTMR_PTR_NULL;
    }

    if (swtmrID == NULL) {  // 返回ID指针为空
        return LOS_ERRNO_SWTMR_RET_PTR_NULL;
    }

    SWTMR_LOCK(intSave);  // 加锁
    if (LOS_ListEmpty(&g_swtmrFreeList)) {  // 无空闲定时器
        SWTMR_UNLOCK(intSave);  // 解锁
        return LOS_ERRNO_SWTMR_MAXSIZE;
    }

    sortList = LOS_DL_LIST_ENTRY(g_swtmrFreeList.pstNext, SortLinkList, sortLinkNode);  // 获取空闲节点
    swtmr = LOS_DL_LIST_ENTRY(sortList, SWTMR_CTRL_S, stSortList);  // 获取控制块
    LOS_ListDelete(LOS_DL_LIST_FIRST(&g_swtmrFreeList));  // 从空闲链表删除
    SWTMR_UNLOCK(intSave);  // 解锁

    swtmr->uwOwnerPid = (UINTPTR)OsCurrProcessGet();  // 设置所有者进程ID
    swtmr->pfnHandler = handler;  // 设置回调函数
    swtmr->ucMode = mode;  // 设置模式
    swtmr->uwOverrun = 0;  // 初始化溢出计数
    swtmr->uwInterval = interval;  // 设置间隔
    swtmr->uwExpiry = interval;  // 设置到期时间
    swtmr->uwArg = arg;  // 设置参数
    swtmr->ucState = OS_SWTMR_STATUS_CREATED;  // 设置为已创建状态
    SET_SORTLIST_VALUE(&swtmr->stSortList, OS_SORT_LINK_INVALID_TIME);  // 设置无效时间
    *swtmrID = swtmr->usTimerID;  // 返回定时器ID
    OsHookCall(LOS_HOOK_TYPE_SWTMR_CREATE, swtmr);  // 调用创建钩子函数
    return LOS_OK;  // 创建成功
}

/**
 * @brief 启动软件定时器
 * @details 将指定ID的定时器添加到计时链表开始计时
 * @param swtmrID 软件定时器ID
 * @return LOS_OK-成功，其他-失败
 */
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrStart(UINT16 swtmrID)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {  // ID超出范围
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;  // 计算控制块索引
    swtmr = g_swtmrCBArray + swtmrCBID;  // 获取控制块

    SWTMR_LOCK(intSave);  // 加锁
    if (swtmr->usTimerID != swtmrID) {  // ID不匹配
        SWTMR_UNLOCK(intSave);  // 解锁
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    switch (swtmr->ucState) {  // 根据状态处理
        case OS_SWTMR_STATUS_UNUSED:  // 未使用
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;  // 未创建
            break;
        /*
         * 如果定时器状态为计时中，应先停止定时器，
         * 然后再次启动定时器。
         */
        case OS_SWTMR_STATUS_TICKING:  // 计时中
            SwtmrStop(swtmr);  // 停止定时器
            /* fall-through */
        case OS_SWTMR_STATUS_CREATED:  // 已创建
            SwtmrStart(swtmr);  // 启动定时器
            break;
        default:  // 无效状态
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }

    SWTMR_UNLOCK(intSave);  // 解锁
    OsHookCall(LOS_HOOK_TYPE_SWTMR_START, swtmr);  // 调用启动钩子函数
    return ret;  // 返回结果
}

/**
 * @brief 停止软件定时器
 * @details 将指定ID的定时器从计时链表中移除
 * @param swtmrID 软件定时器ID
 * @return LOS_OK-成功，其他-失败
 */
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrStop(UINT16 swtmrID)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {  // ID超出范围
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;  // 计算控制块索引
    swtmr = g_swtmrCBArray + swtmrCBID;  // 获取控制块
    SWTMR_LOCK(intSave);  // 加锁

    if (swtmr->usTimerID != swtmrID) {  // ID不匹配
        SWTMR_UNLOCK(intSave);  // 解锁
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    switch (swtmr->ucState) {  // 根据状态处理
        case OS_SWTMR_STATUS_UNUSED:  // 未使用
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;  // 未创建
            break;
        case OS_SWTMR_STATUS_CREATED:  // 已创建
            ret = LOS_ERRNO_SWTMR_NOT_STARTED;  // 未启动
            break;
        case OS_SWTMR_STATUS_TICKING:  // 计时中
            SwtmrStop(swtmr);  // 停止定时器
            break;
        default:  // 无效状态
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }

    SWTMR_UNLOCK(intSave);  // 解锁
    OsHookCall(LOS_HOOK_TYPE_SWTMR_STOP, swtmr);  // 调用停止钩子函数
    return ret;  // 返回结果
}

/**
 * @brief 获取软件定时器剩余时间
 * @details 获取指定ID的定时器剩余超时Tick数
 * @param swtmrID 软件定时器ID
 * @param tick 输出参数，剩余Tick数
 * @return LOS_OK-成功，其他-失败
 */
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrTimeGet(UINT16 swtmrID, UINT32 *tick)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {  // ID超出范围
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    if (tick == NULL) {  // 输出指针为空
        return LOS_ERRNO_SWTMR_TICK_PTR_NULL;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;  // 计算控制块索引
    swtmr = g_swtmrCBArray + swtmrCBID;  // 获取控制块
    SWTMR_LOCK(intSave);  // 加锁

    if (swtmr->usTimerID != swtmrID) {  // ID不匹配
        SWTMR_UNLOCK(intSave);  // 解锁
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }
    switch (swtmr->ucState) {  // 根据状态处理
        case OS_SWTMR_STATUS_UNUSED:  // 未使用
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;  // 未创建
            break;
        case OS_SWTMR_STATUS_CREATED:  // 已创建
            ret = LOS_ERRNO_SWTMR_NOT_STARTED;  // 未启动
            break;
        case OS_SWTMR_STATUS_TICKING:  // 计时中
            *tick = OsSwtmrTimeGet(swtmr);  // 获取剩余时间
            break;
        default:  // 无效状态
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }
    SWTMR_UNLOCK(intSave);  // 解锁
    return ret;  // 返回结果
}

/**
 * @brief 删除软件定时器
 * @details 停止并释放指定ID的定时器资源
 * @param swtmrID 软件定时器ID
 * @return LOS_OK-成功，其他-失败
 */
LITE_OS_SEC_TEXT UINT32 LOS_SwtmrDelete(UINT16 swtmrID)
{
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    UINT16 swtmrCBID;

    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {  // ID超出范围
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    swtmrCBID = swtmrID % LOSCFG_BASE_CORE_SWTMR_LIMIT;  // 计算控制块索引
    swtmr = g_swtmrCBArray + swtmrCBID;  // 获取控制块
    SWTMR_LOCK(intSave);  // 加锁

    if (swtmr->usTimerID != swtmrID) {  // ID不匹配
        SWTMR_UNLOCK(intSave);  // 解锁
        return LOS_ERRNO_SWTMR_ID_INVALID;
    }

    switch (swtmr->ucState) {  // 根据状态处理
        case OS_SWTMR_STATUS_UNUSED:  // 未使用
            ret = LOS_ERRNO_SWTMR_NOT_CREATED;  // 未创建
            break;
        case OS_SWTMR_STATUS_TICKING:  // 计时中
            SwtmrStop(swtmr);  // 停止定时器
            /* fall-through */
        case OS_SWTMR_STATUS_CREATED:  // 已创建
            SwtmrDelete(swtmr);  // 删除定时器
            break;
        default:  // 无效状态
            ret = LOS_ERRNO_SWTMR_STATUS_INVALID;
            break;
    }

    SWTMR_UNLOCK(intSave);  // 解锁
    OsHookCall(LOS_HOOK_TYPE_SWTMR_DELETE, swtmr);  // 调用删除钩子函数
    return ret;  // 返回结果
}

#endif /* LOSCFG_BASE_CORE_SWTMR_ENABLE */