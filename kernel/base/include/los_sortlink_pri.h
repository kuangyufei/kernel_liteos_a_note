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
#include "los_sys_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef enum {//因为任务和定时器都是吃CPU的,让CPU不停工作的主要就是这两宝贝.
    OS_SORT_LINK_TASK = 1,	//任务排序
    OS_SORT_LINK_SWTMR = 2,	//定时器排序
} SortLinkType;

typedef struct {
    LOS_DL_LIST sortLinkNode;//任务排序链表,注意上面挂的是一个个等待的任务
    UINT64      responseTime;//响应时间
#ifdef LOSCFG_KERNEL_SMP
    UINT32      cpuid;//需要哪个CPU处理
#endif
} SortLinkList;

typedef struct {
    LOS_DL_LIST sortLink;
    UINT32      nodeNum;
} SortLinkAttribute;

#define OS_SORT_LINK_INVALID_TIME ((UINT64)-1)
#define SET_SORTLIST_VALUE(sortList, value) (((SortLinkList *)(sortList))->responseTime = (value))

extern UINT64 OsGetNextExpireTime(UINT64 startTime);
extern UINT32 OsSortLinkInit(SortLinkAttribute *sortLinkHeader);
extern VOID OsDeleteNodeSortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList);
extern VOID OsAdd2SortLink(SortLinkList *node, UINT64 startTime, UINT32 waitTicks, SortLinkType type);
extern VOID OsDeleteSortLink(SortLinkList *node, SortLinkType type);
extern UINT32 OsSortLinkGetTargetExpireTime(const SortLinkList *targetSortList);
extern UINT32 OsSortLinkGetNextExpireTime(const SortLinkAttribute *sortLinkHeader);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SORTLINK_PRI_H */
