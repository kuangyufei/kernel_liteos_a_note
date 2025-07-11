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

#ifndef _LOS_QUEUE_PRI_H
#define _LOS_QUEUE_PRI_H

#include "los_queue.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief @note_pic
 * @verbatim
 鸿蒙对消息队列图
						   |<-----消息内容区,有2个消息---->|
+------------+------------------------------------------------------------+
|            |             |---------------|---------------|              |
|            |             |---------------|---------------|              |
|            |             |---------------|---------------|              |
+-------------------------------------------------------------------------+
|            |             ^                               ^              |
|<消息大小>	     |             |                               |              |
|            |             |head                           |tail          |
|            +             +任务读消息                          +任务写消息         |
|                                                                         |
|                                                                         |
+<-------------+      队列长度,消息点个数,                          +------------->+
 * @endverbatim
 */

/**
 * @ingroup los_queue
 * @brief 队列读写操作类型枚举
 * @details 定义队列操作的读写方向，用于标识队列操作是读还是写
 */
typedef enum {
    OS_QUEUE_READ = 0,    /**< 读操作 */
    OS_QUEUE_WRITE = 1,   /**< 写操作 */
    OS_QUEUE_N_RW = 2     /**< 读写操作 */
} QueueReadWrite;

/**
 * @ingroup los_queue
 * @brief 队列操作位置枚举
 * @details 定义队列操作的位置，用于标识从队列头部还是尾部进行操作
 */
typedef enum {
    OS_QUEUE_HEAD = 0,    /**< 队列头部 */
    OS_QUEUE_TAIL = 1     /**< 队列尾部 */
} QueueHeadTail;

/**
 * @ingroup los_queue
 * @brief 组合队列操作类型和位置
 * @param ReadOrWrite 读写类型，取值为QueueReadWrite枚举
 * @param HeadOrTail 操作位置，取值为QueueHeadTail枚举
 * @return 组合后的32位操作类型值，高1位表示位置，低1位表示读写类型
 */
#define OS_QUEUE_OPERATE_TYPE(ReadOrWrite, HeadOrTail) (((UINT32)(HeadOrTail) << 1) | (ReadOrWrite))

/**
 * @ingroup los_queue
 * @brief 从操作类型中提取读写类型
 * @param type 组合后的操作类型值
 * @return 读写类型（OS_QUEUE_READ或OS_QUEUE_WRITE）
 */
#define OS_QUEUE_READ_WRITE_GET(type) ((type) & 0x01U)

/** @ingroup los_queue 从队列头部读取 */
#define OS_QUEUE_READ_HEAD     (OS_QUEUE_READ | (OS_QUEUE_HEAD << 1))
/** @ingroup los_queue 从队列尾部读取 */
#define OS_QUEUE_READ_TAIL     (OS_QUEUE_READ | (OS_QUEUE_TAIL << 1))
/** @ingroup los_queue 向队列头部写入 */
#define OS_QUEUE_WRITE_HEAD    (OS_QUEUE_WRITE | (OS_QUEUE_HEAD << 1))
/** @ingroup los_queue 向队列尾部写入 */
#define OS_QUEUE_WRITE_TAIL    (OS_QUEUE_WRITE | (OS_QUEUE_TAIL << 1))

/**
 * @ingroup los_queue
 * @brief 获取操作类型的低2位有效信息
 * @param type 组合后的操作类型值
 * @return 低2位操作类型掩码值
 */
#define OS_QUEUE_OPERATE_GET(type) ((type) & 0x03U)

/**
 * @ingroup los_queue
 * @brief 判断操作类型是否为读操作
 * @param type 组合后的操作类型值
 * @return 布尔值，TRUE表示读操作，FALSE表示非读操作
 */
#define OS_QUEUE_IS_READ(type) (OS_QUEUE_READ_WRITE_GET(type) == OS_QUEUE_READ)

/**
 * @ingroup los_queue
 * @brief 判断操作类型是否为写操作
 * @param type 组合后的操作类型值
 * @return 布尔值，TRUE表示写操作，FALSE表示非写操作
 */
#define OS_QUEUE_IS_WRITE(type) (OS_QUEUE_READ_WRITE_GET(type) == OS_QUEUE_WRITE)

/**
 * @ingroup los_queue
 * @brief 队列控制块结构体
 * @details 用于管理队列的元数据和状态信息，包括队列长度、大小、头尾部指针及等待链表等
 */
typedef struct TagQueueCB {
    UINT8 *queueHandle;                  /**< 队列句柄指针，指向队列数据存储区域 */
    UINT16 queueState;                   /**< 队列状态，取值为OS_QUEUE_UNUSED或OS_QUEUE_INUSED */
    UINT16 queueLen;                     /**< 队列长度，表示队列可容纳的节点数量 */
    UINT16 queueSize;                    /**< 节点大小，每个队列节点的字节数 */
    UINT32 queueID;                      /**< 队列ID，用于唯一标识队列 */
    UINT16 queueHead;                    /**< 队列头指针，指向当前可读节点位置 */
    UINT16 queueTail;                    /**< 队列尾指针，指向当前可写节点位置 */
    UINT16 readWriteableCnt[OS_QUEUE_N_RW]; /**< 可读写资源计数数组，[0]为可读计数，[1]为可写计数 */
    LOS_DL_LIST readWriteList[OS_QUEUE_N_RW]; /**< 读写等待链表数组，[0]为读等待链表，[1]为写等待链表 */
    LOS_DL_LIST memList;                 /**< 内存链表指针，用于管理队列节点内存 */
} LosQueueCB;

/* queue state */
/**
 * @ingroup los_queue
 * @brief 消息队列状态：未使用
 */
#define OS_QUEUE_UNUSED        0

