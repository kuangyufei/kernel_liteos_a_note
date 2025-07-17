/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "trace_cnv.h"
#include "los_trace.h"
#include "los_task.h"
#include "los_sem.h"
#include "los_mux.h"
#include "los_queue.h"
#include "los_event.h"
#include "los_swtmr.h"
#include "hm_liteipc.h"
#include "los_hook.h"
/**
 * @brief 跟踪内存池初始化事件
 * @param pool 内存池指针
 * @param size 内存池大小
 * @return 无
 */
STATIC VOID LOS_TraceMemInit(VOID *pool, UINT32 size)
{
    LOS_TRACE(MEM_INFO_REQ, pool);  // 发送内存信息请求事件，参数为内存池指针
}

/**
 * @brief 跟踪内存分配事件
 * @param pool 内存池指针
 * @param ptr 分配的内存指针
 * @param size 分配的内存大小
 * @return 无
 */
STATIC VOID LOS_TraceMemAlloc(VOID *pool, VOID *ptr, UINT32 size)
{
    LOS_TRACE(MEM_ALLOC, pool, (UINTPTR)ptr, size);  // 发送内存分配事件，参数为内存池、分配地址和大小
}

/**
 * @brief 跟踪内存释放事件
 * @param pool 内存池指针
 * @param ptr 释放的内存指针
 * @return 无
 */
STATIC VOID LOS_TraceMemFree(VOID *pool, VOID *ptr)
{
    LOS_TRACE(MEM_FREE, pool, (UINTPTR)ptr);  // 发送内存释放事件，参数为内存池和释放地址
}

/**
 * @brief 跟踪内存重分配事件
 * @param pool 内存池指针
 * @param ptr 重分配的内存指针
 * @param size 新分配的内存大小
 * @return 无
 */
STATIC VOID LOS_TraceMemRealloc(VOID *pool, VOID *ptr, UINT32 size)
{
    LOS_TRACE(MEM_REALLOC, pool, (UINTPTR)ptr, size);  // 发送内存重分配事件，参数为内存池、地址和新大小
}

/**
 * @brief 跟踪内存对齐分配事件
 * @param pool 内存池指针
 * @param ptr 分配的内存指针
 * @param size 分配的内存大小
 * @param boundary 对齐边界
 * @return 无
 */
STATIC VOID LOS_TraceMemAllocAlign(VOID *pool, VOID *ptr, UINT32 size, UINT32 boundary)
{
    LOS_TRACE(MEM_ALLOC_ALIGN, pool, (UINTPTR)ptr, size, boundary);  // 发送对齐分配事件，参数含对齐边界
}

/**
 * @brief 跟踪事件初始化事件
 * @param eventCB 事件控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceEventInit(PEVENT_CB_S eventCB)
{
    LOS_TRACE(EVENT_CREATE, (UINTPTR)eventCB);  // 发送事件创建事件，参数为事件控制块地址
}

/**
 * @brief 跟踪事件读取操作
 * @param eventCB 事件控制块指针
 * @param eventMask 事件掩码
 * @param mode 读取模式
 * @param timeout 超时时间
 * @return 无
 */
STATIC VOID LOS_TraceEventRead(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode, UINT32 timeout)
{
    LOS_TRACE(EVENT_READ, (UINTPTR)eventCB, eventCB->uwEventID, eventMask, mode, timeout);  // 发送事件读取事件，包含事件ID和超时参数
}

/**
 * @brief 跟踪事件写入操作
 * @param eventCB 事件控制块指针
 * @param events 事件值
 * @return 无
 */
STATIC VOID LOS_TraceEventWrite(PEVENT_CB_S eventCB, UINT32 events)
{
    LOS_TRACE(EVENT_WRITE, (UINTPTR)eventCB, eventCB->uwEventID, events);  // 发送事件写入事件，包含事件ID和写入值
}

/**
 * @brief 跟踪事件清除操作
 * @param eventCB 事件控制块指针
 * @param events 要清除的事件
 * @return 无
 */
STATIC VOID LOS_TraceEventClear(PEVENT_CB_S eventCB, UINT32 events)
{
    LOS_TRACE(EVENT_CLEAR, (UINTPTR)eventCB, eventCB->uwEventID, events);  // 发送事件清除事件，包含事件ID和清除值
}

