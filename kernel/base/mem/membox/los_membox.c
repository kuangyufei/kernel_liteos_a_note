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

/**
 * @brief 检查内存块有效性
 * @details 验证内存块是否位于内存池合法范围内且魔法数字未被篡改
 * @param boxInfo [IN] 内存池信息结构体指针
 * @param node [IN] 待检查的内存块节点指针
 * @return UINT32 - 检查结果
 * @retval LOS_OK 内存块有效
 * @retval LOS_NOK 内存块无效（块大小为0/偏移量不合法/魔法数字错误）
 */
STATIC INLINE UINT32 OsCheckBoxMem(const LOS_MEMBOX_INFO *boxInfo, const VOID *node)
{
    UINT32 offset;  // 内存块相对于内存池数据区起始地址的偏移量

    if (boxInfo->uwBlkSize == 0) {  // 检查块大小是否合法
        return LOS_NOK;
    }

    // 计算内存块相对于内存池数据区起始地址的偏移量（跳过池头）
    offset = (UINT32)((UINTPTR)node - (UINTPTR)(boxInfo + 1));
    if ((offset % boxInfo->uwBlkSize) != 0) {  // 检查偏移量是否为块大小的整数倍
        return LOS_NOK;
    }

    if ((offset / boxInfo->uwBlkSize) >= boxInfo->uwBlkNum) {  // 检查偏移量是否超出最大块索引
        return LOS_NOK;
    }

    return OS_MEMBOX_CHECK_MAGIC(node);  // 检查内存块魔法数字是否有效
}

/**
 * @brief 初始化内存池
 * @details 创建指定大小的内存池，将内存分割为固定大小的块并初始化空闲链表
 * @param pool [IN] 内存池起始地址
 * @param poolSize [IN] 内存池总大小（字节）
 * @param blkSize [IN] 单个内存块大小（字节）
 * @return UINT32 - 初始化结果
 * @retval LOS_OK 初始化成功
 * @retval LOS_NOK 初始化失败（入参无效/内存不足）
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_MemboxInit(VOID *pool, UINT32 poolSize, UINT32 blkSize)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;  // 内存池头部信息指针
    LOS_MEMBOX_NODE *node = NULL;                        // 内存块节点指针
    UINT32 index;                                       // 循环索引
    UINT32 intSave;                                     // 中断状态保存变量

    if (pool == NULL) {  // 检查内存池地址是否有效
        return LOS_NOK;
    }

    if (blkSize == 0) {  // 检查块大小是否合法
        return LOS_NOK;
    }

    if (poolSize < sizeof(LOS_MEMBOX_INFO)) {  // 检查内存池大小是否能容纳头部信息
        return LOS_NOK;
    }

    MEMBOX_LOCK(intSave);  // 进入临界区，保护内存池操作
    // 计算实际块大小（用户请求大小+节点头大小，向上取整到对齐边界）
    boxInfo->uwBlkSize = LOS_MEMBOX_ALIGNED(blkSize + OS_MEMBOX_NODE_HEAD_SIZE);
    // 计算内存池可容纳的总块数（总大小-头部大小，除以块大小）
    boxInfo->uwBlkNum = (poolSize - sizeof(LOS_MEMBOX_INFO)) / boxInfo->uwBlkSize;
    boxInfo->uwBlkCnt = 0;  // 初始化已分配块计数为0
    if (boxInfo->uwBlkNum == 0) {  // 检查是否能分配至少一个块
        MEMBOX_UNLOCK(intSave);  // 退出临界区
        return LOS_NOK;
    }

    node = (LOS_MEMBOX_NODE *)(boxInfo + 1);  // 定位到第一个内存块（跳过内存池头部）

    boxInfo->stFreeList.pstNext = node;  // 空闲链表头指向第一个块

    // 遍历所有块，构建空闲链表
    for (index = 0; index < boxInfo->uwBlkNum - 1; ++index) {
        // 设置当前块的下一个块指针（按块大小偏移）
        node->pstNext = OS_MEMBOX_NEXT(node, boxInfo->uwBlkSize);
        node = node->pstNext;  // 移动到下一个块
    }

    node->pstNext = NULL;  // 最后一个块的下一个指针设为NULL

    MEMBOX_UNLOCK(intSave);  // 退出临界区

    return LOS_OK;
}

/**
 * @brief 从内存池分配内存块
 * @details 从空闲链表中获取第一个可用块，设置魔法数字并更新分配计数
 * @param pool [IN] 内存池起始地址
 * @return VOID* - 分配的内存块用户地址，失败返回NULL
 */
