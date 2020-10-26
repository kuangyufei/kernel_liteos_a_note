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

#include "los_queue_pri.h"
#include "los_queue_debug_pri.h"
#include "los_task_pri.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_percpu_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#if (LOSCFG_BASE_IPC_QUEUE == YES)
#if (LOSCFG_BASE_IPC_QUEUE_LIMIT <= 0)
#error "queue maxnum cannot be zero"
#endif /* LOSCFG_BASE_IPC_QUEUE_LIMIT <= 0 */

LITE_OS_SEC_BSS LosQueueCB *g_allQueue = NULL;//管理所有IPC队列
LITE_OS_SEC_BSS STATIC LOS_DL_LIST g_freeQueueList;//IPC空闲队列链表,管分配的,需要队列就从free中申请

/*
 * Description : queue initial
 * Return      : LOS_OK on success or error code on failure
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsQueueInit(VOID)//IPC 队列初始化
{
    LosQueueCB *queueNode = NULL;
    UINT32 index;
    UINT32 size;

    size = LOSCFG_BASE_IPC_QUEUE_LIMIT * sizeof(LosQueueCB);//支持1024个IPC队列
    /* system resident memory, don't free */
    g_allQueue = (LosQueueCB *)LOS_MemAlloc(m_aucSysMem0, size);//常驻内存
    if (g_allQueue == NULL) {
        return LOS_ERRNO_QUEUE_NO_MEMORY;
    }
    (VOID)memset_s(g_allQueue, size, 0, size);//清0
    LOS_ListInit(&g_freeQueueList);//初始化空闲链表
    for (index = 0; index < LOSCFG_BASE_IPC_QUEUE_LIMIT; index++) {//循环
        queueNode = ((LosQueueCB *)g_allQueue) + index;//取item
        queueNode->queueID = index;//记录队列index
        LOS_ListTailInsert(&g_freeQueueList, &queueNode->readWriteList[OS_QUEUE_WRITE]);//挂入空闲队列链表上
    }//这里要注意是用 readWriteList 挂到 g_freeQueueList链上的,所以要通过 GET_QUEUE_LIST 来找到 LosQueueCB

    if (OsQueueDbgInitHook() != LOS_OK) {
        return LOS_ERRNO_QUEUE_NO_MEMORY;
    }
    return LOS_OK;
}
//创建一个队列 maxMsgSize有时会 = sizeof(CHAR *)
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
    queue = (UINT8 *)LOS_MemAlloc(m_aucSysMem1, (UINT32)len * msgSize);//从系统内存池中分配
    if (queue == NULL) {
        return LOS_ERRNO_QUEUE_CREATE_NO_MEMORY;
    }

    SCHEDULER_LOCK(intSave);
    if (LOS_ListEmpty(&g_freeQueueList)) {//没有空余的队列ID的处理,注意软时钟定时器是由 g_swtmrCBArray统一管理的,里面有正在使用和可分配空闲的队列
        SCHEDULER_UNLOCK(intSave);//g_freeQueueList是管理可用于分配的队列链表,申请消息队列的ID需要向它要
        OsQueueCheckHook();
        (VOID)LOS_MemFree(m_aucSysMem1, queue);//没有就要释放 queue 内存
        return LOS_ERRNO_QUEUE_CB_UNAVAILABLE;
    }

    unusedQueue = LOS_DL_LIST_FIRST(&g_freeQueueList);//找到一个米有被使用的队列
    LOS_ListDelete(unusedQueue);//将自己从g_freeQueueList中摘除, unusedQueue只是个 LOS_DL_LIST 结点.
    queueCB = GET_QUEUE_LIST(unusedQueue);//通过unusedQueue找到LosQueueCB
    queueCB->queueLen = len;	//队列长度
    queueCB->queueSize = msgSize;//消息大小
    queueCB->queueHandle = queue;	//
    queueCB->queueState = OS_QUEUE_INUSED;	//队列状态使用中
    queueCB->readWriteableCnt[OS_QUEUE_READ] = 0;//可读资源计数，OS_QUEUE_READ(0):可读，
    queueCB->readWriteableCnt[OS_QUEUE_WRITE] = len;//可些资源计数 OS_QUEUE_WRITE(1):可写
    queueCB->queueHead = 0;//队列都节点
    queueCB->queueTail = 0;//队列尾节点
    LOS_ListInit(&queueCB->readWriteList[OS_QUEUE_READ]);//初始化可读队列链表
    LOS_ListInit(&queueCB->readWriteList[OS_QUEUE_WRITE]);//初始化可写队列链表
    LOS_ListInit(&queueCB->memList);//初始化内存链表队列

    OsQueueDbgUpdateHook(queueCB->queueID, OsCurrTaskGet()->taskEntry);//在创建或删除队列调试信息时更新任务条目
    SCHEDULER_UNLOCK(intSave);

    *queueID = queueCB->queueID;//带走队列ID
    return LOS_OK;
}
//读队列参数检查
STATIC LITE_OS_SEC_TEXT UINT32 OsQueueReadParameterCheck(UINT32 queueID, const VOID *bufferAddr,
                                                         const UINT32 *bufferSize, UINT32 timeout)
{
    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {//队列ID不能超上限
        return LOS_ERRNO_QUEUE_INVALID;
    }
    if ((bufferAddr == NULL) || (bufferSize == NULL)) {//目的地不能为NULL
        return LOS_ERRNO_QUEUE_READ_PTR_NULL;
    }

    if ((*bufferSize == 0) || (*bufferSize > (OS_NULL_SHORT - sizeof(UINT32)))) {//限制了读取数据的上限64K少一点点 OS_NULL_SHORT = 0XFFFF 
        return LOS_ERRNO_QUEUE_READSIZE_IS_INVALID;
    }

    OsQueueDbgTimeUpdateHook(queueID);

    if (timeout != LOS_NO_WAIT) {
        if (OS_INT_ACTIVE) {
            return LOS_ERRNO_QUEUE_READ_IN_INTERRUPT;
        }
    }
    return LOS_OK;
}
//写队列参数检查
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
//队列buf操作
STATIC VOID OsQueueBufferOperate(LosQueueCB *queueCB, UINT32 operateType, VOID *bufferAddr, UINT32 *bufferSize)
{
    UINT8 *queueNode = NULL;
    UINT32 msgDataSize;
    UINT16 queuePosion;

    /* get the queue position */
    switch (OS_QUEUE_OPERATE_GET(operateType)) {//获取操作类型
        case OS_QUEUE_READ_HEAD://从列队头开始读
            queuePosion = queueCB->queueHead;//拿到头部位置
            ((queueCB->queueHead + 1) == queueCB->queueLen) ? (queueCB->queueHead = 0) : (queueCB->queueHead++);//调整队列头部位置
            break;
        case OS_QUEUE_WRITE_HEAD://从列队头开始写
            (queueCB->queueHead == 0) ? (queueCB->queueHead = queueCB->queueLen - 1) : (--queueCB->queueHead);//调整队列头部位置
            queuePosion = queueCB->queueHead;//拿到头部位置
            break;
        case OS_QUEUE_WRITE_TAIL://从列队尾部开始写
            queuePosion = queueCB->queueTail;//设置队列位置为尾部位置
            ((queueCB->queueTail + 1) == queueCB->queueLen) ? (queueCB->queueTail = 0) : (queueCB->queueTail++);//调整队列尾部位置
            break;
        default:  /* read tail, reserved. */
            PRINT_ERR("invalid queue operate type!\n");
            return;
    }

    queueNode = &(queueCB->queueHandle[(queuePosion * (queueCB->queueSize))]);//拿到队列节点

    if (OS_QUEUE_IS_READ(operateType)) {//读操作处理,读队列分两步走
        if (memcpy_s(&msgDataSize, sizeof(UINT32), queueNode + queueCB->queueSize - sizeof(UINT32),
            sizeof(UINT32)) != EOK) {//1.先读出队列大小,由队列头四个字节表示
            PRINT_ERR("get msgdatasize failed\n");
            return;
        }
        if (memcpy_s(bufferAddr, *bufferSize, queueNode, msgDataSize) != EOK) {//2.通过大小读取整个消息体
            PRINT_ERR("copy message to buffer failed\n");
            return;
        }

        *bufferSize = msgDataSize;//通过入参 带走消息的大小
    } else {//只有读写两种操作,这里就是写队列了.写也分两步走
        if (memcpy_s(queueNode, queueCB->queueSize, bufferAddr, *bufferSize) != EOK) {//1.写入消息内容长度 UINT32表示
            PRINT_ERR("store message failed\n");
            return;
        }
        if (memcpy_s(queueNode + queueCB->queueSize - sizeof(UINT32), sizeof(UINT32), bufferSize,
            sizeof(UINT32)) != EOK) {//2.写入消息数据
            PRINT_ERR("store message size failed\n");
            return;
        }
    }
}

