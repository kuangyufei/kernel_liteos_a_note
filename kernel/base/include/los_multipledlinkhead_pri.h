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

#ifndef _LOS_MULTIPLE_DLINK_HEAD_PRI_H
#define _LOS_MULTIPLE_DLINK_HEAD_PRI_H

#include "los_base.h"
#include "los_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_MAX_MULTI_DLNK_LOG2  30 	//最大分配内存大小 2^30 = 1G ,LOG2指的是对数的意思
#define OS_MIN_MULTI_DLNK_LOG2  4	//最小分配内存大小 2^4 = 16字节,LOG2指的是对数的意思 例如: log2 16 = 4
#define OS_MULTI_DLNK_NUM       ((OS_MAX_MULTI_DLNK_LOG2 - OS_MIN_MULTI_DLNK_LOG2) + 1)//双向链表数组大小,[4,30],所以必须加1
#define OS_DLNK_HEAD_SIZE       OS_MULTI_DLNK_HEAD_SIZE
#define OS_MULTI_DLNK_HEAD_SIZE sizeof(LosMultipleDlinkHead)
//每个元素是一个双向链表，所有free节点的控制头都会被分类挂在这个数组的双向链表中
typedef struct {//bestfit动态内存管理 第二部分
    LOS_DL_LIST listHead[OS_MULTI_DLNK_NUM];
} LosMultipleDlinkHead;
//返回内存池中,参数链表头节点的下一个链表头节点位置
STATIC INLINE LOS_DL_LIST *OsDLnkNextMultiHead(VOID *headAddr, LOS_DL_LIST *listNodeHead)
{
    LosMultipleDlinkHead *head = (LosMultipleDlinkHead *)headAddr;
	//如果是最后一个了,返回NULL,表示没有下一个了.
    return (&head->listHead[OS_MULTI_DLNK_NUM - 1] == listNodeHead) ? NULL : (listNodeHead + 1);
}

extern VOID OsDLnkInitMultiHead(VOID *headAddr);
extern LOS_DL_LIST *OsDLnkMultiHead(VOID *headAddr, UINT32 size);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_MULTIPLE_DLINK_HEAD_PRI_H */
