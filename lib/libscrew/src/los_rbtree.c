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

#include "los_rbtree.h"
#include "los_memory.h"


STATIC VOID OsRbLeftRotateNode(LosRbTree *pstTree, LosRbNode *pstX);
STATIC VOID OsRbRightRotateNode(LosRbTree *pstTree, LosRbNode *pstY);
STATIC VOID OsRbInsertNodeFixup(LosRbTree *pstTree, VOID *pstData);
STATIC VOID OsRbDeleteNodeFixup(LosRbTree *pstTree, LosRbNode *pstNode);
STATIC VOID OsRbDeleteNode(LosRbTree *pstTree, VOID *pstData);
STATIC VOID OsRbInitTree(LosRbTree *pstTree);
STATIC VOID OsRbClearTree(LosRbTree *pstTree);
/**
 * @brief 红黑树左旋转操作
 * @details 将节点pstX的右子树pstY提升为新的父节点，pstX成为pstY的左子节点，pstY原来的左子树成为pstX的右子树
 * @param pstTree 红黑树控制块指针
 * @param pstX 待旋转的节点指针
 */
STATIC VOID OsRbLeftRotateNode(LosRbTree *pstTree, LosRbNode *pstX)
{
    LosRbNode *pstY = NULL;          // pstX的右子节点（旋转后的新父节点）
    LosRbNode *pstNilT = NULL;       // 红黑树的哨兵节点（代表NULL）
    LosRbNode *pstParent = NULL;     // 保存哨兵节点的父节点（用于旋转后恢复）

    if (pstTree == NULL || pstX == NULL) {  // 参数合法性检查：树或节点为空则直接返回
        return;  
    }
    pstNilT = &(pstTree->stNilT);  // 获取哨兵节点
    /* 特殊处理：若旋转涉及哨兵节点，需保存其原始父节点
     * 原因：旋转可能意外修改哨兵节点的父指针，导致后续删除修复时访问错误
     * 解决方案：旋转前记录父节点，操作完成后恢复
     */
    pstParent = pstNilT->pstParent;  
    pstY = pstX->pstRight;           // pstY指向pstX的右子节点
    pstX->pstRight = pstY->pstLeft;  // 将pstY的左子树设为pstX的新右子树
    pstY->pstLeft->pstParent = pstX;  // 更新pstY左子树的父指针为pstX
    pstY->pstParent = pstX->pstParent;  // 将pstY的父指针指向pstX原来的父节点
    if (pstNilT == pstX->pstParent) {  // 若pstX是根节点
        pstTree->pstRoot = pstY;  // 更新根节点为pstY
    } else {  // 若pstX不是根节点
        if (pstX == pstX->pstParent->pstLeft) {  // pstX是左子节点
            pstX->pstParent->pstLeft = pstY;  // 将pstY设为原父节点的左子节点
        } else {  // pstX是右子节点
            pstX->pstParent->pstRight = pstY;  // 将pstY设为原父节点的右子节点
        }  
    }  
    pstX->pstParent = pstY;  // 将pstX的父指针指向pstY
    pstY->pstLeft = pstX;    // 将pstX设为pstY的左子节点
    pstNilT->pstParent = pstParent;  // 恢复哨兵节点的父指针
    return;  
}

/**
 * @brief 红黑树右旋转操作
 * @details 将节点pstY的左子树pstX提升为新的父节点，pstY成为pstX的右子节点，pstX原来的右子树成为pstY的左子树
 * @param pstTree 红黑树控制块指针
 * @param pstY 待旋转的节点指针
 */
STATIC VOID OsRbRightRotateNode(LosRbTree *pstTree, LosRbNode *pstY)
{
    LosRbNode *pstX = NULL;          // pstY的左子节点（旋转后的新父节点）
    LosRbNode *pstNilT = NULL;       // 红黑树的哨兵节点
    LosRbNode *pstParent = NULL;     // 保存哨兵节点的父节点
    if (NULL == pstTree || NULL == pstY) {  // 参数合法性检查
        return;  
    }

    pstNilT = &(pstTree->stNilT);  // 获取哨兵节点
    /* 哨兵节点父指针保护机制
     * 与左旋转逻辑相同，防止旋转操作修改哨兵节点的父指针
     */
    pstParent = pstNilT->pstParent;  
    pstX = pstY->pstLeft;           // pstX指向pstY的左子节点
    pstY->pstLeft = pstX->pstRight;  // 将pstX的右子树设为pstY的新左子树
    pstX->pstRight->pstParent = pstY;  // 更新pstX右子树的父指针为pstY
    pstX->pstParent = pstY->pstParent;  // 将pstX的父指针指向pstY原来的父节点
    if (pstNilT == pstY->pstParent) {  // 若pstY是根节点
        pstTree->pstRoot = pstX;  // 更新根节点为pstX
    } else {  // 若pstY不是根节点
        if (pstY == pstY->pstParent->pstRight) {  // pstY是右子节点
            pstY->pstParent->pstRight = pstX;  // 将pstX设为原父节点的右子节点
        } else {  // pstY是左子节点
            pstY->pstParent->pstLeft = pstX;  // 将pstX设为原父节点的左子节点
        }  
    }  
    pstY->pstParent = pstX;  // 将pstY的父指针指向pstX
    pstX->pstRight = pstY;    // 将pstY设为pstX的右子节点
    pstNilT->pstParent = pstParent;  // 恢复哨兵节点的父指针
    return;  
}

