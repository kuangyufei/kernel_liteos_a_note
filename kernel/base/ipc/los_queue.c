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
LITE_OS_SEC_BSS LosQueueCB *g_allQueue = NULL;///< 消息队列池
LITE_OS_SEC_BSS STATIC LOS_DL_LIST g_freeQueueList;///< 空闲队列链表,管分配的,需要队列从这里申请
#define FREE_QUEUE_LIST g_freeQueueList
#endif
/**
 * 初始化消息队列控制块池
 * 
 * @param freeQueueList 空闲队列链表指针
 * @return 成功返回消息队列控制块池指针，失败返回NULL
 */
LITE_OS_SEC_TEXT_INIT LosQueueCB *OsAllQueueCBInit(LOS_DL_LIST *freeQueueList)
{
    UINT32 index;

    /* 参数检查 */
    if (freeQueueList == NULL) {
        return NULL;
    }

    /* 计算所需内存大小 */
    UINT32 size = LOSCFG_BASE_IPC_QUEUE_LIMIT * sizeof(LosQueueCB);
    
    /* 从系统内存池分配内存 */
    LosQueueCB *allQueue = (LosQueueCB *)LOS_MemAlloc(m_aucSysMem0, size);
    if (allQueue == NULL) {
        return NULL;
    }

    /* 初始化分配的内存 */
    (VOID)memset_s(allQueue, size, 0, size);

    /* 初始化空闲队列链表 */
    LOS_ListInit(freeQueueList);

    /* 初始化每个队列控制块 */
    for (index = 0; index < LOSCFG_BASE_IPC_QUEUE_LIMIT; index++) {
        LosQueueCB *queueNode = ((LosQueueCB *)allQueue) + index;
        queueNode->queueID = index;  // 设置队列ID
        /* 将队列控制块加入空闲队列链表 */
        LOS_ListTailInsert(freeQueueList, &queueNode->readWriteList[OS_QUEUE_WRITE]);
    }

#ifndef LOSCFG_IPC_CONTAINER
    /* 初始化调试钩子函数 */
    if (OsQueueDbgInitHook() != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem0, allQueue);
        return NULL;
    }
#endif

    return allQueue;
}

