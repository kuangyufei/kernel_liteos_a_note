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

/******************************************************************************
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

参考
	https://gitee.com/LiteOS/LiteOS/blob/master/doc/Huawei_LiteOS_Kernel_Developer_Guide_zh.md#setup
******************************************************************************/
/**
 * @ingroup los_list
 * Structure of a node in a doubly linked list.
 */	
typedef struct LOS_DL_LIST {//双向链表，内核最重要结构体
    struct LOS_DL_LIST *pstPrev; /**< Current node's pointer to the previous node *///前驱节点(左手)
    struct LOS_DL_LIST *pstNext; /**< Current node's pointer to the next node *///后继节点(右手)
} LOS_DL_LIST;

/**
 * @ingroup los_list
 *
 * @par Description:
 * This API is used to initialize a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameter passed in should be ensured to be a legal pointer.</li>
 * </ul>
 *
 * @param list    [IN] Node in a doubly linked list.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//将指定节点初始化为双向链表节点
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListInit(LOS_DL_LIST *list)
{
    list->pstNext = list;
    list->pstPrev = list;
}

/**
 * @ingroup los_list
 * @brief Point to the next node pointed to by the current node.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to point to the next node pointed to by the current node.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param object  [IN] Node in the doubly linked list.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//获取指定节点的后继结点
#define LOS_DL_LIST_FIRST(object) ((object)->pstNext)

/**
 * @ingroup los_list
 * @brief Node is the end of the list.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to test node is the end of the list.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param object  [IN] Node in the doubly linked list.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */ 
#define LOS_DL_LIST_IS_END(list, node) ((list) == (node) ? TRUE : FALSE)

/**
 * @ingroup los_list
 * @brief Node is on the list.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to test node is on the list.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param object  [IN] Node in the doubly linked list.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
#define LOS_DL_LIST_IS_ON_QUEUE(node) ((node)->pstPrev != NULL && (node)->pstNext != NULL)

/**
 * @ingroup los_list
 * @brief Point to the previous node pointed to by the current node.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to point to the previous node pointed to by the current node.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param object  [IN] Node in the doubly linked list.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//获取指定节点的前驱结点
#define LOS_DL_LIST_LAST(object) ((object)->pstPrev)

/**
 * @ingroup los_list
 * @brief Insert a new node to a doubly linked list.
 *
 * @par Description:
 * This API is used to insert a new node to a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameters passed in should be ensured to be legal pointers.</li>
 * </ul>
 *
 * @param list    [IN] Doubly linked list where the new node is inserted.
 * @param node    [IN] New node to be inserted.
 *
 * @retval None
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_ListDelete
 */	//将指定节点插入到双向链表头端
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListAdd(LOS_DL_LIST *list, LOS_DL_LIST *node)
{
    node->pstNext = list->pstNext;
    node->pstPrev = list;
    list->pstNext->pstPrev = node;
    list->pstNext = node;
}

/**
 * @ingroup los_list
 * @brief Insert a node to the tail of a doubly linked list.
 *
 * @par Description:
 * This API is used to insert a new node to the tail of a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameters passed in should be ensured to be legal pointers.</li>
 * </ul>
 *
 * @param list     [IN] Doubly linked list where the new node is inserted.
 * @param node     [IN] New node to be inserted.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_ListAdd | LOS_ListHeadInsert
 */	//将指定节点插入到双向链表尾端
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListTailInsert(LOS_DL_LIST *list, LOS_DL_LIST *node)
{
    LOS_ListAdd(list->pstPrev, node);
}

/**
 * @ingroup los_list
 * @brief Insert a node to the head of a doubly linked list.
 *
 * @par Description:
 * This API is used to insert a new node to the head of a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameters passed in should be ensured to be legal pointers.</li>
 * </ul>
 *
 * @param list     [IN] Doubly linked list where the new node is inserted.
 * @param node     [IN] New node to be inserted.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_ListAdd | LOS_ListTailInsert
 */	//将指定节点插入到双向链表头端
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListHeadInsert(LOS_DL_LIST *list, LOS_DL_LIST *node)
{
    LOS_ListAdd(list, node);
}

