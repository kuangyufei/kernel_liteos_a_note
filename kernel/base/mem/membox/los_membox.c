/*!
 * @file  los_membox.c
 * @brief 静态内存池主文件
 * @link
   @verbatim
   
   使用场景
	   当用户需要使用固定长度的内存时，可以通过静态内存分配的方式获取内存，一旦使用完毕，
	   通过静态内存释放函数归还所占用内存，使之可以重复使用。
   
   开发流程
	   通过make menuconfig配置静态内存管理模块。
	   规划一片内存区域作为静态内存池。
	   调用LOS_MemboxInit初始化静态内存池。
	   初始化会将入参指定的内存区域分割为N块（N值取决于静态内存总大小和块大小），将所有内存块挂到空闲链表，在内存起始处放置控制头。
	   调用LOS_MemboxAlloc接口分配静态内存。
	   系统将会从空闲链表中获取第一个空闲块，并返回该内存块的起始地址。
	   调用LOS_MemboxClr接口。将入参地址对应的内存块清零。
	   调用LOS_MemboxFree接口。将该内存块加入空闲链表。
   
   注意事项
	   静态内存池区域，如果是通过动态内存分配方式获得的，在不需要静态内存池时，
	   需要释放该段内存，避免发生内存泄露。
	   静态内存不常用,因为需要使用者去确保不会超出使用范围

   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2022-04-02
 */

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

#include "los_membox.h"
#include "los_hwi.h"
#include "los_spinlock.h"

#ifdef LOSCFG_AARCH64
#define OS_MEMBOX_MAGIC 0xa55a5aa5a55a5aa5 //魔法数字,@note_good 点赞,设计的很精巧,node内容从下一个节点地址变成魔法数字
#else
#define OS_MEMBOX_MAGIC 0xa55a5aa5 
#endif
#define OS_MEMBOX_SET_MAGIC(addr) \
    ((LOS_MEMBOX_NODE *)(addr))->pstNext = (LOS_MEMBOX_NODE *)OS_MEMBOX_MAGIC //设置魔法数字
#define OS_MEMBOX_CHECK_MAGIC(addr) \
    ((((LOS_MEMBOX_NODE *)(addr))->pstNext == (LOS_MEMBOX_NODE *)OS_MEMBOX_MAGIC) ? LOS_OK : LOS_NOK)

#define OS_MEMBOX_USER_ADDR(addr) \
    ((VOID *)((UINT8 *)(addr) + OS_MEMBOX_NODE_HEAD_SIZE)) //@note_good 使用之前要去掉节点信息,太赞了! 很艺术化!!
#define OS_MEMBOX_NODE_ADDR(addr) \
    ((LOS_MEMBOX_NODE *)(VOID *)((UINT8 *)(addr) - OS_MEMBOX_NODE_HEAD_SIZE)) //节块 = (节头 + 节体) addr = 节体