/**
 * @brief 红黑树插入节点后的平衡修复
 * @details 通过 recoloring 和旋转操作，恢复插入新节点后可能被破坏的红黑树性质
 * @param pstTree 红黑树控制块指针
 * @param pstData 新插入的节点指针（已被染为红色）
 */
STATIC VOID OsRbInsertNodeFixup(LosRbTree *pstTree, VOID *pstData)
{
    LosRbNode *pstParent = NULL;     // 当前节点的父节点
    LosRbNode *pstGParent = NULL;    // 当前节点的祖父节点
    LosRbNode *pstY = NULL;          // 当前节点的叔叔节点
    LosRbNode *pstX = NULL;          // 当前处理的节点

    if ((NULL == pstTree) || (NULL == pstData)) {  // 参数合法性检查
        return;  
    }

    pstX = (LosRbNode *)pstData;  // 类型转换：将通用指针转为红黑树节点指针
    /* NilT is forbidden. */  // 禁止插入哨兵节点（断言）
    (pstTree->ulNodes)++;  // 红黑树节点计数加1
    // 循环条件：当前节点的父节点为红色（违反性质4：红色节点的子节点必须是黑色）
    while (LOS_RB_RED == pstX->pstParent->lColor) {  
        pstParent = pstX->pstParent;       // 获取父节点
        pstGParent = pstParent->pstParent;  // 获取祖父节点（必然存在，因根节点为黑色）

        if (pstParent == pstGParent->pstLeft) {  // 父节点是祖父节点的左子节点（左子树情况）
            pstY = pstGParent->pstRight;        // 叔叔节点是祖父节点的右子节点
            if (LOS_RB_RED == pstY->lColor) {   // 情况1：叔叔节点为红色
                pstY->lColor = LOS_RB_BLACK;     // 叔叔节点染为黑色
                pstParent->lColor = LOS_RB_BLACK;  // 父节点染为黑色
                pstGParent->lColor = LOS_RB_RED;  // 祖父节点染为红色
                pstX = pstGParent;                // 当前节点上移到祖父节点，继续检查
                continue;  // 进入下一轮循环
            }

            if (pstParent->pstRight == pstX) {  // 情况2：叔叔节点为黑色，且当前节点是右子节点
                pstX = pstParent;               // 当前节点上移到父节点
                OsRbLeftRotateNode(pstTree, pstX);  // 对新当前节点执行左旋转，转为情况3
            }

            // 情况3：叔叔节点为黑色，且当前节点是左子节点
            pstX->pstParent->lColor = LOS_RB_BLACK;  // 父节点染为黑色
            pstGParent->lColor = LOS_RB_RED;         // 祖父节点染为红色
            OsRbRightRotateNode(pstTree, pstGParent);  // 对祖父节点执行右旋转
        } else {  // 父节点是祖父节点的右子节点（右子树情况，与左子树对称）
            pstY = pstGParent->pstLeft;         // 叔叔节点是祖父节点的左子节点
            if (LOS_RB_RED == pstY->lColor) {   // 情况1：叔叔节点为红色
                pstY->lColor = LOS_RB_BLACK;     // 叔叔节点染为黑色
                pstParent->lColor = LOS_RB_BLACK;  // 父节点染为黑色
                pstGParent->lColor = LOS_RB_RED;  // 祖父节点染为红色
                pstX = pstGParent;                // 当前节点上移到祖父节点
                continue;  // 进入下一轮循环
            }

            if (pstParent->pstLeft == pstX) {   // 情况2：叔叔节点为黑色，且当前节点是左子节点
                pstX = pstParent;               // 当前节点上移到父节点
                OsRbRightRotateNode(pstTree, pstX);  // 对新当前节点执行右旋转，转为情况3
            }

            // 情况3：叔叔节点为黑色，且当前节点是右子节点
            pstX->pstParent->lColor = LOS_RB_BLACK;  // 父节点染为黑色
            pstGParent->lColor = LOS_RB_RED;         // 祖父节点染为红色
            OsRbLeftRotateNode(pstTree, pstGParent);  // 对祖父节点执行左旋转
        }  
    }  

    pstTree->pstRoot->lColor = LOS_RB_BLACK;  // 确保根节点为黑色（性质2）

    return;  
}

/**
 * @brief 红黑树删除节点后的平衡修复
 * @details 处理删除操作后可能出现的"双黑"节点问题，通过旋转和 recoloring 恢复红黑树性质
 * @param pstTree 红黑树控制块指针
 * @param pstNode 被删除节点的子节点（可能需要修复的节点）
 */
