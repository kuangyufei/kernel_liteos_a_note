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

#ifndef _SHELL_LIST_H
#define _SHELL_LIST_H

#include "sherr.h"
#include "stdint.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup shell_list
 * @brief 布尔类型定义 (使用size_t实现)
 * @note 此处将bool定义为size_t类型，1表示真，0表示假
 */
typedef size_t bool;

/**
 * @ingroup shell_list
 * @brief 双向链表节点结构
 * @par 描述
 * 定义双向链表的基本节点，包含指向前一个节点和后一个节点的指针
 */
typedef struct SH_List {
    struct SH_List *pstPrev; /**< 当前节点指向前一个节点的指针 */
    struct SH_List *pstNext; /**< 当前节点指向后一个节点的指针 */
} SH_List;

/**
 * @ingroup shell_list
 * @brief 初始化双向链表
 *
 * @par 描述
 * 该API用于初始化一个双向链表，将链表节点的前后指针均指向自身
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致未定义行为</li>
 * </ul>
 *
 * @param list    [IN] 双向链表节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListAdd | SH_ListDelete
 */
static inline void SH_ListInit(SH_List *list)
{
    list->pstNext = list;  // 后向指针指向自身
    list->pstPrev = list;  // 前向指针指向自身
}

/**
 * @ingroup shell_list
 * @brief 获取当前节点指向的下一个节点
 *
 * @par 描述
 * 该宏用于获取双向链表中当前节点的下一个节点指针
 * @attention
 * <ul>
 * <li>无特殊注意事项</li>
 * </ul>
 *
 * @param object  [IN] 双向链表节点指针
 *
 * @retval 返回当前节点的下一个节点指针
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_LIST_LAST
 */
#define SH_LIST_FIRST(object) ((object)->pstNext)

/**
 * @ingroup shell_list
 * @brief 获取当前节点指向的前一个节点
 *
 * @par 描述
 * 该宏用于获取双向链表中当前节点的前一个节点指针
 * @attention
 * <ul>
 * <li>无特殊注意事项</li>
 * </ul>
 *
 * @param object  [IN] 双向链表节点指针
 *
 * @retval 返回当前节点的前一个节点指针
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_LIST_FIRST
 */
#define SH_LIST_LAST(object) ((object)->pstPrev)

/**
 * @ingroup shell_list
 * @brief 向双向链表中插入新节点
 *
 * @par 描述
 * 该API用于在指定节点后插入新节点，新节点将成为指定节点的直接后继
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致链表损坏</li>
 * </ul>
 *
 * @param list    [IN] 双向链表节点指针 (新节点将插入到此节点之后)
 * @param node    [IN] 待插入的新节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListDelete | SH_ListTailInsert
 */
static inline void SH_ListAdd(SH_List *list, SH_List *node)
{
    node->pstNext = list->pstNext;  // 新节点的后向指针指向list的原后继节点
    node->pstPrev = list;          // 新节点的前向指针指向list
    list->pstNext->pstPrev = node;  // list原后继节点的前向指针指向新节点
    list->pstNext = node;          // list的后向指针指向新节点
}

/**
 * @ingroup shell_list
 * @brief 将节点插入到双向链表的尾部
 *
 * @par 描述
 * 该API用于将新节点插入到双向链表的尾部，成为链表的最后一个节点
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致未定义行为</li>
 * </ul>
 *
 * @param list     [IN] 双向链表头节点指针
 * @param node     [IN] 待插入的新节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListAdd | SH_ListHeadInsert
 */
static inline void SH_ListTailInsert(SH_List *list, SH_List *node)
{
    if ((list == NULL) || (node == NULL)) {  // 参数合法性检查
        return;                             // 非法指针，直接返回
    }

    SH_ListAdd(list->pstPrev, node);  // 在链表尾节点后插入新节点
}

/**
 * @ingroup shell_list
 * @brief 将节点插入到双向链表的头部
 *
 * @par 描述
 * 该API用于将新节点插入到双向链表的头部，成为链表的第一个有效节点
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致未定义行为</li>
 * </ul>
 *
 * @param list     [IN] 双向链表头节点指针
 * @param node     [IN] 待插入的新节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListAdd | SH_ListTailInsert
 */
static inline void SH_ListHeadInsert(SH_List *list, SH_List *node)
{
    if ((list == NULL) || (node == NULL)) {  // 参数合法性检查
        return;                             // 非法指针，直接返回
    }

    SH_ListAdd(list, node);  // 在链表头节点后插入新节点
}

