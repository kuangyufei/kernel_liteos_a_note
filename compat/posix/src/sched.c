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

/**
 * @brief 获取调度策略的最小优先级
 * @param[in] policy 调度策略
 * @return 成功返回最小优先级，失败返回-1并设置errno
 */
int sched_get_priority_min(int policy)
{
    if (policy != SCHED_RR) {  // 仅支持SCHED_RR调度策略
        errno = EINVAL;        // 设置错误码为参数无效
        return -1;             // 返回错误
    }

    return OS_TASK_PRIORITY_HIGHEST;  // 返回最高任务优先级(数值最小)
}

/**
 * @brief 获取调度策略的最大优先级
 * @param[in] policy 调度策略
 * @return 成功返回最大优先级，失败返回-1并设置errno
 */
int sched_get_priority_max(int policy)
{
    if (policy != SCHED_RR) {  // 仅支持SCHED_RR调度策略
        errno = EINVAL;        // 设置错误码为参数无效
        return -1;             // 返回错误
    }

    return OS_TASK_PRIORITY_LOWEST;  // 返回最低任务优先级(数值最大)
}

/*
 * This API is Linux-specific, not conforming to POSIX.
 */
/**
 * @brief 设置进程的CPU亲和性(仅Linux特有，非POSIX标准)
 * @param[in] pid 进程ID，0表示当前进程
 * @param[in] set_size cpu_set_t结构体大小
 * @param[in] set CPU亲和性集合
 * @return 成功返回0，失败返回-1并设置errno
 */
int sched_setaffinity(pid_t pid, size_t set_size, const cpu_set_t* set)
{
#ifdef LOSCFG_KERNEL_SMP  // 仅在SMP(对称多处理)配置下生效
    UINT32 taskID = (UINT32)pid;  // 将pid转换为任务ID
    UINT32 ret;                   // 返回值

    // 检查参数有效性：集合为空、大小不匹配或CPU掩码超出范围
    if ((set == NULL) || (set_size != sizeof(cpu_set_t)) || (set->__bits[0] > LOSCFG_KERNEL_CPU_MASK)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    if (taskID == 0) {  // pid为0表示设置当前进程
        taskID = LOS_CurTaskIDGet();  // 获取当前任务ID
        if (taskID == LOS_ERRNO_TSK_ID_INVALID) {  // 检查任务ID有效性
            errno = EINVAL;                        // 设置错误码为参数无效
            return -1;                             // 返回错误
        }
    }

    // 调用内核接口设置任务CPU亲和性
    ret = LOS_TaskCpuAffiSet(taskID, (UINT16)set->__bits[0]);
    if (ret != LOS_OK) {  // 检查设置结果
        errno = map_errno(ret);  // 转换内核错误码为posix错误码
        return -1;               // 返回错误
    }
#endif  // LOSCFG_KERNEL_SMP

    return 0;  // 成功返回0
}

/*
 * This API is Linux-specific, not conforming to POSIX.
 */
/**
 * @brief 获取进程的CPU亲和性(仅Linux特有，非POSIX标准)
 * @param[in] pid 进程ID，0表示当前进程
 * @param[in] set_size cpu_set_t结构体大小
 * @param[out] set CPU亲和性集合输出指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int sched_getaffinity(pid_t pid, size_t set_size, cpu_set_t* set)
{
#ifdef LOSCFG_KERNEL_SMP  // 仅在SMP(对称多处理)配置下生效
    UINT32 taskID = (UINT32)pid;  // 将pid转换为任务ID
    UINT16 cpuAffiMask;           // CPU亲和性掩码

    // 检查参数有效性：集合为空或大小不匹配
    if ((set == NULL) || (set_size != sizeof(cpu_set_t))) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    if (taskID == 0) {  // pid为0表示获取当前进程
        taskID = LOS_CurTaskIDGet();  // 获取当前任务ID
        if (taskID == LOS_ERRNO_TSK_ID_INVALID) {  // 检查任务ID有效性
            errno = EINVAL;                        // 设置错误码为参数无效
            return -1;                             // 返回错误
        }
    }

    cpuAffiMask = LOS_TaskCpuAffiGet(taskID);  // 获取任务CPU亲和性掩码
    if (cpuAffiMask == 0) {                    // 检查掩码有效性
        errno = EINVAL;                        // 设置错误码为参数无效
        return -1;                             // 返回错误
    }

    set->__bits[0] = cpuAffiMask;  // 将CPU亲和性掩码存入输出参数
#endif  // LOSCFG_KERNEL_SMP

    return 0;  // 成功返回0
}

/**
 * @brief 计算CPU集合中的CPU数量
 * @param[in] set_size cpu_set_t结构体大小
 * @param[in] set CPU集合
 * @return 成功返回CPU数量，失败返回0
 */
int __sched_cpucount(size_t set_size, const cpu_set_t* set)
{
    INT32 count = 0;    // CPU计数器
    UINT32 i;           // 循环变量

    // 检查参数有效性：集合为空或大小不匹配
    if ((set_size != sizeof(cpu_set_t)) || (set == NULL)) {
        return 0;       // 返回0表示错误
    }

    // 遍历CPU集合的每个位域，统计置位数量
    for (i = 0; i < set_size / sizeof(__CPU_BITTYPE); i++) {
        count += __builtin_popcountl(set->__bits[i]);  // 计算每个位域中的置位数量
    }

    return count;  // 返回CPU总数
}

/**
 * @brief 主动让出CPU执行权
 * @return 始终返回0
 */
int sched_yield()
{
    (void)LOS_TaskYield();  // 调用内核接口让出CPU
    return 0;               // 成功返回0
}