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
 * @file los_queue.c
 * @brief 
 * @verbatim
    基本概念
        队列又称消息队列，是一种常用于任务间通信的数据结构。队列接收来自任务或中断的
        不固定长度消息，并根据不同的接口确定传递的消息是否存放在队列空间中。
        
        任务能够从队列里面读取消息，当队列中的消息为空时，挂起读取任务；当队列中有新消息时，
        挂起的读取任务被唤醒并处理新消息。任务也能够往队列里写入消息，当队列已经写满消息时，
        挂起写入任务；当队列中有空闲消息节点时，挂起的写入任务被唤醒并写入消息。如果将
        读队列和写队列的超时时间设置为0，则不会挂起任务，接口会直接返回，这就是非阻塞模式。

        消息队列提供了异步处理机制，允许将一个消息放入队列，但不立即处理。同时队列还有缓冲消息的作用。
        
    队列特性
        消息以先进先出的方式排队，支持异步读写。
        读队列和写队列都支持超时机制。
        每读取一条消息，就会将该消息节点设置为空闲。
        发送消息类型由通信双方约定，可以允许不同长度（不超过队列的消息节点大小）的消息。
        一个任务能够从任意一个消息队列接收和发送消息。
        多个任务能够从同一个消息队列接收和发送消息。
        创建队列时所需的队列空间，默认支持接口内系统自行动态申请内存的方式，同时也支持将用户分配的队列空间作为接口入参传入的方式。

    队列运作原理
        创建队列时，创建队列成功会返回队列ID。

        在队列控制块中维护着一个消息头节点位置Head和一个消息尾节点位置Tail来，用于表示当前
        队列中消息的存储情况。Head表示队列中被占用的消息节点的起始位置。Tail表示被占用的
        消息节点的结束位置，也是空闲消息节点的起始位置。队列刚创建时，Head和Tail均指向队列起始位置。

        写队列时，根据readWriteableCnt[1]判断队列是否可以写入，不能对已满（readWriteableCnt[1]为0）
        队列进行写操作。写队列支持两种写入方式：向队列尾节点写入，也可以向队列头节点写入。尾节点写入时，
        根据Tail找到起始空闲消息节点作为数据写入对象，如果Tail已经指向队列尾部则采用回卷方式。头节点写入时，
        将Head的前一个节点作为数据写入对象，如果Head指向队列起始位置则采用回卷方式。

        读队列时，根据readWriteableCnt[0]判断队列是否有消息需要读取，对全部空闲（readWriteableCnt[0]为0）
        队列进行读操作会引起任务挂起。如果队列可以读取消息，则根据Head找到最先写入队列的消息节点进行读取。
        如果Head已经指向队列尾部则采用回卷方式。

        删除队列时，根据队列ID找到对应队列，把队列状态置为未使用，把队列控制块置为初始状态。
        如果是通过系统动态申请内存方式创建的队列，还会释放队列所占内存。

    使用场景
        队列用于任务间通信，可以实现消息的异步处理。同时消息的发送方和接收方不需要彼此联系，两者间是解耦的。

    队列错误码
        对存在失败可能性的操作返回对应的错误码，以便快速定位错误原因。
 * @endverbatim
 */
#include "los_queue_pri.h"
#include "los_queue_debug_pri.h"
#include "los_task_pri.h"
#include "los_sched_pri.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_percpu_pri.h"
#include "los_hook.h"
#ifdef LOSCFG_IPC_CONTAINER
#include "los_ipc_container_pri.h"
#endif

#ifdef LOSCFG_BASE_IPC_QUEUE
#if (LOSCFG_BASE_IPC_QUEUE_LIMIT <= 0)
#error "queue maxnum cannot be zero"
#endif /* LOSCFG_BASE_IPC_QUEUE_LIMIT <= 0 */

#ifndef LOSCFG_IPC_CONTAINER
LITE_OS_SEC_BSS LosQueueCB *g_allQueue = NULL;  // 队列控制块数组指针
LITE_OS_SEC_BSS STATIC LOS_DL_LIST g_freeQueueList;  // 空闲队列链表
#define FREE_QUEUE_LIST g_freeQueueList
#endif

/**
 * @brief 初始化所有队列控制块
 * @param freeQueueList 空闲队列链表指针
 * @return 成功返回队列控制块数组指针，失败返回NULL
 */
