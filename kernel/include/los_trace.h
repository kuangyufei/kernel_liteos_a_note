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
 * @defgroup los_trace Trace
 * @ingroup kernel
 */

#ifndef _LOS_TRACE_H
#define _LOS_TRACE_H

#include "los_task.h"
#include "los_base.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_TRACE_CONTROL_AGENT

/**
 * @ingroup los_trace
 * Trace Control agent task's priority.
 */
#define LOSCFG_TRACE_TASK_PRIORITY                              2
#endif

#define LOSCFG_TRACE_OBJ_MAX_NAME_SIZE                          LOS_TASK_NAMELEN

#define LOS_TRACE_LR_RECORD                                     5
#define LOS_TRACE_LR_IGNOR                                      0
/**
 * @ingroup los_trace
 * Trace records the max number of objects(kernel object, like tasks), range is [0, LOSCFG_BASE_CORE_TSK_LIMIT].
 * if set to 0, trace will not record any object.
 */
#define LOSCFG_TRACE_OBJ_MAX_NUM                                0

/**
 * @ingroup los_trace
 * Trace tlv encode buffer size, the buffer is used to encode one piece raw frame to tlv message in online mode.
 */
#define LOSCFG_TRACE_TLV_BUF_SIZE                               100

/**
 * @ingroup los_trace
 * Trace error code: init trace failed.
 *
 * Value: 0x02001400
 *
 * Solution: Follow the trace State Machine.
 */
#define LOS_ERRNO_TRACE_ERROR_STATUS               LOS_ERRNO_OS_ERROR(LOS_MOD_TRACE, 0x00)

/**
 * @ingroup los_trace
 * Trace error code: Insufficient memory for trace buf init.
 *
 * Value: 0x02001401
 *
 * Solution: Expand the configured system memory or decrease the value defined by LOS_TRACE_BUFFER_SIZE.
 */
#define LOS_ERRNO_TRACE_NO_MEMORY                  LOS_ERRNO_OS_ERROR(LOS_MOD_TRACE, 0x01)

/**
 * @ingroup los_trace
 * Trace error code: Insufficient memory for trace struct.
 *
 * Value: 0x02001402
 *
 * Solution: Increase trace buffer's size.
 */
#define LOS_ERRNO_TRACE_BUF_TOO_SMALL              LOS_ERRNO_OS_ERROR(LOS_MOD_TRACE, 0x02)

/**
 * @ingroup los_trace
 * Trace state.
 */
enum TraceState {
    TRACE_UNINIT = 0,  /**< trace isn't inited */
    TRACE_INITED,      /**< trace is inited but not started yet */
    TRACE_STARTED,     /**< trace is started and system is tracing */
    TRACE_STOPED,      /**< trace is stopped */
};

/**
 * @ingroup los_trace
 * Trace mask is used to filter events in runtime. Each mask keep only one unique bit to 1, and user can define own
 * module's trace mask.
 */
typedef enum {
    TRACE_SYS_FLAG          = 0x10,
    TRACE_HWI_FLAG          = 0x20,
    TRACE_TASK_FLAG         = 0x40,
    TRACE_SWTMR_FLAG        = 0x80,
    TRACE_MEM_FLAG          = 0x100,
    TRACE_QUE_FLAG          = 0x200,
    TRACE_EVENT_FLAG        = 0x400,
    TRACE_SEM_FLAG          = 0x800,
    TRACE_MUX_FLAG          = 0x1000,
    TRACE_IPC_FLAG          = 0x2000,

    TRACE_MAX_FLAG          = 0x80000000,
    TRACE_USER_DEFAULT_FLAG = 0xFFFFFFF0,
} LOS_TRACE_MASK;

/**
 * @ingroup los_trace
 * Trace event type which indicate the exactly happend events, user can define own module's event type like
 * TRACE_#MODULE#_FLAG | NUMBER.
 *                   28                     4
 *    0 0 0 0 0 0 0 0 X X X X X X X X 0 0 0 0 0 0
 *    |                                   |     |
 *             trace_module_flag           number
 *
 */