STATIC UINT32 OsQueueOperateParamCheck(const LosQueueCB *queueCB, UINT32 queueID,
                                       UINT32 operateType, const UINT32 *bufferSize)
{
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {
        return LOS_ERRNO_QUEUE_NOT_CREATE;
    }

    if (OS_QUEUE_IS_READ(operateType) && (*bufferSize < (queueCB->queueSize - sizeof(UINT32)))) {
        return LOS_ERRNO_QUEUE_READ_SIZE_TOO_SMALL;
    } else if (OS_QUEUE_IS_WRITE(operateType) && (*bufferSize > (queueCB->queueSize - sizeof(UINT32)))) {
        return LOS_ERRNO_QUEUE_WRITE_SIZE_TOO_BIG;
    }
    return LOS_OK;
}

UINT32 OsQueueOperate(UINT32 queueID, UINT32 operateType, VOID *bufferAddr, UINT32 *bufferSize, UINT32 timeout)
{
    LosQueueCB *queueCB = NULL;
    LosTaskCB *resumedTask = NULL;
    UINT32 ret;
    UINT32 readWrite = OS_QUEUE_READ_WRITE_GET(operateType);
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);
    ret = OsQueueOperateParamCheck(queueCB, queueID, operateType, bufferSize);
    if (ret != LOS_OK) {
        goto QUEUE_END;
    }

    if (queueCB->readWriteableCnt[readWrite] == 0) {
        if (timeout == LOS_NO_WAIT) {
            ret = OS_QUEUE_IS_READ(operateType) ? LOS_ERRNO_QUEUE_ISEMPTY : LOS_ERRNO_QUEUE_ISFULL;
            goto QUEUE_END;
        }

        if (!OsPreemptableInSched()) {
            ret = LOS_ERRNO_QUEUE_PEND_IN_LOCK;
            goto QUEUE_END;
        }

        ret = OsTaskWait(&queueCB->readWriteList[readWrite], timeout, TRUE);
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {
            ret = LOS_ERRNO_QUEUE_TIMEOUT;
            goto QUEUE_END;
        }
    } else {
        queueCB->readWriteableCnt[readWrite]--;
    }

    OsQueueBufferOperate(queueCB, operateType, bufferAddr, bufferSize);

    if (!LOS_ListEmpty(&queueCB->readWriteList[!readWrite])) {
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&queueCB->readWriteList[!readWrite]));
        OsTaskWake(resumedTask);
        SCHEDULER_UNLOCK(intSave);
        LOS_MpSchedule(OS_MP_CPU_ALL);
        LOS_Schedule();
        return LOS_OK;
    } else {
        queueCB->readWriteableCnt[!readWrite]++;
    }

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