/**
 * @brief 跟踪事件销毁操作
 * @param eventCB 事件控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceEventDestroy(PEVENT_CB_S eventCB)
{
    LOS_TRACE(EVENT_DELETE, (UINTPTR)eventCB, LOS_OK);  // 发送事件销毁事件，参数为事件控制块地址和成功状态码
}

/**
 * @brief 跟踪队列创建事件
 * @param queueCB 队列控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceQueueCreate(const LosQueueCB *queueCB)
{
    LOS_TRACE(QUEUE_CREATE, queueCB->queueID, queueCB->queueLen, queueCB->queueSize - sizeof(UINT32),
        (UINTPTR)queueCB, 0);  // 发送队列创建事件，包含队列ID、长度、实际数据大小(减去头部UINT32)
}

/**
 * @brief 跟踪队列读写操作
 * @param queueCB 队列控制块指针
 * @param operateType 操作类型(读/写)
 * @param bufferSize 缓冲区大小
 * @param timeout 超时时间
 * @return 无
 */
STATIC VOID LOS_TraceQueueRW(const LosQueueCB *queueCB, UINT32 operateType,
                    UINT32 bufferSize, UINT32 timeout)
{
    LOS_TRACE(QUEUE_RW, queueCB->queueID, queueCB->queueSize, bufferSize, operateType,
        queueCB->readWriteableCnt[OS_QUEUE_READ], queueCB->readWriteableCnt[OS_QUEUE_WRITE], timeout);  // 发送队列读写事件，包含读写计数和超时参数
}

/**
 * @brief 跟踪队列删除事件
 * @param queueCB 队列控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceQueueDelete(const LosQueueCB *queueCB)
{
    LOS_TRACE(QUEUE_DELETE, queueCB->queueID, queueCB->queueState, queueCB->readWriteableCnt[OS_QUEUE_READ]);  // 发送队列删除事件，包含队列状态和可读计数
}

/**
 * @brief 跟踪信号量创建事件
 * @param semCB 信号量控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceSemCreate(const LosSemCB *semCB)
{
    LOS_TRACE(SEM_CREATE, semCB->semID, 0, semCB->semCount);  // 发送信号量创建事件，包含信号量ID和初始计数
}

/**
 * @brief 跟踪信号量释放操作
 * @param semCB 信号量控制块指针
 * @param resumedTask 被唤醒的任务(未使用)
 * @return 无
 */
STATIC VOID LOS_TraceSemPost(const LosSemCB *semCB, const LosTaskCB *resumedTask)
{
    (VOID)resumedTask;  // 未使用参数，显式转换避免编译警告
    LOS_TRACE(SEM_POST, semCB->semID, 0, semCB->semCount);  // 发送信号量释放事件，包含信号量ID和当前计数
}

/**
 * @brief 跟踪信号量等待操作
 * @param semCB 信号量控制块指针
 * @param runningTask 当前运行任务(未使用)
 * @param timeout 超时时间
 * @return 无
 */
STATIC VOID LOS_TraceSemPend(const LosSemCB *semCB, const LosTaskCB *runningTask, UINT32 timeout)
{
    (VOID)runningTask;  // 未使用参数，显式转换避免编译警告
    LOS_TRACE(SEM_PEND, semCB->semID, semCB->semCount, timeout);  // 发送信号量等待事件，包含信号量ID、当前计数和超时
}

/**
 * @brief 跟踪信号量删除事件
 * @param semCB 信号量控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceSemDelete(const LosSemCB *semCB)
{
    LOS_TRACE(SEM_DELETE, semCB->semID, LOS_OK);  // 发送信号量删除事件，包含信号量ID和成功状态码
}

/**
 * @brief 跟踪互斥锁创建事件
 * @param muxCB 互斥锁控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceMuxCreate(const LosMux *muxCB)
{
    LOS_TRACE(MUX_CREATE, (UINTPTR)muxCB);  // 发送互斥锁创建事件，参数为互斥锁控制块地址
}

/**
 * @brief 跟踪互斥锁释放操作
 * @param muxCB 互斥锁控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceMuxPost(const LosMux *muxCB)
{
    LOS_TRACE(MUX_POST, (UINTPTR)muxCB, muxCB->muxCount,
        (muxCB->owner == NULL) ? 0xffffffff : ((LosTaskCB *)muxCB->owner)->taskID);  // 发送互斥锁释放事件，包含锁计数和所有者任务ID(0xffffffff表示无所有者)
}

/**
 * @brief 跟踪互斥锁等待操作
 * @param muxCB 互斥锁控制块指针
 * @param timeout 超时时间
 * @return 无
 */
