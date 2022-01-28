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

#ifndef _LOS_SORTLINK_PRI_H
#define _LOS_SORTLINK_PRI_H

#include "los_typedef.h"
#include "los_list.h"
#include "los_spinlock.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */


/*!  \struct SortLinkList
*   
*/
typedef struct {
    LOS_DL_LIST sortLinkNode;   ///< 排序链表,注意上面挂的是一个个等待被执行的任务/软件定时器
    UINT64      responseTime;   ///< 响应时间,这里提取了最近需要触发的定时器/任务的时间,见于 OsAddNode2SortLink 的实现
#ifdef LOSCFG_KERNEL_SMP
    UINT32      cpuid;  ///< 需要哪个CPU处理
#endif
} SortLinkList;

/*!  \struct SortLinkAttribute 
* @brief 排序链表属性  
*/
typedef struct {
    LOS_DL_LIST sortLink; ///< 排序链表,上面挂的任务/软件定时器
    UINT32      nodeNum;	///< 链表结点数量
    SPIN_LOCK_S spinLock;     /* swtmr sort link spin lock */
} SortLinkAttribute;

#define OS_SORT_LINK_INVALID_TIME ((UINT64)-1)
#define SET_SORTLIST_VALUE(sortList, value) (((SortLinkList *)(sortList))->responseTime = (value))
#define GET_SORTLIST_VALUE(sortList) (((SortLinkList *)(sortList))->responseTime)

STATIC INLINE VOID OsDeleteNodeSortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_ListDelete(&sortList->sortLinkNode);
    SET_SORTLIST_VALUE(sortList, OS_SORT_LINK_INVALID_TIME);
    sortLinkHeader->nodeNum--;
}

STATIC INLINE UINT64 OsGetSortLinkNextExpireTime(SortLinkAttribute *sortHeader, UINT64 startTime, UINT32 tickPrecision)
{
    LOS_DL_LIST *head = &sortHeader->sortLink;
    LOS_DL_LIST *list = head->pstNext;

    if (LOS_ListEmpty(head)) {
        return OS_SORT_LINK_INVALID_TIME - tickPrecision;
    }

    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(list, SortLinkList, sortLinkNode);
    if (listSorted->responseTime <= (startTime + tickPrecision)) {
        return startTime + tickPrecision;
    }

    return listSorted->responseTime;
}

STATIC INLINE UINT32 OsGetSortLinkNodeNum(SortLinkAttribute *head)
{
    return head->nodeNum;
}

VOID OsSortLinkInit(SortLinkAttribute *sortLinkHeader);
VOID OsAdd2SortLink(SortLinkAttribute *head, SortLinkList *node, UINT64 responseTime, UINT16 idleCpu);
VOID OsDeleteFromSortLink(SortLinkAttribute *head, SortLinkList *node);
UINT64 OsSortLinkGetTargetExpireTime(UINT64 currTime, const SortLinkList *targetSortList);
UINT64 OsSortLinkGetNextExpireTime(UINT64 currTime, const SortLinkAttribute *sortLinkHeader);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SORTLINK_PRI_H */