STATIC VOID OsRbDeleteNodeFixup(LosRbTree *pstTree, LosRbNode *pstNode)
{
    LosRbNode *pstW = NULL;  // 当前节点的兄弟节点

    if (NULL == pstTree || NULL == pstNode) {  // 参数合法性检查
        return;  
    }
    // 循环条件：当前节点不是根节点且为黑色（存在双黑问题）
    while ((pstNode != pstTree->pstRoot) && (LOS_RB_BLACK == pstNode->lColor)) {  
        if (pstNode->pstParent->pstLeft == pstNode) {  // 当前节点是父节点的左子节点
            pstW = pstNode->pstParent->pstRight;       // 兄弟节点是父节点的右子节点
            if (LOS_RB_RED == pstW->lColor) {          // 情况1：兄弟节点为红色
                pstW->lColor = LOS_RB_BLACK;           // 兄弟节点染为黑色
                pstNode->pstParent->lColor = LOS_RB_RED;  // 父节点染为红色
                OsRbLeftRotateNode(pstTree, pstNode->pstParent);  // 对父节点左旋转
                pstW = pstNode->pstParent->pstRight;    // 更新兄弟节点
            }

            // 情况2：兄弟节点为黑色，且两个子节点均为黑色
            if ((LOS_RB_BLACK == pstW->pstLeft->lColor) && (LOS_RB_BLACK == pstW->pstRight->lColor)) {  
                pstW->lColor = LOS_RB_RED;  // 兄弟节点染为红色
                pstNode = pstNode->pstParent;  // 当前节点上移到父节点
            } else {  // 兄弟节点为黑色，且至少一个子节点为红色
                if (LOS_RB_BLACK == pstW->pstRight->lColor) {  // 情况3：兄弟节点右子节点为黑色
                    pstW->pstLeft->lColor = LOS_RB_BLACK;      // 兄弟节点左子节点染为黑色
                    pstW->lColor = LOS_RB_RED;                  // 兄弟节点染为红色
                    OsRbRightRotateNode(pstTree, pstW);         // 对兄弟节点右旋转
                    pstW = pstNode->pstParent->pstRight;        // 更新兄弟节点
                }
                // 情况4：兄弟节点右子节点为红色
                pstW->lColor = pstNode->pstParent->lColor;  // 兄弟节点颜色设为父节点颜色
                pstNode->pstParent->lColor = LOS_RB_BLACK;  // 父节点染为黑色
                pstW->pstRight->lColor = LOS_RB_BLACK;      // 兄弟节点右子节点染为黑色
                OsRbLeftRotateNode(pstTree, pstNode->pstParent);  // 对父节点左旋转
                pstNode = pstTree->pstRoot;  // 当前节点设为根节点，退出循环
            }  
        } else {  // 当前节点是父节点的右子节点（与左子节点情况对称）
            pstW = pstNode->pstParent->pstLeft;  // 兄弟节点是父节点的左子节点
            if (LOS_RB_RED == pstW->lColor) {    // 情况1：兄弟节点为红色
                pstW->lColor = LOS_RB_BLACK;     // 兄弟节点染为黑色
                pstNode->pstParent->lColor = LOS_RB_RED;  // 父节点染为红色
                OsRbRightRotateNode(pstTree, pstNode->pstParent);  // 对父节点右旋转
                pstW = pstNode->pstParent->pstLeft;  // 更新兄弟节点
            }
            // 情况2：兄弟节点为黑色，且两个子节点均为黑色
            if ((LOS_RB_BLACK == pstW->pstLeft->lColor) && (LOS_RB_BLACK == pstW->pstRight->lColor)) {  
                pstW->lColor = LOS_RB_RED;  // 兄弟节点染为红色
                pstNode = pstNode->pstParent;  // 当前节点上移到父节点
            } else {  // 兄弟节点为黑色，且至少一个子节点为红色
                if (LOS_RB_BLACK == pstW->pstLeft->lColor) {  // 情况3：兄弟节点左子节点为黑色
                    pstW->pstRight->lColor = LOS_RB_BLACK;     // 兄弟节点右子节点染为黑色
                    pstW->lColor = LOS_RB_RED;                  // 兄弟节点染为红色
                    OsRbLeftRotateNode(pstTree, pstW);          // 对兄弟节点左旋转
                    pstW = pstNode->pstParent->pstLeft;         // 更新兄弟节点
                }
                // 情况4：兄弟节点左子节点为红色
                pstW->lColor = pstNode->pstParent->lColor;  // 兄弟节点颜色设为父节点颜色
                pstNode->pstParent->lColor = LOS_RB_BLACK;  // 父节点染为黑色
                pstW->pstLeft->lColor = LOS_RB_BLACK;       // 兄弟节点左子节点染为黑色
                OsRbRightRotateNode(pstTree, pstNode->pstParent);  // 对父节点右旋转
                pstNode = pstTree->pstRoot;  // 当前节点设为根节点，退出循环
            }  
        }  
    }  
    pstNode->lColor = LOS_RB_BLACK;  // 将当前节点染为黑色（解决双黑问题）
    if (0 == pstTree->ulNodes) {  // 若树为空（节点数为0）
        OsRbClearTree(pstTree);  // 清空红黑树（重置根节点和哨兵节点）
    }

    return;  
}

