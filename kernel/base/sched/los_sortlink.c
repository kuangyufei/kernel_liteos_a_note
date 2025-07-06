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
/**
 * @brief 初始化排序链表
 * @details 初始化排序链表的头节点、自旋锁和节点计数
 * @param sortLinkHeader 排序链表属性结构体指针
 */
VOID OsSortLinkInit(SortLinkAttribute *sortLinkHeader)
{
    LOS_ListInit(&sortLinkHeader->sortLink);  // 初始化双向链表头
    LOS_SpinInit(&sortLinkHeader->spinLock);  // 初始化自旋锁
    sortLinkHeader->nodeNum = 0;  // 初始化节点计数为0
}

/**
 * @brief 将节点添加到排序链表（内部内联函数）
 * @details 根据响应时间将节点插入到排序链表的适当位置，保持链表按响应时间升序排列
 * @param sortLinkHeader 排序链表属性结构体指针
 * @param sortList 待添加的排序链表节点指针
 */
STATIC INLINE VOID AddNode2SortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_DL_LIST *head = (LOS_DL_LIST *)&sortLinkHeader->sortLink;  // 获取链表头指针

    if (LOS_ListEmpty(head)) {  // 如果链表为空
        LOS_ListHeadInsert(head, &sortList->sortLinkNode);  // 在表头插入节点
        sortLinkHeader->nodeNum++;  // 节点计数加1
        return;  // 返回
    }

    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(head->pstNext, SortLinkList, sortLinkNode);  // 获取第一个节点
    if (listSorted->responseTime > sortList->responseTime) {  // 如果第一个节点响应时间大于当前节点
        LOS_ListAdd(head, &sortList->sortLinkNode);  // 在表头后插入当前节点
        sortLinkHeader->nodeNum++;  // 节点计数加1
        return;  // 返回
    } else if (listSorted->responseTime == sortList->responseTime) {  // 如果响应时间相等
        LOS_ListAdd(head->pstNext, &sortList->sortLinkNode);  // 在第一个节点后插入当前节点
        sortLinkHeader->nodeNum++;  // 节点计数加1
        return;  // 返回
    }

    LOS_DL_LIST *prevNode = head->pstPrev;  // 从表尾开始查找
    do {  // 循环查找插入位置
        listSorted = LOS_DL_LIST_ENTRY(prevNode, SortLinkList, sortLinkNode);  // 获取前一个节点
        if (listSorted->responseTime <= sortList->responseTime) {  // 如果前一个节点响应时间小于等于当前节点
            LOS_ListAdd(prevNode, &sortList->sortLinkNode);  // 在前一个节点后插入当前节点
            sortLinkHeader->nodeNum++;  // 节点计数加1
            break;  // 跳出循环
        }

        prevNode = prevNode->pstPrev;  // 移动到前一个节点
    } while (1);  // 无限循环，直到找到插入位置
}

/**
 * @brief 添加节点到排序链表
 * @details 设置节点响应时间并加锁保护，调用内部函数将节点插入排序链表
 * @param head 排序链表属性结构体指针
 * @param node 待添加的排序链表节点指针
 * @param responseTime 节点的响应时间
 * @param idleCpu 空闲CPU编号（SMP模式下使用）
 */
VOID OsAdd2SortLink(SortLinkAttribute *head, SortLinkList *node, UINT64 responseTime, UINT16 idleCpu)
{
    LOS_SpinLock(&head->spinLock);  // 获取自旋锁
    SET_SORTLIST_VALUE(node, responseTime);  // 设置节点的响应时间
    AddNode2SortLink(head, node);  // 调用内部函数添加节点到排序链表
#ifdef LOSCFG_KERNEL_SMP
    node->cpuid = idleCpu;  // SMP模式下设置节点的CPU编号
#endif
    LOS_SpinUnlock(&head->spinLock);  // 释放自旋锁
}

/**
 * @brief 从排序链表中删除节点
 * @details 加锁保护后，从排序链表中删除指定节点（如果节点有效）
 * @param head 排序链表属性结构体指针
 * @param node 待删除的排序链表节点指针
 */
VOID OsDeleteFromSortLink(SortLinkAttribute *head, SortLinkList *node)
{
    LOS_SpinLock(&head->spinLock);  // 获取自旋锁
    if (node->responseTime != OS_SORT_LINK_INVALID_TIME) {  // 如果节点响应时间有效
        OsDeleteNodeSortLink(head, node);  // 从排序链表中删除节点
    }
    LOS_SpinUnlock(&head->spinLock);  // 释放自旋锁
}

/**
 * @brief 调整排序链表中节点的响应时间
 * @details 加锁保护后，先删除节点再重新插入，实现响应时间的调整
 * @param head 排序链表属性结构体指针
 * @param node 待调整的排序链表节点指针
 * @param responseTime 新的响应时间
 * @return 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 */
UINT32 OsSortLinkAdjustNodeResponseTime(SortLinkAttribute *head, SortLinkList *node, UINT64 responseTime)
{
    UINT32 ret = LOS_NOK;  // 初始化返回值为失败

    LOS_SpinLock(&head->spinLock);  // 获取自旋锁
    if (node->responseTime != OS_SORT_LINK_INVALID_TIME) {  // 如果节点响应时间有效
        OsDeleteNodeSortLink(head, node);  // 从链表中删除节点
        SET_SORTLIST_VALUE(node, responseTime);  // 设置新的响应时间
        AddNode2SortLink(head, node);  // 重新插入节点到链表
        ret = LOS_OK;  // 设置返回值为成功
    }
    LOS_SpinUnlock(&head->spinLock);  // 释放自旋锁
    return ret;  // 返回操作结果
}

/**
 * @brief 获取目标节点的到期时间
 * @details 计算当前时间到目标节点响应时间的时间差，如果已到期则返回0
 * @param currTime 当前时间
 * @param targetSortList 目标排序链表节点指针
 * @return 到期时间（单位：系统时钟周期），0表示已到期
 */
UINT64 OsSortLinkGetTargetExpireTime(UINT64 currTime, const SortLinkList *targetSortList)
{
    if (currTime >= targetSortList->responseTime) {  // 如果当前时间大于等于节点响应时间
        return 0;  // 返回0表示已到期
    }

    return (UINT32)(targetSortList->responseTime - currTime);  // 返回剩余到期时间
}

/**
 * @brief 获取排序链表中下一个到期时间
 * @details 获取链表中第一个节点的响应时间与当前时间的差值，即下一个到期时间
 * @param currTime 当前时间
 * @param sortLinkHeader 排序链表属性结构体指针
 * @return 下一个到期时间（单位：系统时钟周期），OS_SORT_LINK_INVALID_TIME表示链表为空
 */
UINT64 OsSortLinkGetNextExpireTime(UINT64 currTime, const SortLinkAttribute *sortLinkHeader)
{
    LOS_DL_LIST *head = (LOS_DL_LIST *)&sortLinkHeader->sortLink;  // 获取链表头指针

    if (LOS_ListEmpty(head)) {  // 如果链表为空
        return OS_SORT_LINK_INVALID_TIME;  // 返回无效时间
    }

    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(head->pstNext, SortLinkList, sortLinkNode);  // 获取第一个节点
    return OsSortLinkGetTargetExpireTime(currTime, listSorted);  // 计算并返回下一个到期时间
}