LITE_OS_SEC_TEXT VOID *LOS_MemboxAlloc(VOID *pool)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;  // 内存池头部信息指针
    LOS_MEMBOX_NODE *node = NULL;                        // 空闲链表头指针
    LOS_MEMBOX_NODE *nodeTmp = NULL;                     // 待分配的块节点指针
    UINT32 intSave;                                     // 中断状态保存变量

    if (pool == NULL) {  // 检查内存池地址是否有效
        return NULL;
    }

    MEMBOX_LOCK(intSave);  // 进入临界区，保护内存池操作
    node = &(boxInfo->stFreeList);  // 获取空闲链表头
    if (node->pstNext != NULL) {  // 检查是否有可用块
        nodeTmp = node->pstNext;  // 获取第一个可用块
        node->pstNext = nodeTmp->pstNext;  // 将空闲链表头指向下一个可用块
        OS_MEMBOX_SET_MAGIC(nodeTmp);  // 设置已分配块的魔法数字
        boxInfo->uwBlkCnt++;  // 增加已分配块计数
    }
    MEMBOX_UNLOCK(intSave);  // 退出临界区

    // 返回用户可用地址（跳过节点头），无可用块则返回NULL
    return (nodeTmp == NULL) ? NULL : OS_MEMBOX_USER_ADDR(nodeTmp);
}

/**
 * @brief 释放内存块到内存池
 * @details 验证内存块有效性后，将其重新加入空闲链表并更新分配计数
 * @param pool [IN] 内存池起始地址
 * @param box [IN] 待释放的内存块用户地址
 * @return UINT32 - 释放结果
 * @retval LOS_OK 释放成功
 * @retval LOS_NOK 释放失败（入参无效/内存块无效）
 */
LITE_OS_SEC_TEXT UINT32 LOS_MemboxFree(VOID *pool, VOID *box)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;  // 内存池头部信息指针
    UINT32 ret = LOS_NOK;                               // 函数返回值，默认失败
    UINT32 intSave;                                     // 中断状态保存变量

    if ((pool == NULL) || (box == NULL)) {  // 检查入参是否有效
        return LOS_NOK;
    }

    MEMBOX_LOCK(intSave);  // 进入临界区，保护内存池操作
    do {  // 使用do-while(0)结构实现break退出逻辑
        // 将用户地址转换为节点地址（向前偏移节点头大小）
        LOS_MEMBOX_NODE *node = OS_MEMBOX_NODE_ADDR(box);
        if (OsCheckBoxMem(boxInfo, node) != LOS_OK) {  // 检查内存块有效性
            break;  // 内存块无效，退出循环
        }

        node->pstNext = boxInfo->stFreeList.pstNext;  // 待释放块指向当前空闲链表头
        boxInfo->stFreeList.pstNext = node;  // 空闲链表头指向待释放块（头插法）
        boxInfo->uwBlkCnt--;  // 减少已分配块计数
        ret = LOS_OK;  // 设置释放成功
    } while (0);  // 确保循环体只执行一次
    MEMBOX_UNLOCK(intSave);  // 退出临界区

    return ret;
}