LITE_OS_SEC_TEXT UINT32 LOS_QueueReadCopy(UINT32 queueID,
                                          VOID *bufferAddr,
                                          UINT32 *bufferSize,
                                          UINT32 timeout)
{
    UINT32 ret;
    UINT32 operateType;

    ret = OsQueueReadParameterCheck(queueID, bufferAddr, bufferSize, timeout);
    if (ret != LOS_OK) {
        return ret;
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_READ, OS_QUEUE_HEAD);
    return OsQueueOperate(queueID, operateType, bufferAddr, bufferSize, timeout);
}

LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteHeadCopy(UINT32 queueID,
                                               VOID *bufferAddr,
                                               UINT32 bufferSize,
                                               UINT32 timeout)
{
    UINT32 ret;
    UINT32 operateType;

    ret = OsQueueWriteParameterCheck(queueID, bufferAddr, &bufferSize, timeout);
    if (ret != LOS_OK) {
        return ret;
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_WRITE, OS_QUEUE_HEAD);
    return OsQueueOperate(queueID, operateType, bufferAddr, &bufferSize, timeout);
}

LITE_OS_SEC_TEXT UINT32 LOS_QueueWriteCopy(UINT32 queueID,
                                           VOID *bufferAddr,
                                           UINT32 bufferSize,
                                           UINT32 timeout)
{
    UINT32 ret;
    UINT32 operateType;

    ret = OsQueueWriteParameterCheck(queueID, bufferAddr, &bufferSize, timeout);
    if (ret != LOS_OK) {
        return ret;
    }

    operateType = OS_QUEUE_OPERATE_TYPE(OS_QUEUE_WRITE, OS_QUEUE_TAIL);
    return OsQueueOperate(queueID, operateType, bufferAddr, &bufferSize, timeout);
}

