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

#define LOS_RB_RED 0
#define LOS_RB_BLACK 1

typedef struct TagRbNode {//节点
    struct TagRbNode *pstParent; ///< 爸爸是谁 ?
    struct TagRbNode *pstRight;  ///< 右孩子是谁 ?
    struct TagRbNode *pstLeft;	 ///< 左孩子是谁 ?
    ULONG_T lColor; //是红还是黑节点
} LosRbNode;

typedef ULONG_T (*pfRBCmpKeyFn)(const VOID *, const VOID *);
typedef ULONG_T (*pfRBFreeFn)(LosRbNode *);
typedef VOID *(*pfRBGetKeyFn)(LosRbNode *);

typedef struct TagRbTree {//红黑树控制块
    LosRbNode *pstRoot;//根节点
    LosRbNode stNilT;//叶子节点
    LOS_DL_LIST stWalkHead;
    ULONG_T ulNodes;

    pfRBCmpKeyFn pfCmpKey; //比较大小处理函数
    pfRBFreeFn pfFree;	//释放结点处理函数
    pfRBGetKeyFn pfGetKey;//获取节点内容处理函数
} LosRbTree;
///记录步数
typedef struct TagRbWalk {
    LOS_DL_LIST stLink;
    LosRbNode *pstCurrNode; //当前节点
    struct TagRbTree *pstTree;//红黑树
} LosRbWalk;
// 常用于两个线性区的虚拟地址的大小和范围
#define RB_EQUAL (0) //相等
#define RB_BIGGER (1) //更大
#define RB_SMALLER (2) //更小

#define RB_SCAN(pstTree, pstNode) do {                                     \
        (pstNode) = LOS_RbFirstNode((pstTree)); \
        for (; NULL != (pstNode); (pstNode) = LOS_RbSuccessorNode((pstTree), (pstNode))) {

#define RB_SCAN_END(pstTree, pstNode) }                                 \
    }                                 \
    while (0);

#define RB_SCAN_SAFE(pstTree, pstNode, pstNodeTemp) do {                                                        \
    (pstNode) = LOS_RbFirstNode((pstTree));                    \
    (pstNodeTemp) = LOS_RbSuccessorNode((pstTree), (pstNode)); \
    for (; NULL != (pstNode); (pstNode) = (pstNodeTemp), (pstNodeTemp) = LOS_RbSuccessorNode((pstTree), (pstNode))) {

#define RB_SCAN_SAFE_END(pstTree, pstNode, pstNodeTemp) }                                                   \
    }                                                   \
    while (0);

#define RB_MID_SCAN(pstTree, pstNode) do {                              \
        for (; NULL != (pstNode); (pstNode) = LOS_RbSuccessorNode((pstTree), (pstNode))) {

#define RB_MID_SCAN_END(pstTree, pstNode) }                                     \
    }                                     \
    while (0);

#define RB_WALK(pstTree, pstNode, pstRbWalk) do {                                     \
        LosRbWalk *(pstRbWalk) = NULL;       \
        pstRbWalk = LOS_RbCreateWalk(pstTree);  \
        (pstNode) = LOS_RbWalkNext(pstRbWalk);  \
        for (; NULL != (pstNode); (pstNode) = LOS_RbWalkNext(pstRbWalk)) {

#define RB_WALK_END(pstTree, pstNode, pstRbWalk) }                                            \
    LOS_RbDeleteWalk(pstRbWalk);                    \
    }                                            \
    while (0);

#define RB_WALK_TERMINATE(pstRbWalk) LOS_RbDeleteWalk(pstRbWalk);
#define RB_COUNT(pstTree) ((pstTree)->ulNodes)
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