LITE_OS_SEC_TEXT_INIT LosQueueCB *OsAllQueueCBInit(LOS_DL_LIST *freeQueueList)
{
    UINT32 index;  // 循环索引

    if (freeQueueList == NULL) {  // 检查空闲队列链表指针是否为空
        return NULL;
    }

    UINT32 size = LOSCFG_BASE_IPC_QUEUE_LIMIT * sizeof(LosQueueCB);  // 计算队列控制块数组大小
    /* system resident memory, don't free */
    LosQueueCB *allQueue = (LosQueueCB *)LOS_MemAlloc(m_aucSysMem0, size);  // 分配队列控制块数组内存
    if (allQueue == NULL) {  // 检查内存分配是否成功
        return NULL;
    }
    (VOID)memset_s(allQueue, size, 0, size);  // 初始化队列控制块数组内存
    LOS_ListInit(freeQueueList);  // 初始化空闲队列链表
    for (index = 0; index < LOSCFG_BASE_IPC_QUEUE_LIMIT; index++) {  // 遍历队列控制块数组
        LosQueueCB *queueNode = ((LosQueueCB *)allQueue) + index;  // 获取当前队列控制块
        queueNode->queueID = index;  // 设置队列ID
        LOS_ListTailInsert(freeQueueList, &queueNode->readWriteList[OS_QUEUE_WRITE]);  // 将队列控制块插入空闲队列链表尾部
    }

#ifndef LOSCFG_IPC_CONTAINER
    if (OsQueueDbgInitHook() != LOS_OK) {  // 调试钩子初始化
        (VOID)LOS_MemFree(m_aucSysMem0, allQueue);  // 释放已分配的内存
        return NULL;
    }
#endif
    return allQueue;  // 返回队列控制块数组指针
}

/*
 * Description : queue initial
 * Return      : LOS_OK on success or error code on failure
 */
/**
 * @brief 队列模块初始化
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsQueueInit(VOID)
{
#ifndef LOSCFG_IPC_CONTAINER
    g_allQueue = OsAllQueueCBInit(&g_freeQueueList);  // 初始化所有队列控制块
    if (g_allQueue == NULL) {  // 检查队列控制块数组是否初始化成功
        return LOS_ERRNO_QUEUE_NO_MEMORY;  // 返回内存不足错误
    }
#endif
    return LOS_OK;  // 返回成功
}

/**
 * @brief 创建消息队列
 * @param queueName 队列名称
 * @param len 队列长度
 * @param queueID 输出参数，返回创建的队列ID
 * @param flags 队列标志
 * @param maxMsgSize 最大消息大小
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_QueueCreate(CHAR *queueName, UINT16 len, UINT32 *queueID,
                                             UINT32 flags, UINT16 maxMsgSize)
{
    LosQueueCB *queueCB = NULL;  // 队列控制块指针
    UINT32 intSave;  // 中断状态保存变量
    LOS_DL_LIST *unusedQueue = NULL;  // 未使用队列节点
    UINT8 *queue = NULL;  // 队列内存指针
    UINT16 msgSize;  // 消息大小（包含消息头）

    (VOID)queueName;  // 未使用的参数
    (VOID)flags;  // 未使用的参数

    if (queueID == NULL) {  // 检查队列ID指针是否为空
        return LOS_ERRNO_QUEUE_CREAT_PTR_NULL;  // 返回指针为空错误
    }

    if (maxMsgSize > (OS_NULL_SHORT - sizeof(UINT32))) {  // 检查最大消息大小是否超过限制
        return LOS_ERRNO_QUEUE_SIZE_TOO_BIG;  // 返回消息大小过大错误
    }

    if ((len == 0) || (maxMsgSize == 0)) {  // 检查队列长度和消息大小是否为零
        return LOS_ERRNO_QUEUE_PARA_ISZERO;  // 返回参数为零错误
    }

    msgSize = maxMsgSize + sizeof(UINT32);  // 计算消息总大小（包含消息长度字段）
    /*
     * Memory allocation is time-consuming, to shorten the time of disable interrupt,
     * move the memory allocation to here.
     */
    queue = (UINT8 *)LOS_MemAlloc(m_aucSysMem1, (UINT32)len * msgSize);  // 分配队列内存
    if (queue == NULL) {  // 检查内存分配是否成功
        return LOS_ERRNO_QUEUE_CREATE_NO_MEMORY;  // 返回内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    if (LOS_ListEmpty(&FREE_QUEUE_LIST)) {  // 检查是否有空闲队列控制块
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        OsQueueCheckHook();  // 队列检查钩子
        (VOID)LOS_MemFree(m_aucSysMem1, queue);  // 释放已分配的队列内存
        return LOS_ERRNO_QUEUE_CB_UNAVAILABLE;  // 返回队列控制块不可用错误
    }

    unusedQueue = LOS_DL_LIST_FIRST(&FREE_QUEUE_LIST);  // 获取第一个空闲队列控制块
    LOS_ListDelete(unusedQueue);  // 从空闲队列链表中删除该节点
    queueCB = GET_QUEUE_LIST(unusedQueue);  // 获取队列控制块指针
    queueCB->queueLen = len;  // 设置队列长度
    queueCB->queueSize = msgSize;  // 设置消息大小
    queueCB->queueHandle = queue;  // 设置队列内存指针
    queueCB->queueState = OS_QUEUE_INUSED;  // 设置队列状态为使用中
    queueCB->readWriteableCnt[OS_QUEUE_READ] = 0;  // 初始化可读计数
    queueCB->readWriteableCnt[OS_QUEUE_WRITE] = len;  // 初始化可写计数
    queueCB->queueHead = 0;  // 初始化队列头指针
    queueCB->queueTail = 0;  // 初始化队列尾指针
    LOS_ListInit(&queueCB->readWriteList[OS_QUEUE_READ]);  // 初始化读等待链表
    LOS_ListInit(&queueCB->readWriteList[OS_QUEUE_WRITE]);  // 初始化写等待链表
    LOS_ListInit(&queueCB->memList);  // 初始化内存等待链表

    OsQueueDbgUpdateHook(queueCB->queueID, OsCurrTaskGet()->taskEntry);  // 更新调试信息
    SCHEDULER_UNLOCK(intSave);  // 开调度器

    *queueID = queueCB->queueID;  // 设置输出队列ID
    OsHookCall(LOS_HOOK_TYPE_QUEUE_CREATE, queueCB);  // 调用队列创建钩子
    return LOS_OK;  // 返回成功
}

