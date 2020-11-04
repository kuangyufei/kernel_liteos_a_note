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

#ifndef _LOS_SORTLINK_PRI_H
#define _LOS_SORTLINK_PRI_H

#include "los_typedef.h"
#include "los_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/*
 * Sortlink Rollnum Structure:
 *   ------------------------------------------
 *  | 31 | 30 | 29 |.......| 4 | 3 | 2 | 1 | 0 |
 *   ------------------------------------------
 *  |<-High Bits->|<---------Low Bits--------->|
 *
 *  Low Bits  : circles		
 *  High Bits : sortlink index
 */
#define OS_TSK_HIGH_BITS       3U
#define OS_TSK_LOW_BITS        (32U - OS_TSK_HIGH_BITS) //29
#define OS_TSK_SORTLINK_LOGLEN OS_TSK_HIGH_BITS	//3U
#define OS_TSK_SORTLINK_LEN    (1U << OS_TSK_SORTLINK_LOGLEN)//链表长度 1<<3 = 8
#define OS_TSK_SORTLINK_MASK   (OS_TSK_SORTLINK_LEN - 1U)// 0b111
#define OS_TSK_MAX_ROLLNUM     (0xFFFFFFFFU - OS_TSK_SORTLINK_LEN)//最大滚动数0xFFFFFFFC
#define OS_TSK_HIGH_BITS_MASK  (OS_TSK_SORTLINK_MASK << OS_TSK_LOW_BITS)//高位掩码 0b111000..00
#define OS_TSK_LOW_BITS_MASK   (~OS_TSK_HIGH_BITS_MASK)//低位掩码 0b000...111

#define EVALUATE_L(NUM, VALUE) NUM = (((NUM) & OS_TSK_HIGH_BITS_MASK) | (VALUE))

#define EVALUATE_H(NUM, VALUE) NUM = (((NUM) & OS_TSK_LOW_BITS_MASK) | ((VALUE) << OS_TSK_LOW_BITS))

#define ROLLNUM_SUB(NUM1, NUM2)                 \
    NUM1 = (((NUM1) & OS_TSK_HIGH_BITS_MASK) |  \
    (ROLLNUM(NUM1) - ROLLNUM(NUM2)))

#define ROLLNUM_ADD(NUM1, NUM2)                 \
    NUM1 = (((NUM1) & OS_TSK_HIGH_BITS_MASK) |  \
    (ROLLNUM(NUM1) + ROLLNUM(NUM2)))

#define ROLLNUM_DEC(NUM) NUM = ((NUM) - 1)

#define ROLLNUM(NUM) ((NUM) & OS_TSK_LOW_BITS_MASK)

#define SORT_INDEX(NUM) ((NUM) >> OS_TSK_LOW_BITS)

#define SET_SORTLIST_VALUE(sortList, value) (((SortLinkList *)(sortList))->idxRollNum = (value))

/**************************************** @note_pic
 
 sortLink[0:7] 
+-------->+-----------------+
          |   sortLinkNode  |
          +-----------------+
          |   sortLinkNode  |
          +-----------------+
          |   sortLinkNode  |
          +-----------------+
          |   sortLinkNode  |
          +-----------------+
          |   sortLinkNode  |
          +-----------------+
          |   sortLinkNode  |
          +-----------------+
          |   sortLinkNode  |
          +-----------------+
          |   sortLinkNode  |
          +-----------------+
*/

typedef struct {
    LOS_DL_LIST sortLinkNode;	//链表节点
    UINT32 idxRollNum;			//滚动数
} SortLinkList;
typedef struct {
    LOS_DL_LIST *sortLink;//排序链表,看上图它指向的结构.
    UINT16 cursor;	//游标
    UINT16 reserved;	//保留用
} SortLinkAttribute;

extern UINT32 OsSortLinkInit(SortLinkAttribute *sortLinkHeader);
extern VOID OsAdd2SortLink(const SortLinkAttribute *sortLinkHeader, SortLinkList *sortList);
extern VOID OsDeleteSortLink(const SortLinkAttribute *sortLinkHeader, SortLinkList *sortList);
extern UINT32 OsSortLinkGetNextExpireTime(const SortLinkAttribute *sortLinkHeader);
extern UINT32 OsSortLinkGetTargetExpireTime(const SortLinkAttribute *sortLinkHeader,
                                            const SortLinkList *targetSortList);
extern VOID OsSortLinkUpdateExpireTime(UINT32 sleepTicks, SortLinkAttribute *sortLinkHeader);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SORTLINK_PRI_H */