typedef enum {
    /* 0x10~0x1F */
    SYS_ERROR             = TRACE_SYS_FLAG | 0,
    SYS_START             = TRACE_SYS_FLAG | 1,
    SYS_STOP              = TRACE_SYS_FLAG | 2,

    /* 0x20~0x2F */
    HWI_CREATE              = TRACE_HWI_FLAG | 0,
    HWI_CREATE_SHARE        = TRACE_HWI_FLAG | 1,
    HWI_DELETE              = TRACE_HWI_FLAG | 2,
    HWI_DELETE_SHARE        = TRACE_HWI_FLAG | 3,
    HWI_RESPONSE_IN         = TRACE_HWI_FLAG | 4,
    HWI_RESPONSE_OUT        = TRACE_HWI_FLAG | 5,
    HWI_ENABLE              = TRACE_HWI_FLAG | 6,
    HWI_DISABLE             = TRACE_HWI_FLAG | 7,
    HWI_TRIGGER             = TRACE_HWI_FLAG | 8,
    HWI_SETPRI              = TRACE_HWI_FLAG | 9,
    HWI_CLEAR               = TRACE_HWI_FLAG | 10,
    HWI_SETAFFINITY         = TRACE_HWI_FLAG | 11,
    HWI_SENDIPI             = TRACE_HWI_FLAG | 12,

    /* 0x40~0x4F */
    TASK_CREATE           = TRACE_TASK_FLAG | 0,
    TASK_PRIOSET          = TRACE_TASK_FLAG | 1,
    TASK_DELETE           = TRACE_TASK_FLAG | 2,
    TASK_SUSPEND          = TRACE_TASK_FLAG | 3,
    TASK_RESUME           = TRACE_TASK_FLAG | 4,
    TASK_SWITCH           = TRACE_TASK_FLAG | 5,
    TASK_SIGNAL           = TRACE_TASK_FLAG | 6,

    /* 0x80~0x8F */
    SWTMR_CREATE          = TRACE_SWTMR_FLAG | 0,
    SWTMR_DELETE          = TRACE_SWTMR_FLAG | 1,
    SWTMR_START           = TRACE_SWTMR_FLAG | 2,
    SWTMR_STOP            = TRACE_SWTMR_FLAG | 3,
    SWTMR_EXPIRED         = TRACE_SWTMR_FLAG | 4,

    /* 0x100~0x10F */
    MEM_ALLOC             = TRACE_MEM_FLAG | 0,
    MEM_ALLOC_ALIGN       = TRACE_MEM_FLAG | 1,
    MEM_REALLOC           = TRACE_MEM_FLAG | 2,
    MEM_FREE              = TRACE_MEM_FLAG | 3,
    MEM_INFO_REQ          = TRACE_MEM_FLAG | 4,
    MEM_INFO              = TRACE_MEM_FLAG | 5,

    /* 0x200~0x20F */
    QUEUE_CREATE          = TRACE_QUE_FLAG | 0,
    QUEUE_DELETE          = TRACE_QUE_FLAG | 1,
    QUEUE_RW              = TRACE_QUE_FLAG | 2,

    /* 0x400~0x40F */
    EVENT_CREATE          = TRACE_EVENT_FLAG | 0,
    EVENT_DELETE          = TRACE_EVENT_FLAG | 1,
    EVENT_READ            = TRACE_EVENT_FLAG | 2,
    EVENT_WRITE           = TRACE_EVENT_FLAG | 3,
    EVENT_CLEAR           = TRACE_EVENT_FLAG | 4,

    /* 0x800~0x80F */
    SEM_CREATE            = TRACE_SEM_FLAG | 0,
    SEM_DELETE            = TRACE_SEM_FLAG | 1,
    SEM_PEND              = TRACE_SEM_FLAG | 2,
    SEM_POST              = TRACE_SEM_FLAG | 3,

    /* 0x1000~0x100F */
    MUX_CREATE            = TRACE_MUX_FLAG | 0,
    MUX_DELETE            = TRACE_MUX_FLAG | 1,
    MUX_PEND              = TRACE_MUX_FLAG | 2,
    MUX_POST              = TRACE_MUX_FLAG | 3,

    /* 0x2000~0x200F */
    IPC_WRITE_DROP        = TRACE_IPC_FLAG | 0,
    IPC_WRITE             = TRACE_IPC_FLAG | 1,
    IPC_READ_DROP         = TRACE_IPC_FLAG | 2,
    IPC_READ              = TRACE_IPC_FLAG | 3,
    IPC_TRY_READ          = TRACE_IPC_FLAG | 4,
    IPC_READ_TIMEOUT      = TRACE_IPC_FLAG | 5,
    IPC_KILL              = TRACE_IPC_FLAG | 6,
} LOS_TRACE_TYPE;

