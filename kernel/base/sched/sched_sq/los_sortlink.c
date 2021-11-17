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

#include "los_sortlink_pri.h"
#include "los_memory.h"
#include "los_exc.h"
#include "los_percpu_pri.h"
#include "los_sched_pri.h"
#include "los_mp.h"
/// 排序链表初始化
UINT32 OsSortLinkInit(SortLinkAttribute *sortLinkHeader)
{
    LOS_ListInit(&sortLinkHeader->sortLink);//初始化双向链表
    sortLinkHeader->nodeNum = 0;//nodeNum背后的含义是记录需要CPU工作的数量
    return LOS_OK;
}

/*!
 * @brief OsAddNode2SortLink 向链表中插入结点,并按时间顺序排列	
 *
 * @param sortLinkHeader 被插入的链表	
 * @param sortList	要插入的结点
 * @return	
 *
 * @see
 */
STATIC INLINE VOID OsAddNode2SortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_DL_LIST *head = (LOS_DL_LIST *)&sortLinkHeader->sortLink; //获取双向链表 

    if (LOS_ListEmpty(head)) { //空链表,直接插入
        LOS_ListHeadInsert(head, &sortList->sortLinkNode);//插入结点
        sortLinkHeader->nodeNum++;//CPU的工作量增加了
        return;
    }
	//链表不为空时,插入分三种情况, responseTime 大于,等于,小于的处理
    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(head->pstNext, SortLinkList, sortLinkNode);
    if (listSorted->responseTime > sortList->responseTime) {//如果要插入的节点 responseTime 最小 
        LOS_ListAdd(head, &sortList->sortLinkNode);//能跑进来说明是最小的,直接插入到第一的位置
        sortLinkHeader->nodeNum++;//CPU的工作量增加了
        return;//直接返回了
    } else if (listSorted->responseTime == sortList->responseTime) {//相等的情况
        LOS_ListAdd(head->pstNext, &sortList->sortLinkNode);//插到第二的位置
        sortLinkHeader->nodeNum++;
        return;
    }
	//处理大于链表中第一个responseTime的情况,需要遍历链表
    LOS_DL_LIST *prevNode = head->pstPrev;//注意这里用的前一个结点,也就是说前一个结点中的responseTime 是最大的
    do { // @note_good 这里写的有点妙,也是双向链表的魅力所在
        listSorted = LOS_DL_LIST_ENTRY(prevNode, SortLinkList, sortLinkNode);//一个个遍历,先比大的再比小的
        if (listSorted->responseTime <= sortList->responseTime) {//如果时间比你小,就插到后面
            LOS_ListAdd(prevNode, &sortList->sortLinkNode);
            sortLinkHeader->nodeNum++;
            break;
        }

        prevNode = prevNode->pstPrev;//再拿上一个更小的responseTime进行比较
    } while (1);//死循环
}
/// 从排序链表上摘除指定节点
VOID OsDeleteNodeSortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_ListDelete(&sortList->sortLinkNode);//摘除工作量
    SET_SORTLIST_VALUE(sortList, OS_SORT_LINK_INVALID_TIME);//重置响应时间
    sortLinkHeader->nodeNum--;//cpu的工作量减少一份
}
/// 获取下一个结点的到期时间
STATIC INLINE UINT64 OsGetSortLinkNextExpireTime(SortLinkAttribute *sortHeader, UINT64 startTime)
{
    LOS_DL_LIST *head = &sortHeader->sortLink;
    LOS_DL_LIST *list = head->pstNext;

    if (LOS_ListEmpty(head)) {//链表为空
        return OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION;
    }

    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(list, SortLinkList, sortLinkNode);//获取结点实体
    if (listSorted->responseTime <= (startTime + OS_TICK_RESPONSE_PRECISION)) { //没看明白 @note_thinking 
        return startTime + OS_TICK_RESPONSE_PRECISION;
    }

    return listSorted->responseTime;
}
/// 根据函数的具体实现,这个函数应该是找出最空闲的CPU, 函数的实现有待优化, @note_thinking 
STATIC Percpu *OsFindIdleCpu(UINT16 *idleCpuID)
{
    Percpu *idleCpu = OsPercpuGetByID(0); //获取0号CPU,0号CPU也可称主核
    *idleCpuID = 0;

#ifdef LOSCFG_KERNEL_SMP //多核情况下
    UINT16 cpuID = 1;
    UINT32 nodeNum = idleCpu->taskSortLink.nodeNum + idleCpu->swtmrSortLink.nodeNum; //获取还未跑完的工作数量
	//cpu执行两种工作: 1.普通任务 2.软件定时器
    do {
        Percpu *cpu = OsPercpuGetByID(cpuID); //一个个cpu遍历
        UINT32 temp = cpu->taskSortLink.nodeNum + cpu->swtmrSortLink.nodeNum;//获取cpu的工作量
        if (nodeNum > temp) {//对工作量比较
            idleCpu = cpu;//取工作量最小的cpu实体
            *idleCpuID = cpuID;//获取cpu id
        }

        cpuID++;//下一个cpu id 
    } while (cpuID < LOSCFG_KERNEL_CORE_NUM);
#endif

    return idleCpu;
}
/// 向cpu的排序链表上添加指定节点
VOID OsAdd2SortLink(SortLinkList *node, UINT64 startTime, UINT32 waitTicks, SortLinkType type)
{
    UINT32 intSave;
    Percpu *cpu = NULL;
    SortLinkAttribute *sortLinkHeader = NULL;
    SPIN_LOCK_S *spinLock = NULL;
    UINT16 idleCpu;

    if (OS_SCHEDULER_ACTIVE) {//当前CPU正在调度
        cpu = OsFindIdleCpu(&idleCpu);//找一个最空闲的CPU
    } else {
        idleCpu = ArchCurrCpuid();//使用当前cpu
        cpu = OsPercpuGet();
    }

    if (type == OS_SORT_LINK_TASK) {//任务类型
        sortLinkHeader = &cpu->taskSortLink; //获取任务链表
        spinLock = &cpu->taskSortLinkSpin;
    } else if (type == OS_SORT_LINK_SWTMR) {//软件定时器类型
        sortLinkHeader = &cpu->swtmrSortLink;//获取软件定时器链表
        spinLock = &cpu->swtmrSortLinkSpin;
    } else {
        LOS_Panic("Sort link type error : %u\n", type);
    }

    LOS_SpinLockSave(spinLock, &intSave);
    SET_SORTLIST_VALUE(node, startTime + (UINT64)waitTicks * OS_CYCLE_PER_TICK);//设置节点响应时间
    OsAddNode2SortLink(sortLinkHeader, node);//插入节点
#ifdef LOSCFG_KERNEL_SMP
    node->cpuid = idleCpu;
    if (idleCpu != ArchCurrCpuid()) { //如果插入的链表不是当前CPU的链表
        LOS_MpSchedule(CPUID_TO_AFFI_MASK(idleCpu));//核间中断,对该CPU发生一次调度申请
    }
#endif
    LOS_SpinUnlockRestore(spinLock, intSave);
}
/// 从cpu的排序链表上摘除指定节点
VOID OsDeleteSortLink(SortLinkList *node, SortLinkType type)
{
    UINT32 intSave;
#ifdef LOSCFG_KERNEL_SMP
    Percpu *cpu = OsPercpuGetByID(node->cpuid);//获取CPU
#else
    Percpu *cpu = OsPercpuGetByID(0);
#endif

    SPIN_LOCK_S *spinLock = NULL;
    SortLinkAttribute *sortLinkHeader = NULL;
    if (type == OS_SORT_LINK_TASK) {//当为任务时
        sortLinkHeader = &cpu->taskSortLink;//获取该CPU的任务链表
        spinLock = &cpu->taskSortLinkSpin;
    } else if (type == OS_SORT_LINK_SWTMR) {
        sortLinkHeader = &cpu->swtmrSortLink;//获取该CPU的定时器链表
        spinLock = &cpu->swtmrSortLinkSpin;
    } else {
        LOS_Panic("Sort link type error : %u\n", type);
    }

    LOS_SpinLockSave(spinLock, &intSave);
    if (node->responseTime != OS_SORT_LINK_INVALID_TIME) {
        OsDeleteNodeSortLink(sortLinkHeader, node);//从CPU的执行链表上摘除
    }
    LOS_SpinUnlockRestore(spinLock, intSave);
}