/**
 * @brief 队列读操作参数检查
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC LITE_OS_SEC_TEXT UINT32 OsQueueReadParameterCheck(UINT32 queueID, const VOID *bufferAddr,
                                                         const UINT32 *bufferSize, UINT32 timeout)
{
    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {  // 检查队列ID是否有效
        return LOS_ERRNO_QUEUE_INVALID;  // 返回队列无效错误
    }
    if ((bufferAddr == NULL) || (bufferSize == NULL)) {  // 检查缓冲区地址和大小指针是否为空
        return LOS_ERRNO_QUEUE_READ_PTR_NULL;  // 返回指针为空错误
    }

    if ((*bufferSize == 0) || (*bufferSize > (OS_NULL_SHORT - sizeof(UINT32)))) {  // 检查缓冲区大小是否有效
        return LOS_ERRNO_QUEUE_READSIZE_IS_INVALID;  // 返回缓冲区大小无效错误
    }

    OsQueueDbgTimeUpdateHook(queueID);  // 更新调试时间

    if (timeout != LOS_NO_WAIT) {  // 检查是否需要超时等待
        if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
            return LOS_ERRNO_QUEUE_READ_IN_INTERRUPT;  // 返回中断中不允许等待错误
        }
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 队列写操作参数检查
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC LITE_OS_SEC_TEXT UINT32 OsQueueWriteParameterCheck(UINT32 queueID, const VOID *bufferAddr,
                                                          const UINT32 *bufferSize, UINT32 timeout)
{
    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {  // 检查队列ID是否有效
        return LOS_ERRNO_QUEUE_INVALID;  // 返回队列无效错误
    }

    if (bufferAddr == NULL) {  // 检查缓冲区地址是否为空
        return LOS_ERRNO_QUEUE_WRITE_PTR_NULL;  // 返回指针为空错误
    }

    if (*bufferSize == 0) {  // 检查缓冲区大小是否为零
        return LOS_ERRNO_QUEUE_WRITESIZE_ISZERO;  // 返回缓冲区大小为零错误
    }

    OsQueueDbgTimeUpdateHook(queueID);  // 更新调试时间

    if (timeout != LOS_NO_WAIT) {  // 检查是否需要超时等待
        if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
            return LOS_ERRNO_QUEUE_WRITE_IN_INTERRUPT;  // 返回中断中不允许等待错误
        }
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 队列缓冲区操作（读/写）
 * @param queueCB 队列控制块指针
 * @param operateType 操作类型（读/写，头/尾）
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 */
STATIC VOID OsQueueBufferOperate(LosQueueCB *queueCB, UINT32 operateType, VOID *bufferAddr, UINT32 *bufferSize)
{
    UINT8 *queueNode = NULL;  // 队列节点指针
    UINT32 msgDataSize;  // 消息数据大小
    UINT16 queuePosition;  // 队列位置

    /* get the queue position */
    switch (OS_QUEUE_OPERATE_GET(operateType)) {  // 根据操作类型获取队列位置
        case OS_QUEUE_READ_HEAD:  // 从头部读
            queuePosition = queueCB->queueHead;  // 获取当前头位置
            ((queueCB->queueHead + 1) == queueCB->queueLen) ? (queueCB->queueHead = 0) : (queueCB->queueHead++);  // 更新头指针
            break;
        case OS_QUEUE_WRITE_HEAD:  // 向头部写
            (queueCB->queueHead == 0) ? (queueCB->queueHead = queueCB->queueLen - 1) : (--queueCB->queueHead);  // 更新头指针
            queuePosition = queueCB->queueHead;  // 获取当前头位置
            break;
        case OS_QUEUE_WRITE_TAIL:  // 向尾部写
            queuePosition = queueCB->queueTail;  // 获取当前尾位置
            ((queueCB->queueTail + 1) == queueCB->queueLen) ? (queueCB->queueTail = 0) : (queueCB->queueTail++);  // 更新尾指针
            break;
        default:  /* read tail, reserved. */
            PRINT_ERR("invalid queue operate type!\n");  // 打印无效操作类型错误
            return;
    }

    queueNode = &(queueCB->queueHandle[(queuePosition * (queueCB->queueSize))]);  // 计算队列节点地址

    if (OS_QUEUE_IS_READ(operateType)) {  // 读操作
        if (memcpy_s(&msgDataSize, sizeof(UINT32), queueNode + queueCB->queueSize - sizeof(UINT32),
            sizeof(UINT32)) != EOK) {  // 读取消息大小
            PRINT_ERR("get msgdatasize failed\n");  // 打印读取消息大小失败错误
            return;
        }
        msgDataSize = (*bufferSize < msgDataSize) ? *bufferSize : msgDataSize;  // 确定实际读取大小
        if (memcpy_s(bufferAddr, *bufferSize, queueNode, msgDataSize) != EOK) {  // 读取消息数据
            PRINT_ERR("copy message to buffer failed\n");  // 打印消息复制失败错误
            return;
        }

        *bufferSize = msgDataSize;  // 设置实际读取大小
    } else {  // 写操作
        if (memcpy_s(queueNode, queueCB->queueSize, bufferAddr, *bufferSize) != EOK) {  // 写入消息数据
            PRINT_ERR("store message failed\n");  // 打印消息存储失败错误
            return;
        }
        if (memcpy_s(queueNode + queueCB->queueSize - sizeof(UINT32), sizeof(UINT32), bufferSize,
            sizeof(UINT32)) != EOK) {  // 写入消息大小
            PRINT_ERR("store message size failed\n");  // 打印消息大小存储失败错误
            return;
        }
    }
}