/**
 * @ingroup los_list
 *
 * @par Description:
 * <ul>
 * <li>This API is used to delete a specified node from a doubly linked list.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The parameter passed in should be ensured to be a legal pointer.</li>
 * </ul>
 *
 * @param node    [IN] Node to be deleted.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_ListAdd
 */	//将指定节点从链表中删除
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListDelete(LOS_DL_LIST *node)
{
    node->pstNext->pstPrev = node->pstPrev;
    node->pstPrev->pstNext = node->pstNext;
    node->pstNext = NULL;
    node->pstPrev = NULL;
}

/**
 * @ingroup los_list
 * @brief Identify whether a specified doubly linked list is empty.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to return whether a doubly linked list is empty.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The parameter passed in should be ensured to be a legal pointer.</li>
 * </ul>
 *
 * @param list  [IN] Doubly linked list.
 *
 * @retval TRUE The doubly linked list is empty.
 * @retval FALSE The doubly linked list is not empty.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//判断链表是否为空
LITE_OS_SEC_ALW_INLINE STATIC INLINE BOOL LOS_ListEmpty(LOS_DL_LIST *list)
{
    return (BOOL)(list->pstNext == list);
}

/**
 * @ingroup los_list
 * @brief Insert a new list to a doubly linked list.
 *
 * @par Description:
 * This API is used to insert a new list to a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameters passed in should be ensured to be legal pointers.</li>
 * </ul>
 *
 * @param oldList    [IN] Doubly linked list where the new list is inserted.
 * @param newList    [IN] New list to be inserted.
 *
 * @retval None
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
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
 * @brief Insert a doubly list to the tail of a doubly linked list.
 *
 * @par Description:
 * This API is used to insert a new doubly list to the tail of a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameters passed in should be ensured to be legal pointers.</li>
 * </ul>
 *
 * @param oldList     [IN] Doubly linked list where the new list is inserted.
 * @param newList     [IN] New list to be inserted.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_ListAddList | LOS_ListHeadInsertList
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListTailInsertList(LOS_DL_LIST *oldList, LOS_DL_LIST *newList)
{
    LOS_ListAddList(oldList->pstPrev, newList);
}

/**
 * @ingroup los_list
 * @brief Insert a doubly list to the head of a doubly linked list.
 *
 * @par Description:
 * This API is used to insert a new doubly list to the head of a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameters passed in should be ensured to be legal pointers.</li>
 * </ul>
 *
 * @param oldList     [IN] Doubly linked list where the new list is inserted.
 * @param newList     [IN] New list to be inserted.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_ListAddList | LOS_ListTailInsertList
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListHeadInsertList(LOS_DL_LIST *oldList, LOS_DL_LIST *newList)
{
    LOS_ListAddList(oldList, newList);
}

/**
 * @ingroup los_list
 * @brief Obtain the offset of a field to a structure address.
 *
 * @par  Description:
 * This API is used to obtain the offset of a field to a structure address.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param type   [IN] Structure name.
 * @param field  [IN] Name of the field of which the offset is to be measured.
 *
 * @retval Offset of the field to the structure address.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
#define OFFSET_OF_FIELD(type, field) ((UINTPTR)&((type *)0)->field)

/**
 * @ingroup los_list
 * @brief Obtain the pointer to a doubly linked list in a structure.
 *
 * @par Description:
 * This API is used to obtain the pointer to a doubly linked list in a structure.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param type    [IN] Structure name.
 * @param member  [IN] Member name of the doubly linked list in the structure.
 *
 * @retval Pointer to the doubly linked list in the structure.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//获取指定结构体内的成员相对于结构体起始地址的偏移量
#define LOS_OFF_SET_OF(type, member) ((UINTPTR)&((type *)0)->member)

/**
 * @ingroup los_list
 * @brief Obtain the pointer to a structure that contains a doubly linked list.
 *
 * @par Description:
 * This API is used to obtain the pointer to a structure that contains a doubly linked list.
 * <ul>
 * <li>None.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param item    [IN] Current node's pointer to the next node.
 * @param type    [IN] Structure name.
 * @param member  [IN] Member name of the doubly linked list in the structure.
 *
 * @retval Pointer to the structure that contains the doubly linked list.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//获取包含链表的结构体地址，接口的第一个入参表示的是链表中的下一个节点，第二个入参是要获取的结构体名称，第三个入参是链表在该结构体中的名称
#define LOS_DL_LIST_ENTRY(item, type, member) \
    ((type *)(VOID *)((CHAR *)(item) - LOS_OFF_SET_OF(type, member)))

/**
 * @ingroup los_list
 * @brief Iterate over a doubly linked list of given type.
 *
 * @par Description:
 * This API is used to iterate over a doubly linked list of given type.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param item           [IN] Pointer to the structure that contains the doubly linked list that is to be traversed.
 * @param list           [IN] Pointer to the doubly linked list to be traversed.
 * @param type           [IN] Structure name.
 * @param member         [IN] Member name of the doubly linked list in the structure.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */ //遍历指定双向链表，获取包含该链表节点的结构体地址