/**
 * @brief 红黑树节点删除操作
 * @details 实现红黑树节点的删除功能，包括找到后继节点、调整子树、调用删除修复等完整流程
 * @param pstTree 红黑树控制块指针
 * @param pstData 待删除的节点指针
 */
STATIC VOID OsRbDeleteNode(LosRbTree *pstTree, VOID *pstData)
{
    LosRbNode *pstChild = NULL;    // 被删除节点的子节点
    LosRbNode *pstDel = NULL;      // 实际被删除的节点（可能是后继节点）
    ULONG_T lColor;                // 被删除节点的原始颜色
    LosRbWalk *pstWalk = NULL;     // 遍历器指针（用于更新遍历状态）
    LosRbNode *pstNilT = NULL;     // 哨兵节点
    LosRbNode *pstZ = NULL;        // 待删除的节点
    LOS_DL_LIST *pstNode = NULL;   // 双向链表节点（用于遍历器链表）

    if ((NULL == pstTree) || (NULL == pstData)) {  // 参数合法性检查
        return;  
    }

    pstZ = (LosRbNode *)pstData;  // 类型转换：将通用指针转为红黑树节点指针
    pstNilT = &(pstTree->stNilT);  // 获取哨兵节点

    /* NilT is forbidden. */  // 禁止删除哨兵节点
    if (!RB_IS_NOT_NILT(pstZ)) {  // 检查是否为哨兵节点（宏定义判断）
        return;  
    }

    /* 检查节点是否属于当前红黑树
     * 条件：若节点的父节点、左右子节点均为哨兵节点，且不是根节点，则不属于该树
     */
    if ((pstZ->pstParent == pstNilT) && (pstZ->pstLeft == pstNilT) && (pstZ->pstRight == pstNilT) &&
        (pstTree->pstRoot != pstZ)) {  
        return;  
    }

    (pstTree->ulNodes)--;  // 红黑树节点计数减1

    // 如果存在遍历器，更新遍历器状态（避免遍历器指向已删除节点）
    if (!LOS_ListEmpty(&pstTree->stWalkHead)) {  // 遍历器链表非空
        LOS_DL_LIST_FOR_EACH(pstNode, &pstTree->stWalkHead) {  // 遍历所有遍历器
            pstWalk = LOS_DL_LIST_ENTRY(pstNode, LosRbWalk, stLink);  // 获取遍历器
            if (pstWalk->pstCurrNode == pstZ) {  // 若遍历器当前节点指向被删除节点
                // 更新遍历器当前节点为被删除节点的后继节点
                pstWalk->pstCurrNode = LOS_RbSuccessorNode(pstTree, pstZ);  
            }  
        }  
    }

    // 情况A：被删除节点有0个或1个子节点
    if ((pstNilT == pstZ->pstLeft) || (pstNilT == pstZ->pstRight)) {  
        // 确定唯一子节点（左子节点为空则取右子节点，反之亦然）
        pstChild = ((pstNilT != pstZ->pstLeft) ? pstZ->pstLeft : pstZ->pstRight);  
        if (NULL == pstChild) { /* Edit by r60958 for Coverity */  // Coverity静态检查修复：防止空指针
            return;  
        }

        pstChild->pstParent = pstZ->pstParent;  // 子节点的父指针指向被删除节点的父节点

        if (pstNilT == pstZ->pstParent) {  // 被删除节点是根节点
            pstTree->pstRoot = pstChild;   // 更新根节点为子节点
        } else {  // 被删除节点不是根节点
            if (pstZ->pstParent->pstLeft == pstZ) {  // 被删除节点是左子节点
                pstZ->pstParent->pstLeft = pstChild;  // 父节点的左指针指向子节点
            } else {  // 被删除节点是右子节点
                pstZ->pstParent->pstRight = pstChild;  // 父节点的右指针指向子节点
            }  
        }

        if (LOS_RB_BLACK == pstZ->lColor) {  // 若被删除节点是黑色（可能导致双黑问题）
            OsRbDeleteNodeFixup(pstTree, pstChild);  // 调用删除修复函数
        }

        /* re-initialize the pstZ */  // 重置被删除节点（便于后续复用）
        pstZ->lColor = LOS_RB_RED;  // 颜色设为红色
        pstZ->pstLeft = pstZ->pstRight = pstZ->pstParent = pstNilT;  // 指针均指向哨兵节点

        return;  
    }

    /* 情况B：被删除节点有两个子节点
     * 与《算法导论》不同，本实现直接删除目标节点而非后继节点，因数据结构无内部节点
     * 故代码逻辑更复杂：需将后继节点内容复制到目标节点位置，再删除后继节点
     */
    pstDel = pstZ;  // 保存原始待删除节点（目标节点）

    /* 获取pstZ的后继节点（中序遍历的下一个节点）
     * 后继节点是右子树中最左的节点（最小值节点）
     */
    pstZ = pstZ->pstRight;  // 进入右子树
    while (pstNilT != pstZ->pstLeft) {  // 循环找到最左节点
        pstZ = pstZ->pstLeft;  
    }

    /* Because left is nilT, so child must be right. */  // 后继节点的左子节点必为哨兵，故子节点只能是右子节点
    pstChild = pstZ->pstRight;  // 后继节点的右子节点
    if (NULL == pstChild) { /* Edit by r60958 for Coverity */  // Coverity静态检查修复
        return;  
    }

    lColor = pstZ->lColor;  // 保存后继节点的原始颜色

    /* Remove successor node out of tree. */  // 将后继节点从原位置移除
    pstChild->pstParent = pstZ->pstParent;  // 后继节点的子节点指向后继节点的父节点

    if (pstNilT == pstZ->pstParent) {  // 后继节点是根节点（实际不可能，因后继节点是右子树最左节点）
        /* In fact, we never go here. */  // 调试注释：此分支实际不会执行
        pstTree->pstRoot = pstChild;  // 更新根节点
    } else {  // 后继节点不是根节点
        if (pstZ->pstParent->pstLeft == pstZ) {  // 后继节点是左子节点
            pstZ->pstParent->pstLeft = pstChild;  // 父节点左指针指向后继节点的子节点
        } else {  // 后继节点是右子节点
            pstZ->pstParent->pstRight = pstChild;  // 父节点右指针指向后继节点的子节点
        }  
    }

    /* Insert successor node into tree and remove pstZ out of tree. */  // 将后继节点插入到目标节点位置
    pstZ->pstParent = pstDel->pstParent;  // 后继节点的父指针指向目标节点的父节点
    pstZ->lColor = pstDel->lColor;        // 继承目标节点的颜色
    pstZ->pstRight = pstDel->pstRight;    // 后继节点的右指针指向目标节点的右子树
    pstZ->pstLeft = pstDel->pstLeft;      // 后继节点的左指针指向目标节点的左子树

    if (pstNilT == pstDel->pstParent) {  // 目标节点是根节点
        /* if we arrive here, pstTree is no NULL */  // 调试注释：树非空
        pstTree->pstRoot = pstZ;  // 更新根节点为后继节点
    } else {  // 目标节点不是根节点
        if (pstDel->pstParent->pstLeft == pstDel) {  // 目标节点是左子节点
            pstDel->pstParent->pstLeft = pstZ;  // 目标节点的父节点左指针指向后继节点
        } else {  // 目标节点是右子节点
            pstDel->pstParent->pstRight = pstZ;  // 目标节点的父节点右指针指向后继节点
        }  
    }

    pstDel->pstLeft->pstParent = pstZ;  // 目标节点的左子树父指针指向后继节点
    pstDel->pstRight->pstParent = pstZ;  // 目标节点的右子树父指针指向后继节点

    if (LOS_RB_BLACK == lColor) {  // 若被删除的后继节点是黑色
        OsRbDeleteNodeFixup(pstTree, pstChild);  // 调用删除修复函数
    }

    /* re-initialize the pstDel */  // 重置原始待删除节点（目标节点）
    pstDel->lColor = LOS_RB_RED;  // 颜色设为红色
    pstDel->pstLeft = pstDel->pstRight = pstDel->pstParent = pstNilT;  // 指针均指向哨兵节点
    return;  
}

