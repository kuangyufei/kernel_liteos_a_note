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
 * @file    los_list.h
 * @brief	双向链表由内联函数实现
 * @link dll http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-apx-dll.html @endlink
   @verbatim
   基本概念
	   双向链表是指含有往前和往后两个方向的链表，即每个结点中除存放下一个节点指针外，
	   还增加一个指向前一个节点的指针。其头指针head是唯一确定的。
   
	   从双向链表中的任意一个结点开始，都可以很方便地访问它的前驱结点和后继结点，这种
	   数据结构形式使得双向链表在查找时更加方便，特别是大量数据的遍历。由于双向链表
	   具有对称性，能方便地完成各种插入、删除等操作，但需要注意前后方向的操作。
   
   双向链表的典型开发流程：
	   调用LOS_ListInit/LOS_DL_LIST_HEAD初始双向链表。
	   调用LOS_ListAdd/LOS_ListHeadInsert向链表头部插入节点。
	   调用LOS_ListTailInsert向链表尾部插入节点。
	   调用LOS_ListDelete删除指定节点。
	   调用LOS_ListEmpty判断链表是否为空。
	   调用LOS_ListDelInit删除指定节点并以此节点初始化链表。 
   
   注意事项
	   需要注意节点指针前后方向的操作。
	   链表操作接口，为底层接口，不对入参进行判空，需要使用者确保传参合法。
	   如果链表节点的内存是动态申请的，删除节点时，要注意释放内存。
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-10
 */

/**
 * @defgroup los_list Doubly linked list
 * @ingroup kernel
 */

#ifndef _LOS_LIST_H
#define _LOS_LIST_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @ingroup los_list
 * 双向链表节点结构
 */
typedef struct LOS_DL_LIST {
    struct LOS_DL_LIST *pstPrev; /**< 当前节点指向前一个节点的指针 */
    struct LOS_DL_LIST *pstNext; /**< 当前节点指向后一个节点的指针 */
} LOS_DL_LIST;

