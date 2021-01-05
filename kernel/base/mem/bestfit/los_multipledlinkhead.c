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

#include "los_multipledlinkhead_pri.h"
#include "los_bitmap.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//获取size的最高位, 意思就是将size变成2^return 对数表示
STATIC INLINE UINT32 OsLog2(UINT32 size)
{
    return (size > 0) ? (UINT32)LOS_HighBitGet(size) : 0;
}
//初始化内存池双向链表 ，默认OS_MULTI_DLNK_NUM      = 27个
LITE_OS_SEC_TEXT_INIT VOID OsDLnkInitMultiHead(VOID *headAddr)
{
    LosMultipleDlinkHead *dlinkHead = (LosMultipleDlinkHead *)headAddr;
    LOS_DL_LIST *listNodeHead = dlinkHead->listHead;//拿到双向链表数组
    UINT32 index;

    for (index = 0; index < OS_MULTI_DLNK_NUM; ++index, ++listNodeHead) {//遍历链表数组
        LOS_ListInit(listNodeHead);//初始化链表
    }
}
//找到内存池和size相匹配的链表头节点,读懂函数之前一定要充分理解LosMultipleDlinkHead中存放的是一个双向链表数组
LITE_OS_SEC_TEXT_MINOR LOS_DL_LIST *OsDLnkMultiHead(VOID *headAddr, UINT32 size)
{
    LosMultipleDlinkHead *dlinkHead = (LosMultipleDlinkHead *)headAddr;
    UINT32 index = OsLog2(size);
    if (index > OS_MAX_MULTI_DLNK_LOG2) {//换算后的对数值大于上限
        return NULL;
    } else if (index <= OS_MIN_MULTI_DLNK_LOG2) {//换算后的对数值小于下限
        index = OS_MIN_MULTI_DLNK_LOG2;//意思是如果申请小于16个字节内存,将分配16个字节的内存
    }
	//注者会将下句写成 return dlinkHead->listHead[index - OS_MIN_MULTI_DLNK_LOG2];
    return dlinkHead->listHead + (index - OS_MIN_MULTI_DLNK_LOG2);//@note_good 这里快速的定位到对应内存块的头节点处
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
