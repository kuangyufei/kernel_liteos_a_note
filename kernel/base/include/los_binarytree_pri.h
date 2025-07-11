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

#ifndef _LOS_BINARYTREE_PRI_H
#define _LOS_BINARYTREE_PRI_H

#include "los_typedef.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_binarytree
 * @brief 二叉树节点基类
 * @details 所有二叉树节点的基础结构，包含左右子树指针、节点ID和柔性数组键值
 */
typedef struct tagBinNode {
    struct tagBinNode *left;    ///< 左子树节点指针
    struct tagBinNode *right;   ///< 右子树节点指针
    UINT32 nodeID;              ///< 节点唯一标识ID
    CHAR keyValue[0];           ///< 柔性数组，存储节点键值（长度动态分配）
} BinNode;

/**
 * @ingroup los_binarytree
 * @brief 链接寄存器二叉树节点
 * @details 继承自BinNode，用于存储链接寄存器相关信息的二叉树节点
 */
typedef struct {
    BinNode leaf;               ///< 二叉树节点基类成员
    UINTPTR linkReg1;           ///< 链接寄存器1的值
    UINTPTR linkReg2;           ///< 链接寄存器2的值
    UINTPTR linkReg3;           ///< 链接寄存器3的值
} LinkRegNode;

/**
 * @ingroup los_binarytree
 * @brief 链接寄存器节点数量上限
 * @note 十进制值为4096，表示系统最多支持4096个链接寄存器节点
 */
#define LR_COUNT 4096

/**
 * @ingroup los_binarytree
 * @brief 链接寄存器节点数组
 * @note 存储所有链接寄存器节点的全局数组，大小为LR_COUNT
 */
extern LinkRegNode g_linkRegNode[LR_COUNT];

/**
 * @ingroup los_binarytree
 * @brief 链接寄存器节点索引
 * @note 记录当前已使用的链接寄存器节点数量，用于节点分配
 */
extern UINT32 g_linkRegNodeIndex;

/**
 * @ingroup los_binarytree
 * @brief 链接寄存器二叉树的根节点
 * @note 指向链接寄存器二叉树的根节点，用于树的遍历和操作
 */
extern LinkRegNode *g_linkRegRoot;

/**
 * @ingroup los_binarytree
 * @brief 地址信息二叉树节点
 * @details 继承自BinNode，用于存储地址相关信息的二叉树节点
 */
typedef struct {
    BinNode leaf;               ///< 二叉树节点基类成员
    UINTPTR addr;               ///< 存储的地址值
} AddrNode;

/**
 * @ingroup los_binarytree
 * @brief 地址节点数量上限
 * @note 十进制值为40960，表示系统最多支持40960个地址节点
 */
#define ADDR_COUNT 40960

/**
 * @ingroup los_binarytree
 * @brief 地址节点数组
 * @note 存储所有地址节点的全局数组，大小为ADDR_COUNT
 */
extern AddrNode g_addrNode[ADDR_COUNT];

/**
 * @ingroup los_binarytree
 * @brief 地址节点索引
 * @note 记录当前已使用的地址节点数量，用于节点分配
 */
extern UINT32 g_addrNodeIndex;

/**
 * @ingroup los_binarytree
 * @brief 地址二叉树的根节点
 * @note 指向地址二叉树的根节点，用于树的遍历和操作
 */
extern AddrNode *g_addrRoot;

/**
 * @ingroup los_binarytree
 * @brief 请求大小二叉树节点
 * @details 继承自BinNode，用于存储请求大小相关信息的二叉树节点
 */
typedef struct {
    BinNode leaf;               ///< 二叉树节点基类成员
    UINT32 reqSize;             ///< 请求大小值
} ReqSizeNode;

/**
 * @ingroup los_binarytree
 * @brief 请求大小节点数量上限
 * @note 十进制值为4096，表示系统最多支持4096个请求大小节点
 */
#define REQ_SIZE_COUNT 4096

/**
 * @ingroup los_binarytree
 * @brief 请求大小节点数组
 * @note 存储所有请求大小节点的全局数组，大小为REQ_SIZE_COUNT
 */
extern ReqSizeNode g_reqSizeNode[REQ_SIZE_COUNT];

/**
 * @ingroup los_binarytree
 * @brief 请求大小节点索引
 * @note 记录当前已使用的请求大小节点数量，用于节点分配
 */
extern UINT32 g_reqSizeNodeIndex;

/**
 * @ingroup los_binarytree
 * @brief 请求大小二叉树的根节点
 * @note 指向请求大小二叉树的根节点，用于树的遍历和操作
 */
extern ReqSizeNode *g_reqSizeRoot;

/**
 * @ingroup los_binarytree
 * @brief 任务ID二叉树节点
 * @details 继承自BinNode，用于存储任务ID相关信息的二叉树节点
 */
typedef struct {
    BinNode leaf;               ///< 二叉树节点基类成员
    UINT32 taskID;              ///< 任务ID
} TaskIDNode;

/**
 * @ingroup los_binarytree
 * @brief 任务ID节点数量上限
 * @note 十进制值为1024，表示系统最多支持1024个任务ID节点
 */
#define TASK_ID_COUNT 1024

extern UINT32 OsBinTreeInsert(const VOID *node, UINT32 nodeLen, BinNode **leaf,
                              BinNode *(*GetMyBinNode)(UINT32 *nodeID),
                              INT32 (*CompareNode)(const VOID *node1, const VOID *node2));

extern INT32 OsCompareLRNode(const VOID *node1, const VOID *node2);
extern BinNode *OsGetLRBinNode(UINT32 *nodeID);

extern INT32 OsCompareAddrNode(const VOID *node1, const VOID *node2);
extern BinNode *OsGetAddrBinNode(UINT32 *nodeID);

extern INT32 OsCompareReqSizeNode(const VOID *node1, const VOID *node2);
extern BinNode *OsGetReqSizeBinNode(UINT32 *nodeID);

extern INT32 OsCompareTaskIDNode(const VOID *node1, const VOID *node2);
extern BinNode *OsGetTaskIDBinNode(UINT32 *nodeID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_BINARYTREE_PRI_H */