/**
 * @ingroup los_queue
 * @brief 消息队列状态：已使用
 */
#define OS_QUEUE_INUSED        1

/**
 * @ingroup los_queue
 * @brief 队列状态：等待内存池
 * @note 当前未使用此状态
 */
#define OS_QUEUE_WAIT_FOR_POOL 1

/**
 * @ingroup los_queue
 * @brief 队列类型：普通消息队列
 */
#define OS_QUEUE_NORMAL        0

/**
 * @ingroup los_queue
 * @brief 队列信息控制块全局指针
 * @details 指向系统中所有队列控制块的数组首地址
 */
extern LosQueueCB *g_allQueue;

/**
 * @ingroup los_queue
 * @brief 队列控制块数组引用宏
 * @details 当未启用IPC容器功能时，直接引用全局队列控制块数组
 */
#ifndef LOSCFG_IPC_CONTAINER
#define IPC_ALL_QUEUE g_allQueue
#endif

/**
 * @ingroup los_queue
 * @brief 队列ID中COUNT和INDEX的分割位
 * @details 用于将队列ID分割为高16位的计数部分和低16位的索引部分
 */
#define QUEUE_SPLIT_BIT        16  /**< 分割位，十进制16，对应二进制第17位 */

/**
 * @ingroup los_queue
 * @brief 设置队列ID
 * @param count 队列计数，用于版本控制或重分配检测
 * @param queueID 队列索引，用于数组访问
 * @return 组合后的32位队列ID（高16位为count，低16位为queueID）
 */
#define SET_QUEUE_ID(count, queueID)    (((count) << QUEUE_SPLIT_BIT) | (queueID))

/**
 * @ingroup los_queue
 * @brief 从队列ID中获取队列索引
 * @param queueID 完整的32位队列ID
 * @return 低16位的队列索引值
 */
#define GET_QUEUE_INDEX(queueID)        ((queueID) & ((1U << QUEUE_SPLIT_BIT) - 1))

/**
 * @ingroup los_queue
 * @brief 从队列ID中获取队列计数
 * @param queueID 完整的32位队列ID
 * @return 高16位的队列计数值
 */
#define GET_QUEUE_COUNT(queueID)        ((queueID) >> QUEUE_SPLIT_BIT)

/**
 * @ingroup los_queue
 * @brief 通过队列ID获取队列句柄
 * @param queueID 完整的32位队列ID
 * @return 指向对应队列控制块的指针
 */
#define GET_QUEUE_HANDLE(queueID)       (((LosQueueCB *)IPC_ALL_QUEUE) + GET_QUEUE_INDEX(queueID))

/**
 * @ingroup los_queue
 * @brief 获取队列双向链表的头节点
 * @param ptr 链表节点指针
 * @return 指向包含该链表节点的队列控制块指针
 */
#define GET_QUEUE_LIST(ptr) LOS_DL_LIST_ENTRY(ptr, LosQueueCB, readWriteList[OS_QUEUE_WRITE])

/**
 * @ingroup los_queue
 * @brief Alloc a stationary memory for a mail.
 *
 * @par Description:
 * This API is used to alloc a stationary memory for a mail according to queueID.
 * @attention
 * <ul>
 * <li>Do not alloc memory in unblocking modes such as interrupt.</li>
 * <li>This API cannot be called before the Huawei LiteOS is initialized.</li>
 * <li>The argument timeout is a relative time.</li>
 * </ul>
 *
 * @param queueID        [IN]        Queue ID. The value range is [1,LOSCFG_BASE_IPC_QUEUE_LIMIT].
 * @param mailPool        [IN]        The memory poll that stores the mail.
 * @param timeout        [IN]        Expiry time. The value range is [0,LOS_WAIT_FOREVER].
 *
 * @retval   #NULL                     The memory allocation is failed.
 * @retval   #pMem                     The address of alloc memory.
 * @par Dependency:
 * <ul><li>los_queue_pri.h: the header file that contains the API declaration.</li></ul>
 * @see OsQueueMailFree
 */
extern VOID *OsQueueMailAlloc(UINT32 queueID, VOID *mailPool, UINT32 timeout);

/**
 * @ingroup los_queue
 * @brief Free a stationary memory of a mail.
 *
 * @par Description:
 * This API is used to free a stationary memory for a mail according to queueID.
 * @attention
 * <ul>
 * <li>This API cannot be called before the Huawei LiteOS is initialized.</li>
 * </ul>
 *
 * @param queueID        [IN]        Queue ID. The value range is [1,LOSCFG_BASE_IPC_QUEUE_LIMIT].
 * @param mailPool        [IN]        The mail memory poll address.
 * @param mailMem         [IN]        The mail memory block address.
 *
 * @retval   #LOS_OK                                 0x00000000: The memory free successfully.
 * @retval   #OS_ERRNO_QUEUE_MAIL_HANDLE_INVALID     0x02000619: The handle of the queue passed-in when the memory
 *                                                               for the queue is being freed is invalid.
 * @retval   #OS_ERRNO_QUEUE_MAIL_PTR_INVALID        0x0200061a: The pointer to the memory to be freed is null.
 * @retval   #OS_ERRNO_QUEUE_MAIL_FREE_ERROR         0x0200061b: The memory for the queue fails to be freed.
 * @par Dependency:
 * <ul><li>los_queue_pri.h: the header file that contains the API declaration.</li></ul>
 * @see OsQueueMailAlloc
 */
extern UINT32 OsQueueMailFree(UINT32 queueID, VOID *mailPool, VOID *mailMem);

extern LosQueueCB *OsAllQueueCBInit(LOS_DL_LIST *freeQueueList);

extern UINT32 OsQueueInit(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_QUEUE_PRI_H */