/**
 * @ingroup los_list
 *
 * @par 描述
 * 此API用于初始化双向链表
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param list    [IN] 双向链表节点
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListInit(LOS_DL_LIST *list)
{
    list->pstNext = list;
    list->pstPrev = list;
}

/**
 * @ingroup los_list
 * @brief 获取当前节点指向的下一个节点
 *
 * @par 描述
 * <ul>
 * <li>此API用于获取当前节点指向的下一个节点</li>
 * </ul>
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param object  [IN] 双向链表中的节点
 *
 * @retval 下一个节点指针
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_DL_LIST_FIRST(object) ((object)->pstNext)

/**
 * @ingroup los_list
 * @brief 判断节点是否为链表末尾
 *
 * @par 描述
 * <ul>
 * <li>此API用于测试节点是否为链表末尾</li>
 * </ul>
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param list  [IN] 双向链表
 * @param node  [IN] 待测试是否为链表末尾的节点
 *
 * @retval TRUE 节点是链表末尾
 * @retval FALSE 节点不是链表末尾
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_DL_LIST_IS_END(list, node) ((list) == (node) ? TRUE : FALSE)

/**
 * @ingroup los_list
 * @brief 判断节点是否在链表中
 *
 * @par 描述
 * <ul>
 * <li>此API用于测试节点是否在链表中</li>
 * </ul>
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param object  [IN] 双向链表中的节点
 *
 * @retval TRUE 节点在链表中
 * @retval FALSE 节点不在链表中
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_DL_LIST_IS_ON_QUEUE(node) ((node)->pstPrev != NULL && (node)->pstNext != NULL)

/**
 * @ingroup los_list
 * @brief 获取当前节点指向的前一个节点
 *
 * @par 描述
 * <ul>
 * <li>此API用于获取当前节点指向的前一个节点</li>
 * </ul>
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param object  [IN] 双向链表中的节点
 *
 * @retval 前一个节点指针
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_DL_LIST_LAST(object) ((object)->pstPrev)

/**
 * @ingroup los_list
 * @brief 向双向链表中插入新节点
 *
 * @par 描述
 * 此API用于向双向链表中插入新节点
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param list    [IN] 要插入新节点的双向链表
 * @param node    [IN] 要插入的新节点
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListDelete
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListAdd(LOS_DL_LIST *list, LOS_DL_LIST *node)
{
    node->pstNext = list->pstNext;
    node->pstPrev = list;
    list->pstNext->pstPrev = node;
    list->pstNext = node;
}

/**
 * @ingroup los_list
 * @brief 向双向链表尾部插入节点
 *
 * @par 描述
 * 此API用于向双向链表尾部插入新节点
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param list     [IN] 要插入新节点的双向链表
 * @param node     [IN] 要插入的新节点
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListAdd | LOS_ListHeadInsert
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListTailInsert(LOS_DL_LIST *list, LOS_DL_LIST *node)
{
    LOS_ListAdd(list->pstPrev, node);
}

/**
 * @ingroup los_list
 * @brief 向双向链表头部插入节点
 *
 * @par 描述
 * 此API用于向双向链表头部插入新节点
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param list     [IN] 要插入新节点的双向链表
 * @param node     [IN] 要插入的新节点
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListAdd | LOS_ListTailInsert
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListHeadInsert(LOS_DL_LIST *list, LOS_DL_LIST *node)
{
    LOS_ListAdd(list, node);
}

/**
 * @ingroup los_list
 *
 * @par 描述
 * <ul>
 * <li>此API用于从双向链表中删除指定节点</li>
 * </ul>
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param node    [IN] 要删除的节点
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListAdd
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListDelete(LOS_DL_LIST *node)
{
    node->pstNext->pstPrev = node->pstPrev;
    node->pstPrev->pstNext = node->pstNext;
    node->pstNext = NULL;
    node->pstPrev = NULL;
}

/**
 * @ingroup los_list
 * @brief 判断指定双向链表是否为空
 *
 * @par 描述
 * <ul>
 * <li>此API用于返回双向链表是否为空</li>
 * </ul>
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param list  [IN] 双向链表
 *
 * @retval TRUE 双向链表为空
 * @retval FALSE 双向链表不为空
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE BOOL LOS_ListEmpty(LOS_DL_LIST *list)
{
    return (BOOL)(list->pstNext == list);
}

/**
 * @ingroup los_list
 * @brief 向双向链表中插入新链表
 *
 * @par 描述
 * 此API用于向双向链表中插入新链表
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param oldList    [IN] 要插入新链表的双向链表
 * @param newList    [IN] 要插入的新链表
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListDelete
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListAddList(LOS_DL_LIST *oldList, LOS_DL_LIST *newList)
{
    LOS_DL_LIST *oldListHead = oldList->pstNext;
    LOS_DL_LIST *oldListTail = oldList;
    LOS_DL_LIST *newListHead = newList;
    LOS_DL_LIST *newListTail = newList->pstPrev;

    oldListTail->pstNext = newListHead;
    newListHead->pstPrev = oldListTail;
    oldListHead->pstPrev = newListTail;
    newListTail->pstNext = oldListHead;
}

/**
 * @ingroup los_list
 * @brief 向双向链表尾部插入另一个链表
 *
 * @par 描述
 * 此API用于向双向链表尾部插入另一个链表
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param oldList     [IN] 要插入新链表的双向链表
 * @param newList     [IN] 要插入的新链表
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListAddList | LOS_ListHeadInsertList
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListTailInsertList(LOS_DL_LIST *oldList, LOS_DL_LIST *newList)
{
    LOS_ListAddList(oldList->pstPrev, newList);
}

/**
 * @ingroup los_list
 * @brief 向双向链表头部插入另一个链表
 *
 * @par 描述
 * 此API用于向双向链表头部插入另一个链表
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param oldList     [IN] 要插入新链表的双向链表
 * @param newList     [IN] 要插入的新链表
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListAddList | LOS_ListTailInsertList
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListHeadInsertList(LOS_DL_LIST *oldList, LOS_DL_LIST *newList)
{
    LOS_ListAddList(oldList, newList);
}

/**
 * @ingroup los_list
 * @brief 获取结构体中某个成员相对于结构体地址的偏移量
 *
 * @par  描述
 * 此API用于获取结构体中某个成员相对于结构体地址的偏移量
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param type    [IN] 结构体名称
 * @param member  [IN] 要测量偏移量的成员名称
 *
 * @retval 成员相对于结构体地址的偏移量（字节数）
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_OFF_SET_OF(type, member) ((UINTPTR)&((type *)0)->member)

/**
 * @ingroup los_list
 * @brief 获取包含双向链表节点的结构体指针
 *
 * @par 描述
 * 此API用于通过双向链表节点指针获取包含该节点的结构体指针
 * <ul>
 * <li>无</li>
 * </ul>
 * @attention
 * <ul>
 * <li>确保节点指针有效且确实是该结构体的成员</li>
 * </ul>
 *
 * @param item    [IN] 双向链表节点指针
 * @param type    [IN] 结构体名称
 * @param member  [IN] 结构体中双向链表成员的名称
 *
 * @retval 包含双向链表节点的结构体指针
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_OFF_SET_OF
 */
