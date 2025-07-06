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

#include "los_queue_debug_pri.h"
#include "los_hw_pri.h"
#include "los_ipcdebug_pri.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#endif /* LOSCFG_SHELL */


#ifdef LOSCFG_DEBUG_QUEUE
/**
 * @brief 队列调试控制块结构体定义
 * @struct QueueDebugCB
 * @param creator 创建队列的任务入口函数
 * @param lastAccessTime 最后访问时间
 */
typedef struct {
    TSK_ENTRY_FUNC creator; /* The task entry who created this queue */
    UINT64  lastAccessTime; /* The last access time */
} QueueDebugCB;
STATIC QueueDebugCB *g_queueDebugArray = NULL; /* 队列调试控制块数组指针 */

/**
 * @brief 比较队列调试控制块中的时间值
 * @param sortParam 排序参数结构体指针
 * @param left 左侧元素索引
 * @param right 右侧元素索引
 * @return BOOL 如果左侧时间大于右侧则返回TRUE，否则返回FALSE
 */
STATIC BOOL QueueCompareValue(const IpcSortParam *sortParam, UINT32 left, UINT32 right)
{
    return (*((UINT64 *)(VOID *)SORT_ELEM_ADDR(sortParam, left)) >  // 比较左侧元素时间值
            *((UINT64 *)(VOID *)SORT_ELEM_ADDR(sortParam, right))); // 比较右侧元素时间值
}

/**
 * @brief 初始化队列调试模块
 * @return UINT32 初始化结果，LOS_OK表示成功，LOS_NOK表示失败
 */
UINT32 OsQueueDbgInit(VOID)
{
    UINT32 size = LOSCFG_BASE_IPC_QUEUE_LIMIT * sizeof(QueueDebugCB);  // 计算队列调试控制块数组大小
    /* system resident memory, don't free */
    g_queueDebugArray = (QueueDebugCB *)LOS_MemAlloc(m_aucSysMem1, size);  // 分配队列调试控制块数组内存
    if (g_queueDebugArray == NULL) {  // 检查内存分配是否成功
        PRINT_ERR("%s: malloc failed!\n", __FUNCTION__);  // 打印内存分配失败错误信息
        return LOS_NOK;  // 返回初始化失败
    }
    (VOID)memset_s(g_queueDebugArray, size, 0, size);  // 将队列调试控制块数组初始化为0
    return LOS_OK;  // 返回初始化成功
}

/**
 * @brief 更新队列的最后访问时间
 * @param queueID 队列ID
 * @return VOID
 */
VOID OsQueueDbgTimeUpdate(UINT32 queueID)
{
    QueueDebugCB *queueDebug = &g_queueDebugArray[GET_QUEUE_INDEX(queueID)];  // 获取队列调试控制块指针
    queueDebug->lastAccessTime = LOS_TickCountGet();  // 更新最后访问时间为当前系统滴答数
    return;
}

/**
 * @brief 更新队列调试信息
 * @param queueID 队列ID
 * @param entry 创建队列的任务入口函数
 * @return VOID
 */
VOID OsQueueDbgUpdate(UINT32 queueID, TSK_ENTRY_FUNC entry)
{
    QueueDebugCB *queueDebug = &g_queueDebugArray[GET_QUEUE_INDEX(queueID)];  // 获取队列调试控制块指针
    queueDebug->creator = entry;  // 更新创建者任务入口函数
    queueDebug->lastAccessTime = LOS_TickCountGet();  // 更新最后访问时间
    return;
}

/**
 * @brief 输出队列信息
 * @param node 队列控制块指针
 * @return VOID
 */
STATIC INLINE VOID OsQueueInfoOutPut(const LosQueueCB *node)
{
    PRINTK("Queue ID <0x%x> may leak, queue len is 0x%x, "
           "readable cnt:0x%x, writable cnt:0x%x, ",
           node->queueID,  // 输出队列ID
           node->queueLen,  // 输出队列长度
           node->readWriteableCnt[OS_QUEUE_READ],  // 输出可读计数
           node->readWriteableCnt[OS_QUEUE_WRITE]);  // 输出可写计数
}

/**
 * @brief 输出队列操作信息
 * @param node 队列调试控制块指针
 * @return VOID
 */
STATIC INLINE VOID OsQueueOpsOutput(const QueueDebugCB *node)
{
    PRINTK("TaskEntry of creator:0x%p, Latest operation time: 0x%llx\n",
           node->creator, node->lastAccessTime);  // 输出创建者任务入口和最后操作时间
}

/**
 * @brief 排序队列索引数组
 * @param indexArray 索引数组指针
 * @param count 数组元素数量
 * @return VOID
 */