/*
 * Description : queue initial | 消息队列模块初始化
 * Return      : LOS_OK on success or error code on failure
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsQueueInit(VOID)
{
#ifndef LOSCFG_IPC_CONTAINER
    g_allQueue = OsAllQueueCBInit(&g_freeQueueList);
    if (g_allQueue == NULL) {
        return LOS_ERRNO_QUEUE_NO_MEMORY;
    }
#endif
    return LOS_OK;
}
///创建一个队列,根据用户传入队列长度和消息节点大小来开辟相应的内存空间以供该队列使用，参数queueID带走队列ID
LITE_OS_SEC_TEXT_INIT UINT32 LOS_QueueCreate(CHAR *queueName, UINT16 len, UINT32 *queueID,
                                             UINT32 flags, UINT16 maxMsgSize)
{
    LosQueueCB *queueCB = NULL;
    UINT32 intSave;
    LOS_DL_LIST *unusedQueue = NULL;
    UINT8 *queue = NULL;
    UINT16 msgSize;

    (VOID)queueName;
    (VOID)flags;

    if (queueID == NULL) {
        return LOS_ERRNO_QUEUE_CREAT_PTR_NULL;
    }

    if (maxMsgSize > (OS_NULL_SHORT - sizeof(UINT32))) {// maxMsgSize上限 为啥要减去 sizeof(UINT32) ,因为前面存的是队列的大小
        return LOS_ERRNO_QUEUE_SIZE_TOO_BIG;
    }

    if ((len == 0) || (maxMsgSize == 0)) {
        return LOS_ERRNO_QUEUE_PARA_ISZERO;
    }

    msgSize = maxMsgSize + sizeof(UINT32);//总size = 消息体内容长度 + 消息大小(UINT32) 
    /*
     * Memory allocation is time-consuming, to shorten the time of disable interrupt,
     * move the memory allocation to here.
     *///内存分配非常耗时，为了缩短禁用中断的时间，将内存分配移到此处,用的时候分配队列内存
    queue = (UINT8 *)LOS_MemAlloc(m_aucSysMem1, (UINT32)len * msgSize);//从系统内存池中分配,由这里提供读写队列的内存
    if (queue == NULL) {//这里是一次把队列要用到的所有最大内存都申请下来了,能保证不会出现后续使用过程中内存不够的问题出现
        return LOS_ERRNO_QUEUE_CREATE_NO_MEMORY;//调用处有 OsSwtmrInit sys_mbox_new DoMqueueCreate ==
    }

    SCHEDULER_LOCK(intSave);
    if (LOS_ListEmpty(&FREE_QUEUE_LIST)) {//没有空余的队列ID的处理,注意软时钟定时器是由 g_swtmrCBArray统一管理的,里面有正在使用和可分配空闲的队列
        SCHEDULER_UNLOCK(intSave);//g_freeQueueList是管理可用于分配的队列链表,申请消息队列的ID需要向它要
        OsQueueCheckHook();
        (VOID)LOS_MemFree(m_aucSysMem1, queue);//没有就要释放 queue申请的内存
        return LOS_ERRNO_QUEUE_CB_UNAVAILABLE;
    }

    unusedQueue = LOS_DL_LIST_FIRST(&FREE_QUEUE_LIST);//找到一个没有被使用的队列
    LOS_ListDelete(unusedQueue);//将自己从g_freeQueueList中摘除, unusedQueue只是个 LOS_DL_LIST 结点.
    queueCB = GET_QUEUE_LIST(unusedQueue);//通过unusedQueue找到整个消息队列(LosQueueCB)
    queueCB->queueLen = len;	//队列中消息的总个数,注意这个一旦创建是不能变的.
    queueCB->queueSize = msgSize;//消息节点的大小,注意这个一旦创建也是不能变的.
    queueCB->queueHandle = queue;	//队列句柄,队列内容存储区. 
    queueCB->queueState = OS_QUEUE_INUSED;	//队列状态使用中
    queueCB->readWriteableCnt[OS_QUEUE_READ] = 0;//可读资源计数，OS_QUEUE_READ(0):可读.
    queueCB->readWriteableCnt[OS_QUEUE_WRITE] = len;//可些资源计数 OS_QUEUE_WRITE(1):可写, 默认len可写.
    queueCB->queueHead = 0;//队列头节点
    queueCB->queueTail = 0;//队列尾节点
    LOS_ListInit(&queueCB->readWriteList[OS_QUEUE_READ]);//初始化可读队列任务链表
    LOS_ListInit(&queueCB->readWriteList[OS_QUEUE_WRITE]);//初始化可写队列任务链表
    LOS_ListInit(&queueCB->memList);//

    OsQueueDbgUpdateHook(queueCB->queueID, OsCurrTaskGet()->taskEntry);//在创建或删除队列调试信息时更新任务条目
    SCHEDULER_UNLOCK(intSave);

    *queueID = queueCB->queueID;//带走队列ID
    OsHookCall(LOS_HOOK_TYPE_QUEUE_CREATE, queueCB);
    return LOS_OK;
}
///读队列参数检查
STATIC LITE_OS_SEC_TEXT UINT32 OsQueueReadParameterCheck(UINT32 queueID, const VOID *bufferAddr,
                                                         const UINT32 *bufferSize, UINT32 timeout)
{
    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {//队列ID不能超上限
        return LOS_ERRNO_QUEUE_INVALID;
    }
    if ((bufferAddr == NULL) || (bufferSize == NULL)) {//缓存地址和大小参数判断
        return LOS_ERRNO_QUEUE_READ_PTR_NULL;
    }

    if ((*bufferSize == 0) || (*bufferSize > (OS_NULL_SHORT - sizeof(UINT32)))) {//限制了读取数据的上限64K, sizeof(UINT32)代表的是队列的长度
        return LOS_ERRNO_QUEUE_READSIZE_IS_INVALID;					//所以要减去
    }

    OsQueueDbgTimeUpdateHook(queueID);

    if (timeout != LOS_NO_WAIT) {//等待一定时间再读取
        if (OS_INT_ACTIVE) {//如果碰上了硬中断
            return LOS_ERRNO_QUEUE_READ_IN_INTERRUPT;//意思是:硬中断发生时是不能读消息队列的
        }
    }
    return LOS_OK;
}
///写队列参数检查
STATIC LITE_OS_SEC_TEXT UINT32 OsQueueWriteParameterCheck(UINT32 queueID, const VOID *bufferAddr,
                                                          const UINT32 *bufferSize, UINT32 timeout)
{
    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {//队列ID不能超上限
        return LOS_ERRNO_QUEUE_INVALID;
    }

    if (bufferAddr == NULL) {//没有数据源
        return LOS_ERRNO_QUEUE_WRITE_PTR_NULL;
    }

    if (*bufferSize == 0) {//这里没有限制写队列的大小,如果写入一个很大buf 会怎样?
        return LOS_ERRNO_QUEUE_WRITESIZE_ISZERO;
    }

    OsQueueDbgTimeUpdateHook(queueID);

    if (timeout != LOS_NO_WAIT) {
        if (OS_INT_ACTIVE) {
            return LOS_ERRNO_QUEUE_WRITE_IN_INTERRUPT;
        }
    }
    return LOS_OK;
}
///队列buf操作,注意队列数据是按顺序来读取的,要不从头,要不从尾部,不会出现从中间读写,所有可由 head 和 tail 来管理队列.
STATIC VOID OsQueueBufferOperate(LosQueueCB *queueCB, UINT32 operateType, VOID *bufferAddr, UINT32 *bufferSize)
{
    UINT8 *queueNode = NULL;
    UINT32 msgDataSize;
    UINT16 queuePosition;

    /* get the queue position | 先找到队列的位置*/
    switch (OS_QUEUE_OPERATE_GET(operateType)) {//获取操作类型
        case OS_QUEUE_READ_HEAD://从列队头开始读
            queuePosition = queueCB->queueHead;//拿到头部位置
            ((queueCB->queueHead + 1) == queueCB->queueLen) ? (queueCB->queueHead = 0) : (queueCB->queueHead++);//调整队列头部位置
            break;
        case OS_QUEUE_WRITE_HEAD://从列队头开始写
            (queueCB->queueHead == 0) ? (queueCB->queueHead = queueCB->queueLen - 1) : (--queueCB->queueHead);//调整队列头部位置
            queuePosition = queueCB->queueHead;//拿到头部位置
            break;
        case OS_QUEUE_WRITE_TAIL://从列队尾部开始写
            queuePosition = queueCB->queueTail;//设置队列位置为尾部位置
            ((queueCB->queueTail + 1) == queueCB->queueLen) ? (queueCB->queueTail = 0) : (queueCB->queueTail++);//调整队列尾部位置
            break;
        default:  /* read tail, reserved. */
            PRINT_ERR("invalid queue operate type!\n");
            return;
    }
	//queueHandle是create队列时,由外界参数申请的一块内存. 用于copy 使用
    queueNode = &(queueCB->queueHandle[(queuePosition * (queueCB->queueSize))]);//拿到队列节点

    if (OS_QUEUE_IS_READ(operateType)) {//读操作处理,读队列分两步走
        if (memcpy_s(&msgDataSize, sizeof(UINT32), queueNode + queueCB->queueSize - sizeof(UINT32),
            sizeof(UINT32)) != EOK) {//1.先读出队列大小,由队列头四个字节表示
            PRINT_ERR("get msgdatasize failed\n");
            return;
        }
        msgDataSize = (*bufferSize < msgDataSize) ? *bufferSize : msgDataSize;
        if (memcpy_s(bufferAddr, *bufferSize, queueNode, msgDataSize) != EOK) {//2.读表示读走已有数据,所以相当于bufferAddr接着了queueNode的数据
            PRINT_ERR("copy message to buffer failed\n");
            return;
        }

        *bufferSize = msgDataSize;//通过入参 带走消息的大小
    } else {//只有读写两种操作,这里就是写队列了.写也分两步走 , @note_thinking 这里建议鸿蒙加上 OS_QUEUE_IS_WRITE 判断 
        if (memcpy_s(queueNode, queueCB->queueSize, bufferAddr, *bufferSize) != EOK) {//1.写入消息内容
            PRINT_ERR("store message failed\n");//表示把外面数据写进来,所以相当于queueNode接着了bufferAddr的数据
            return;
        }
        if (memcpy_s(queueNode + queueCB->queueSize - sizeof(UINT32), sizeof(UINT32), bufferSize,
            sizeof(UINT32)) != EOK) {//2.写入消息数据的长度,sizeof(UINT32)
            PRINT_ERR("store message size failed\n");
            return;
        }
    }
}
///队列操作参数检查
STATIC UINT32 OsQueueOperateParamCheck(const LosQueueCB *queueCB, UINT32 queueID,
                                       UINT32 operateType, const UINT32 *bufferSize)
{
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {//队列ID和状态判断
        return LOS_ERRNO_QUEUE_NOT_CREATE;
    }

    if (OS_QUEUE_IS_WRITE(operateType) && (*bufferSize > (queueCB->queueSize - sizeof(UINT32)))) {//写时判断
        return LOS_ERRNO_QUEUE_WRITE_SIZE_TOO_BIG;//塞进来的数据太大,大于队列节点能承受的范围
    }
    return LOS_OK;
}