#define LOS_DL_LIST_ENTRY(item, type, member) \
    ((type *)(VOID *)((CHAR *)(item) - LOS_OFF_SET_OF(type, member)))

/**
 * @ingroup los_list
 * @brief 遍历指定类型的双向链表
 *
 * @par 描述
 * 此API用于遍历包含指定类型结构体的双向链表
 * @attention
 * <ul>
 * <li>遍历时不要删除节点，否则可能导致遍历异常</li>
 * </ul>
 *
 * @param item           [IN] 用于遍历的结构体指针
 * @param list           [IN] 要遍历的双向链表
 * @param type           [IN] 结构体名称
 * @param member         [IN] 结构体中双向链表成员的名称
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_DL_LIST_ENTRY
 */
#define LOS_DL_LIST_FOR_EACH_ENTRY(item, list, type, member)             \
    for (item = LOS_DL_LIST_ENTRY((list)->pstNext, type, member);        \
         &(item)->member != (list);                                      \
         item = LOS_DL_LIST_ENTRY((item)->member.pstNext, type, member))

/**
 * @ingroup los_list
 * @brief 安全遍历双向链表（允许遍历中删除节点）
 *
 * @par 描述
 * 此API用于安全遍历双向链表，支持在遍历过程中删除节点
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param item           [IN] 用于遍历的结构体指针
 * @param next           [IN] 保存下一个节点的指针
 * @param list           [IN] 要遍历的双向链表
 * @param type           [IN] 结构体名称
 * @param member         [IN] 结构体中双向链表成员的名称
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_DL_LIST_ENTRY
 */
#define LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, next, list, type, member)               \
    for (item = LOS_DL_LIST_ENTRY((list)->pstNext, type, member),                     \
         next = LOS_DL_LIST_ENTRY((item)->member.pstNext, type, member);              \
         &(item)->member != (list);                                                   \
         item = next, next = LOS_DL_LIST_ENTRY((item)->member.pstNext, type, member))

/**
 * @ingroup los_list
 * @brief 删除节点并初始化
 *
 * @par 描述
 * 此API用于从链表中删除节点并将其重新初始化为空链表
 * @attention
 * <ul>
 * <li>传入的参数必须确保是合法指针</li>
 * </ul>
 *
 * @param list    [IN] 要删除并初始化的双向链表节点
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListInit
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListDelInit(LOS_DL_LIST *list)
{
    list->pstNext->pstPrev = list->pstPrev;
    list->pstPrev->pstNext = list->pstNext;
    LOS_ListInit(list);
}

/**
 * @ingroup los_list
 * @brief 遍历双向链表节点
 *
 * @par 描述
 * 此API用于遍历双向链表的节点
 * @attention
 * <ul>
 * <li>遍历时不要删除节点，否则可能导致遍历异常</li>
 * </ul>
 *
 * @param item           [IN] 用于遍历的节点指针
 * @param list           [IN] 要遍历的双向链表
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_DL_LIST_FOR_EACH(item, list) \
    for (item = (list)->pstNext;         \
         (item) != (list);               \
         item = (item)->pstNext)

/**
 * @ingroup los_list
 * @brief 安全遍历双向链表节点（允许遍历中删除节点）
 *
 * @par 描述
 * 此API用于安全遍历双向链表节点，支持在遍历过程中删除节点
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param item           [IN] 用于遍历的节点指针
 * @param next           [IN] 保存下一个节点的指针
 * @param list           [IN] 要遍历的双向链表
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_DL_LIST_FOR_EACH_SAFE(item, next, list)      \
    for (item = (list)->pstNext, next = (item)->pstNext; \
         (item) != (list);                               \
         item = next, next = (item)->pstNext)

/**
 * @ingroup los_list
 * @brief 定义并初始化双向链表头
 *
 * @par 描述
 * 此宏用于定义并初始化一个双向链表头
 * @attention
 * <ul>
 * <li>通常在全局或栈上定义链表时使用</li>
 * </ul>
 *
 * @param list           [IN] 要定义的双向链表头名称
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_ListInit
 */
