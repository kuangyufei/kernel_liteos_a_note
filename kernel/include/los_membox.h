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
 * @defgroup los_membox Static memory
 * @ingroup kernel
 */

#ifndef _LOS_MEMBOX_H
#define _LOS_MEMBOX_H

#include "los_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_membox
 * @brief 获取内存池中下一个空闲节点的地址
 * @param addr 当前节点地址
 * @param blkSize 内存块大小（字节）
 * @return LOS_MEMBOX_NODE* 下一个空闲节点的指针
 * @note 通过当前节点地址加上块大小计算下一个节点位置，实现链表遍历
 */
#define OS_MEMBOX_NEXT(addr, blkSize) (LOS_MEMBOX_NODE *)(VOID *)((UINT8 *)(addr) + (blkSize))

/**
 * @ingroup los_membox
 * @brief 内存池空闲节点头部大小
 * @note 等于LOS_MEMBOX_NODE结构体的大小，即单个指针的大小（32位系统4字节，64位系统8字节）
 */
#define OS_MEMBOX_NODE_HEAD_SIZE sizeof(LOS_MEMBOX_NODE)

/**
 * @ingroup los_membox
 * @brief 内存池空闲节点结构体
 * @note 用于维护内存池中的空闲块链表
 */
typedef struct tagMEMBOX_NODE {
    struct tagMEMBOX_NODE *pstNext; /**< 指向下一个空闲节点的指针，构成单向链表 */
} LOS_MEMBOX_NODE;

/**
 * @ingroup los_membox
 * @brief 内存池信息结构体
 * @note 管理整个内存池的元数据和空闲块列表
 */
typedef struct {
    UINT32 uwBlkSize;           /**< 单个内存块大小（字节），已按系统字长对齐 */
    UINT32 uwBlkNum;            /**< 内存池总块数 */
    UINT32 uwBlkCnt;            /**< 当前已分配的块数 */
    LOS_MEMBOX_NODE stFreeList; /**< 空闲块链表头，通过pstNext串联所有空闲块 */
} LOS_MEMBOX_INFO;

typedef LOS_MEMBOX_INFO OS_MEMBOX_S; /**< 内存池信息结构体的兼容别名 */

/**
 * @ingroup los_membox
 * @brief 内存地址按系统字长对齐
 * @param memAddr 需要对齐的内存地址
 * @return UINTPTR 对齐后的内存地址
 * @par 计算原理：
 * 将地址向上取整到最近的系统字长边界，公式为：
 * (memAddr + sizeof(UINTPTR) - 1) & ~(sizeof(UINTPTR) - 1)
 * @note sizeof(UINTPTR)在32位系统为4（0x4），64位系统为8（0x8）
 */
#define LOS_MEMBOX_ALIGNED(memAddr) (((UINTPTR)(memAddr) + sizeof(UINTPTR) - 1) & (~(sizeof(UINTPTR) - 1)))

/**
 * @ingroup los_membox
 * @brief 计算内存池所需总大小
 * @param blkSize 单个块大小（字节）
 * @param blkNum 块数量
 * @return UINT32 内存池总大小（字节）
 * @par 计算公式：
 * 总大小 = 内存池信息结构体大小 + 对齐后的单个块大小 * 块数量
 * 其中：对齐后的单个块大小 = LOS_MEMBOX_ALIGNED(blkSize + OS_MEMBOX_NODE_HEAD_SIZE)
 * @note OS_MEMBOX_NODE_HEAD_SIZE是空闲节点头部大小，每个块包含此头部用于链表管理
 */
#define LOS_MEMBOX_SIZE(blkSize, blkNum) \
    (sizeof(LOS_MEMBOX_INFO) + (LOS_MEMBOX_ALIGNED((blkSize) + OS_MEMBOX_NODE_HEAD_SIZE) * (blkNum)))