/**
 * @ingroup los_trace
 * struct to store the trace config information.
 */
typedef struct {
    UINT32 bigLittleEndian;     /**< big little endian flag */
    UINT32 clockFreq;           /**< system clock frequency */
    UINT32 version;             /**< trace version */
} TraceBaseHeaderInfo;

/**
 * @ingroup los_trace
 * struct to store the event infomation
 */
typedef struct {
    UINT32  eventType;                               /**< event type */
    UINT32  curTask;                                 /**< current running task */
    UINT32  curPid;                                  /**< current running processID */
    UINT64  curTime;                                 /**< current timestamp */
    UINTPTR identity;                                /**< subject of the event description */
#ifdef LOSCFG_TRACE_FRAME_CORE_MSG
    struct CoreStatus {
        UINT32 cpuId      : 8,                       /**< cpuid */
               hwiActive  : 4,                       /**< whether is in hwi response */
               taskLockCnt : 4,                      /**< task lock count */
               paramCount : 4,                       /**< event frame params' number */
               reserves   : 12;                      /**< reserves */
    } core;
#endif

#ifdef LOSCFG_TRACE_FRAME_EVENT_COUNT
    UINT32  eventCount;                               /**< the sequence of happend events */
#endif

#ifdef LOS_TRACE_FRAME_LR
    UINTPTR linkReg[LOS_TRACE_LR_RECORD];
#endif

#ifdef LOSCFG_TRACE_FRAME_MAX_PARAMS
    UINTPTR params[LOSCFG_TRACE_FRAME_MAX_PARAMS];    /**< event frame's params */
#endif
} TraceEventFrame;

#ifdef LOSCFG_DRIVERS_TRACE
typedef struct {
    UINT32  eventType;
    UINTPTR identity;
    UINTPTR params[3];
} UsrEventInfo;
#endif

/**
 * @ingroup los_trace
 * struct to store the kernel obj information, we defined task as kernel obj in this system.
 */
typedef struct {
    UINT32      id;                                     /**< kernel obj's id */
    UINT32      prio;                                   /**< kernel obj's priority */
    CHAR        name[LOSCFG_TRACE_OBJ_MAX_NAME_SIZE];   /**< kernel obj's name */
} ObjData;

/**
 * @ingroup los_trace
 * struct to store the trace data.
 */
typedef struct {
    TraceBaseHeaderInfo baseInfo;          /**< basic info, include bigLittleEndian flag, system clock freq */
    UINT16 totalLen;                       /**< trace data's total length */
    UINT16 objSize;                        /**< sizeof #ObjData */
    UINT16 frameSize;                      /**< sizeof #TraceEventFrame */
    UINT16 objOffset;                      /**< the offset of the first obj data to record beginning */
    UINT16 frameOffset;                    /**< the offset of the first event frame data to record beginning */
} OfflineHead;

/**
 * @ingroup  los_trace
 * @brief Define the type of trace hardware interrupt filter hook function.
 *
 * @par Description:
 * User can register fliter function by LOS_TraceHwiFilterHookReg to filter hardware interrupt events. Return true if
 * user don't need trace the certain number.
 *
 * @attention
 * None.
 *
 * @param hwiNum        [IN] Type #UINT32. The hardware interrupt number.
 * @retval #TRUE        0x00000001: Not record the certain number.
 * @retval #FALSE       0x00000000: Need record the certain number.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 */
typedef BOOL (*TRACE_HWI_FILTER_HOOK)(UINT32 hwiNum);

typedef VOID (*TRACE_EVENT_HOOK)(UINT32 eventType, UINTPTR identity, const UINTPTR *params, UINT16 paramCount);
extern TRACE_EVENT_HOOK g_traceEventHook;