STATIC VOID SortQueueIndexArray(UINT32 *indexArray, UINT32 count)
{
    LosQueueCB queueNode = {0};  // 队列控制块结构体变量
    QueueDebugCB queueDebugNode = {0};  // 队列调试控制块结构体变量
    UINT32 index, intSave;  // 循环索引和中断保存变量
    IpcSortParam queueSortParam;  // IPC排序参数结构体
    queueSortParam.buf = (CHAR *)g_queueDebugArray;  // 设置排序缓冲区为队列调试控制块数组
    queueSortParam.ipcDebugCBSize = sizeof(QueueDebugCB);  // 设置调试控制块大小
    queueSortParam.ipcDebugCBCnt = LOSCFG_BASE_IPC_QUEUE_LIMIT;  // 设置调试控制块数量
    queueSortParam.sortElemOff = LOS_OFF_SET_OF(QueueDebugCB, lastAccessTime);  // 设置排序元素偏移量

    if (count > 0) {  // 检查计数是否大于0
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        OsArraySortByTime(indexArray, 0, count - 1, &queueSortParam, QueueCompareValue);  // 按时间排序数组
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        for (index = 0; index < count; index++) {  // 遍历索引数组
            SCHEDULER_LOCK(intSave);  // 关闭调度器
            (VOID)memcpy_s(&queueNode, sizeof(LosQueueCB),
                           GET_QUEUE_HANDLE(indexArray[index]), sizeof(LosQueueCB));  // 复制队列控制块信息
            (VOID)memcpy_s(&queueDebugNode, sizeof(QueueDebugCB),
                           &g_queueDebugArray[indexArray[index]], sizeof(QueueDebugCB));  // 复制队列调试控制块信息
            SCHEDULER_UNLOCK(intSave);  // 开启调度器
            if (queueNode.queueState == OS_QUEUE_UNUSED) {  // 检查队列状态是否为未使用
                continue;  // 跳过未使用队列
            }
            OsQueueInfoOutPut(&queueNode);  // 输出队列信息
            OsQueueOpsOutput(&queueDebugNode);  // 输出队列操作信息
        }
    }
    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, indexArray);  // 释放索引数组内存
}

/**
 * @brief 检查队列状态
 * @return VOID
 */
VOID OsQueueCheck(VOID)
{
    LosQueueCB queueNode = {0};  // 队列控制块结构体变量
    QueueDebugCB queueDebugNode = {0};  // 队列调试控制块结构体变量
    UINT32 index, intSave;  // 循环索引和中断保存变量
    UINT32 count = 0;  // 计数变量

    /*
     * This return value does not need to be judged immediately,
     * and the following code logic has already distinguished the return value from null and non-empty,
     * and there is no case of accessing the null pointer.
     */
    UINT32 *indexArray = (UINT32 *)LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, LOSCFG_BASE_IPC_QUEUE_LIMIT * sizeof(UINT32));  // 分配索引数组内存

    for (index = 0; index < LOSCFG_BASE_IPC_QUEUE_LIMIT; index++) {  // 遍历所有队列
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        (VOID)memcpy_s(&queueNode, sizeof(LosQueueCB),
                       GET_QUEUE_HANDLE(index), sizeof(LosQueueCB));  // 复制队列控制块信息
        (VOID)memcpy_s(&queueDebugNode, sizeof(QueueDebugCB),
                       &g_queueDebugArray[index], sizeof(QueueDebugCB));  // 复制队列调试控制块信息
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        if ((queueNode.queueState == OS_QUEUE_UNUSED) ||  // 检查队列状态是否为未使用
            ((queueNode.queueState == OS_QUEUE_INUSED) && (queueDebugNode.creator == NULL))) {  // 检查队列是否已使用但创建者为空
            continue;  // 跳过不符合条件的队列
        }
        if ((queueNode.queueState == OS_QUEUE_INUSED) &&  // 检查队列状态是否为已使用
            (queueNode.queueLen == queueNode.readWriteableCnt[OS_QUEUE_WRITE]) &&  // 检查队列长度是否等于可写计数
            LOS_ListEmpty(&queueNode.readWriteList[OS_QUEUE_READ]) &&  // 检查读列表是否为空
            LOS_ListEmpty(&queueNode.readWriteList[OS_QUEUE_WRITE]) &&  // 检查写列表是否为空
            LOS_ListEmpty(&queueNode.memList)) {  // 检查内存列表是否为空
            PRINTK("Queue ID <0x%x> may leak, No task uses it, "
                   "QueueLen is 0x%x, ",
                   queueNode.queueID,  // 输出队列ID
                   queueNode.queueLen);  // 输出队列长度
            OsQueueOpsOutput(&queueDebugNode);  // 输出队列操作信息
        } else {
            if (indexArray != NULL) {  // 检查索引数组是否为空
                *(indexArray + count) = index;  // 将索引添加到数组
                count++;  // 增加计数
            } else {
                OsQueueInfoOutPut(&queueNode);  // 输出队列信息
                OsQueueOpsOutput(&queueDebugNode);  // 输出队列操作信息
            }
        }
    }

    if (indexArray != NULL) {  // 检查索引数组是否为空
        SortQueueIndexArray(indexArray, count);  // 排序索引数组
    }

    return;
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
/**
 * @brief 获取队列信息的shell命令实现
 * @param argc 参数数量
 * @param argv 参数数组
 * @return UINT32 执行结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdQueueInfoGet(UINT32 argc, const CHAR **argv)
{
    if (argc > 0) {  // 检查参数数量是否大于0
        PRINTK("\nUsage: queue\n");  // 输出使用方法
        return OS_ERROR;  // 返回错误
    }
    PRINTK("used queues information: \n");  // 输出已使用队列信息标题
    OsQueueCheck();  // 检查并输出队列信息
    return LOS_OK;  // 返回成功
}
#endif
SHELLCMD_ENTRY(queue_shellcmd, CMD_TYPE_EX, "queue", 0, (CmdCallBackFunc)OsShellCmdQueueInfoGet);
#endif /* LOSCFG_SHELL */
#endif /* LOSCFG_DEBUG_QUEUE */