/**
 * @ingroup shell_list
 * @brief 从双向链表中删除指定节点
 *
 * @par 描述
 * 该API用于从双向链表中删除指定节点，并将该节点的前后指针置空
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针且节点已在链表中，否则可能导致链表损坏</li>
 * </ul>
 *
 * @param node    [IN] 待删除的节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListAdd
 */
static inline void SH_ListDelete(SH_List *node)
{
    node->pstNext->pstPrev = node->pstPrev;  // 待删除节点的后继节点的前向指针指向待删除节点的前驱节点
    node->pstPrev->pstNext = node->pstNext;  // 待删除节点的前驱节点的后向指针指向待删除节点的后继节点
    node->pstNext = NULL;                    // 待删除节点的后向指针置空
    node->pstPrev = NULL;                    // 待删除节点的前向指针置空
}

/**
 * @ingroup shell_list
 * @brief 判断双向链表是否为空
 *
 * @par 描述
 * 该API用于判断指定的双向链表是否为空链表
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则返回FALSE</li>
 * </ul>
 *
 * @param list  [IN] 双向链表头节点指针
 *
 * @retval TRUE  双向链表为空
 * @retval FALSE 双向链表不为空
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListInit
 */
static inline bool SH_ListEmpty(SH_List *list)
{
    if (list == NULL) {          // 参数合法性检查
        return FALSE;            // 非法指针，返回FALSE
    }

    return (bool)(list->pstNext == list);  // 头节点的后向指针指向自身表示链表为空
}

/**
 * @ingroup shell_list
 * @brief 将一个链表插入到另一个链表中
 *
 * @par 描述
 * 该API用于将新链表(newList)整体插入到旧链表(oldList)中，插入位置在oldList的当前位置之后
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致链表损坏</li>
 * </ul>
 *
 * @param oldList    [IN] 目标链表指针
 * @param newList    [IN] 待插入的新链表指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListDelete
 */
static inline void SH_ListAddList(SH_List *oldList, SH_List *newList)
{
    SH_List *oldListHead = oldList->pstNext;  // 保存oldList的原头节点
    SH_List *oldListTail = oldList;           // oldList的当前节点作为尾节点
    SH_List *newListHead = newList;           // newList的头节点
    SH_List *newListTail = newList->pstPrev;  // newList的尾节点

    oldListTail->pstNext = newListHead;       // oldList尾节点的后向指针指向newList头节点
    newListHead->pstPrev = oldListTail;       // newList头节点的前向指针指向oldList尾节点
    oldListHead->pstPrev = newListTail;       // oldList原头节点的前向指针指向newList尾节点
    newListTail->pstNext = oldListHead;       // newList尾节点的后向指针指向oldList原头节点
}

/**
 * @ingroup shell_list
 * @brief 将一个链表插入到另一个链表的尾部
 *
 * @par 描述
 * 该API用于将新链表(newList)整体插入到旧链表(oldList)的尾部
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致链表损坏</li>
 * </ul>
 *
 * @param oldList     [IN] 目标链表指针
 * @param newList     [IN] 待插入的新链表指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListAddList | SH_ListHeadInsertList
 */
static inline void SH_ListTailInsertList(SH_List *oldList, SH_List *newList)
{
    SH_ListAddList(oldList->pstPrev, newList);  // 在oldList的尾节点后插入newList
}

/**
 * @ingroup shell_list
 * @brief 将一个链表插入到另一个链表的头部
 *
 * @par 描述
 * 该API用于将新链表(newList)整体插入到旧链表(oldList)的头部
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致链表损坏</li>
 * </ul>
 *
 * @param oldList     [IN] 目标链表指针
 * @param newList     [IN] 待插入的新链表指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListAddList | SH_ListTailInsertList
 */
static inline void SH_ListHeadInsertList(SH_List *oldList, SH_List *newList)
{
    SH_ListAddList(oldList, newList);  // 在oldList的头节点后插入newList
}

/**
 * @ingroup shell_list
 * @brief 获取结构体中某个成员相对于结构体起始地址的偏移量
 *
 * @par 描述
 * 该宏用于计算结构体中指定成员的偏移量，通常用于从成员指针反推结构体指针
 * @attention
 * <ul>
 * <li>无特殊注意事项</li>
 * </ul>
 *
 * @param type    [IN] 结构体类型名
 * @param member  [IN] 结构体成员名
 *
 * @retval 返回成员相对于结构体起始地址的偏移量(字节数)
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_LIST_ENTRY
 */
#define LOS_OFF_SET_OF(type, member) ((uintptr_t)&((type *)0)->member)