/**
 * @brief 队列操作参数检查
 * @param queueCB 队列控制块指针
 * @param queueID 队列ID
 * @param operateType 操作类型
 * @param bufferSize 缓冲区大小
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsQueueOperateParamCheck(const LosQueueCB *queueCB, UINT32 queueID,
                                       UINT32 operateType, const UINT32 *bufferSize)
{
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {  // 检查队列是否存在且处于使用中
        return LOS_ERRNO_QUEUE_NOT_CREATE;  // 返回队列未创建错误
    }

    if (OS_QUEUE_IS_WRITE(operateType) && (*bufferSize > (queueCB->queueSize - sizeof(UINT32)))) {  // 检查写操作消息大小是否超过限制
        return LOS_ERRNO_QUEUE_WRITE_SIZE_TOO_BIG;  // 返回消息大小过大错误
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 队列操作（读/写）
 * @param queueID 队列ID
 * @param operateType 操作类型
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsQueueOperate(UINT32 queueID, UINT32 operateType, VOID *bufferAddr, UINT32 *bufferSize, UINT32 timeout)
{
    UINT32 ret;  // 返回值
    UINT32 readWrite = OS_QUEUE_READ_WRITE_GET(operateType);  // 获取读写类型
    UINT32 intSave;  // 中断状态保存变量
    OsHookCall(LOS_HOOK_TYPE_QUEUE_READ, (LosQueueCB *)GET_QUEUE_HANDLE(queueID), operateType, *bufferSize, timeout);  // 调用队列读钩子

    SCHEDULER_LOCK(intSave);  // 关调度器
    LosQueueCB *queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);  // 获取队列控制块
    ret = OsQueueOperateParamCheck(queueCB, queueID, operateType, bufferSize);  // 检查操作参数
    if (ret != LOS_OK) {  // 参数检查失败
        goto QUEUE_END;  // 跳转到结束处理
    }

    if (queueCB->readWriteableCnt[readWrite] == 0) {  // 检查是否有可读写空间
        if (timeout == LOS_NO_WAIT) {  // 非阻塞模式
            ret = OS_QUEUE_IS_READ(operateType) ? LOS_ERRNO_QUEUE_ISEMPTY : LOS_ERRNO_QUEUE_ISFULL;  // 设置队列为空或满错误
            goto QUEUE_END;  // 跳转到结束处理
        }

        if (!OsPreemptableInSched()) {  // 检查是否允许抢占
            ret = LOS_ERRNO_QUEUE_PEND_IN_LOCK;  // 返回锁中不允许等待错误
            goto QUEUE_END;  // 跳转到结束处理
        }

        LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前任务
        OsTaskWaitSetPendMask(OS_TASK_WAIT_QUEUE, queueCB->queueID, timeout);  // 设置任务等待掩码
        ret = runTask->ops->wait(runTask, &queueCB->readWriteList[readWrite], timeout);  // 等待队列
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {  // 等待超时
            ret = LOS_ERRNO_QUEUE_TIMEOUT;  // 设置队列超时错误
            goto QUEUE_END;  // 跳转到结束处理
        }
    } else {  // 有可读写空间
        queueCB->readWriteableCnt[readWrite]--;  // 减少可读写计数
    }

    OsQueueBufferOperate(queueCB, operateType, bufferAddr, bufferSize);  // 执行缓冲区操作

    if (!LOS_ListEmpty(&queueCB->readWriteList[!readWrite])) {  // 检查是否有等待的对立操作任务
        LosTaskCB *resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&queueCB->readWriteList[!readWrite]));  // 获取第一个等待任务
        OsTaskWakeClearPendMask(resumedTask);  // 清除任务等待掩码
        resumedTask->ops->wake(resumedTask);  // 唤醒任务
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        LOS_MpSchedule(OS_MP_CPU_ALL);  // 多处理器调度
        LOS_Schedule();  // 任务调度
        return LOS_OK;  // 返回成功
    } else {  // 没有等待的对立操作任务
        queueCB->readWriteableCnt[!readWrite]++;  // 增加对立操作可读写计数
    }

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
}

/**
 * @brief 从队列头部读取消息（复制模式）
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueReadCopy(UINT32 queueID,
                                          VOID *bufferAddr,
                                          UINT32 *bufferSize,
                                          UINT32 timeout)
{
    UINT32 ret;  // 返回值
    UINT32 operateType;  // 操作类型

    ret = OsQueueReadParameterCheck(queueID, bufferAddr, bufferSize, timeout);  // 检查读参数
    if (ret != LOS_OK) {  // 参数检查失败
        return ret;  // 返回错误码
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_READ, OS_QUEUE_HEAD);  // 设置操作类型为从头部读
    return OsQueueOperate(queueID, operateType, bufferAddr, bufferSize, timeout);  // 执行队列操作
}

/**
 * @brief 向队列头部写入消息（复制模式）
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteHeadCopy(UINT32 queueID,
                                               VOID *bufferAddr,
                                               UINT32 bufferSize,
                                               UINT32 timeout)
{
    UINT32 ret;  // 返回值
    UINT32 operateType;  // 操作类型

    ret = OsQueueWriteParameterCheck(queueID, bufferAddr, &bufferSize, timeout);  // 检查写参数
    if (ret != LOS_OK) {  // 参数检查失败
        return ret;  // 返回错误码
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_WRITE, OS_QUEUE_HEAD);  // 设置操作类型为向头部写
    return OsQueueOperate(queueID, operateType, bufferAddr, &bufferSize, timeout);  // 执行队列操作
}

/**
 * @brief 向队列尾部写入消息（复制模式）
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteCopy(UINT32 queueID,
                                           VOID *bufferAddr,
                                           UINT32 bufferSize,
                                           UINT32 timeout)
{
    UINT32 ret;  // 返回值
    UINT32 operateType;  // 操作类型

    ret = OsQueueWriteParameterCheck(queueID, bufferAddr, &bufferSize, timeout);  // 检查写参数
    if (ret != LOS_OK) {  // 参数检查失败
        return ret;  // 返回错误码
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_WRITE, OS_QUEUE_TAIL);  // 设置操作类型为向尾部写
    return OsQueueOperate(queueID, operateType, bufferAddr, &bufferSize, timeout);  // 执行队列操作
}

/**
 * @brief 从队列头部读取消息
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueRead(UINT32 queueID, VOID *bufferAddr, UINT32 bufferSize, UINT32 timeout)
{
    return LOS_QueueReadCopy(queueID, bufferAddr, &bufferSize, timeout);  // 调用复制模式读操作
}

/**
 * @brief 向队列尾部写入消息
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueWrite(UINT32 queueID, VOID *bufferAddr, UINT32 bufferSize, UINT32 timeout)
{
    if (bufferAddr == NULL) {  // 检查缓冲区地址是否为空
        return LOS_ERRNO_QUEUE_WRITE_PTR_NULL;  // 返回指针为空错误
    }
    bufferSize = sizeof(CHAR *);  // 设置缓冲区大小为指针大小
    return LOS_QueueWriteCopy(queueID, &bufferAddr, bufferSize, timeout);  // 调用复制模式写操作
}

/**
 * @brief 向队列头部写入消息
 * @param queueID 队列ID
 * @param bufferAddr 缓冲区地址
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteHead(UINT32 queueID,
                                           VOID *bufferAddr,
                                           UINT32 bufferSize,
                                           UINT32 timeout)
{
    if (bufferAddr == NULL) {  // 检查缓冲区地址是否为空
        return LOS_ERRNO_QUEUE_WRITE_PTR_NULL;  // 返回指针为空错误
    }
    bufferSize = sizeof(CHAR *);  // 设置缓冲区大小为指针大小
    return LOS_QueueWriteHeadCopy(queueID, &bufferAddr, bufferSize, timeout);  // 调用复制模式头部写操作
}

/**
 * @brief 删除消息队列
 * @param queueID 队列ID
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_QueueDelete(UINT32 queueID)
{
    LosQueueCB *queueCB = NULL;  // 队列控制块指针
    UINT8 *queue = NULL;  // 队列内存指针
    UINT32 intSave;  // 中断状态保存变量
    UINT32 ret;  // 返回值

    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {  // 检查队列ID是否有效
        return LOS_ERRNO_QUEUE_NOT_FOUND;  // 返回队列未找到错误
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);  // 获取队列控制块
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {  // 检查队列是否存在且处于使用中
        ret = LOS_ERRNO_QUEUE_NOT_CREATE;  // 返回队列未创建错误
        goto QUEUE_END;  // 跳转到结束处理
    }

    if (!LOS_ListEmpty(&queueCB->readWriteList[OS_QUEUE_READ])) {  // 检查读等待链表是否为空
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;  // 返回队列被任务使用错误
        goto QUEUE_END;  // 跳转到结束处理
    }

    if (!LOS_ListEmpty(&queueCB->readWriteList[OS_QUEUE_WRITE])) {  // 检查写等待链表是否为空
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;  // 返回队列被任务使用错误
        goto QUEUE_END;  // 跳转到结束处理
    }

    if (!LOS_ListEmpty(&queueCB->memList)) {  // 检查内存等待链表是否为空
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;  // 返回队列被任务使用错误
        goto QUEUE_END;  // 跳转到结束处理
    }

    if ((queueCB->readWriteableCnt[OS_QUEUE_WRITE] + queueCB->readWriteableCnt[OS_QUEUE_READ]) !=
        queueCB->queueLen) {  // 检查队列是否有未处理消息
        ret = LOS_ERRNO_QUEUE_IN_TSKWRITE;  // 返回队列有未处理消息错误
        goto QUEUE_END;  // 跳转到结束处理
    }

    queue = queueCB->queueHandle;  // 保存队列内存指针
    queueCB->queueHandle = NULL;  // 清空队列内存指针
    queueCB->queueState = OS_QUEUE_UNUSED;  // 设置队列状态为未使用
    queueCB->queueID = SET_QUEUE_ID(GET_QUEUE_COUNT(queueCB->queueID) + 1, GET_QUEUE_INDEX(queueCB->queueID));  // 更新队列ID
    OsQueueDbgUpdateHook(queueCB->queueID, NULL);  // 更新调试信息

    LOS_ListTailInsert(&FREE_QUEUE_LIST, &queueCB->readWriteList[OS_QUEUE_WRITE]);  // 将队列控制块插入空闲队列链表
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    OsHookCall(LOS_HOOK_TYPE_QUEUE_DELETE, queueCB);  // 调用队列删除钩子
    ret = LOS_MemFree(m_aucSysMem1, (VOID *)queue);  // 释放队列内存
    return ret;  // 返回结果

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
}

/**
 * @brief 获取队列信息
 * @param queueID 队列ID
 * @param queueInfo 队列信息结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_QueueInfoGet(UINT32 queueID, QUEUE_INFO_S *queueInfo)
{
    UINT32 intSave;  // 中断状态保存变量
    UINT32 ret = LOS_OK;  // 返回值
    LosQueueCB *queueCB = NULL;  // 队列控制块指针
    LosTaskCB *tskCB = NULL;  // 任务控制块指针

    if (queueInfo == NULL) {  // 检查队列信息结构体指针是否为空
        return LOS_ERRNO_QUEUE_PTR_NULL;  // 返回指针为空错误
    }

    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {  // 检查队列ID是否有效
        return LOS_ERRNO_QUEUE_INVALID;  // 返回队列无效错误
    }

    (VOID)memset_s((VOID *)queueInfo, sizeof(QUEUE_INFO_S), 0, sizeof(QUEUE_INFO_S));  // 初始化队列信息结构体
    SCHEDULER_LOCK(intSave);  // 关调度器

    queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);  // 获取队列控制块
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {  // 检查队列是否存在且处于使用中
        ret = LOS_ERRNO_QUEUE_NOT_CREATE;  // 返回队列未创建错误
        goto QUEUE_END;  // 跳转到结束处理
    }

    queueInfo->uwQueueID = queueID;  // 设置队列ID
    queueInfo->usQueueLen = queueCB->queueLen;  // 设置队列长度
    queueInfo->usQueueSize = queueCB->queueSize;  // 设置消息大小
    queueInfo->usQueueHead = queueCB->queueHead;  // 设置队列头指针
    queueInfo->usQueueTail = queueCB->queueTail;  // 设置队列尾指针
    queueInfo->usReadableCnt = queueCB->readWriteableCnt[OS_QUEUE_READ];  // 设置可读计数
    queueInfo->usWritableCnt = queueCB->readWriteableCnt[OS_QUEUE_WRITE];  // 设置可写计数

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->readWriteList[OS_QUEUE_READ], LosTaskCB, pendList) {  // 遍历读等待任务
        queueInfo->uwWaitReadTask |= 1ULL << tskCB->taskID;  // 设置读等待任务掩码
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->readWriteList[OS_QUEUE_WRITE], LosTaskCB, pendList) {  // 遍历写等待任务
        queueInfo->uwWaitWriteTask |= 1ULL << tskCB->taskID;  // 设置写等待任务掩码
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->memList, LosTaskCB, pendList) {  // 遍历内存等待任务
        queueInfo->uwWaitMemTask |= 1ULL << tskCB->taskID;  // 设置内存等待任务掩码
    }

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
}

#endif /* LOSCFG_BASE_IPC_QUEUE */