#define LOS_DL_LIST_FOR_EACH_ENTRY(item, list, type, member)             \
    for (item = LOS_DL_LIST_ENTRY((list)->pstNext, type, member);        \
         &(item)->member != (list);                                      \
         item = LOS_DL_LIST_ENTRY((item)->member.pstNext, type, member))

/**
 * @ingroup los_list
 * @brief iterate over a doubly linked list safe against removal of list entry.
 *
 * @par Description:
 * This API is used to iterate over a doubly linked list safe against removal of list entry.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param item           [IN] Pointer to the structure that contains the doubly linked list that is to be traversed.
 * @param next           [IN] Save the next node.
 * @param list           [IN] Pointer to the doubly linked list to be traversed.
 * @param type           [IN] Structure name.
 * @param member         [IN] Member name of the doubly linked list in the structure.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */ //遍历指定双向链表，获取包含该链表节点的结构体地址，并存储包含当前节点的后继节点的结构体地址
#define LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, next, list, type, member)               \
    for (item = LOS_DL_LIST_ENTRY((list)->pstNext, type, member),                     \
         next = LOS_DL_LIST_ENTRY((item)->member.pstNext, type, member);              \
         &(item)->member != (list);                                                   \
         item = next, next = LOS_DL_LIST_ENTRY((item)->member.pstNext, type, member))

/**
 * @ingroup los_list
 * @brief Delete initialize a doubly linked list.
 *
 * @par Description:
 * This API is used to delete initialize a doubly linked list.
 * @attention
 * <ul>
 * <li>The parameter passed in should be ensured to be s legal pointer.</li>
 * </ul>
 *
 * @param list    [IN] Doubly linked list.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//将指定节点从链表中删除，并使用该节点初始化链表
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_ListDelInit(LOS_DL_LIST *list)
{
    list->pstNext->pstPrev = list->pstPrev;
    list->pstPrev->pstNext = list->pstNext;
    LOS_ListInit(list);
}

/**
 * @ingroup los_list
 * @brief iterate over a doubly linked list.
 *
 * @par Description:
 * This API is used to iterate over a doubly linked list.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param item           [IN] Pointer to the structure that contains the doubly linked list that is to be traversed.
 * @param list           [IN] Pointer to the doubly linked list to be traversed.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */ //遍历双向链表
#define LOS_DL_LIST_FOR_EACH(item, list) \
    for (item = (list)->pstNext;         \
         (item) != (list);               \
         item = (item)->pstNext)

/**
 * @ingroup los_list
 * @brief Iterate over a doubly linked list safe against removal of list entry.
 *
 * @par Description:
 * This API is used to iterate over a doubly linked list safe against removal of list entry.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param item           [IN] Pointer to the structure that contains the doubly linked list that is to be traversed.
 * @param next           [IN] Save the next node.
 * @param list           [IN] Pointer to the doubly linked list to be traversed.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */ //遍历双向链表，并存储当前节点的后继节点用于安全校验
#define LOS_DL_LIST_FOR_EACH_SAFE(item, next, list)      \
    for (item = (list)->pstNext, next = (item)->pstNext; \
         (item) != (list);                               \
         item = next, next = (item)->pstNext)

/**
 * @ingroup los_list
 * @brief Initialize a double linked list.
 *
 * @par Description:
 * This API is used to initialize a double linked list.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param list           [IN] Pointer to the doubly linked list to be traversed.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_list.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//定义一个节点并初始化为双向链表节点
#define LOS_DL_LIST_HEAD(list) LOS_DL_LIST list = { &(list), &(list) }

#define LOS_ListPeekHeadType(list, type, element) ({             \
    type *__t;                                                   \
    if ((list)->pstNext == list) {                               \
        __t = NULL;                                              \
    } else {                                                     \
        __t = LOS_DL_LIST_ENTRY((list)->pstNext, type, element); \
    }                                                            \
    __t;                                                         \
})

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
