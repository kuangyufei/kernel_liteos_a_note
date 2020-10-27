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

#include "los_sortlink_pri.h"
#include "los_memory.h"
#include "los_exc.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
//排序链表,这是通用处理函数 内核有两处使用 OsSwtmrInit 和 OsTaskInit 见于 Percpu 结构体
LITE_OS_SEC_TEXT_INIT UINT32 OsSortLinkInit(SortLinkAttribute *sortLinkHeader)
{
    UINT32 size;
    LOS_DL_LIST *listObject = NULL;
    UINT32 index;

    size = sizeof(LOS_DL_LIST) << OS_TSK_SORTLINK_LOGLEN;//这行代码很精彩,得到 8个 LOS_DL_LIST
    listObject = (LOS_DL_LIST *)LOS_MemAlloc(m_aucSysMem0, size); /* system resident resource *///常驻内存
    if (listObject == NULL) {
        return LOS_NOK;
    }

    (VOID)memset_s(listObject, size, 0, size);//清0
    sortLinkHeader->sortLink = listObject;//可以知道 sortLink是个链表数组
    sortLinkHeader->cursor = 0;//游标默认为0
    for (index = 0; index < OS_TSK_SORTLINK_LEN; index++, listObject++) {// OS_TSK_SORTLINK_LEN = 8
        LOS_ListInit(listObject);//初始化8个链表
    }
    return LOS_OK;
}

LITE_OS_SEC_TEXT VOID OsAdd2SortLink(const SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    SortLinkList *listSorted = NULL;
    LOS_DL_LIST *listObject = NULL;
    UINT32 sortIndex;
    UINT32 rollNum;
    UINT32 timeout;

    /*
     * huge rollnum could cause carry to invalid high bit
     * and eventually affect the calculation of sort index.
     */ //巨大的滚动数可能导致进位无效最终影响排序指标的计算。
    if (sortList->idxRollNum > OS_TSK_MAX_ROLLNUM) { //滚动数索引最大就是 OS_TSK_MAX_ROLLNUM 
        SET_SORTLIST_VALUE(sortList, OS_TSK_MAX_ROLLNUM);
    }
    timeout = sortList->idxRollNum;
    sortIndex = timeout & OS_TSK_SORTLINK_MASK;//  决定放在哪个链表中
    rollNum = (timeout >> OS_TSK_SORTLINK_LOGLEN) + 1;
    if (sortIndex == 0) {
        rollNum--;
    }
    EVALUATE_L(sortList->idxRollNum, rollNum);//计算idxRollNum的低位
    sortIndex = sortIndex + sortLinkHeader->cursor;//通过游标确定最终链表位置
    sortIndex = sortIndex & OS_TSK_SORTLINK_MASK;//sortIndex不能大于OS_TSK_SORTLINK_MASK
    EVALUATE_H(sortList->idxRollNum, sortIndex);//计算idxRollNum的高位

    listObject = sortLinkHeader->sortLink + sortIndex;//找到最终要挂入的链表
    if (listObject->pstNext == listObject) {//为空时
        LOS_ListTailInsert(listObject, &sortList->sortLinkNode);//直接挂上去
    } else {
        listSorted = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//取出SortLinkList
        do {
            if (ROLLNUM(listSorted->idxRollNum) <= ROLLNUM(sortList->idxRollNum)) {// @note_? 这块没看懂,谁能帮帮我
                ROLLNUM_SUB(sortList->idxRollNum, listSorted->idxRollNum);
            } else {
                ROLLNUM_SUB(listSorted->idxRollNum, sortList->idxRollNum);
                break;
            }

            listSorted = LOS_DL_LIST_ENTRY(listSorted->sortLinkNode.pstNext, SortLinkList, sortLinkNode);//取下一个
        } while (&listSorted->sortLinkNode != listObject);//一直查询直到回到起点位置

        LOS_ListTailInsert(&listSorted->sortLinkNode, &sortList->sortLinkNode);//插入链表
    }
}

LITE_OS_SEC_TEXT STATIC VOID OsCheckSortLink(const LOS_DL_LIST *listHead, const LOS_DL_LIST *listNode)
{
    LOS_DL_LIST *tmp = listNode->pstPrev;

    /* recursive check until double link round to itself */
    while (tmp != listNode) {
        if (tmp == listHead) {
            goto FOUND;
        }
        tmp = tmp->pstPrev;
    }

    /* delete invalid sortlink node */
    PRINT_ERR("the node is not on this sortlink!\n");
    OsBackTrace();

FOUND:
    return;
}