/**
 * @ingroup shell_list
 * @brief 从链表节点指针获取包含该节点的结构体指针
 *
 * @par 描述
 * 该宏用于通过结构体中包含的链表节点成员指针，反推得到整个结构体的指针
 * @attention
 * <ul>
 * <li>确保传入的item指针有效，且member参数正确指定了结构体中的链表成员</li>
 * </ul>
 *
 * @param item    [IN] 链表节点指针
 * @param type    [IN] 结构体类型名
 * @param member  [IN] 结构体中链表成员的名称
 *
 * @retval 返回包含该链表节点的结构体指针
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see LOS_OFF_SET_OF
 */
#define SH_LIST_ENTRY(item, type, member) \
    ((type *)(void *)((char *)(item) - LOS_OFF_SET_OF(type, member)))

/**
 * @ingroup shell_list
 * @brief 遍历包含双向链表的结构体
 *
 * @par 描述
 * 该宏用于遍历包含双向链表节点的结构体数组，通过链表节点将结构体串联起来
 * @attention
 * <ul>
 * <li>遍历时不要修改链表结构，否则可能导致遍历异常</li>
 * </ul>
 *
 * @param item           [IN] 结构体指针变量，用于存储当前遍历到的结构体
 * @param list           [IN] 双向链表头节点指针
 * @param type           [IN] 结构体类型名
 * @param member         [IN] 结构体中链表成员的名称
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_LIST_FOR_EACH_ENTRY_SAFE
 */
#define SH_LIST_FOR_EACH_ENTRY(item, list, type, member)             \
    for (item = SH_LIST_ENTRY((list)->pstNext, type, member);        \
         &(item)->member != (list);                                      \
         item = SH_LIST_ENTRY((item)->member.pstNext, type, member))

/**
 * @ingroup shell_list
 * @brief 安全遍历包含双向链表的结构体(允许遍历中删除节点)
 *
 * @par 描述
 * 该宏用于安全遍历包含双向链表节点的结构体数组，在遍历过程中可以删除节点而不导致遍历异常
 * @attention
 * <ul>
 * <li>需要额外提供一个next变量用于保存下一个节点信息</li>
 * </ul>
 *
 * @param item           [IN] 结构体指针变量，用于存储当前遍历到的结构体
 * @param next           [IN] 结构体指针变量，用于保存下一个结构体节点
 * @param list           [IN] 双向链表头节点指针
 * @param type           [IN] 结构体类型名
 * @param member         [IN] 结构体中链表成员的名称
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_LIST_FOR_EACH_ENTRY
 */
#define SH_LIST_FOR_EACH_ENTRY_SAFE(item, next, list, type, member)               \
    for (item = SH_LIST_ENTRY((list)->pstNext, type, member),                     \
         next = SH_LIST_ENTRY((item)->member.pstNext, type, member);              \
         &(item)->member != (list);                                                   \
         item = next, next = SH_LIST_ENTRY((item)->member.pstNext, type, member))

/**
 * @ingroup shell_list
 * @brief 删除节点并初始化该节点
 *
 * @par 描述
 * 该API用于从双向链表中删除指定节点，并将该节点重新初始化为空链表
 * @attention
 * <ul>
 * <li>传入的参数应确保为合法指针，否则可能导致未定义行为</li>
 * </ul>
 *
 * @param list    [IN] 待删除并初始化的节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此API声明的头文件</li></ul>
 * @see SH_ListDelete | SH_ListInit
 */
static inline void SH_ListDelInit(SH_List *list)
{
    list->pstNext->pstPrev = list->pstPrev;  // 待删除节点的后继节点的前向指针指向待删除节点的前驱节点
    list->pstPrev->pstNext = list->pstNext;  // 待删除节点的前驱节点的后向指针指向待删除节点的后继节点
    SH_ListInit(list);                       // 初始化该节点为新的空链表
}

/**
 * @ingroup shell_list
 * @brief 遍历双向链表节点
 *
 * @par 描述
 * 该宏用于遍历双向链表的所有节点
 * @attention
 * <ul>
 * <li>遍历时不要修改链表结构，否则可能导致遍历异常</li>
 * </ul>
 *
 * @param item           [IN] 链表节点指针变量，用于存储当前遍历到的节点
 * @param list           [IN] 双向链表头节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_LIST_FOR_EACH_SAFE
 */
#define SH_LIST_FOR_EACH(item, list) \
    for (item = (list)->pstNext;         \
         (item) != (list);               \
         item = (item)->pstNext)

