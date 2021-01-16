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

#include "los_timeslice_pri.h"
#include "los_process_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//检查进程和任务的时间片,如果没有时间片了直接调度
LITE_OS_SEC_TEXT VOID OsTimesliceCheck(VOID)
{
    LosTaskCB *runTask = NULL;
    LosProcessCB *runProcess = OsCurrProcessGet();//获取当前进程
    if (runProcess->policy != LOS_SCHED_RR) {//进程调度算法是否是抢占式
        goto SCHED_TASK;//进程不是抢占式调度直接去检查任务的时间片
    }

    if (runProcess->timeSlice != 0) {//进程还有时间片吗?
        runProcess->timeSlice--;//进程时间片减少一次
        if (runProcess->timeSlice == 0) {//没有时间片了
            LOS_Schedule();//进程时间片用完,发起调度
        }
    }

SCHED_TASK:
    runTask = OsCurrTaskGet();//获取当前任务
    if (runTask->policy != LOS_SCHED_RR) {//任务调度算法是否是抢占式
        return;//任务不是抢占式调度直接结束检查
    }

    if (runTask->timeSlice != 0) {//任务还有时间片吗?
        runTask->timeSlice--;//任务时间片也减少一次
        if (runTask->timeSlice == 0) {//没有时间片了
            LOS_Schedule();//任务时间片用完,发起调度
        }
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