/**
 * @brief 清除内存块内容
 * @details 将内存块的用户数据区域填充为0（不包括节点头）
 * @param pool [IN] 内存池起始地址
 * @param box [IN] 待清除的内存块用户地址
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_MemboxClr(VOID *pool, VOID *box)
{
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;  // 内存池头部信息指针

    if ((pool == NULL) || (box == NULL)) {  // 检查入参是否有效
        return;
    }
    // 使用memset_s清除用户数据区域（块大小-节点头大小）
    (VOID)memset_s(box, (boxInfo->uwBlkSize - OS_MEMBOX_NODE_HEAD_SIZE), 0,
        (boxInfo->uwBlkSize - OS_MEMBOX_NODE_HEAD_SIZE));
}

/**
 * @brief 显示内存池信息
 * @details 打印内存池基本信息、空闲块列表和所有块列表
 * @param pool [IN] 内存池起始地址
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_ShowBox(VOID *pool)
{
    UINT32 index;  // 循环索引
    UINT32 intSave;  // 中断状态保存变量
    LOS_MEMBOX_INFO *boxInfo = (LOS_MEMBOX_INFO *)pool;  // 内存池头部信息指针
    LOS_MEMBOX_NODE *node = NULL;  // 内存块节点指针

    if (pool == NULL) {  // 检查内存池地址是否有效
        return;
    }
    MEMBOX_LOCK(intSave);  // 进入临界区，保护内存池操作
    
    PRINT_INFO("membox(%p,0x%x,0x%x):\r\n", pool, boxInfo->uwBlkSize, boxInfo->uwBlkNum);  // 打印内存池基本信息（地址、块大小、总块数）
    PRINT_INFO("free node list:\r\n");

    for (node = boxInfo->stFreeList.pstNext, index = 0; node != NULL;  // 遍历空闲链表并打印所有空闲块
         node = node->pstNext, ++index) {
        PRINT_INFO("(%u,%p)\r\n", index, node);
    }

    PRINT_INFO("all node list:\r\n");
    node = (LOS_MEMBOX_NODE *)(boxInfo + 1);  // 定位到第一个块
    
    for (index = 0; index < boxInfo->uwBlkNum; ++index, node = OS_MEMBOX_NEXT(node, boxInfo->uwBlkSize)) {  // 遍历所有块并打印块索引、地址和下一个块地址
        PRINT_INFO("(%u,%p,%p)\r\n", index, node, node->pstNext);
    }
    MEMBOX_UNLOCK(intSave);  // 退出临界区
}

/**
 * @brief  获取内存池统计信息
 * @details 该函数用于查询指定内存池的关键统计参数，包括总块数、已使用块数和块大小
 * @param  boxMem   内存池控制块起始地址
 * @param  maxBlk   输出参数，返回内存池总块数
 * @param  blkCnt   输出参数，返回内存池已使用块数
 * @param  blkSize  输出参数，返回单个内存块大小（字节）
 * @return UINT32   操作结果
 *         - LOS_OK: 成功，统计信息已通过输出参数返回
 *         - LOS_NOK: 失败，输入参数无效
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_MemboxStatisticsGet(const VOID *boxMem, UINT32 *maxBlk,
                                                      UINT32 *blkCnt, UINT32 *blkSize)
{
    if ((boxMem == NULL) || (maxBlk == NULL) || (blkCnt == NULL) || (blkSize == NULL)) {  // 参数合法性检查：内存池指针和所有输出参数指针均不可为NULL
        return LOS_NOK;  // 参数无效，返回错误码
    }

    *maxBlk = ((OS_MEMBOX_S *)boxMem)->uwBlkNum;  // 获取内存池总块数：转换为内存池控制块结构体并读取uwBlkNum成员
    *blkCnt = ((OS_MEMBOX_S *)boxMem)->uwBlkCnt;  // 获取内存池已使用块数：读取uwBlkCnt成员（注意：uwBlkCnt通常表示已分配块数）
    *blkSize = ((OS_MEMBOX_S *)boxMem)->uwBlkSize;  // 获取单个内存块大小：读取uwBlkSize成员（单位：字节）

    return LOS_OK;  // 统计信息获取成功
}