#define LOS_DL_LIST_HEAD(list) LOS_DL_LIST list = { &(list), &(list) }

/**
 * @ingroup los_list
 * @brief 获取链表头部节点的结构体指针
 *
 * @par 描述
 * 此宏用于获取链表头部节点对应的结构体指针，不删除节点
 * @attention
 * <ul>
 * <li>如果链表为空，返回NULL</li>
 * </ul>
 *
 * @param list           [IN] 双向链表头
 * @param type           [IN] 结构体类型
 * @param element        [IN] 结构体中链表成员的名称
 *
 * @retval 成功：返回头部节点的结构体指针；失败：返回NULL
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_DL_LIST_ENTRY
 */
#define LOS_ListPeekHeadType(list, type, element) ({             \
    type *__t;                                                   \
    if ((list)->pstNext == list) {                               \
        __t = NULL;                                              \
    } else {                                                     \
        __t = LOS_DL_LIST_ENTRY((list)->pstNext, type, element); \
    }                                                            \
    __t;                                                         \
})

/**
 * @ingroup los_list
 * @brief 移除并获取链表头部节点的结构体指针
 *
 * @par 描述
 * 此宏用于移除链表头部节点并返回其对应的结构体指针
 * @attention
 * <ul>
 * <li>如果链表为空，返回NULL</li>
 * </ul>
 *
 * @param list           [IN] 双向链表头
 * @param type           [IN] 结构体类型
 * @param element        [IN] 结构体中链表成员的名称
 *
 * @retval 成功：返回被移除的头部节点结构体指针；失败：返回NULL
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_DL_LIST_ENTRY, LOS_ListDelete
 */
#define LOS_ListRemoveHeadType(list, type, element) ({           \
    type *__t;                                                   \
    if ((list)->pstNext == list) {                               \
        __t = NULL;                                              \
    } else {                                                     \
        __t = LOS_DL_LIST_ENTRY((list)->pstNext, type, element); \
        LOS_ListDelete((list)->pstNext);                         \
    }                                                            \
    __t;                                                         \
})

/**
 * @ingroup los_list
 * @brief 获取当前节点的下一个节点结构体指针
 *
 * @par 描述
 * 此宏用于获取当前节点的下一个节点对应的结构体指针
 * @attention
 * <ul>
 * <li>如果当前节点是链表末尾，返回NULL</li>
 * </ul>
 *
 * @param list           [IN] 双向链表头
 * @param item           [IN] 当前节点的结构体指针
 * @param type           [IN] 结构体类型
 * @param element        [IN] 结构体中链表成员的名称
 *
 * @retval 成功：返回下一个节点的结构体指针；失败：返回NULL
 * @par 依赖
 * <ul><li>los_list.h: 包含API声明的头文件</li></ul>
 * @see LOS_DL_LIST_ENTRY
 */
#define LOS_ListNextType(list, item, type, element) ({           \
    type *__t;                                                   \
    if ((item)->pstNext == list) {                               \
        __t = NULL;                                              \
    } else {                                                     \
        __t = LOS_DL_LIST_ENTRY((item)->pstNext, type, element); \
    }                                                            \
    __t;                                                         \
})

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_LIST_H */