/**
 * @ingroup los_membox
 * @brief Initialize a memory pool.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to initialize a memory pool.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The poolSize parameter value should match the following two conditions :
 * 1) Be less than or equal to the Memory pool size;
 * 2) Be greater than the size of LOS_MEMBOX_INFO.</li>
 * </ul>
 *
 * @param pool     [IN] Memory pool address.
 * @param poolSize [IN] Memory pool size.
 * @param blkSize  [IN] Memory block size.
 *
 * @retval #LOS_NOK   The memory pool fails to be initialized.
 * @retval #LOS_OK    The memory pool is successfully initialized.
 * @par Dependency:
 * <ul>
 * <li>los_membox.h: the header file that contains the API declaration.</li>
 * </ul>
 * @see None.
 */
extern UINT32 LOS_MemboxInit(VOID *pool, UINT32 poolSize, UINT32 blkSize);

/**
 * @ingroup los_membox
 * @brief Request a memory block.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to request a memory block.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The input pool parameter must be initialized via func LOS_MemboxInit.</li>
 * </ul>
 *
 * @param pool    [IN] Memory pool address.
 *
 * @retval #VOID*      The request is accepted, and return a memory block address.
 * @retval #NULL       The request fails.
 * @par Dependency:
 * <ul>
 * <li>los_membox.h: the header file that contains the API declaration.</li>
 * </ul>
 * @see LOS_MemboxFree
 */
extern VOID *LOS_MemboxAlloc(VOID *pool);

/**
 * @ingroup los_membox
 * @brief Free a memory block.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to free a memory block.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The input pool parameter must be initialized via func LOS_MemboxInit.</li>
 * <li>The input box parameter must be allocated by LOS_MemboxAlloc.</li>
 * </ul>
 *
 * @param pool     [IN] Memory pool address.
 * @param box      [IN] Memory block address.
 *
 * @retval #LOS_NOK   This memory block fails to be freed.
 * @retval #LOS_OK    This memory block is successfully freed.
 * @par Dependency:
 * <ul>
 * <li>los_membox.h: the header file that contains the API declaration.</li>
 * </ul>
 * @see LOS_MemboxAlloc
 */
extern UINT32 LOS_MemboxFree(VOID *pool, VOID *box);

/**
 * @ingroup los_membox
 * @brief Clear a memory block.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to set the memory block value to be 0.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The input pool parameter must be initialized via func LOS_MemboxInit.</li>
 * <li>The input box parameter must be allocated by LOS_MemboxAlloc.</li>
 * </ul>
 *
 * @param pool    [IN] Memory pool address.
 * @param box     [IN] Memory block address.
 *
 * @retval VOID
 * @par Dependency:
 * <ul>
 * <li>los_membox.h: the header file that contains the API declaration.</li>
 * </ul>
 * @see None.
 */
extern VOID LOS_MemboxClr(VOID *pool, VOID *box);

/**
 * @ingroup los_membox
 * @brief show membox info.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to show the memory pool info.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The input pool parameter must be initialized via func LOS_MemboxInit.</li>
 * </ul>
 *
 * @param pool    [IN] Memory pool address.
 *
 * @retval VOID
 * @par Dependency:
 * <ul>
 * <li>los_membox.h: the header file that contains the API declaration.</li>
 * </ul>
 * @see None.
 */
extern VOID LOS_ShowBox(VOID *pool);

/**
 * @ingroup los_membox
 * @brief calculate membox information.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to calculate membox information.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>One parameter of this interface is a pointer, it should be a correct value, otherwise, the system may
 * be abnormal.</li>
 * </ul>
 *
 * @param  boxMem        [IN]  Type  #VOID*   Pointer to the calculate membox.
 * @param  maxBlk       [OUT] Type  #UINT32* Record membox max block.
 * @param  blkCnt       [OUT] Type  #UINT32* Record membox block count already allocated.
 * @param  blkSize      [OUT] Type  #UINT32* Record membox block size.
 *
 * @retval #LOS_OK        The heap status calculate success.
 * @retval #LOS_NOK       The membox  status calculate with some error.
 * @par Dependency:
 * <ul><li>los_memory.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MemAlloc | LOS_MemRealloc | LOS_MemFree
 */
extern UINT32 LOS_MemboxStatisticsGet(const VOID *boxMem, UINT32 *maxBlk, UINT32 *blkCnt, UINT32 *blkSize);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_MEMBOX_H */