/**
 * @brief 初始化红黑树结构
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @note 初始化树节点计数、根节点、哨兵节点属性及遍历链表头
 */
STATIC VOID OsRbInitTree(LosRbTree *pstTree)
{
    if (NULL == pstTree) {  // 检查树指针有效性
        return;
    }

    pstTree->ulNodes = 0;   // 初始化节点计数为0
    pstTree->pstRoot = &(pstTree->stNilT);  // 根节点初始指向哨兵节点
    pstTree->stNilT.lColor = LOS_RB_BLACK;  // 哨兵节点设为黑色
    pstTree->stNilT.pstLeft = NULL;   /* 哨兵左孩子始终为NULL */
    pstTree->stNilT.pstRight = NULL;  /* 哨兵右孩子始终为NULL */
    pstTree->stNilT.pstParent = NULL; /* 树非空时哨兵父节点不为NULL */
    LOS_ListInit(&pstTree->stWalkHead);  // 初始化遍历链表头

    pstTree->pfCmpKey = NULL;  // 比较函数指针初始化为NULL
    pstTree->pfFree = NULL;    // 释放函数指针初始化为NULL
    pstTree->pfGetKey = NULL;  // 获取键值函数指针初始化为NULL

    return;
}

/**
 * @brief 清除红黑树的遍历结构
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @note 遍历并释放所有遍历器，重置遍历状态
 */
STATIC VOID OsRbClearTree(LosRbTree *pstTree)
{
    LosRbWalk *pstWalk = NULL;
    LOS_DL_LIST *pstNode = NULL;

    if (NULL == pstTree) {  // 检查树指针有效性
        return;
    }

    pstNode = LOS_DL_LIST_FIRST(&pstTree->stWalkHead);  // 获取遍历链表头节点
    // 遍历所有遍历器节点
    while (!LOS_DL_LIST_IS_END(&pstTree->stWalkHead, pstNode)) {
        pstWalk = LOS_DL_LIST_ENTRY(pstNode, LosRbWalk, stLink);  // 将链表节点转换为遍历器指针
        pstWalk->pstCurrNode = NULL;  // 重置当前节点指针
        pstWalk->pstTree = NULL;      // 重置树指针

        LOS_ListDelete(pstNode);  // 从链表删除当前遍历器
        pstNode = LOS_DL_LIST_FIRST(&pstTree->stWalkHead);  // 获取下一个节点
    }

    return;
}

