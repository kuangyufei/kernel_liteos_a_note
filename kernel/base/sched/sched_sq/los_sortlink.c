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

UINT32 OsSortLinkInit(SortLinkAttribute *sortLinkHeader)
{
    LOS_ListInit(&sortLinkHeader->sortLink);
    sortLinkHeader->nodeNum = 0;
    return LOS_OK;
}

STATIC INLINE VOID OsAddNode2SortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_DL_LIST *head = (LOS_DL_LIST *)&sortLinkHeader->sortLink;

    if (LOS_ListEmpty(head)) {
        LOS_ListHeadInsert(head, &sortList->sortLinkNode);
        sortLinkHeader->nodeNum++;
        return;
    }

    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(head->pstNext, SortLinkList, sortLinkNode);
    if (listSorted->responseTime > sortList->responseTime) {
        LOS_ListAdd(head, &sortList->sortLinkNode);
        sortLinkHeader->nodeNum++;
        return;
    } else if (listSorted->responseTime == sortList->responseTime) {
        LOS_ListAdd(head->pstNext, &sortList->sortLinkNode);
        sortLinkHeader->nodeNum++;
        return;
    }

    LOS_DL_LIST *prevNode = head->pstPrev;
    do {
        listSorted = LOS_DL_LIST_ENTRY(prevNode, SortLinkList, sortLinkNode);
        if (listSorted->responseTime <= sortList->responseTime) {
            LOS_ListAdd(prevNode, &sortList->sortLinkNode);
            sortLinkHeader->nodeNum++;
            break;
        }

        prevNode = prevNode->pstPrev;
    } while (1);
}

VOID OsDeleteNodeSortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_ListDelete(&sortList->sortLinkNode);
    SET_SORTLIST_VALUE(sortList, OS_SORT_LINK_INVALID_TIME);
    sortLinkHeader->nodeNum--;
}

STATIC INLINE UINT64 OsGetSortLinkNextExpireTime(SortLinkAttribute *sortHeader, UINT64 startTime)
{
    LOS_DL_LIST *head = &sortHeader->sortLink;
    LOS_DL_LIST *list = head->pstNext;

    if (LOS_ListEmpty(head)) {
        return OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION;
    }

    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(list, SortLinkList, sortLinkNode);
    if (listSorted->responseTime <= (startTime + OS_TICK_RESPONSE_PRECISION)) {
        return startTime + OS_TICK_RESPONSE_PRECISION;
    }

    return listSorted->responseTime;
}

STATIC Percpu *OsFindIdleCpu(UINT16 *idleCpuID)
{
    Percpu *idleCpu = OsPercpuGetByID(0);
    *idleCpuID = 0;

#ifdef LOSCFG_KERNEL_SMP
    UINT16 cpuID = 1;
    UINT32 nodeNum = idleCpu->taskSortLink.nodeNum + idleCpu->swtmrSortLink.nodeNum;

    do {
        Percpu *cpu = OsPercpuGetByID(cpuID);
        UINT32 temp = cpu->taskSortLink.nodeNum + cpu->swtmrSortLink.nodeNum;
        if (nodeNum > temp) {
            idleCpu = cpu;
            *idleCpuID = cpuID;
        }

        cpuID++;
    } while (cpuID < LOSCFG_KERNEL_CORE_NUM);
#endif

    return idleCpu;
}

VOID OsAdd2SortLink(SortLinkList *node, UINT64 startTime, UINT32 waitTicks, SortLinkType type)
{
    UINT32 intSave;
    Percpu *cpu = NULL;
    SortLinkAttribute *sortLinkHeader = NULL;
    SPIN_LOCK_S *spinLock = NULL;
    UINT16 idleCpu;

    if (OS_SCHEDULER_ACTIVE) {
        cpu = OsFindIdleCpu(&idleCpu);
    } else {
        idleCpu = ArchCurrCpuid();
        cpu = OsPercpuGet();
    }

    if (type == OS_SORT_LINK_TASK) {
        sortLinkHeader = &cpu->taskSortLink;
        spinLock = &cpu->taskSortLinkSpin;
    } else if (type == OS_SORT_LINK_SWTMR) {
        sortLinkHeader = &cpu->swtmrSortLink;
        spinLock = &cpu->swtmrSortLinkSpin;
    } else {
        LOS_Panic("Sort link type error : %u\n", type);
    }

    LOS_SpinLockSave(spinLock, &intSave);
    SET_SORTLIST_VALUE(node, startTime + (UINT64)waitTicks * OS_CYCLE_PER_TICK);
    OsAddNode2SortLink(sortLinkHeader, node);
#ifdef LOSCFG_KERNEL_SMP
    node->cpuid = idleCpu;
    if (idleCpu != ArchCurrCpuid()) {
        LOS_MpSchedule(CPUID_TO_AFFI_MASK(idleCpu));
    }
#endif
    LOS_SpinUnlockRestore(spinLock, intSave);
}

VOID OsDeleteSortLink(SortLinkList *node, SortLinkType type)
{
    UINT32 intSave;
#ifdef LOSCFG_KERNEL_SMP
    Percpu *cpu = OsPercpuGetByID(node->cpuid);
#else
    Percpu *cpu = OsPercpuGetByID(0);
#endif

    SPIN_LOCK_S *spinLock = NULL;
    SortLinkAttribute *sortLinkHeader = NULL;
    if (type == OS_SORT_LINK_TASK) {
        sortLinkHeader = &cpu->taskSortLink;
        spinLock = &cpu->taskSortLinkSpin;
    } else if (type == OS_SORT_LINK_SWTMR) {
        sortLinkHeader = &cpu->swtmrSortLink;
        spinLock = &cpu->swtmrSortLinkSpin;
    } else {
        LOS_Panic("Sort link type error : %u\n", type);
    }

    LOS_SpinLockSave(spinLock, &intSave);
    if (node->responseTime != OS_SORT_LINK_INVALID_TIME) {
        OsDeleteNodeSortLink(sortLinkHeader, node);
    }
    LOS_SpinUnlockRestore(spinLock, intSave);
}

UINT64 OsGetNextExpireTime(UINT64 startTime)
{
    UINT32 intSave;
    Percpu *cpu = OsPercpuGet();
    SortLinkAttribute *taskHeader = &cpu->taskSortLink;
    SortLinkAttribute *swtmrHeader = &cpu->swtmrSortLink;

    LOS_SpinLockSave(&cpu->taskSortLinkSpin, &intSave);
    UINT64 taskExpirTime = OsGetSortLinkNextExpireTime(taskHeader, startTime);
    LOS_SpinUnlockRestore(&cpu->taskSortLinkSpin, intSave);

    LOS_SpinLockSave(&cpu->swtmrSortLinkSpin, &intSave);
    UINT64 swtmrExpirTime = OsGetSortLinkNextExpireTime(swtmrHeader, startTime);
    LOS_SpinUnlockRestore(&cpu->swtmrSortLinkSpin, intSave);

    return (taskExpirTime < swtmrExpirTime) ? taskExpirTime : swtmrExpirTime;
}

UINT32 OsSortLinkGetTargetExpireTime(const SortLinkList *targetSortList)
{
    UINT64 currTimes = OsGetCurrSchedTimeCycle();
    if (currTimes >= targetSortList->responseTime) {
        return 0;
    }

    return (UINT32)(targetSortList->responseTime - currTimes) / OS_CYCLE_PER_TICK;
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