LITE_OS_SEC_TEXT VOID OsDeleteSortLink(const SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_DL_LIST *listObject = NULL;
    SortLinkList *nextSortList = NULL;
    UINT32 sortIndex;

    sortIndex = SORT_INDEX(sortList->idxRollNum);
    listObject = sortLinkHeader->sortLink + sortIndex;

    /* check if pstSortList node is on the right sortlink */
    OsCheckSortLink(listObject, &sortList->sortLinkNode);

    if (listObject != sortList->sortLinkNode.pstNext) {
        nextSortList = LOS_DL_LIST_ENTRY(sortList->sortLinkNode.pstNext, SortLinkList, sortLinkNode);
        ROLLNUM_ADD(nextSortList->idxRollNum, sortList->idxRollNum);
    }
    LOS_ListDelete(&sortList->sortLinkNode);
}
//计算超时时间
LITE_OS_SEC_TEXT STATIC UINT32 OsCalcExpierTime(UINT32 rollNum, UINT32 sortIndex, UINT16 curSortIndex)
{
    UINT32 expireTime;

    if (sortIndex > curSortIndex) {
        sortIndex = sortIndex - curSortIndex;
    } else {
        sortIndex = OS_TSK_SORTLINK_LEN - curSortIndex + sortIndex;
    }
    expireTime = ((rollNum - 1) << OS_TSK_SORTLINK_LOGLEN) + sortIndex;
    return expireTime;
}
//从sortLink中获取下一个过期时间
LITE_OS_SEC_TEXT UINT32 OsSortLinkGetNextExpireTime(const SortLinkAttribute *sortLinkHeader)
{
    UINT16 cursor;
    UINT32 minSortIndex = OS_INVALID_VALUE;
    UINT32 minRollNum = OS_TSK_LOW_BITS_MASK;
    UINT32 expireTime = OS_INVALID_VALUE;
    LOS_DL_LIST *listObject = NULL;
    SortLinkList *listSorted = NULL;
    UINT32 i;

    cursor = (sortLinkHeader->cursor + 1) & OS_TSK_SORTLINK_MASK;

    for (i = 0; i < OS_TSK_SORTLINK_LEN; i++) {
        listObject = sortLinkHeader->sortLink + ((cursor + i) & OS_TSK_SORTLINK_MASK);
        if (!LOS_ListEmpty(listObject)) {
            listSorted = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
            if (minRollNum > ROLLNUM(listSorted->idxRollNum)) {
                minRollNum = ROLLNUM(listSorted->idxRollNum);
                minSortIndex = (cursor + i) & OS_TSK_SORTLINK_MASK;
            }
        }
    }

    if (minRollNum != OS_TSK_LOW_BITS_MASK) {
        expireTime = OsCalcExpierTime(minRollNum, minSortIndex, sortLinkHeader->cursor);
    }

    return expireTime;
}

LITE_OS_SEC_TEXT VOID OsSortLinkUpdateExpireTime(UINT32 sleepTicks, SortLinkAttribute *sortLinkHeader)
{
    SortLinkList *sortList = NULL;
    LOS_DL_LIST *listObject = NULL;
    UINT32 i;
    UINT32 sortIndex;
    UINT32 rollNum;

    if (sleepTicks == 0) {
        return;
    }
    sortIndex = sleepTicks & OS_TSK_SORTLINK_MASK;
    rollNum = (sleepTicks >> OS_TSK_SORTLINK_LOGLEN) + 1;
    if (sortIndex == 0) {
        rollNum--;
        sortIndex = OS_TSK_SORTLINK_LEN;
    }

    for (i = 0; i < OS_TSK_SORTLINK_LEN; i++) {
        listObject = sortLinkHeader->sortLink + ((sortLinkHeader->cursor + i) & OS_TSK_SORTLINK_MASK);
        if (listObject->pstNext != listObject) {
            sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
            ROLLNUM_SUB(sortList->idxRollNum, rollNum - 1);
            if ((i > 0) && (i < sortIndex)) {
                ROLLNUM_DEC(sortList->idxRollNum);
            }
        }
    }
    sortLinkHeader->cursor = (sortLinkHeader->cursor + sleepTicks - 1) % OS_TSK_SORTLINK_LEN;
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsSortLinkGetTargetExpireTime(const SortLinkAttribute *sortLinkHeader,
                                                            const SortLinkList *targetSortList)
{
    SortLinkList *listSorted = NULL;
    LOS_DL_LIST *listObject = NULL;
    UINT32 sortIndex = SORT_INDEX(targetSortList->idxRollNum);
    UINT32 rollNum = ROLLNUM(targetSortList->idxRollNum);

    listObject = sortLinkHeader->sortLink + sortIndex;

    listSorted = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    while (listSorted != targetSortList) {
        rollNum += ROLLNUM(listSorted->idxRollNum);
        listSorted = LOS_DL_LIST_ENTRY((listSorted->sortLinkNode).pstNext, SortLinkList, sortLinkNode);
    }
    return OsCalcExpierTime(rollNum, sortIndex, sortLinkHeader->cursor);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