/**
 * @ingroup shell_list
 * @brief 安全遍历双向链表节点(允许遍历中删除节点)
 *
 * @par 描述
 * 该宏用于安全遍历双向链表的所有节点，在遍历过程中可以删除节点而不导致遍历异常
 * @attention
 * <ul>
 * <li>需要额外提供一个next变量用于保存下一个节点信息</li>
 * </ul>
 *
 * @param item           [IN] 链表节点指针变量，用于存储当前遍历到的节点
 * @param next           [IN] 链表节点指针变量，用于保存下一个节点
 * @param list           [IN] 双向链表头节点指针
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_LIST_FOR_EACH
 */
#define SH_LIST_FOR_EACH_SAFE(item, next, list)      \
    for (item = (list)->pstNext, next = (item)->pstNext; \
         (item) != (list);                               \
         item = next, next = (item)->pstNext)

/**
 * @ingroup shell_list
 * @brief 定义并初始化一个双向链表头节点
 *
 * @par 描述
 * 该宏用于定义一个双向链表头节点并初始化为空链表
 * @attention
 * <ul>
 * <li>无特殊注意事项</li>
 * </ul>
 *
 * @param list           [IN] 要定义的链表头节点名称
 *
 * @retval None
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_ListInit
 */
#define SH_LIST_HEAD(list) SH_List list = { &(list), &(list) }

/**
 * @ingroup shell_list
 * @brief 获取链表头部的结构体指针
 *
 * @par 描述
 * 该宏用于获取链表头部的第一个结构体元素指针，若链表为空则返回NULL
 * @attention
 * <ul>
 * <li>此宏在do-while(0)循环中实现，确保可以在条件语句中安全使用</li>
 * </ul>
 *
 * @param list           [IN] 双向链表头节点指针
 * @param type           [IN] 结构体类型名
 * @param element        [IN] 结构体中链表成员的名称
 *
 * @retval 返回链表头部的结构体指针，若链表为空则返回NULL
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_ListRemoveHeadType
 */
#define SH_ListPeekHeadType(list, type, element) do {            \
    type *__t;                                                  \
    if ((list)->pstNext == list) {                              \
        __t = NULL;                                             \
    } else {                                                    \
        __t = SH_LIST_ENTRY((list)->pstNext, type, element);    \
    }                                                           \
    __t;                                                        \
} while (0)

/**
 * @ingroup shell_list
 * @brief 移除并获取链表头部的结构体指针
 *
 * @par 描述
 * 该宏用于移除链表头部的第一个结构体元素并返回其指针，若链表为空则返回NULL
 * @attention
 * <ul>
 * <li>此宏在do-while(0)循环中实现，确保可以在条件语句中安全使用</li>
 * </ul>
 *
 * @param list           [IN] 双向链表头节点指针
 * @param type           [IN] 结构体类型名
 * @param element        [IN] 结构体中链表成员的名称
 *
 * @retval 返回被移除的结构体指针，若链表为空则返回NULL
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_ListPeekHeadType
 */
#define SH_ListRemoveHeadType(list, type, element) do {          \
    type *__t;                                                  \
    if ((list)->pstNext == list) {                              \
        __t = NULL;                                             \
    } else {                                                    \
        __t = SH_LIST_ENTRY((list)->pstNext, type, element);    \
        SH_ListDelete((list)->pstNext);                        \
    }                                                           \
    __t;                                                        \
} while (0)

/**
 * @ingroup shell_list
 * @brief 获取当前结构体的下一个结构体指针
 *
 * @par 描述
 * 该宏用于获取当前结构体在链表中的下一个结构体指针，若当前已是最后一个则返回NULL
 * @attention
 * <ul>
 * <li>此宏在do-while(0)循环中实现，确保可以在条件语句中安全使用</li>
 * </ul>
 *
 * @param list           [IN] 双向链表头节点指针
 * @param item           [IN] 当前结构体指针
 * @param type           [IN] 结构体类型名
 * @param element        [IN] 结构体中链表成员的名称
 *
 * @retval 返回下一个结构体指针，若已是最后一个则返回NULL
 * @par 依赖
 * <ul><li>shell_list.h: 包含此宏定义的头文件</li></ul>
 * @see SH_ListPeekHeadType
 */
#define SH_ListNextType(list, item, type, element) do {          \
    type *__t;                                                  \
    if ((item)->pstNext == list) {                              \
        __t = NULL;                                             \
    } else {                                                    \
        __t = SH_LIST_ENTRY((item)->pstNext, type, element);    \
    }                                                           \
    __t;                                                        \
} while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _SHELL_LIST_H */