STATIC VOID LOS_TraceMuxPend(const LosMux *muxCB, UINT32 timeout)
{
    LOS_TRACE(MUX_PEND, (UINTPTR)muxCB, muxCB->muxCount,
        (muxCB->owner == NULL) ? 0xffffffff : ((LosTaskCB *)muxCB->owner)->taskID, timeout);  // 发送互斥锁等待事件，包含锁计数、所有者任务ID和超时
}

/**
 * @brief 跟踪互斥锁删除事件
 * @param muxCB 互斥锁控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceMuxDelete(const LosMux *muxCB)
{
    LOS_TRACE(MUX_DELETE, (UINTPTR)muxCB, muxCB->attr.type, muxCB->muxCount,
        (muxCB->owner == NULL) ? 0xffffffff : ((LosTaskCB *)muxCB->owner)->taskID);  // 发送互斥锁删除事件，包含锁类型、计数和所有者任务ID
}

/**
 * @brief 跟踪任务创建事件
 * @param taskCB 任务控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceTaskCreate(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_TRACE
    SchedParam param = { 0 };  // 调度参数结构体初始化
    taskCB->ops->schedParamGet(taskCB, &param);  // 获取任务调度参数
    LOS_TRACE(TASK_CREATE, taskCB->taskID, taskCB->taskStatus, param.priority);  // 发送任务创建事件，包含任务ID、状态和优先级
#else
    (VOID)taskCB;  // 未启用跟踪时，未使用参数显式转换避免警告
#endif
}

/**
 * @brief 跟踪任务优先级修改事件
 * @param taskCB 任务控制块指针
 * @param prio 新优先级
 * @return 无
 */
STATIC VOID LOS_TraceTaskPriModify(const LosTaskCB *taskCB, UINT32 prio)
{
#ifdef LOSCFG_KERNEL_TRACE
    SchedParam param = { 0 };  // 调度参数结构体初始化
    taskCB->ops->schedParamGet(taskCB, &param);  // 获取当前调度参数
    LOS_TRACE(TASK_PRIOSET, taskCB->taskID, taskCB->taskStatus, param.priority, prio);  // 发送优先级修改事件，包含原优先级和新优先级
#else
    (VOID)taskCB;  // 未启用跟踪时，未使用参数显式转换避免警告
    (VOID)prio;   // 未使用参数，显式转换避免编译警告
#endif
}

/**
 * @brief 跟踪任务删除事件
 * @param taskCB 任务控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceTaskDelete(const LosTaskCB *taskCB)
{
    LOS_TRACE(TASK_DELETE, taskCB->taskID, taskCB->taskStatus, (UINTPTR)taskCB->stackPointer);  // 发送任务删除事件，包含任务ID、状态和栈指针
}

/**
 * @brief 跟踪任务切换事件
 * @param newTask 新切换的任务
 * @param runTask 原运行任务
 * @return 无
 */
STATIC VOID LOS_TraceTaskSwitchedIn(const LosTaskCB *newTask, const LosTaskCB *runTask)
{
#ifdef LOSCFG_KERNEL_TRACE
    SchedParam runParam = { 0 };  // 原任务调度参数
    SchedParam newParam = { 0 };  // 新任务调度参数
    runTask->ops->schedParamGet(runTask, &runParam);  // 获取原任务优先级
    newTask->ops->schedParamGet(newTask, &newParam);  // 获取新任务优先级
    LOS_TRACE(TASK_SWITCH, newTask->taskID, runParam.priority, runTask->taskStatus,
              newParam.priority, newTask->taskStatus);  // 发送任务切换事件，包含新旧任务ID、优先级和状态
#else
    (VOID)newTask;  // 未启用跟踪时，未使用参数显式转换避免警告
    (VOID)runTask;  // 未使用参数，显式转换避免编译警告
#endif
}

