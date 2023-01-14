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

#ifndef _LOS_CPUP_PRI_H
#define _LOS_CPUP_PRI_H

#include "los_cpup.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
* @ingroup los_cpup
* Number of historical running time records
*/
#define OS_CPUP_HISTORY_RECORD_NUM   11

typedef struct {
    UINT64 allTime;    /**< Total running time | 总运行时间*/
    UINT64 startTime;  /**< Time before a task is invoked */
    UINT64 historyTime[OS_CPUP_HISTORY_RECORD_NUM + 1]; /**< Historical running time, the last one saves zero */
} OsCpupBase;

/**
 * @ingroup los_cpup
 * Count the CPU usage structures of a task.
 */
typedef struct {
    UINT32 id;         /**< irq ID */
    UINT16 status;     /**< irq status */
    UINT64 allTime;
    UINT64 timeMax;
    UINT64 count;
    OsCpupBase cpup;   /**< irq cpup base */
} OsIrqCpupCB;

typedef struct TagTaskCB LosTaskCB;
typedef struct TagTaskInfo TaskInfo;
typedef struct TagProcessInfo ProcessInfo;

extern UINT32 OsCpupInit(VOID);
extern UINT32 OsCpupGuardCreator(VOID);
extern VOID OsCpupCycleEndStart(LosTaskCB *runTask, LosTaskCB *newTask);
extern UINT32 OsGetProcessAllCpuUsageUnsafe(OsCpupBase *processCpup, ProcessInfo *processInfo);
extern UINT32 OsGetTaskAllCpuUsageUnsafe(OsCpupBase *taskCpup, TaskInfo *taskInfo);
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
extern UINT32 OsGetAllIrqCpuUsageUnsafe(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len);
extern VOID OsCpupIrqStart(UINT16);
extern VOID OsCpupIrqEnd(UINT16, UINT32);
extern OsIrqCpupCB *OsGetIrqCpupArrayBase(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_CPUP_PRI_H */