/**
 * @brief 创建红黑树遍历器
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @return 成功返回遍历器指针，失败返回NULL
 * @note 遍历器用于按序访问树节点，需配合LOS_RbWalkNext使用
 */
LosRbWalk *LOS_RbCreateWalk(LosRbTree *pstTree)
{
    LosRbNode *pstNode = NULL;
    LosRbWalk *pstWalk = NULL;

    if (NULL == pstTree) {  // 检查树指针有效性
        return NULL;
    }

    pstNode = LOS_RbFirstNode(pstTree);  // 获取树的第一个节点（最左节点）
    if (NULL == pstNode) {  // 树为空则无法创建遍历器
        return NULL;
    }

    // 分配遍历器内存
    pstWalk = (LosRbWalk *)LOS_MemAlloc(m_aucSysMem0, sizeof(LosRbWalk));
    if (NULL == pstWalk) {  // 内存分配失败检查
        return NULL;
    }

    LOS_ListAdd(&pstTree->stWalkHead, &pstWalk->stLink);  // 将遍历器添加到树的遍历链表
    pstWalk->pstCurrNode = pstNode;  // 初始化当前节点为树的第一个节点
    pstWalk->pstTree = pstTree;      // 关联遍历器与树
    return pstWalk;
}

/**
 * @brief 获取遍历器的下一个节点
 * @param pstWalk [IN] 指向红黑树遍历器的指针
 * @return 成功返回当前节点指针，遍历结束返回NULL
 * @note 每次调用后自动更新遍历器的当前节点
 */
VOID *LOS_RbWalkNext(LosRbWalk *pstWalk)
{
    LosRbNode *pstNode = NULL;

    /*
     * 此处为历史版本修复：原代码未检查pstCurrNode和pstTree
     * 在调用RB_ClearTree删除树时会将这两个指针置NULL
     * 若此时正在遍历（如遍历groups和ports）则需避免空指针访问
     */
    // 检查遍历器及关联的节点和树是否有效
    if ((NULL == pstWalk) || (NULL == pstWalk->pstCurrNode) || (NULL == pstWalk->pstTree)) {
        return NULL;
    }

    pstNode = pstWalk->pstCurrNode;  // 保存当前节点
    // 更新当前节点为后继节点
    pstWalk->pstCurrNode = LOS_RbSuccessorNode(pstWalk->pstTree, pstNode);
    return pstNode;  // 返回当前节点
}

/**
 * @brief 删除红黑树遍历器
 * @param pstWalk [IN] 指向红黑树遍历器的指针
 * @note 从遍历链表移除并释放遍历器内存，避免内存泄漏
 */
VOID LOS_RbDeleteWalk(LosRbWalk *pstWalk)
{
    if (NULL == pstWalk) {  // 检查遍历器指针有效性
        return;
    }

    // 如果遍历器仍在链表中，则从链表删除
    if (LOS_DL_LIST_IS_ON_QUEUE(&pstWalk->stLink)) {
        LOS_ListDelete(&pstWalk->stLink);
    }
    pstWalk->pstCurrNode = NULL;  // 重置当前节点指针
    pstWalk->pstTree = NULL;      // 重置树指针
    LOS_MemFree(m_aucSysMem0, pstWalk);  // 释放遍历器内存

    return;
}

/**
 * @brief 插入新节点到红黑树的处理函数
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @param pstParent [IN] 新节点的父节点
 * @param pstNew [IN] 待插入的新节点
 * @note 负责设置新节点属性、确定插入位置并调用修复函数维持红黑树性质
 */
VOID LOS_RbInsertOneNodeProcess(LosRbTree *pstTree, LosRbNode *pstParent, LosRbNode *pstNew)
{
    LosRbNode *pstNilT = &pstTree->stNilT;  // 获取哨兵节点
    VOID *pNodeKey = NULL;                  // 新节点的键值
    VOID *pKey = NULL;                      // 父节点的键值
    ULONG_T ulCmpResult;                    // 键值比较结果

    pstNew->lColor = LOS_RB_RED;  // 新节点默认为红色（红黑树插入规则）
    pstNew->pstLeft = pstNew->pstRight = pstNilT;  // 左右孩子指向哨兵
    if ((pstNew->pstParent = pstParent) == pstNilT) {  // 父节点为哨兵表示树为空
        pstTree->pstRoot = pstNew;  // 新节点成为根节点
    } else {
        pNodeKey = pstTree->pfGetKey(pstNew);  // 获取新节点键值
        pKey = pstTree->pfGetKey(pstParent);   // 获取父节点键值
        ulCmpResult = pstTree->pfCmpKey(pNodeKey, pKey);  // 比较键值
        if (RB_SMALLER == ulCmpResult) {  // 新节点键值小于父节点，插入左子树
            pstParent->pstLeft = pstNew;
        } else {  // 新节点键值大于等于父节点，插入右子树
            pstParent->pstRight = pstNew;
        }
    }

    OsRbInsertNodeFixup(pstTree, pstNew);  // 插入后修复红黑树性质

    return;
}