/**
 * @brief 队列操作.是读是写由operateType定
    本函数是消息队列最重要的一个函数,可以分析出读取消息过程中
    发生的细节,涉及任务的唤醒和阻塞,阻塞链表任务的相互提醒.
 * @param queueID 
 * @param operateType 
 * @param bufferAddr 
 * @param bufferSize 
 * @param timeout 
 * @return UINT32 
 */
UINT32 OsQueueOperate(UINT32 queueID, UINT32 operateType, VOID *bufferAddr, UINT32 *bufferSize, UINT32 timeout)
{
    UINT32 ret;
    UINT32 readWrite = OS_QUEUE_READ_WRITE_GET(operateType);//获取读/写操作标识
    UINT32 intSave;
    OsHookCall(LOS_HOOK_TYPE_QUEUE_READ, (LosQueueCB *)GET_QUEUE_HANDLE(queueID), operateType, *bufferSize, timeout);

    SCHEDULER_LOCK(intSave);
    LosQueueCB *queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);//获取对应的队列控制块
    ret = OsQueueOperateParamCheck(queueCB, queueID, operateType, bufferSize);//参数检查
    if (ret != LOS_OK) {
        goto QUEUE_END;
    }

    if (queueCB->readWriteableCnt[readWrite] == 0) {//根据readWriteableCnt判断队列是否有消息读/写
        if (timeout == LOS_NO_WAIT) {//不等待直接退出
            ret = OS_QUEUE_IS_READ(operateType) ? LOS_ERRNO_QUEUE_ISEMPTY : LOS_ERRNO_QUEUE_ISFULL;
            goto QUEUE_END;
        }

        if (!OsPreemptableInSched()) {//不支持抢占式调度
            ret = LOS_ERRNO_QUEUE_PEND_IN_LOCK;
            goto QUEUE_END;
        }
		//任务等待,这里很重要啊,将自己从就绪列表摘除,让出了CPU并发起了调度,并挂在readWriteList[readWrite]上,挂的都等待读/写消息的task
        LosTaskCB *runTask = OsCurrTaskGet();
        OsTaskWaitSetPendMask(OS_TASK_WAIT_QUEUE, queueCB->queueID, timeout);
        ret = runTask->ops->wait(runTask, &queueCB->readWriteList[readWrite], timeout);
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {//唤醒后如果超时了,返回读/写消息失败
            ret = LOS_ERRNO_QUEUE_TIMEOUT;
            goto QUEUE_END;//
        }
    } else {
        queueCB->readWriteableCnt[readWrite]--;//对应队列中计数器--,说明一条消息只能被读/写一次
    }

    OsQueueBufferOperate(queueCB, operateType, bufferAddr, bufferSize);//发起读或写队列操作

    if (!LOS_ListEmpty(&queueCB->readWriteList[!readWrite])) {//如果还有任务在排着队等待读/写入消息(当时不能读/写的原因有可能当时队列满了==)
        LosTaskCB *resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&queueCB->readWriteList[!readWrite]));//取出要读/写消息的任务
        OsTaskWakeClearPendMask(resumedTask);
        resumedTask->ops->wake(resumedTask);
        SCHEDULER_UNLOCK(intSave);
        LOS_MpSchedule(OS_MP_CPU_ALL);//让所有CPU发出调度申请,因为很可能那个要读/写消息的队列是由其他CPU执行
        LOS_Schedule();//申请调度
        return LOS_OK;
    } else {
        queueCB->readWriteableCnt[!readWrite]++;//对应队列读/写中计数器++
    }

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
///接口函数定时读取消息队列
LITE_OS_SEC_TEXT UINT32 LOS_QueueReadCopy(UINT32 queueID,
                                          VOID *bufferAddr,
                                          UINT32 *bufferSize,
                                          UINT32 timeout)
{
    UINT32 ret;
    UINT32 operateType;

    ret = OsQueueReadParameterCheck(queueID, bufferAddr, bufferSize, timeout);//参数检查
    if (ret != LOS_OK) {
        return ret;
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_READ, OS_QUEUE_HEAD);//从头开始读
    return OsQueueOperate(queueID, operateType, bufferAddr, bufferSize, timeout);//定时执行读操作
}
///接口函数从队列头开始写
LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteHeadCopy(UINT32 queueID,
                                               VOID *bufferAddr,
                                               UINT32 bufferSize,
                                               UINT32 timeout)
{
    UINT32 ret;
    UINT32 operateType;

    ret = OsQueueWriteParameterCheck(queueID, bufferAddr, &bufferSize, timeout);//参数检查
    if (ret != LOS_OK) {
        return ret;
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_WRITE, OS_QUEUE_HEAD);//从头开始写
    return OsQueueOperate(queueID, operateType, bufferAddr, &bufferSize, timeout);//执行写操作
}
///接口函数 从队列尾部开始写
LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteCopy(UINT32 queueID,
                                           VOID *bufferAddr,
                                           UINT32 bufferSize,
                                           UINT32 timeout)
{
    UINT32 ret;
    UINT32 operateType;

    ret = OsQueueWriteParameterCheck(queueID, bufferAddr, &bufferSize, timeout);//参数检查
    if (ret != LOS_OK) {
        return ret;
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_WRITE, OS_QUEUE_TAIL);//从尾部开始写
    return OsQueueOperate(queueID, operateType, bufferAddr, &bufferSize, timeout);//执行写操作
}