/**
 * @ingroup los_trace
 * Trace event params:
 1. Configure the macro without parameters so as not to record events of this type;
 2. Configure the macro at least with one parameter to record this type of event;
 3. User can delete unnecessary parameters which defined in corresponding marco;
 * @attention
 * <ul>
 * <li>The first param is treat as key, keep at least this param if you want trace this event.</li>
 * <li>All parameters were treated as UINTPTR.</li>
 * </ul>
 * eg. Trace a event as:
 * #define TASK_PRIOSET_PARAMS(taskId, taskStatus, oldPrio, newPrio) taskId, taskStatus, oldPrio, newPrio
 * eg. Not Trace a event as:
 * #define TASK_PRIOSET_PARAMS(taskId, taskStatus, oldPrio, newPrio)
 * eg. Trace only you need parmas as:
 * #define TASK_PRIOSET_PARAMS(taskId, taskStatus, oldPrio, newPrio) taskId
 */
#define TASK_SWITCH_PARAMS(taskId, oldPriority, oldTaskStatus, newPriority, newTaskStatus) \
    taskId, oldPriority, oldTaskStatus, newPriority, newTaskStatus
#define TASK_PRIOSET_PARAMS(taskId, taskStatus, oldPrio, newPrio) taskId, taskStatus, oldPrio, newPrio
#define TASK_CREATE_PARAMS(taskId, taskStatus, prio)     taskId, taskStatus, prio
#define TASK_DELETE_PARAMS(taskId, taskStatus, usrStack) taskId, taskStatus, usrStack
#define TASK_SUSPEND_PARAMS(taskId, taskStatus, runTaskId) taskId, taskStatus, runTaskId
#define TASK_RESUME_PARAMS(taskId, taskStatus, prio)     taskId, taskStatus, prio
#define TASK_SIGNAL_PARAMS(taskId, signal, schedFlag)    // taskId, signal, schedFlag

#define SWTMR_START_PARAMS(swtmrId, mode, interval)  swtmrId, mode, interval
#define SWTMR_DELETE_PARAMS(swtmrId)                                  swtmrId
#define SWTMR_EXPIRED_PARAMS(swtmrId)                                 swtmrId
#define SWTMR_STOP_PARAMS(swtmrId)                                    swtmrId
#define SWTMR_CREATE_PARAMS(swtmrId)                                  swtmrId

#define HWI_CREATE_PARAMS(hwiNum, hwiPrio, hwiMode, hwiHandler) hwiNum, hwiPrio, hwiMode, hwiHandler
#define HWI_CREATE_SHARE_PARAMS(hwiNum, pDevId, ret)    hwiNum, pDevId, ret
#define HWI_DELETE_PARAMS(hwiNum)                       hwiNum
#define HWI_DELETE_SHARE_PARAMS(hwiNum, pDevId, ret)    hwiNum, pDevId, ret
#define HWI_RESPONSE_IN_PARAMS(hwiNum)                  hwiNum
#define HWI_RESPONSE_OUT_PARAMS(hwiNum)                 hwiNum
#define HWI_ENABLE_PARAMS(hwiNum)                       hwiNum
#define HWI_DISABLE_PARAMS(hwiNum)                      hwiNum
#define HWI_TRIGGER_PARAMS(hwiNum)                      hwiNum
#define HWI_SETPRI_PARAMS(hwiNum, priority)             hwiNum, priority
#define HWI_CLEAR_PARAMS(hwiNum)                        hwiNum
#define HWI_SETAFFINITY_PARAMS(hwiNum, cpuMask)         hwiNum, cpuMask
#define HWI_SENDIPI_PARAMS(hwiNum, cpuMask)             hwiNum, cpuMask

#define EVENT_CREATE_PARAMS(eventCB)                    eventCB
#define EVENT_DELETE_PARAMS(eventCB, delRetCode)        eventCB, delRetCode
#define EVENT_READ_PARAMS(eventCB, eventId, mask, mode, timeout) \
    eventCB, eventId, mask, mode, timeout
#define EVENT_WRITE_PARAMS(eventCB, eventId, events)    eventCB, eventId, events
#define EVENT_CLEAR_PARAMS(eventCB, eventId, events)    eventCB, eventId, events

#define QUEUE_CREATE_PARAMS(queueId, queueSz, itemSz, queueAddr, memType) \
    queueId, queueSz, itemSz, queueAddr, memType
#define QUEUE_DELETE_PARAMS(queueId, state, readable)   queueId, state, readable
#define QUEUE_RW_PARAMS(queueId, queueSize, bufSize, operateType, readable, writeable, timeout) \
    queueId, queueSize, bufSize, operateType, readable, writeable, timeout