LITE_OS_SEC_TEXT UINT32 LOS_QueueRead(UINT32 queueID, VOID *bufferAddr, UINT32 bufferSize, UINT32 timeout)
{
    return LOS_QueueReadCopy(queueID, bufferAddr, &bufferSize, timeout);
}

LITE_OS_SEC_TEXT UINT32 LOS_QueueWrite(UINT32 queueID, VOID *bufferAddr, UINT32 bufferSize, UINT32 timeout)
{
    if (bufferAddr == NULL) {
        return LOS_ERRNO_QUEUE_WRITE_PTR_NULL;
    }
    bufferSize = sizeof(CHAR *);
    return LOS_QueueWriteCopy(queueID, &bufferAddr, bufferSize, timeout);
}

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
    queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {
        ret = LOS_ERRNO_QUEUE_NOT_CREATE;
        goto QUEUE_END;
    }

    if (!LOS_ListEmpty(&queueCB->readWriteList[OS_QUEUE_READ])) {
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;
        goto QUEUE_END;
    }

    if (!LOS_ListEmpty(&queueCB->readWriteList[OS_QUEUE_WRITE])) {
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;
        goto QUEUE_END;
    }

    if (!LOS_ListEmpty(&queueCB->memList)) {
        ret = LOS_ERRNO_QUEUE_IN_TSKUSE;
        goto QUEUE_END;
    }

    if ((queueCB->readWriteableCnt[OS_QUEUE_WRITE] + queueCB->readWriteableCnt[OS_QUEUE_READ]) !=
        queueCB->queueLen) {
        ret = LOS_ERRNO_QUEUE_IN_TSKWRITE;
        goto QUEUE_END;
    }

    queue = queueCB->queueHandle;
    queueCB->queueHandle = NULL;
    queueCB->queueState = OS_QUEUE_UNUSED;
    queueCB->queueID = SET_QUEUE_ID(GET_QUEUE_COUNT(queueCB->queueID) + 1, GET_QUEUE_INDEX(queueCB->queueID));
    OsQueueDbgUpdateHook(queueCB->queueID, NULL);

    LOS_ListTailInsert(&g_freeQueueList, &queueCB->readWriteList[OS_QUEUE_WRITE]);
    SCHEDULER_UNLOCK(intSave);

    ret = LOS_MemFree(m_aucSysMem1, (VOID *)queue);
    return ret;

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

LITE_OS_SEC_TEXT_MINOR UINT32 LOS_QueueInfoGet(UINT32 queueID, QUEUE_INFO_S *queueInfo)
{
    UINT32 intSave;
    UINT32 ret = LOS_OK;
    LosQueueCB *queueCB = NULL;
    LosTaskCB *tskCB = NULL;

    if (queueInfo == NULL) {
        return LOS_ERRNO_QUEUE_PTR_NULL;
    }

    if (GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {
        return LOS_ERRNO_QUEUE_INVALID;
    }

    (VOID)memset_s((VOID *)queueInfo, sizeof(QUEUE_INFO_S), 0, sizeof(QUEUE_INFO_S));
    SCHEDULER_LOCK(intSave);

    queueCB = (LosQueueCB *)GET_QUEUE_HANDLE(queueID);
    if ((queueCB->queueID != queueID) || (queueCB->queueState == OS_QUEUE_UNUSED)) {
        ret = LOS_ERRNO_QUEUE_NOT_CREATE;
        goto QUEUE_END;
    }

    queueInfo->uwQueueID = queueID;
    queueInfo->usQueueLen = queueCB->queueLen;
    queueInfo->usQueueSize = queueCB->queueSize;
    queueInfo->usQueueHead = queueCB->queueHead;
    queueInfo->usQueueTail = queueCB->queueTail;
    queueInfo->usReadableCnt = queueCB->readWriteableCnt[OS_QUEUE_READ];
    queueInfo->usWritableCnt = queueCB->readWriteableCnt[OS_QUEUE_WRITE];

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->readWriteList[OS_QUEUE_READ], LosTaskCB, pendList) {
        queueInfo->uwWaitReadTask |= 1ULL << tskCB->taskID;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->readWriteList[OS_QUEUE_WRITE], LosTaskCB, pendList) {
        queueInfo->uwWaitWriteTask |= 1ULL << tskCB->taskID;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &queueCB->memList, LosTaskCB, pendList) {
        queueInfo->uwWaitMemTask |= 1ULL << tskCB->taskID;
    }

QUEUE_END:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

#endif /* (LOSCFG_BASE_IPC_QUEUE == YES) */

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