/**
 * @brief 跟踪任务恢复事件
 * @param taskCB 任务控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceTaskResume(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_TRACE
    SchedParam param = { 0 };  // 调度参数结构体初始化
    taskCB->ops->schedParamGet(taskCB, &param);  // 获取任务优先级
    LOS_TRACE(TASK_RESUME, taskCB->taskID, taskCB->taskStatus, param.priority);  // 发送任务恢复事件，包含任务ID、状态和优先级
#else
    (VOID)taskCB;  // 未启用跟踪时，未使用参数显式转换避免警告
#endif
}

/**
 * @brief 跟踪任务挂起事件
 * @param taskCB 任务控制块指针
 * @return 无
 */
STATIC VOID LOS_TraceTaskSuspend(const LosTaskCB *taskCB)
{
    LOS_TRACE(TASK_SUSPEND, taskCB->taskID, taskCB->taskStatus, OsCurrTaskGet()->taskID);  // 发送任务挂起事件，包含任务ID、状态和当前任务ID
}

/**
 * @brief 跟踪中断进入事件
 * @param hwiNum 硬件中断号
 * @return 无
 */
STATIC VOID LOS_TraceIsrEnter(UINT32 hwiNum)
{
    LOS_TRACE(HWI_RESPONSE_IN, hwiNum);  // 发送中断进入事件，参数为中断号
}

/**
 * @brief 跟踪中断退出事件
 * @param hwiNum 硬件中断号
 * @return 无
 */
STATIC VOID LOS_TraceIsrExit(UINT32 hwiNum)
{
    LOS_TRACE(HWI_RESPONSE_OUT, hwiNum);  // 发送中断退出事件，参数为中断号
}

/**
 * @brief 跟踪软件定时器创建事件
 * @param swtmr 软件定时器控制块
 * @return 无
 */
STATIC VOID LOS_TraceSwtmrCreate(const SWTMR_CTRL_S *swtmr)
{
    LOS_TRACE(SWTMR_CREATE, swtmr->usTimerID);  // 发送定时器创建事件，参数为定时器ID
}

/**
 * @brief 跟踪软件定时器删除事件
 * @param swtmr 软件定时器控制块
 * @return 无
 */
STATIC VOID LOS_TraceSwtmrDelete(const SWTMR_CTRL_S *swtmr)
{
    LOS_TRACE(SWTMR_DELETE, swtmr->usTimerID);  // 发送定时器删除事件，参数为定时器ID
}

/**
 * @brief 跟踪软件定时器超时事件
 * @param swtmr 软件定时器控制块
 * @return 无
 */
STATIC VOID LOS_TraceSwtmrExpired(const SWTMR_CTRL_S *swtmr)
{
    LOS_TRACE(SWTMR_EXPIRED, swtmr->usTimerID);  // 发送定时器超时事件，参数为定时器ID
}

/**
 * @brief 跟踪软件定时器启动事件
 * @param swtmr 软件定时器控制块
 * @return 无
 */
STATIC VOID LOS_TraceSwtmrStart(const SWTMR_CTRL_S *swtmr)
{
    LOS_TRACE(SWTMR_START, swtmr->usTimerID, swtmr->ucMode, swtmr->uwInterval);  // 发送定时器启动事件，包含定时器ID、模式和间隔
}

/**
 * @brief 跟踪软件定时器停止事件
 * @param swtmr 软件定时器控制块
 * @return 无
 */
STATIC VOID LOS_TraceSwtmrStop(const SWTMR_CTRL_S *swtmr)
{
    LOS_TRACE(SWTMR_STOP, swtmr->usTimerID);  // 发送定时器停止事件，参数为定时器ID
}

/**
 * @brief 跟踪IPC消息写入丢弃事件
 * @param msg IPC消息指针
 * @param dstTid 目标任务ID
 * @param dstPid 目标进程ID
 * @param ipcStatus IPC状态码
 * @return 无
 */
STATIC VOID LOS_TraceIpcWriteDrop(const IpcMsg *msg, UINT32 dstTid, UINT32 dstPid, UINT32 ipcStatus)
{
    LOS_TRACE(IPC_WRITE_DROP, dstTid, dstPid, msg->type, msg->code, ipcStatus);  // 发送IPC写入丢弃事件，包含目标ID、消息类型和状态码
}

