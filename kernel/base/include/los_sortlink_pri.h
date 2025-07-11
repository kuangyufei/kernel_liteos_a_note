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

/**
 * @brief 排序链表节点结构体
 * @details 用于构建有序链表的节点，存储响应时间及CPU亲和性信息
 * @attention 该结构体需嵌入到用户数据结构中使用，通过sortLinkNode接入排序链表
 */
typedef struct {
    LOS_DL_LIST sortLinkNode;       /* 双向链表节点，用于接入排序链表 */
    UINT64      responseTime;       /* 响应时间戳（单位：系统滴答数），用于链表排序 */
#ifdef LOSCFG_KERNEL_SMP
    UINT32      cpuid;              /* CPU核心ID，仅SMP配置有效，用于指定任务运行的CPU */
#endif
} SortLinkList; 

/**
 * @brief 排序链表属性结构体
 * @details 管理排序链表的元数据，包括链表头、节点数量及同步锁
 * @core 提供基于响应时间的有序链表管理能力，支持高效插入和查询操作
 */
typedef struct {
    LOS_DL_LIST sortLink;           /* 排序链表头，所有节点按responseTime升序排列 */
    UINT32      nodeNum;            /* 链表节点数量，记录当前链表中的节点总数 */
    SPIN_LOCK_S spinLock;           /* 排序链表自旋锁，用于多线程安全访问（如软件定时器链表） */
} SortLinkAttribute; 

/**
 * @brief 无效的排序链表时间值
 * @details 表示排序链表节点的无效时间戳，用于标记未使用或已删除的节点
 * @note 实际值为UINT64_MAX（十进制：18446744073709551615）
 */
#define OS_SORT_LINK_INVALID_TIME ((UINT64)-1)

/**
 * @brief 设置排序链表节点的响应时间
 * @details 将排序链表节点的responseTime字段设置为指定值
 * @param[in,out] sortList 指向SortLinkList结构体的指针
 * @param[in] value 要设置的响应时间值（单位：系统滴答数）
 * @note 宏内部会将sortList强制转换为SortLinkList*类型
 */
#define SET_SORTLIST_VALUE(sortList, value) (((SortLinkList *)(sortList))->responseTime = (value))

/**
 * @brief 获取排序链表节点的响应时间
 * @details 返回排序链表节点的responseTime字段值
 * @param[in] sortList 指向SortLinkList结构体的指针
 * @return UINT64 - 节点的响应时间值（单位：系统滴答数）
 * @note 宏内部会将sortList强制转换为SortLinkList*类型
 */
#define GET_SORTLIST_VALUE(sortList) (((SortLinkList *)(sortList))->responseTime)

/**
 * @brief 从排序链表中删除节点
 * @details 将指定节点从排序链表中移除，并标记为无效时间
 * @param[in,out] sortLinkHeader 排序链表属性结构体指针
 * @param[in,out] sortList 要删除的排序链表节点指针
 * @note 调用前需确保节点已在链表中，删除后节点数量自动减1
 */
STATIC INLINE VOID OsDeleteNodeSortLink(SortLinkAttribute *sortLinkHeader, SortLinkList *sortList)
{
    LOS_ListDelete(&sortList->sortLinkNode); /* 从双向链表中删除当前节点 */
    SET_SORTLIST_VALUE(sortList, OS_SORT_LINK_INVALID_TIME); /* 将节点时间标记为无效 */
    sortLinkHeader->nodeNum--; /* 链表节点数量减1 */
}

/**
 * @brief 获取排序链表中下一个到期时间
 * @details 计算并返回链表中下一个节点的到期时间，考虑系统当前时间和滴答精度
 * @param[in] sortHeader 排序链表属性结构体指针
 * @param[in] startTime 当前系统时间（单位：系统滴答数）
 * @param[in] tickPrecision 系统滴答精度（单位：系统滴答数）
 * @return UINT64 - 下一个到期时间（单位：系统滴答数），无效返回OS_SORT_LINK_INVALID_TIME - tickPrecision
 * @note 函数内部通过自旋锁保证线程安全
 */
STATIC INLINE UINT64 OsGetSortLinkNextExpireTime(SortLinkAttribute *sortHeader, UINT64 startTime, UINT32 tickPrecision)
{
    LOS_DL_LIST *head = &sortHeader->sortLink; /* 获取链表头指针 */
    LOS_DL_LIST *list = head->pstNext; /* 获取第一个节点指针 */

    LOS_SpinLock(&sortHeader->spinLock); /* 加自旋锁，保护链表访问 */
    if (LOS_ListEmpty(head)) { /* 检查链表是否为空 */
        LOS_SpinUnlock(&sortHeader->spinLock); /* 空链表，释放锁 */
        return OS_SORT_LINK_INVALID_TIME - tickPrecision; /* 返回无效时间 */
    }

    /* 将链表节点转换为SortLinkList类型 */
    SortLinkList *listSorted = LOS_DL_LIST_ENTRY(list, SortLinkList, sortLinkNode);
    /* 检查节点是否已到期（考虑滴答精度） */
    if (listSorted->responseTime <= (startTime + tickPrecision)) {
        LOS_SpinUnlock(&sortHeader->spinLock); /* 已到期，释放锁 */
        return startTime + tickPrecision; /* 返回当前时间+精度作为到期时间 */
    }

    LOS_SpinUnlock(&sortHeader->spinLock); /* 未到期，释放锁 */
    return listSorted->responseTime; /* 返回节点的响应时间 */
}

/**
 * @brief 获取排序链表的节点数量
 * @details 返回排序链表中当前的节点总数
 * @param[in] head 排序链表属性结构体指针（const限定，只读）
 * @return UINT32 - 节点数量
 * @note 该函数为只读操作，无需加锁
 */
STATIC INLINE UINT32 OsGetSortLinkNodeNum(const SortLinkAttribute *head)
{
    return head->nodeNum; /* 返回nodeNum成员值 */
}

/**
 * @brief 获取排序链表节点的CPU ID
 * @details 返回节点关联的CPU核心ID，SMP配置有效
 * @param[in] node 排序链表节点指针
 * @return UINT16 - CPU核心ID，非SMP配置返回0
 * @note 仅当LOSCFG_KERNEL_SMP宏定义时有效
 */
STATIC INLINE UINT16 OsGetSortLinkNodeCpuid(const SortLinkList *node)
{
#ifdef LOSCFG_KERNEL_SMP
    return node->cpuid; /* SMP配置：返回节点的cpuid成员 */
#else
    return 0; /* 非SMP配置：返回0 */
#endif
}

VOID OsSortLinkInit(SortLinkAttribute *sortLinkHeader);
VOID OsAdd2SortLink(SortLinkAttribute *head, SortLinkList *node, UINT64 responseTime, UINT16 idleCpu);
VOID OsDeleteFromSortLink(SortLinkAttribute *head, SortLinkList *node);
UINT64 OsSortLinkGetTargetExpireTime(UINT64 currTime, const SortLinkList *targetSortList);
UINT64 OsSortLinkGetNextExpireTime(UINT64 currTime, const SortLinkAttribute *sortLinkHeader);
UINT32 OsSortLinkAdjustNodeResponseTime(SortLinkAttribute *head, SortLinkList *node, UINT64 responseTime);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SORTLINK_PRI_H */