#define SEM_CREATE_PARAMS(semId, type, count)           semId, type, count
#define SEM_DELETE_PARAMS(semId, delRetCode)            semId, delRetCode
#define SEM_PEND_PARAMS(semId, count, timeout)          semId, count, timeout
#define SEM_POST_PARAMS(semId, type, count)             semId, type, count

#define MUX_CREATE_PARAMS(muxId)                        muxId
#define MUX_DELETE_PARAMS(muxId, state, count, owner)   muxId, state, count, owner
#define MUX_PEND_PARAMS(muxId, count, owner, timeout)   muxId, count, owner, timeout
#define MUX_POST_PARAMS(muxId, count, owner)            muxId, count, owner

#define MEM_ALLOC_PARAMS(pool, ptr, size)                   pool, ptr, size
#define MEM_ALLOC_ALIGN_PARAMS(pool, ptr, size, boundary)   pool, ptr, size, boundary
#define MEM_REALLOC_PARAMS(pool, ptr, size)                 pool, ptr, size
#define MEM_FREE_PARAMS(pool, ptr)                          pool, ptr
#define MEM_INFO_REQ_PARAMS(pool)                           pool
#define MEM_INFO_PARAMS(pool, usedSize, freeSize)           pool, usedSize, freeSize

#define IPC_WRITE_DROP_PARAMS(dstTid, dstPid, msgType, code, ipcStatus)   \
    dstTid, dstPid, msgType, code, ipcStatus
#define IPC_WRITE_PARAMS(dstTid, dstPid, msgType, code, ipcStatus)        \
    dstTid, dstPid, msgType, code, ipcStatus
#define IPC_READ_DROP_PARAMS(srcTid, srcPid, msgType, code, ipcStatus)    \
    srcTid, srcPid, msgType, code, ipcStatus
#define IPC_READ_PARAMS(srcTid, srcPid, msgType, code, ipcStatus)         \
    srcTid, srcPid, msgType, code, ipcStatus
#define IPC_TRY_READ_PARAMS(msgType, ipcStatus)      msgType, ipcStatus
#define IPC_READ_TIMEOUT_PARAMS(msgType, ipcStatus)  msgType, ipcStatus
#define IPC_KILL_PARAMS(msgType, ipcStatus)          msgType, ipcStatus

#define SYS_ERROR_PARAMS(errno)                         errno

#ifdef LOSCFG_KERNEL_TRACE

/**
 * @ingroup los_trace
 * @brief Trace static code stub.
 *
 * @par Description:
 * This API is used to instrument trace code stub in source code, to track events.
 * @attention
 * None.
 *
 * @param TYPE           [IN] Type #LOS_TRACE_TYPE. The event type.
 * @param IDENTITY       [IN] Type #UINTPTR. The subject of this event description.
 * @param ...            [IN] Type #UINTPTR. This piece of event's params.
 * @retval None.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 */