/**
 * @brief 
 * @verbatim
    外部接口 读一个队列数据
    读队列时，根据Head找到最先写入队列中的消息节点进行读取。如果Head已经指向队列尾则采用回卷方式。
    根据usReadableCnt判断队列是否有消息读取，对全部空闲（usReadableCnt为0）队列进行读队列操作会引起任务挂起。
 * @endverbatim
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueRead(UINT32 queueID, VOID *bufferAddr, UINT32 bufferSize, UINT32 timeout)
{
    return LOS_QueueReadCopy(queueID, bufferAddr, &bufferSize, timeout);
}

/**
 * @brief 
 * @verbatim
    外部接口 写一个队列数据
    根据Tail找到被占用消息节点末尾的空闲节点作为数据写入对象。如果Tail已经指向队列尾则采用回卷方式。
    根据usWritableCnt判断队列是否可以写入，不能对已满（usWritableCnt为0）队列进行写队列操作
 * @endverbatim
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueWrite(UINT32 queueID, VOID *bufferAddr, UINT32 bufferSize, UINT32 timeout)
{
    if (bufferAddr == NULL) {
        return LOS_ERRNO_QUEUE_WRITE_PTR_NULL;
    }
    bufferSize = sizeof(CHAR *);
    return LOS_QueueWriteCopy(queueID, &bufferAddr, bufferSize, timeout);
}

/**
 * @brief 
 * @verbatim
    外部接口 从头部写入
    写队列时，根据Tail找到被占用消息节点末尾的空闲节点作为数据写入对象。如果Tail已经指向队列尾则采用回卷方式。
    根据usWritableCnt判断队列是否可以写入，不能对已满（usWritableCnt为0）队列进行写队列操作
 * @endverbatim
 */
LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteHead(UINT32 queueID,
                                           VOID *bufferAddr,
                                           UINT32 bufferSize,
                                           UINT32 timeout)
{
    if (bufferAddr == NULL) {
        return LOS_ERRNO_QUEUE_WRITE_PTR_NULL;
    }
    bufferSize = sizeof(CHAR *);
    return LOS_QueueWriteHeadCopy(queueID, &bufferAddr, bufferSize, timeout);
}

/**
 * @brief 
 * @verbatim
    外部接口 删除队列,还有任务要读/写消息时不能删除
    删除队列时，根据传入的队列ID寻找到对应的队列，把队列状态置为未使用，
    释放原队列所占的空间，对应的队列控制头置为初始状态。
 * @endverbatim
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_QueueDelete(UINT32 queueID)
{
    LosQueueCB *queueCB = NULL;
    UINT8 *queue = NULL;
    UINT32 intSave;
    UINT32 ret;

    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {
        return LOS_ERRNO_QUEUE_NOT_FOUND;
    }

    SCHEDULER_LOCK(intSave);
    queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);//拿到队列实体
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {
        ret = LOS_ERRNO_QUEUE_NOT_CREATE;
        goto QUEUE_END;
    }

    if (!LOS_ListEmpty(&queueCB->readWriteList[OS_QUEUE_READ])) {//尚有任务要读数据
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;
        goto QUEUE_END;
    }

    if (!LOS_ListEmpty(&queueCB->readWriteList[OS_QUEUE_WRITE])) {//尚有任务要写数据
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;
        goto QUEUE_END;
    }

    if (!LOS_ListEmpty(&queueCB->memList)) {//
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;
        goto QUEUE_END;
    }

    if ((queueCB->readWriteableCnt[OS_QUEUE_WRITE] + queueCB->readWriteableCnt[OS_QUEUE_READ]) !=
        queueCB->queueLen) {//读写队列的内容长度不等于总长度
        ret = LOS_ERRNO_QUEUE_IN_TSKWRITE;
        goto QUEUE_END;
    }

    queue = queueCB->queueHandle;	//队列buf
    queueCB->queueHandle = NULL;	//
    queueCB->queueState = OS_QUEUE_UNUSED;//重置队列状态
    queueCB->queueID = SET_QUEUE_ID(GET_QUEUE_COUNT(queueCB->queueID) + 1, GET_QUEUE_INDEX(queueCB->queueID));//@note_why 这里需要这样做吗?
    OsQueueDbgUpdateHook(queueCB->queueID, NULL);

    LOS_ListTailInsert(&FREE_QUEUE_LIST, &queueCB->readWriteList[OS_QUEUE_WRITE]);//回收，将节点挂入可分配链表,等待重新被分配再利用
    SCHEDULER_UNLOCK(intSave);									
    OsHookCall(LOS_HOOK_TYPE_QUEUE_DELETE, queueCB);
    ret = LOS_MemFree(m_aucSysMem1, (VOID *)queue);//释放队列句柄
    return ret;

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
///外部接口, 获取队列信息,用queueInfo 把 LosQueueCB数据接走,QUEUE_INFO_S对内部数据的封装 
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_QueueInfoGet(UINT32 queueID, QUEUE_INFO_S *queueInfo)
{
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    LosQueueCB *queueCB = NULL;
    LosTaskCB *tskCB = NULL;

    if (queueInfo == NULL) {
        return LOS_ERRNO_QUEUE_PTR_NULL;
    }

    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {//1024
        return LOS_ERRNO_QUEUE_INVALID;
    }

    (VOID)memset_s((VOID *)queueInfo, sizeof(QUEUE_INFO_S), 0, sizeof(QUEUE_INFO_S));//接走数据之前先清0
    SCHEDULER_LOCK(intSave);

    queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);//通过队列ID获取 QCB
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {
        ret = LOS_ERRNO_QUEUE_NOT_CREATE;
        goto QUEUE_END;
    }

    queueInfo->uwQueueID = queueID;
    queueInfo->usQueueLen = queueCB->queueLen;
    queueInfo->usQueueSize = queueCB->queueSize;
    queueInfo->usQueueHead = queueCB->queueHead;
    queueInfo->usQueueTail = queueCB->queueTail;
    queueInfo->usReadableCnt = queueCB->readWriteableCnt[OS_QUEUE_READ];//可读数
    queueInfo->usWritableCnt = queueCB->readWriteableCnt[OS_QUEUE_WRITE];//可写数

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->readWriteList[OS_QUEUE_READ], LosTaskCB, pendList) {//找出哪些task需要读消息
        queueInfo->uwWaitReadTask |= 1ULL << tskCB->taskID;//记录等待读消息的任务号, uwWaitReadTask 每一位代表一个任务编号
    }//0b..011011011 代表   0,1,3,4,6,7 号任务有数据等待读消息.

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->readWriteList[OS_QUEUE_WRITE], LosTaskCB, pendList) {//找出哪些task需要写消息
        queueInfo->uwWaitWriteTask |= 1ULL << tskCB->taskID;//记录等待写消息的任务号, uwWaitWriteTask 每一位代表一个任务编号
    }////0b..011011011 代表   0,1,3,4,6,7 号任务有数据等待写消息.

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->memList, LosTaskCB, pendList) {//同上
        queueInfo->uwWaitMemTask |= 1ULL << tskCB->taskID; //MailBox模块使用
    }

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

#endif /* (LOSCFG_BASE_IPC_QUEUE == YES) */