/**
 * @brief 带函数指针初始化的红黑树初始化函数
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @param pfCmpKey [IN] 键值比较函数指针
 * @param pfFree [IN] 节点释放函数指针
 * @param pfGetKey [IN] 键值获取函数指针
 * @note 初始化树结构并设置核心操作函数，是红黑树使用前的必要步骤
 */
VOID LOS_RbInitTree(LosRbTree *pstTree, pfRBCmpKeyFn pfCmpKey, pfRBFreeFn pfFree, pfRBGetKeyFn pfGetKey)
{
    if (NULL == pstTree) {  // 检查树指针有效性
        return;
    }

    OsRbInitTree(pstTree);  // 调用基础初始化函数

    pstTree->pfCmpKey = pfCmpKey;  // 设置键值比较函数
    pstTree->pfFree = pfFree;      // 设置节点释放函数
    pstTree->pfGetKey = pfGetKey;  // 设置键值获取函数

    return;
}

/**
 * @brief 销毁红黑树并释放所有节点
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @note 需确保树已设置pfFree函数，否则无法释放节点内存
 */
VOID LOS_RbDestroyTree(LosRbTree *pstTree)
{
    LosRbNode *pstNode = NULL;

    if (NULL == pstTree) {  // 检查树指针有效性
        return;
    }
    if (NULL == pstTree->pfFree) {  // 未设置释放函数则无法销毁
        return;
    }

    // 使用RB_WALK宏遍历树中所有节点
    RB_WALK(pstTree, pstNode, pstWalk)
    {
        OsRbDeleteNode(pstTree, pstNode);  // 从树中删除节点
        (VOID)pstTree->pfFree(pstNode);    // 调用释放函数释放节点内存
    }
    RB_WALK_END(pstTree, pstNode, pstWalk);

    OsRbClearTree(pstTree);  // 清除树的遍历结构

    return;
}

/**
 * @brief 获取红黑树的第一个节点（最小键值节点）
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @return 成功返回第一个节点指针，树为空返回NULL
 * @note 红黑树的第一个节点是最左叶子节点
 */
VOID *LOS_RbFirstNode(LosRbTree *pstTree)
{
    LosRbNode *pstNode = NULL;
    LosRbNode *pstNilT = NULL;

    if (NULL == pstTree) {  // 检查树指针有效性
        return NULL;
    }

    pstNode = pstTree->pstRoot;  // 从根节点开始查找
    if (pstNode == NULL) {       // 根节点为空直接返回
        return NULL;
    }
    pstNilT = &(pstTree->stNilT);  // 获取哨兵节点

    if (pstNilT == pstNode) {  // 根节点为哨兵表示树为空
        return NULL;
    }

    // 遍历至最左节点（最小键值节点）
    while (pstNilT != pstNode->pstLeft) {
        pstNode = pstNode->pstLeft;
    }

    return pstNode;
}

/**
 * @brief 获取指定节点的后继节点
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @param pstData [IN] 指向当前节点的指针
 * @return 成功返回后继节点指针，无后继返回NULL
 * @note 后继节点是中序遍历中的下一个节点，即键值大于当前节点的最小节点
 */
VOID *LOS_RbSuccessorNode(LosRbTree *pstTree, VOID *pstData)
{
    LosRbNode *pstY = NULL;      // 父节点临时变量
    LosRbNode *pstNilT = NULL;   // 哨兵节点
    LosRbNode *pstNode = NULL;   // 当前节点

    if (NULL == pstTree) {  // 检查树指针有效性
        return NULL;
    }

    pstNode = (LosRbNode *)pstData;  // 转换节点类型

    if (NULL == pstNode) {  // 检查节点指针有效性
        return NULL;
    }

    /* 禁止传入哨兵节点 */
    if (!RB_IS_NOT_NILT(pstNode)) {
        return NULL;
    }

    /* 若执行到此处，pstTree不为NULL */
    pstNilT = &(pstTree->stNilT);  // 获取哨兵节点

    if (pstNilT != pstNode->pstRight) {  // 右子树不为空
        pstNode = pstNode->pstRight;     // 转到右子树
        // 查找右子树最左节点（右子树最小节点）
        while (pstNilT != pstNode->pstLeft) {
            pstNode = pstNode->pstLeft;
        }

        return pstNode;
    }

    pstY = pstNode->pstParent;  // 右子树为空，向上查找父节点
    // 当当前节点是父节点的右孩子时，继续向上追溯
    while ((pstNilT != pstY) && (pstNode == pstY->pstRight)) {
        pstNode = pstY;
        pstY = pstY->pstParent;
    }

    return ((pstNilT == pstY) ? NULL : pstY);  // 父节点为哨兵表示无后继
}

/**
 * @brief 获取大于指定键值的第一个节点
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @param pKey [IN] 目标键值
 * @return 成功返回节点指针，无匹配返回NULL
 * @note 用于查找大于等于目标键值的最小节点
 */