/**
 * @brief 跟踪IPC消息写入事件
 * @param msg IPC消息指针
 * @param dstTid 目标任务ID
 * @param dstPid 目标进程ID
 * @param ipcStatus IPC状态码
 * @return 无
 */
STATIC VOID LOS_TraceIpcWrite(const IpcMsg *msg, UINT32 dstTid, UINT32 dstPid, UINT32 ipcStatus)
{
    LOS_TRACE(IPC_WRITE, dstTid, dstPid, msg->type, msg->code, ipcStatus);  // 发送IPC写入事件，包含目标ID、消息类型和状态码
}

/**
 * @brief 跟踪IPC消息读取丢弃事件
 * @param msg IPC消息指针
 * @param ipcStatus IPC状态码
 * @return 无
 */
STATIC VOID LOS_TraceIpcReadDrop(const IpcMsg *msg, UINT32 ipcStatus)
{
    LOS_TRACE(IPC_READ_DROP, msg->taskID, msg->processID, msg->type, msg->code, ipcStatus);  // 发送IPC读取丢弃事件，包含源ID、消息类型和状态码
}

/**
 * @brief 跟踪IPC消息读取事件
 * @param msg IPC消息指针
 * @param ipcStatus IPC状态码
 * @return 无
 */
STATIC VOID LOS_TraceIpcRead(const IpcMsg *msg, UINT32 ipcStatus)
{
    LOS_TRACE(IPC_READ_DROP, msg->taskID, msg->processID, msg->type, msg->code, ipcStatus);  // 发送IPC读取事件，包含源ID、消息类型和状态码
}

/**
 * @brief 跟踪IPC尝试读取事件
 * @param msgType 消息类型
 * @param ipcStatus IPC状态码
 * @return 无
 */
STATIC VOID LOS_TraceIpcTryRead(UINT32 msgType, UINT32 ipcStatus)
{
    LOS_TRACE(IPC_TRY_READ, msgType, ipcStatus);  // 发送IPC尝试读取事件，包含消息类型和状态码
}

/**
 * @brief 跟踪IPC读取超时事件
 * @param msgType 消息类型
 * @param ipcStatus IPC状态码
 * @return 无
 */
STATIC VOID LOS_TraceIpcReadTimeout(UINT32 msgType, UINT32 ipcStatus)
{
    LOS_TRACE(IPC_READ_TIMEOUT, msgType, ipcStatus);  // 发送IPC读取超时事件，包含消息类型和状态码
}

/**
 * @brief 跟踪IPC终止事件
 * @param msgType 消息类型
 * @param ipcStatus IPC状态码
 * @return 无
 */
STATIC VOID LOS_TraceIpcKill(UINT32 msgType, UINT32 ipcStatus)
{
    LOS_TRACE(IPC_KILL, msgType, ipcStatus);  // 发送IPC终止事件，包含消息类型和状态码
}

/**
 * @brief 跟踪用户自定义事件
 * @param buffer 事件缓冲区指针
 * @param len 缓冲区长度
 * @return 无
 */
STATIC VOID LOS_TraceUsrEvent(VOID *buffer, UINT32 len)
{
#ifdef LOSCFG_DRIVERS_TRACE
    UsrEventInfo *info = (UsrEventInfo *)buffer;  // 将缓冲区指针转换为用户事件信息结构体
    if ((info == NULL) || (len != sizeof(UsrEventInfo))) {  // 检查缓冲区有效性和长度
        return;  // 无效缓冲区，直接返回
    }
    LOS_TRACE_EASY(info->eventType & (~TRACE_USER_DEFAULT_FLAG), info->identity, info->params[0], info->params[1],
        info->params[2]); /* 2, params num, no special meaning */  // 发送用户事件，清除默认标志位，传递3个参数
    LOS_MemFree(m_aucSysMem0, buffer);  // 释放系统内存池中的事件缓冲区
#endif
}

/**
 * @brief 初始化跟踪转换模块，注册所有跟踪钩子函数
 * @return 无
 */
