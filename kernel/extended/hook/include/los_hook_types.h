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

#ifndef _LOS_HOOK_TYPES_H
#define _LOS_HOOK_TYPES_H

#include "los_config.h"
#include "los_event_pri.h"
#include "los_mux_pri.h"
#include "los_queue_pri.h"
#include "los_sem_pri.h"
#include "los_task_pri.h"
#include "los_swtmr_pri.h"
#include "hm_liteipc.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
#ifdef LOSCFG_KERNEL_HOOK  // 如果启用内核钩子功能
#define LOS_HOOK_ALL_TYPES_DEF                                                                              \  // 定义所有钩子类型的宏
    /* Hook types supported by memory modules */                                                            \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_INIT, (VOID *pool, UINT32 size))                                    \  // 内存池初始化钩子，参数:内存池指针,大小
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_DEINIT, (VOID *pool))                                               \  // 内存池去初始化钩子，参数:内存池指针
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_ALLOC, (VOID *pool, VOID *ptr, UINT32 size))                        \  // 内存分配钩子，参数:内存池,分配指针,大小
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_FREE, (VOID *pool, VOID *ptr))                                      \  // 内存释放钩子，参数:内存池,释放指针
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_REALLOC, (VOID *pool, VOID *ptr, UINT32 size))                      \  // 内存重分配钩子，参数:内存池,原指针,新大小
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_ALLOCALIGN, (VOID *pool, VOID *ptr, UINT32 size, UINT32 boundary))  \  // 内存对齐分配钩子，参数:内存池,指针,大小,对齐边界
    /* Hook types supported by event modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_INIT, (PEVENT_CB_S eventCB))                                      \  // 事件初始化钩子，参数:事件控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_READ, (PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode,        \  // 事件读取钩子，参数:事件控制块,事件掩码,模式
                        UINT32 timeout))                                                                    \  // 超时时间
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_WRITE, (PEVENT_CB_S eventCB, UINT32 events))                      \  // 事件写入钩子，参数:事件控制块,事件集
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_CLEAR, (PEVENT_CB_S eventCB, UINT32 events))                      \  // 事件清除钩子，参数:事件控制块,事件集
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_DESTROY, (PEVENT_CB_S eventCB))                                   \  // 事件销毁钩子，参数:事件控制块
    /* Hook types supported by queue modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_CREATE, (const LosQueueCB *queueCB))                              \  // 队列创建钩子，参数:队列控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_READ, (const LosQueueCB *queueCB, UINT32 operateType,             \  // 队列读取钩子，参数:队列控制块,操作类型
                        UINT32 bufferSize, UINT32 timeout))                                                 \  // 缓冲区大小,超时时间
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_WRITE, (const LosQueueCB *queueCB, UINT32 operateType,            \  // 队列写入钩子，参数:队列控制块,操作类型
                        UINT32 bufferSize, UINT32 timeout))                                                 \  // 缓冲区大小,超时时间
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_DELETE, (const LosQueueCB *queueCB))                              \  // 队列删除钩子，参数:队列控制块
    /* Hook types supported by semaphore modules */                                                          \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_CREATE, (const LosSemCB *semCreated))                               \  // 信号量创建钩子，参数:信号量控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_POST, (const LosSemCB *semPosted, const LosTaskCB *resumedTask))    \  // 信号量释放钩子，参数:信号量控制块,恢复任务
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_PEND, (const LosSemCB *semPended, const LosTaskCB *runningTask,     \  // 信号量等待钩子，参数:信号量控制块,运行任务
                        UINT32 timeout))                                                                    \  // 超时时间
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_DELETE, (const LosSemCB *semDeleted))                               \  // 信号量删除钩子，参数:信号量控制块
    /* Hook types supported by mutex modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_CREATE, (const LosMux *muxCreated))                                 \  // 互斥锁创建钩子，参数:互斥锁控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_POST, (const LosMux *muxPosted))                                    \  // 互斥锁释放钩子，参数:互斥锁控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_PEND, (const LosMux *muxPended, UINT32 timeout))                    \  // 互斥锁等待钩子，参数:互斥锁控制块,超时时间
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_DELETE, (const LosMux *muxDeleted))                                 \  // 互斥锁删除钩子，参数:互斥锁控制块
    /* Hook types supported by task modules */                                                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_CREATE, (const LosTaskCB *taskCB))                                 \  // 任务创建钩子，参数:任务控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_DELAY, (UINT32 tick))                                              \  // 任务延迟钩子，参数:延迟 ticks
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_PRIMODIFY, (const LosTaskCB *pxTask, UINT32 uxNewPriority))        \  // 任务优先级修改钩子，参数:任务控制块,新优先级
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_DELETE, (const LosTaskCB *taskCB))                                 \  // 任务删除钩子，参数:任务控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_SWITCHEDIN, (const LosTaskCB *newTask, const LosTaskCB *runTask))  \  // 任务切换进入钩子，参数:新任务,运行中任务
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MOVEDTASKTOREADYSTATE, (const LosTaskCB *pstTaskCB))                    \  // 任务移至就绪队列钩子，参数:任务控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MOVEDTASKTODELAYEDLIST, (const LosTaskCB *pstTaskCB))                   \  // 任务移至延迟队列钩子，参数:任务控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MOVEDTASKTOSUSPENDEDLIST, (const LosTaskCB *pstTaskCB))                 \  // 任务移至挂起队列钩子，参数:任务控制块
    /* Hook types supported by interrupt modules */                                                         \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_ISR_EXITTOSCHEDULER, (VOID))                                            \  // ISR退出到调度器钩子，无参数
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_ISR_ENTER, (UINT32 hwiIndex))                                           \  // ISR进入钩子，参数:硬件中断索引
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_ISR_EXIT, (UINT32 hwiIndex))                                            \  // ISR退出钩子，参数:硬件中断索引
    /* Hook types supported by swtmr modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_CREATE, (const SWTMR_CTRL_S *swtmr))                              \  // 软件定时器创建钩子，参数:定时器控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_DELETE, (const SWTMR_CTRL_S *swtmr))                              \  // 软件定时器删除钩子，参数:定时器控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_EXPIRED, (const SWTMR_CTRL_S *swtmr))                             \  // 软件定时器超时钩子，参数:定时器控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_START, (const SWTMR_CTRL_S *swtmr))                               \  // 软件定时器启动钩子，参数:定时器控制块
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_STOP, (const SWTMR_CTRL_S *swtmr))                                \  // 软件定时器停止钩子，参数:定时器控制块
    /* Hook types supported by liteipc modules  */                                                          \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_WRITE_DROP, (const IpcMsg *msg, UINT32 dstTid, UINT32 dstPid,       \  // IPC写丢弃钩子，参数:消息,目标TID,目标PID
                        UINT32 ipcStatus))                                                                  \  // IPC状态
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_WRITE, (const IpcMsg *msg, UINT32 dstTid, UINT32 dstPid,            \  // IPC写钩子，参数:消息,目标TID,目标PID
                        UINT32 ipcStatus))                                                                  \  // IPC状态
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_READ_DROP, (const IpcMsg *msg, UINT32 ipcStatus))                   \  // IPC读丢弃钩子，参数:消息,IPC状态
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_READ, (const IpcMsg *msg, UINT32 ipcStatus))                        \  // IPC读钩子，参数:消息,IPC状态
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_TRY_READ, (UINT32 msgType, UINT32 ipcStatus))                       \  // IPC尝试读钩子，参数:消息类型,IPC状态
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_READ_TIMEOUT, (UINT32 msgType, UINT32 ipcStatus))                   \  // IPC读超时钩子，参数:消息类型,IPC状态
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_KILL, (UINT32 msgType, UINT32 ipcStatus))                           \  // IPC终止钩子，参数:消息类型,IPC状态
    /* Hook types supported by usr modules  */                                                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_USR_EVENT, (VOID *buffer, UINT32 len))                                 // 用户事件钩子，参数:缓冲区,长度