LosRbNode *LOS_RbGetNextNode(LosRbTree *pstTree, VOID *pKey)
{
    LosRbNode *pNode = NULL;

    // 先尝试精确查找键值
    if (TRUE == LOS_RbGetNode(pstTree, pKey, &pNode)) {
        return LOS_RbSuccessorNode(pstTree, pNode);  // 找到则返回其后继
    } else if ((NULL == pNode) || (&pstTree->stNilT == pNode)) {  // 未找到且无候选节点
        return NULL;
    } else if (RB_BIGGER == pstTree->pfCmpKey(pKey, pstTree->pfGetKey(pNode))) {  // 键值大于候选节点
        // 遍历后继节点直到找到第一个大于目标键值的节点
        while (NULL != pNode) {
            pNode = LOS_RbSuccessorNode(pstTree, pNode);
            if (NULL == pNode) {
                return NULL;
            }

            if (RB_SMALLER == pstTree->pfCmpKey(pKey, pstTree->pfGetKey(pNode))) {
                break;
            }
        }
        return pNode;
    } else {  // 键值小于候选节点，直接返回候选节点
        return pNode;
    }
}

/**
 * @brief 在红黑树中查找指定键值的节点
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @param pKey [IN] 要查找的键值
 * @param ppstNode [OUT] 输出参数，返回找到的节点或最后比较的节点
 * @return 找到返回TRUE，未找到返回FALSE
 * @note 未找到时返回最后比较的节点，可用于插入位置确定
 */
ULONG_T LOS_RbGetNode(LosRbTree *pstTree, VOID *pKey, LosRbNode **ppstNode)
{
    LosRbNode *pstNilT = NULL;   // 哨兵节点
    LosRbNode *pstX = NULL;      // 当前遍历节点
    LosRbNode *pstY = NULL;      // 父节点/候选节点
    VOID *pNodeKey = NULL;       // 当前节点键值
    ULONG_T ulCmpResult;         // 比较结果

    // 检查输入参数有效性
    if ((NULL == pstTree) || (NULL == pKey) || (NULL == ppstNode)) {
        if (NULL != ppstNode) {
            *ppstNode = NULL;
        }
        return FALSE;
    }
    // 检查函数指针有效性
    if ((NULL == pstTree->pfGetKey) || (NULL == pstTree->pfCmpKey)) {
        *ppstNode = NULL;
        return FALSE;
    }

    pstNilT = &pstTree->stNilT;  // 获取哨兵节点
    pstY = pstTree->pstRoot;     // 从根节点开始查找
    pstX = pstY;

    // 遍历树查找目标键值
    while (pstX != pstNilT) {
        pNodeKey = pstTree->pfGetKey(pstX);  // 获取当前节点键值

        ulCmpResult = pstTree->pfCmpKey(pKey, pNodeKey);  // 比较键值
        if (RB_EQUAL == ulCmpResult) {  // 找到匹配节点
            *ppstNode = pstX;
            return TRUE;
        }
        if (RB_SMALLER == ulCmpResult) {  // 目标键值较小，向左子树查找
            pstY = pstX;
            pstX = pstX->pstLeft;
        } else {  // 目标键值较大，向右子树查找
            pstY = pstX;
            pstX = pstX->pstRight;
        }
    }

    *ppstNode = pstY;  // 返回最后比较的节点（插入候选节点）
    return FALSE;
}

/**
 * @brief 从红黑树中删除指定节点
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @param pstNode [IN] 要删除的节点
 * @note 实际删除操作由OsRbDeleteNode完成，本函数为封装接口
 */
VOID LOS_RbDelNode(LosRbTree *pstTree, LosRbNode *pstNode)
{
    OsRbDeleteNode(pstTree, pstNode);  // 调用内部删除函数
}

/**
 * @brief 向红黑树添加新节点
 * @param pstTree [IN] 指向红黑树结构体的指针
 * @param pstNew [IN] 待添加的新节点
 * @return 添加成功返回TRUE，失败返回FALSE
 * @note 包含重复键值检查，确保树中键值唯一
 */
ULONG_T LOS_RbAddNode(LosRbTree *pstTree, LosRbNode *pstNew)
{
    ULONG_T ulSearchNode;        // 查找结果
    VOID *pNodeKey = NULL;       // 新节点键值
    LosRbNode *pstX = NULL;      // 插入位置的父节点

    // 检查树和新节点指针有效性
    if ((NULL == pstTree) || (NULL == pstNew)) {
        return FALSE;
    }
    // 检查必要的函数指针
    if ((NULL == pstTree->pfGetKey) || (NULL == pstTree->pfCmpKey)) {
        return FALSE;
    }

    pNodeKey = pstTree->pfGetKey(pstNew);  // 获取新节点键值
    // 查找键值是否已存在
    ulSearchNode = LOS_RbGetNode(pstTree, pNodeKey, &pstX);
    if (TRUE == ulSearchNode) {  // 键值已存在，返回失败
        return FALSE;
    }

    if (NULL == pstX) {  // 未找到插入位置
        return FALSE;
    }

    LOS_RbInsertOneNodeProcess(pstTree, pstX, pstNew);  // 执行插入处理

    return TRUE;
}