VOID OsTraceCnvInit(VOID)
{
    LOS_HookReg(LOS_HOOK_TYPE_MEM_ALLOC, LOS_TraceMemAlloc);               // 注册内存分配跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MEM_FREE, LOS_TraceMemFree);                 // 注册内存释放跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MEM_INIT, LOS_TraceMemInit);                 // 注册内存初始化跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MEM_REALLOC, LOS_TraceMemRealloc);           // 注册内存重分配跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MEM_ALLOCALIGN, LOS_TraceMemAllocAlign);     // 注册内存对齐分配跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_EVENT_INIT, LOS_TraceEventInit);             // 注册事件初始化跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_EVENT_READ, LOS_TraceEventRead);             // 注册事件读取跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_EVENT_WRITE, LOS_TraceEventWrite);           // 注册事件写入跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_EVENT_CLEAR, LOS_TraceEventClear);           // 注册事件清除跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_EVENT_DESTROY, LOS_TraceEventDestroy);       // 注册事件销毁跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_QUEUE_CREATE, LOS_TraceQueueCreate);         // 注册队列创建跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_QUEUE_DELETE, LOS_TraceQueueDelete);         // 注册队列删除跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_QUEUE_READ, LOS_TraceQueueRW);               // 注册队列读取跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_QUEUE_WRITE, LOS_TraceQueueRW);              // 注册队列写入跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SEM_CREATE, LOS_TraceSemCreate);             // 注册信号量创建跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SEM_DELETE, LOS_TraceSemDelete);             // 注册信号量删除跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SEM_POST, LOS_TraceSemPost);                 // 注册信号量释放跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SEM_PEND, LOS_TraceSemPend);                 // 注册信号量等待跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MUX_CREATE, LOS_TraceMuxCreate);             // 注册互斥锁创建跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MUX_POST, LOS_TraceMuxPost);                 // 注册互斥锁释放跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MUX_PEND, LOS_TraceMuxPend);                 // 注册互斥锁等待跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MUX_DELETE, LOS_TraceMuxDelete);             // 注册互斥锁删除跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_TASK_PRIMODIFY, LOS_TraceTaskPriModify);     // 注册任务优先级修改跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_TASK_DELETE, LOS_TraceTaskDelete);           // 注册任务删除跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_TASK_CREATE, LOS_TraceTaskCreate);           // 注册任务创建跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_TASK_SWITCHEDIN, LOS_TraceTaskSwitchedIn);   // 注册任务切换跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MOVEDTASKTOREADYSTATE, LOS_TraceTaskResume); // 注册任务恢复跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_MOVEDTASKTOSUSPENDEDLIST, LOS_TraceTaskSuspend); // 注册任务挂起跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_ISR_ENTER, LOS_TraceIsrEnter);               // 注册中断进入跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_ISR_EXIT, LOS_TraceIsrExit);                 // 注册中断退出跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SWTMR_CREATE, LOS_TraceSwtmrCreate);         // 注册软件定时器创建跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SWTMR_DELETE, LOS_TraceSwtmrDelete);         // 注册软件定时器删除跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SWTMR_EXPIRED, LOS_TraceSwtmrExpired);       // 注册软件定时器超时跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SWTMR_START, LOS_TraceSwtmrStart);           // 注册软件定时器启动跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_SWTMR_STOP, LOS_TraceSwtmrStop);             // 注册软件定时器停止跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_USR_EVENT, LOS_TraceUsrEvent);               // 注册用户自定义事件跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_IPC_WRITE_DROP, LOS_TraceIpcWriteDrop);      // 注册IPC写入丢弃跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_IPC_WRITE, LOS_TraceIpcWrite);               // 注册IPC写入跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_IPC_READ_DROP, LOS_TraceIpcReadDrop);        // 注册IPC读取丢弃跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_IPC_READ, LOS_TraceIpcRead);                 // 注册IPC读取跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_IPC_TRY_READ, LOS_TraceIpcTryRead);          // 注册IPC尝试读取跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_IPC_READ_TIMEOUT, LOS_TraceIpcReadTimeout);  // 注册IPC读取超时跟踪钩子
    LOS_HookReg(LOS_HOOK_TYPE_IPC_KILL, LOS_TraceIpcKill);                 // 注册IPC终止跟踪钩子
}