/* spinlock for mem module, only available on SMP mode */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_memboxSpin);
#define MEMBOX_LOCK(state)       LOS_SpinLockSave(&g_memboxSpin, &(state)) ///< 获取静态内存池自旋锁
#define MEMBOX_UNLOCK(state)     LOS_SpinUnlockRestore(&g_memboxSpin, (state))///< 释放静态内存池自旋锁
/// 检查静态内存块
STATIC INLINE UINT32 OsCheckBoxMem(const LOS_MEMBOX_INFO *boxInfo, const VOID *node)
{
    UINT32 offset;

    if (boxInfo->uwBlkSize == 0) {
        return LOS_NOK;
    }

    offset = (UINT32)((UINTPTR)node - (UINTPTR)(boxInfo + 1));
    if ((offset % boxInfo->uwBlkSize) != 0) {
        return LOS_NOK;
    }

    if ((offset / boxInfo->uwBlkSize) >= boxInfo->uwBlkNum) {
        return LOS_NOK;
    }

    return OS_MEMBOX_CHECK_MAGIC(node);//检查魔法数字是否被修改过了
}
/// 初始化一个静态内存池，根据入参设定其起始地址、总大小及每个内存块大小
LITE_OS_SEC_TEXT_INIT UINT32 LOS_MemboxInit(VOID *pool, UINT32 poolSize, UINT32 blkSize)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;//在内存起始处安置池头
    LOS_MEMBOX_NODE *node = NULL;
    UINT32 index;
    UINT32 intSave;

    if (pool == NULL) {
        return LOS_NOK;
    }

    if (blkSize == 0) {
        return LOS_NOK;
    }

    if (poolSize < sizeof(LOS_MEMBOX_INFO)) {
        return LOS_NOK;
    }

    MEMBOX_LOCK(intSave);
    boxInfo->uwBlkSize = LOS_MEMBOX_ALIGNED(blkSize + OS_MEMBOX_NODE_HEAD_SIZE); //节块总大小(节头+节体)
    boxInfo->uwBlkNum = (poolSize - sizeof(LOS_MEMBOX_INFO)) / boxInfo->uwBlkSize;//总节块数量
    boxInfo->uwBlkCnt = 0;	//已分配的数量
    if (boxInfo->uwBlkNum == 0) {//只有0块的情况
        MEMBOX_UNLOCK(intSave);
        return LOS_NOK;
    }

    node = (LOS_MEMBOX_NODE *)(boxInfo + 1);//去除池头,找到第一个节块位置

    boxInfo->stFreeList.pstNext = node;//池头空闲链表指向第一个节块

    for (index = 0; index < boxInfo->uwBlkNum - 1; ++index) {//切割节块,挂入空闲链表
        node->pstNext = OS_MEMBOX_NEXT(node, boxInfo->uwBlkSize);//按块大小切割好,统一由pstNext指向
        node = node->pstNext;//node存储了下一个节点的地址信息
    }

    node->pstNext = NULL;//最后一个为null

    MEMBOX_UNLOCK(intSave);

    return LOS_OK;
}
/// 从指定的静态内存池中申请一块静态内存块,整个内核源码只有 OsSwtmrScan中用到了静态内存.
LITE_OS_SEC_TEXT VOID *LOS_MemboxAlloc(VOID *pool)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;
    LOS_MEMBOX_NODE *node = NULL;
    LOS_MEMBOX_NODE *nodeTmp = NULL;
    UINT32 intSave;

    if (pool == NULL) {
        return NULL;
    }

    MEMBOX_LOCK(intSave);
    node = &(boxInfo->stFreeList);//拿到空闲单链表
    if (node->pstNext != NULL) {//不需要遍历链表,因为这是空闲链表
        nodeTmp = node->pstNext;//先记录要使用的节点
        node->pstNext = nodeTmp->pstNext;//不再空闲了,把节点摘出去了.
        OS_MEMBOX_SET_MAGIC(nodeTmp);//为已使用的节块设置魔法数字
        boxInfo->uwBlkCnt++;//已使用块数增加
    }
    MEMBOX_UNLOCK(intSave);

    return (nodeTmp == NULL) ? NULL : OS_MEMBOX_USER_ADDR(nodeTmp);//返回可用的虚拟地址
}
/// 释放指定的一块静态内存块
LITE_OS_SEC_TEXT UINT32 LOS_MemboxFree(VOID *pool, VOID *box)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;
    UINT32 ret = LOS_NOK;
    UINT32 intSave;

    if ((pool == NULL) || (box == NULL)) {
        return LOS_NOK;
    }

    MEMBOX_LOCK(intSave);
    do {
        LOS_MEMBOX_NODE *node = OS_MEMBOX_NODE_ADDR(box);//通过节体获取节块首地址
        if (OsCheckBoxMem(boxInfo, node) != LOS_OK) {
            break;
        }

        node->pstNext = boxInfo->stFreeList.pstNext;//节块指向空闲链表表头
        boxInfo->stFreeList.pstNext = node;//空闲链表表头反指向它,意味节块排到第一,下次申请将首个分配它
        boxInfo->uwBlkCnt--;//已经使用的内存块减一
        ret = LOS_OK;
    } while (0);//将被编译时优化
    MEMBOX_UNLOCK(intSave);

    return ret;
}
/// 清零指定静态内存块的内容
LITE_OS_SEC_TEXT_MINOR VOID LOS_MemboxClr(VOID *pool, VOID *box)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;

    if ((pool == NULL) || (box == NULL)) {
        return;
    }
	//将魔法数字一并清除了.
    (VOID)memset_s(box, (boxInfo->uwBlkSize - OS_MEMBOX_NODE_HEAD_SIZE), 0,
        (boxInfo->uwBlkSize - OS_MEMBOX_NODE_HEAD_SIZE));
}
/// 打印指定静态内存池所有节点信息（打印等级是LOS_INFO_LEVEL），包括内存池起始地址、
/// 内存块大小、总内存块数量、每个空闲内存块的起始地址、所有内存块的起始地址
LITE_OS_SEC_TEXT_MINOR VOID LOS_ShowBox(VOID *pool)
{
    UINT32 index;
    UINT32 intSave;
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;
    LOS_MEMBOX_NODE *node = NULL;

    if (pool == NULL) {
        return;
    }
    MEMBOX_LOCK(intSave);
    PRINT_INFO("membox(%p,0x%x,0x%x):\r\n", pool, boxInfo->uwBlkSize, boxInfo->uwBlkNum);
    PRINT_INFO("free node list:\r\n");

    for (node = boxInfo->stFreeList.pstNext, index = 0; node != NULL;
         node = node->pstNext, ++index) {
        PRINT_INFO("(%u,%p)\r\n", index, node);
    }

    PRINT_INFO("all node list:\r\n");
    node = (LOS_MEMBOX_NODE *)(boxInfo + 1);
    for (index = 0; index < boxInfo->uwBlkNum; ++index, node = OS_MEMBOX_NEXT(node, boxInfo->uwBlkSize)) {
        PRINT_INFO("(%u,%p,%p)\r\n", index, node, node->pstNext);
    }
    MEMBOX_UNLOCK(intSave);
}
/// 获取指定静态内存池的信息，包括内存池中总内存块数量、已经分配出去的内存块数量、每个内存块的大小
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemboxStatisticsGet(const VOID *boxMem, UINT32 *maxBlk,
                                                      UINT32 *blkCnt, UINT32 *blkSize)
{
    if ((boxMem == NULL) || (maxBlk == NULL) || (blkCnt == NULL) || (blkSize == NULL)) {
        return LOS_NOK;
    }

    *maxBlk = ((OS_MEMBOX_S *)boxMem)->uwBlkNum;
    *blkCnt = ((OS_MEMBOX_S *)boxMem)->uwBlkCnt;
    *blkSize = ((OS_MEMBOX_S *)boxMem)->uwBlkSize;

    return LOS_OK;
}

