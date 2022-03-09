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

#include "sched.h"
#include "map_error.h"
#include "sys/types.h"
#include "unistd.h"
#include "los_task_pri.h"


int sched_get_priority_min(int policy)
{
    if (policy != SCHED_RR) {
        errno = EINVAL;
        return -1;
    }

    return OS_TASK_PRIORITY_HIGHEST;
}

int sched_get_priority_max(int policy)
{
    if (policy != SCHED_RR) {
        errno = EINVAL;
        return -1;
    }

    return OS_TASK_PRIORITY_LOWEST;
}

/*
 * This API is Linux-specific, not conforming to POSIX.
 */
 //此 API 是 Linux 特定的，不符合 POSIX
int sched_setaffinity(pid_t pid, size_t set_size, const cpu_set_t* set)
{
#ifdef LOSCFG_KERNEL_SMP
    UINT32 taskID = (UINT32)pid;
    UINT32 ret;

    if ((set == NULL) || (set_size != sizeof(cpu_set_t)) || (set->__bits[0] > LOSCFG_KERNEL_CPU_MASK)) {
        errno = EINVAL;
        return -1;
    }

    if (taskID == 0) {
        taskID = LOS_CurTaskIDGet();
        if (taskID == LOS_ERRNO_TSK_ID_INVALID) {
            errno = EINVAL;
            return -1;
        }
    }

    ret = LOS_TaskCpuAffiSet(taskID, (UINT16)set->__bits[0]);
    if (ret != LOS_OK) {
        errno = map_errno(ret);
        return -1;
    }
#endif

    return 0;
}

/*
 * This API is Linux-specific, not conforming to POSIX.
 */
int sched_getaffinity(pid_t pid, size_t set_size, cpu_set_t* set)
{
#ifdef LOSCFG_KERNEL_SMP
    UINT32 taskID = (UINT32)pid;
    UINT16 cpuAffiMask;

    if ((set == NULL) || (set_size != sizeof(cpu_set_t))) {
        errno = EINVAL;
        return -1;
    }

    if (taskID == 0) {
        taskID = LOS_CurTaskIDGet();
        if (taskID == LOS_ERRNO_TSK_ID_INVALID) {
            errno = EINVAL;
            return -1;
        }
    }

    cpuAffiMask = LOS_TaskCpuAffiGet(taskID);
    if (cpuAffiMask == 0) {
        errno = EINVAL;
        return -1;
    }

    set->__bits[0] = cpuAffiMask;
#endif

    return 0;
}

int __sched_cpucount(size_t set_size, const cpu_set_t* set)
{
    INT32 count = 0;
    UINT32 i;

    if ((set_size != sizeof(cpu_set_t)) || (set == NULL)) {
        return 0;
    }

    for (i = 0; i < set_size / sizeof(__CPU_BITTYPE); i++) {
        count += __builtin_popcountl(set->__bits[i]);
    }

    return count;
}
/// 任务让出CPU
int sched_yield()
{
    (void)LOS_TaskYield();
    return 0;
}