/**
 * 定义所有钩子的类型枚举
 */
#define LOS_HOOK_TYPE_DEF(type, paramList)                  type,  // 枚举类型定义宏，用于生成HookType枚举值

typedef enum {                                                                 // 钩子类型枚举
    /* 用于管理钩子池的边界值 */
    LOS_HOOK_TYPE_START = 0,  // 钩子类型起始标记
    /* 所有支持的钩子类型 */
    LOS_HOOK_ALL_TYPES_DEF    // 展开所有钩子类型枚举值
    /* 用于管理钩子池的边界值 */
    LOS_HOOK_TYPE_END         // 钩子类型结束标记
} HookType;                                                                   // 钩子类型枚举定义

#undef LOS_HOOK_TYPE_DEF  // 取消LOS_HOOK_TYPE_DEF宏定义

/**
 * 声明钩子函数的类型和接口
 */
#define LOS_HOOK_TYPE_DEF(type, paramList)                  \  // 钩子函数接口声明宏
    typedef VOID (*type##_FN) paramList;                    \  // 定义钩子函数指针类型
    extern UINT32 type##_RegHook(type##_FN func);           \  // 声明钩子注册函数
    extern UINT32 type##_UnRegHook(type##_FN func);         \  // 声明钩子注销函数
    extern VOID type##_CallHook paramList;                   // 声明钩子调用函数

LOS_HOOK_ALL_TYPES_DEF  // 展开所有钩子函数接口声明

#undef LOS_HOOK_TYPE_DEF  // 取消LOS_HOOK_TYPE_DEF宏定义

#endif /* LOSCFG_KERNEL_HOOK */  // 内核钩子功能条件编译结束

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HOOK_TYPES_H */
