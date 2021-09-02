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

#ifdef LOSCFG_KERNEL_HOOK
#define LOS_HOOK_ALL_TYPES_DEF                                                                              \
    /* Hook types supported by memory modules */                                                            \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_INIT, (VOID *pool, UINT32 size))                                    \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_DEINIT, (VOID *pool))                                               \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_ALLOC, (VOID *pool, VOID *ptr, UINT32 size))                        \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_FREE, (VOID *pool, VOID *ptr))                                      \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_REALLOC, (VOID *pool, VOID *ptr, UINT32 size))                      \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MEM_ALLOCALIGN, (VOID *pool, VOID *ptr, UINT32 size, UINT32 boundary))  \
    /* Hook types supported by event modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_INIT, (PEVENT_CB_S eventCB))                                      \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_READ, (PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode,        \
                        UINT32 timeout))                                                                    \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_WRITE, (PEVENT_CB_S eventCB, UINT32 events))                      \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_CLEAR, (PEVENT_CB_S eventCB, UINT32 events))                      \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_EVENT_DESTROY, (PEVENT_CB_S eventCB))                                   \
    /* Hook types supported by queue modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_CREATE, (const LosQueueCB *queueCB))                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_READ, (const LosQueueCB *queueCB, UINT32 operateType,             \
                        UINT32 bufferSize, UINT32 timeout))                                                 \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_WRITE, (const LosQueueCB *queueCB, UINT32 operateType,            \
                        UINT32 bufferSize, UINT32 timeout))                                                 \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_QUEUE_DELETE, (const LosQueueCB *queueCB))                              \
    /* Hook types supported by semphore modules */                                                          \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_CREATE, (const LosSemCB *semCreated))                               \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_POST, (const LosSemCB *semPosted, const LosTaskCB *resumedTask))    \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_PEND, (const LosSemCB *semPended, const LosTaskCB *runningTask,     \
                        UINT32 timeout))                                                                    \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SEM_DELETE, (const LosSemCB *semDeleted))                               \
    /* Hook types supported by mutex modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_CREATE, (const LosMux *muxCreated))                                 \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_POST, (const LosMux *muxPosted))                                    \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_PEND, (const LosMux *muxPended, UINT32 timeout))                    \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MUX_DELETE, (const LosMux *muxDeleted))                                 \
    /* Hook types supported by task modules */                                                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_CREATE, (const LosTaskCB *taskCB))                                 \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_DELAY, (UINT32 tick))                                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_PRIMODIFY, (const LosTaskCB *pxTask, UINT32 uxNewPriority))        \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_DELETE, (const LosTaskCB *taskCB))                                 \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_TASK_SWITCHEDIN, (const LosTaskCB *newTask, const LosTaskCB *runTask))  \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MOVEDTASKTOREADYSTATE, (const LosTaskCB *pstTaskCB))                    \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MOVEDTASKTODELAYEDLIST, (const LosTaskCB *pstTaskCB))                   \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_MOVEDTASKTOSUSPENDEDLIST, (const LosTaskCB *pstTaskCB))                 \
    /* Hook types supported by interrupt modules */                                                         \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_ISR_EXITTOSCHEDULER, (VOID))                                            \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_ISR_ENTER, (UINT32 hwiIndex))                                           \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_ISR_EXIT, (UINT32 hwiIndex))                                            \
    /* Hook types supported by swtmr modules */                                                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_CREATE, (const SWTMR_CTRL_S *swtmr))                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_DELETE, (const SWTMR_CTRL_S *swtmr))                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_EXPIRED, (const SWTMR_CTRL_S *swtmr))                             \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_START, (const SWTMR_CTRL_S *swtmr))                               \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_SWTMR_STOP, (const SWTMR_CTRL_S *swtmr))                                \
    /* Hook types supported by liteipc modules  */                                                          \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_WRITE_DROP, (const IpcMsg *msg, UINT32 dstTid, UINT32 dstPid,       \
                        UINT32 ipcStatus))                                                                  \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_WRITE, (const IpcMsg *msg, UINT32 dstTid, UINT32 dstPid,            \
                        UINT32 ipcStatus))                                                                  \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_READ_DROP, (const IpcMsg *msg, UINT32 ipcStatus))                   \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_READ, (const IpcMsg *msg, UINT32 ipcStatus))                        \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_TRY_READ, (UINT32 msgType, UINT32 ipcStatus))                       \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_READ_TIMEOUT, (UINT32 msgType, UINT32 ipcStatus))                   \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_IPC_KILL, (UINT32 msgType, UINT32 ipcStatus))                           \
    /* Hook types supported by usr modules  */                                                              \
    LOS_HOOK_TYPE_DEF(LOS_HOOK_TYPE_USR_EVENT, (VOID *buffer, UINT32 len))

/**
 * Defines the types of all hooks.
 */
#define LOS_HOOK_TYPE_DEF(type, paramList)                  type,

typedef enum {
    /* Used to manage hook pools */
    LOS_HOOK_TYPE_START = 0,
    /* All supported hook types */
    LOS_HOOK_ALL_TYPES_DEF
    /* Used to manage hook pools */
    LOS_HOOK_TYPE_END
} HookType;

#undef LOS_HOOK_TYPE_DEF

/**
 * Declare the type and interface of the hook functions.
 */
#define LOS_HOOK_TYPE_DEF(type, paramList)                  \
    typedef VOID (*type##_FN) paramList;                    \
    extern UINT32 type##_RegHook(type##_FN func);           \
    extern UINT32 type##_UnRegHook(type##_FN func);         \
    extern VOID type##_CallHook paramList;

LOS_HOOK_ALL_TYPES_DEF

#undef LOS_HOOK_TYPE_DEF

#endif /* LOSCFG_KERNEL_HOOK */

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HOOK_TYPES_H */
