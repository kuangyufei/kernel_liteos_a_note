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
#ifndef _LOS_TRACE_FRAME_H
#define _LOS_TRACE_FRAME_H

#include "los_typedef.h"
#include "stdarg.h"

#define LOSCFG_TRACE_LR
#define LOSCFG_TRACE_LR_RECORD 5
#define LOSCFG_TRACE_LR_IGNOR 0

#pragma pack (4)
typedef struct {
    UINT16  frameSize;
    UINT8   type;
    UINT8   cpuID;
    INT32   taskID;
    UINT64  timestamp;
#ifdef LOSCFG_TRACE_LR
    UINTPTR linkReg[LOSCFG_TRACE_LR_RECORD];
#endif
}FrameHead;
#pragma pack ()

#define SETPARAM(ap, st, member, type) ((st)->member = (type)va_arg((ap), unsigned int))
#define SETPARAM_LL(ap, st, member, type) ((st)->member = (type)va_arg((ap), unsigned long long))

#define LOS_TRACE_TASK 0
#define LOS_TRACE_TASK_NAME "task"
#define LOS_TRACE_IPC 1
#define LOS_TRACE_IPC_NAME "liteipc"
#define LOS_TRACE_MEM_TIME 2
#define LOS_TRACE_MEM_TIME_NAME "mem_time"
#define LOS_TRACE_MEM_INFO 3
#define LOS_TRACE_MEM_INFO_NAME "mem_info"

/* task trace frame */
typedef struct {
    UINTPTR taskEntry;
    UINT16  status;     /**< Task status:
                           OS_TASK_STATUS_READY
                           OS_TASK_STATUS_RUNNING
                           OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME */
    UINT16  mask;       /**< Task status Subdivision */
    UINTPTR ipcId;
} TaskTraceFrame;

/* liteipc trace frame */
typedef struct {
    UINT32  srcTid;
    UINT32  srcPid;
    UINT32  dstTid;
    UINT32  dstPid;
    UINT8  msgType;
    UINT8  code;
    UINT8  operation;
    UINT8  ipcStatus;
} IpcTraceFrame;

#define MEM_POOL_ADDR_MASK  0xffff
#define MEM_TRACE_MALLOC    0
#define MEM_TRACE_FREE      1
#define MEM_TRACE_MEMALIGN  2
#define MEM_TRACE_REALLOC   3

#define MEM_TRACE_CYCLE_TO_US(cycles)    (UINT32)((UINT64)(cycles) * 1000000 / OS_SYS_CLOCK) /* 1000000: unit is us */

/* mem time use trace frame */
typedef struct {
    UINT32 poolAddr : 16;     /* Record the low 16 bits of the memory pool address for distinction. */
    UINT32 type : 16;         /* 0：malloc, 1: free, 2: memalign, 3: realloc. */
    UINT32 timeUsed;          /* Time-consuming for each type of interface about type, uint: us. */
} MemTimeTraceFrame;

/* mem pool info trace frame */
typedef struct {
    UINT32 poolAddr : 16;      /* Record the low 16 bits of the memory pool address for distinction. */
    UINT32 fragment : 8;       /* 100: percent denominator. */
    UINT32 usage : 8;          /* Memory pool usage. */
    UINT32 freeTotalSize;      /* Total remaining memory. */
    UINT32 maxFreeSize;        /* Maximum memory block size. */
    UINT32 usedNodeNum : 16;   /* Number of used memory blocks. */
    UINT32 freeNodeNum : 16;   /* Number of unused memory blocks. */
} MemInfoTraceFrame;

extern INT32 OsTaskTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap);
extern INT32 OsIpcTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap);
extern INT32 OsMemTimeTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap);
extern INT32 OsMemInfoTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap);

#endif