/*!
 * @brief OsGetNextExpireTime 获取下一个超时时间	
 *
 * @param startTime	
 * @return	
 *
 * @see
 */
UINT64 OsGetNextExpireTime(UINT64 startTime)
{
    UINT32 intSave;
    Percpu *cpu = OsPercpuGet();//获取当前CPU
    SortLinkAttribute *taskHeader = &cpu->taskSortLink;
    SortLinkAttribute *swtmrHeader = &cpu->swtmrSortLink;

    LOS_SpinLockSave(&cpu->taskSortLinkSpin, &intSave);
    UINT64 taskExpirTime = OsGetSortLinkNextExpireTime(taskHeader, startTime);//拿到下一个到期时间,注意此处拿到的一定是最短的时间
    LOS_SpinUnlockRestore(&cpu->taskSortLinkSpin, intSave);

    LOS_SpinLockSave(&cpu->swtmrSortLinkSpin, &intSave);
    UINT64 swtmrExpirTime = OsGetSortLinkNextExpireTime(swtmrHeader, startTime);
    LOS_SpinUnlockRestore(&cpu->swtmrSortLinkSpin, intSave);

    return (taskExpirTime < swtmrExpirTime) ? taskExpirTime : swtmrExpirTime;//比较返回更短的那个.
}

/*!
 * @brief OsSortLinkGetTargetExpireTime	
 * 返回离触发目标时间的tick数
 * @param targetSortList	
 * @return	
 *
 * @see
 */
UINT32 OsSortLinkGetTargetExpireTime(const SortLinkList *targetSortList)
{
    UINT64 currTimes = OsGetCurrSchedTimeCycle();
    if (currTimes >= targetSortList->responseTime) {
        return 0;
    }

    return (UINT32)(targetSortList->responseTime - currTimes) / OS_CYCLE_PER_TICK;//响应时间减去当前时间置算出剩余tick数
}

UINT32 OsSortLinkGetNextExpireTime(const SortLinkAttribute *sortLinkHeader)
{
    LOS_DL_LIST *head = (LOS_DL_LIST *)&sortLinkHeader->sortLink;

    if (LOS_ListEmpty(head)) {
        return 0;
    }

    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(head->pstNext, SortLinkList, sortLinkNode);
    return OsSortLinkGetTargetExpireTime(listSorted);
}

