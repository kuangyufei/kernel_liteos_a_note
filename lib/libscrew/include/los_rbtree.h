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

/* *
 * @defgroup los_rbtree Rbtree
 * @ingroup kernel
 */

#ifndef _LOS_RBTREE_H
#define _LOS_RBTREE_H

#include "los_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* 红黑树节点颜色定义 */
#define LOS_RB_RED 0          // 红色节点标记  
#define LOS_RB_BLACK 1        // 黑色节点标记  

/* 红黑树节点结构体定义 */
typedef struct TagRbNode {
    struct TagRbNode *pstParent;  // 父节点指针  
    struct TagRbNode *pstRight;   // 右子节点指针  
    struct TagRbNode *pstLeft;    // 左子节点指针  
    ULONG_T lColor;               // 节点颜色，取值为LOS_RB_RED或LOS_RB_BLACK  
} LosRbNode;  

/* 红黑树比较函数指针类型定义 */
typedef ULONG_T (*pfRBCmpKeyFn)(const VOID *, const VOID *);  // 键比较函数，返回RB_EQUAL/RB_BIGGER/RB_SMALLER
/* 红黑树节点释放函数指针类型定义 */
typedef ULONG_T (*pfRBFreeFn)(LosRbNode *);                   // 节点释放函数，返回释放结果
/* 红黑树键获取函数指针类型定义 */
typedef VOID *(*pfRBGetKeyFn)(LosRbNode *);                   // 从节点获取键值的函数

/* 红黑树结构体定义 */
typedef struct TagRbTree {
    LosRbNode *pstRoot;          // 根节点指针  
    LosRbNode stNilT;            // 哨兵节点，代表空节点  
    LOS_DL_LIST stWalkHead;      // 遍历链表头  
    ULONG_T ulNodes;             // 树中节点总数  

    pfRBCmpKeyFn pfCmpKey;       // 键比较函数指针  
    pfRBFreeFn pfFree;           // 节点释放函数指针  
    pfRBGetKeyFn pfGetKey;       // 键获取函数指针  
} LosRbTree;  

/* 红黑树遍历结构体定义 */
typedef struct TagRbWalk {
    LOS_DL_LIST stLink;          // 遍历链表节点  
    LosRbNode *pstCurrNode;      // 当前遍历节点  
    struct TagRbTree *pstTree;   // 关联的红黑树  
} LosRbWalk;  

/* 比较结果宏定义 */
#define RB_EQUAL (0)             // 键值相等  
#define RB_BIGGER (1)            // 前者大于后者  
#define RB_SMALLER (2)           // 前者小于后者  

/* 红黑树遍历宏 - 基础遍历 */
#define RB_SCAN(pstTree, pstNode) do {                                     \
        (pstNode) = LOS_RbFirstNode((pstTree)); /* 获取树的第一个节点 */     \
        /* 从第一个节点开始遍历，直到节点为空 */                         
        for (; NULL != (pstNode); (pstNode) = LOS_RbSuccessorNode((pstTree), (pstNode))) {

#define RB_SCAN_END(pstTree, pstNode) }                                 \
    }                                 \
    while (0);  /* 遍历结束宏，与RB_SCAN配对使用 */

/* 红黑树遍历宏 - 安全遍历（支持遍历中删除节点） */
#define RB_SCAN_SAFE(pstTree, pstNode, pstNodeTemp) do {                                                        \
    (pstNode) = LOS_RbFirstNode((pstTree));                    /* 获取树的第一个节点 */                          \
    (pstNodeTemp) = LOS_RbSuccessorNode((pstTree), (pstNode)); /* 预存下一个节点，防止当前节点被删除后丢失遍历位置 */ \
    /* 遍历循环，每次使用预存的下一个节点更新当前节点 */                                                        
    for (; NULL != (pstNode); (pstNode) = (pstNodeTemp), (pstNodeTemp) = LOS_RbSuccessorNode((pstTree), (pstNode))) {

#define RB_SCAN_SAFE_END(pstTree, pstNode, pstNodeTemp) }                                                   \
    }                                                   \
    while (0);  /* 安全遍历结束宏，与RB_SCAN_SAFE配对使用 */

/* 红黑树遍历宏 - 中间遍历（需外部初始化起始节点） */
#define RB_MID_SCAN(pstTree, pstNode) do {                              \
        /* 从指定节点开始遍历，直到节点为空 */                           
        for (; NULL != (pstNode); (pstNode) = LOS_RbSuccessorNode((pstTree), (pstNode))) {

#define RB_MID_SCAN_END(pstTree, pstNode) }                                     \
    }                                     \
    while (0);  /* 中间遍历结束宏，与RB_MID_SCAN配对使用 */

/* 红黑树遍历宏 - 基于Walk结构体的遍历 */
#define RB_WALK(pstTree, pstNode, pstRbWalk) do {                                     \
        LosRbWalk *(pstRbWalk) = NULL;       /* 声明遍历控制结构体指针 */              \
        pstRbWalk = LOS_RbCreateWalk(pstTree);  /* 创建遍历控制结构体 */                \
        (pstNode) = LOS_RbWalkNext(pstRbWalk);  /* 获取第一个遍历节点 */                \
        /* 使用遍历控制结构体进行安全遍历 */                                        
        for (; NULL != (pstNode); (pstNode) = LOS_RbWalkNext(pstRbWalk)) {

#define RB_WALK_END(pstTree, pstNode, pstRbWalk) }                                            \
    LOS_RbDeleteWalk(pstRbWalk);                    /* 销毁遍历控制结构体，释放资源 */    \
    }                                            \
    while (0);  /* Walk遍历结束宏，与RB_WALK配对使用 */

#define RB_WALK_TERMINATE(pstRbWalk) LOS_RbDeleteWalk(pstRbWalk);  /* 终止遍历并释放遍历结构体 */
#define RB_COUNT(pstTree) ((pstTree)->ulNodes)                     /* 获取树的节点总数 */
/* 判断节点是否为非哨兵节点（哨兵节点左右子节点均为NULL） */
#define RB_IS_NOT_NILT(pstX) ((NULL != (pstX)->pstLeft) && (NULL != (pstX)->pstRight))  


VOID *LOS_RbFirstNode(LosRbTree *pstTree);
VOID *LOS_RbSuccessorNode(LosRbTree *pstTree, VOID *pstData);
VOID LOS_RbInitTree(LosRbTree *pstTree, pfRBCmpKeyFn pfCmpKey, pfRBFreeFn pfFree, pfRBGetKeyFn pfGetKey);
VOID LOS_RbDestroyTree(LosRbTree *pstTree);
LosRbNode *LOS_RbGetNextNode(LosRbTree *pstTree, VOID *pKey);
ULONG_T LOS_RbGetNode(LosRbTree *pstTree, VOID *pKey, LosRbNode **ppstNode);
VOID LOS_RbDelNode(LosRbTree *pstTree, LosRbNode *pstNode);
ULONG_T LOS_RbAddNode(LosRbTree *pstTree, LosRbNode *pstNew);

/* Following 3 functions support protection walk. */
LosRbWalk *LOS_RbCreateWalk(LosRbTree *pstTree);
VOID *LOS_RbWalkNext(LosRbWalk *pstWalk);
VOID LOS_RbDeleteWalk(LosRbWalk *pstWalk);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_RBTREE_H */