#define LOS_TRACE(TYPE, IDENTITY, ...)                                             \
    do {                                                                           \
        UINTPTR _inner[] = {0, TYPE##_PARAMS((UINTPTR)IDENTITY, ##__VA_ARGS__)};   \
        UINTPTR _n = sizeof(_inner) / sizeof(UINTPTR);                             \
        if ((_n > 1) && (g_traceEventHook != NULL)) {                              \
            g_traceEventHook(TYPE, _inner[1], _n > 2 ? &_inner[2] : NULL, _n - 2); \
        }                                                                          \
    } while (0)
#else
#define LOS_TRACE(TYPE, ...)
#endif

#ifdef LOSCFG_KERNEL_TRACE

/**
 * @ingroup los_trace
 * @brief Trace static easier user-defined code stub.
 *
 * @par Description:
 * This API is used to instrument user-defined trace code stub in source code, to track events simply.
 * @attention
 * None.
 *
 * @param TYPE           [IN] Type #UINT32. The event type, only low 4 bits take effect.
 * @param IDENTITY       [IN] Type #UINTPTR. The subject of this event description.
 * @param ...            [IN] Type #UINTPTR. This piece of event's params.
 * @retval None.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 */
#define LOS_TRACE_EASY(TYPE, IDENTITY, ...)                                                                          \
    do {                                                                                                             \
        UINTPTR _inner[] = {0, ##__VA_ARGS__};                                                                       \
        UINTPTR _n = sizeof(_inner) / sizeof(UINTPTR);                                                               \
        if (g_traceEventHook != NULL) {                                                                              \
            g_traceEventHook(TRACE_USER_DEFAULT_FLAG | TYPE, (UINTPTR)IDENTITY, _n > 1 ? &_inner[1] : NULL, _n - 1); \
        }                                                                                                            \
    } while (0)
#else
#define LOS_TRACE_EASY(...)
#endif

/**
 * @ingroup los_trace
 * @brief Start trace.
 *
 * @par Description:
 * This API is used to start trace.
 * @attention
 * <ul>
 * <li>Start trace</li>
 * </ul>
 *
 * @param  None.
 * @retval #LOS_ERRNO_TRACE_ERROR_STATUS        0x02001400: Trace start failed.
 * @retval #LOS_OK                              0x00000000: Trace start success.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceStart
 */
extern UINT32 LOS_TraceStart(VOID);

/**
 * @ingroup los_trace
 * @brief Stop trace.
 *
 * @par Description:
 * This API is used to stop trace.
 * @attention
 * <ul>
 * <li>Stop trace</li>
 * </ul>
 *
 * @param  None.
 * @retval #None.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceStop
 */
extern VOID LOS_TraceStop(VOID);

/**
 * @ingroup los_trace
 * @brief Clear the trace buf.
 *
 * @par Description:
 * Clear the event frames in trace buf only at offline mode.
 * @attention
 * <ul>
 * <li>This API can be called only after that trace buffer has been established.</li>
 * Otherwise, the trace will be failed.</li>
 * </ul>
 *
 * @param  None.
 * @retval #NA
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceReset
 */
extern VOID LOS_TraceReset(VOID);

/**
 * @ingroup los_trace
 * @brief Set trace event mask.
 *
 * @par Description:
 * Set trace event mask.
 * @attention
 * <ul>
 * <li>Set trace event filter mask.</li>
 * <li>The Default mask is (TRACE_HWI_FLAG | TRACE_TASK_FLAG), stands for switch on task and hwi events.</li>
 * <li>Customize mask according to the type defined in enum LOS_TRACE_MASK to switch on corresponding module's
 * trace.</li>
 * <li>The system's trace mask will be overrode by the input parameter.</li>
 * </ul>
 *
 * @param  mask [IN] Type #UINT32. The mask used to filter events of LOS_TRACE_MASK.
 * @retval #NA.
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceEventMaskSet
 */
extern VOID LOS_TraceEventMaskSet(UINT32 mask);

/**
 * @ingroup los_trace
 * @brief Offline trace buffer display.
 *
 * @par Description:
 * Display trace buf data only at offline mode.
 * @attention
 * <ul>
 * <li>This API can be called only after that trace stopped. Otherwise the trace dump will be failed.</li>
 * <li>Trace data will be send to pipeline when user set toClient = TRUE. Otherwise it will be formatted and printed
 * out.</li>
 * </ul>
 *
 * @param toClient           [IN] Type #BOOL. Whether send trace data to Client through pipeline.
 * @retval #NA
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceRecordDump
 */
extern VOID LOS_TraceRecordDump(BOOL toClient);

/**
 * @ingroup los_trace
 * @brief Offline trace buffer export.
 *
 * @par Description:
 * Return the trace buf only at offline mode.
 * @attention
 * <ul>
 * <li>This API can be called only after that trace buffer has been established. </li>
 * <li>The return buffer's address is a critical resource, user can only ready.</li>
 * </ul>
 *
 * @param NA
 * @retval #OfflineHead*   The trace buffer's address, analyze this buffer according to the structure of
 * OfflineHead.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceRecordGet
 */
extern OfflineHead *LOS_TraceRecordGet(VOID);

/**
 * @ingroup los_trace
 * @brief Hwi num fliter hook.
 *
 * @par Description:
 * Hwi fliter function.
 * @attention
 * <ul>
 * <li>Filter the hwi events by hwi num</li>
 * </ul>
 *
 * @param  hook [IN] Type #TRACE_HWI_FILTER_HOOK. The user defined hook for hwi num filter,
 *                             the hook should return true if you don't want trace this hwi num.
 * @retval #None
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceHwiFilterHookReg
 */
extern VOID LOS_TraceHwiFilterHookReg(TRACE_HWI_FILTER_HOOK hook);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TRACE_H */
