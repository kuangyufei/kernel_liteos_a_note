/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_process_pri.h"
#include "los_task_pri.h"
#include "los_sched_pri.h"
#include "los_hw_pri.h"
#include "los_sys_pri.h"
#include "los_futex_pri.h"
#include "los_mp.h"
#include "sys/wait.h"
#include "user_copy.h"
#include "time.h"
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_api.h"
#endif
/**
 * @brief 检查进程操作权限
 * @details 验证当前进程是否有权限操作目标进程，通过进程组ID判断权限等级
 * @param pid 目标进程ID
 * @param who 操作发起进程ID
 * @return 0表示权限允许，非0表示权限拒绝（返回错误码的负值）
 */
static int OsPermissionToCheck(unsigned int pid, unsigned int who)
{
    uintptr_t pgroupID = 0;  // 进程组ID
    unsigned int ret = OsGetProcessGroupCB(pid, &pgroupID);  // 获取目标进程的进程组ID
    if (ret != 0) {
        return -ret;  // 获取进程组ID失败，返回错误码
    } else if (pgroupID == OS_KERNEL_PROCESS_GROUP) {
        return -EPERM;  // 内核进程组，拒绝操作
    } else if ((pgroupID == OS_USER_PRIVILEGE_PROCESS_GROUP) && (pid != who)) {
        return -EPERM;  // 特权用户进程组且非自身进程，拒绝操作
    } else if ((UINTPTR)OS_PCB_FROM_PID(pid) == OS_USER_PRIVILEGE_PROCESS_GROUP) {
        return -EPERM;  // 进程控制块属于特权用户进程组，拒绝操作
    }

    return 0;  // 权限检查通过
}

/**
 * @brief 检查用户任务调度参数合法性
 * @details 验证任务ID、调度策略及参数是否符合系统要求
 * @param tid 任务ID
 * @param policy 调度策略
 * @param schedParam 调度参数
 * @param policyFlag 是否检查调度策略标志
 * @return 0表示合法，非0表示错误码
 */
static int UserTaskSchedulerCheck(unsigned int tid, int policy, const LosSchedParam *schedParam, bool policyFlag)
{
    int ret;  // 返回值
    int processPolicy;  // 进程调度策略

    if (OS_TID_CHECK_INVALID(tid)) {
        return EINVAL;  // 任务ID无效
    }

    ret = OsSchedulerParamCheck(policy, TRUE, schedParam);  // 检查调度参数合法性
    if (ret != 0) {
        return ret;  // 参数不合法，返回错误码
    }

    if (policyFlag) {
        ret = LOS_GetProcessScheduler(LOS_GetCurrProcessID(), &processPolicy, NULL);  // 获取当前进程调度策略
        if (ret < 0) {
            return -ret;  // 获取失败，返回错误码
        }
        if ((processPolicy != LOS_SCHED_DEADLINE) && (policy == LOS_SCHED_DEADLINE)) {
            return EPERM;  // 进程非 deadline 调度策略，不能设置为 deadline
        } else if ((processPolicy == LOS_SCHED_DEADLINE) && (policy != LOS_SCHED_DEADLINE)) {
            return EPERM;  // 进程为 deadline 调度策略，不能修改为其他策略
        }
    }
    return 0;  // 检查通过
}

/**
 * @brief 设置用户任务调度参数
 * @details 根据指定的调度策略和参数修改任务的调度属性
 * @param tid 任务ID
 * @param policy 调度策略
 * @param schedParam 调度参数
 * @param policyFlag 是否修改调度策略标志
 * @return 0表示成功，非0表示错误码
 */
static int OsUserTaskSchedulerSet(unsigned int tid, int policy, const LosSchedParam *schedParam, bool policyFlag)
{
    int ret;  // 返回值
    unsigned int intSave;  // 中断状态保存
    bool needSched = false;  // 是否需要重新调度
    SchedParam param = { 0 };  // 调度参数结构体

    ret = UserTaskSchedulerCheck(tid, policy, schedParam, policyFlag);  // 检查调度参数合法性
    if (ret != 0) {
        return ret;  // 参数不合法，返回错误码
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(tid);  // 获取任务控制块
    SCHEDULER_LOCK(intSave);  // 关调度器
    ret = OsUserTaskOperatePermissionsCheck(taskCB);  // 检查任务操作权限
    if (ret != LOS_OK) {
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        return ret;  // 权限不足，返回错误码
    }

    taskCB->ops->schedParamGet(taskCB, &param);  // 获取当前调度参数
    param.policy = (policyFlag == true) ? (UINT16)policy : param.policy;  // 设置调度策略（如果需要）
    if ((param.policy == LOS_SCHED_RR) || (param.policy == LOS_SCHED_FIFO)) {
        param.priority = schedParam->priority;  // 设置RR/FIFO调度优先级
    } else if (param.policy == LOS_SCHED_DEADLINE) {
#ifdef LOSCFG_SECURITY_CAPABILITY
        /* 用户态进程需要CAP_SCHED_SETPRIORITY权限才能修改优先级 */
        if (!IsCapPermit(CAP_SCHED_SETPRIORITY)) {
            SCHEDULER_UNLOCK(intSave);  // 开调度器
            return EPERM;  // 权限不足，返回错误
        }
#endif
        param.runTimeUs = schedParam->runTimeUs;  // 设置运行时间（微秒）
        param.deadlineUs = schedParam->deadlineUs;  // 设置截止时间（微秒）
        param.periodUs = schedParam->periodUs;  // 设置周期（微秒）
    }

    needSched = taskCB->ops->schedParamModify(taskCB, &param);  // 修改调度参数
    SCHEDULER_UNLOCK(intSave);  // 开调度器

    LOS_MpSchedule(OS_MP_CPU_ALL);  // 多处理器调度
    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_Schedule();  // 需要时触发调度
    }

    return LOS_OK;  // 设置成功
}

/**
 * @brief 任务主动让出CPU
 * @details 系统调用实现，使当前任务主动放弃CPU使用权
 * @param type 调度类型（未使用）
 */
void SysSchedYield(int type)
{
    (void)type;  // 未使用的参数

    (void)LOS_TaskYield();  // 调用内核任务让出接口
    return;
}

/**
 * @brief 获取调度策略
 * @details 获取指定任务或进程的调度策略
 * @param id 任务ID或进程ID
 * @param flag 标志（<0表示任务，>=0表示进程）
 * @return 调度策略值，负数表示错误码
 */
int SysSchedGetScheduler(int id, int flag)
{
    unsigned int intSave;  // 中断状态保存
    SchedParam param = { 0 };  // 调度参数结构体
    int policy;  // 调度策略
    int ret;  // 返回值

    if (flag < 0) {
        if (OS_TID_CHECK_INVALID(id)) {
            return -EINVAL;  // 任务ID无效
        }

        LosTaskCB *taskCB = OS_TCB_FROM_TID(id);  // 获取任务控制块
        SCHEDULER_LOCK(intSave);  // 关调度器
        ret = OsUserTaskOperatePermissionsCheck(taskCB);  // 检查操作权限
        if (ret != LOS_OK) {
            SCHEDULER_UNLOCK(intSave);  // 开调度器
            return -ret;  // 权限不足
        }

        taskCB->ops->schedParamGet(taskCB, &param);  // 获取任务调度参数
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        return (int)param.policy;  // 返回任务调度策略
    }

    if (id == 0) {
        id = (int)LOS_GetCurrProcessID();  // id为0时使用当前进程ID
    }

    ret = LOS_GetProcessScheduler(id, &policy, NULL);  // 获取进程调度策略
    if (ret < 0) {
        return ret;  // 获取失败，返回错误码
    }

    return policy;  // 返回进程调度策略
}

/**
 * @brief 设置调度策略
 * @details 设置指定任务或进程的调度策略及参数
 * @param id 任务ID或进程ID
 * @param policy 调度策略
 * @param userParam 用户空间调度参数
 * @param flag 标志（<0表示任务，>=0表示进程）
 * @return 0表示成功，负数表示错误码
 */
int SysSchedSetScheduler(int id, int policy, const LosSchedParam *userParam, int flag)
{
    int ret;  // 返回值
    LosSchedParam param;  // 内核空间调度参数

    if (LOS_ArchCopyFromUser(&param, userParam, sizeof(LosSchedParam)) != 0) {
        return -EFAULT;  // 从用户空间拷贝参数失败
    }

    if (flag < 0) {
        return -OsUserTaskSchedulerSet(id, policy, &param, true);  // 设置任务调度策略
    }

    if (policy == LOS_SCHED_RR) {
#ifdef LOSCFG_KERNEL_PLIMITS
        if (param.priority < OsPidLimitGetPriorityLimit()) {
            return -EINVAL;  // 优先级低于限制值
        }
#else
        if (param.priority < OS_USER_PROCESS_PRIORITY_HIGHEST) {
            return -EINVAL;  // 优先级低于用户进程最高优先级
        }
#endif
    }

    if (id == 0) {
        id = (int)LOS_GetCurrProcessID();  // id为0时使用当前进程ID
    }

    ret = OsPermissionToCheck(id, LOS_GetCurrProcessID());  // 检查操作权限
    if (ret < 0) {
        return ret;  // 权限不足
    }

    return OsSetProcessScheduler(LOS_PRIO_PROCESS, id, policy, &param);  // 设置进程调度策略
}

/**
 * @brief 获取调度参数
 * @details 获取指定任务或进程的调度参数
 * @param id 任务ID或进程ID
 * @param userParam 用户空间存储调度参数的指针
 * @param flag 标志（<0表示任务，>=0表示进程）
 * @return 0表示成功，负数表示错误码
 */
int SysSchedGetParam(int id, LosSchedParam *userParam, int flag)
{
    LosSchedParam schedParam = {0};  // 调度参数结构体
    SchedParam param = { 0 };  // 内核调度参数
    unsigned int intSave;  // 中断状态保存
    int ret;  // 返回值

    if (flag < 0) {
        if (OS_TID_CHECK_INVALID(id)) {
            return -EINVAL;  // 任务ID无效
        }

        LosTaskCB *taskCB = OS_TCB_FROM_TID(id);  // 获取任务控制块
        SCHEDULER_LOCK(intSave);  // 关调度器
        ret = OsUserTaskOperatePermissionsCheck(taskCB);  // 检查操作权限
        if (ret != LOS_OK) {
            SCHEDULER_UNLOCK(intSave);  // 开调度器
            return -ret;  // 权限不足
        }

        taskCB->ops->schedParamGet(taskCB, &param);  // 获取任务调度参数
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        if (param.policy == LOS_SCHED_DEADLINE) {
            schedParam.runTimeUs = param.runTimeUs;  // 设置deadline调度运行时间
            schedParam.deadlineUs = param.deadlineUs;  // 设置deadline调度截止时间
            schedParam.periodUs = param.periodUs;  // 设置deadline调度周期
        } else {
            schedParam.priority = param.priority;  // 设置RR/FIFO调度优先级
        }
    } else {
        if (id == 0) {
            id = (int)LOS_GetCurrProcessID();  // id为0时使用当前进程ID
        }

        if (OS_PID_CHECK_INVALID(id)) {
            return -EINVAL;  // 进程ID无效
        }

        ret = LOS_GetProcessScheduler(id, NULL, &schedParam);  // 获取进程调度参数
        if (ret < 0) {
            return ret;  // 获取失败
        }
    }

    if (LOS_ArchCopyToUser(userParam, &schedParam, sizeof(LosSchedParam))) {
        return -EFAULT;  // 拷贝参数到用户空间失败
    }
    return 0;  // 成功
}

/**
 * @brief 设置进程优先级
 * @details 修改指定进程的调度优先级
 * @param which 优先级类型（必须为进程优先级）
 * @param who 进程ID（0表示当前进程）
 * @param prio 目标优先级
 * @return 0表示成功，负数表示错误码
 */
int SysSetProcessPriority(int which, int who, int prio)
{
    int ret;  // 返回值

    if (which != LOS_PRIO_PROCESS) {
        return -EINVAL;  // 不支持的优先级类型
    }

    if (who == 0) {
        who = (int)LOS_GetCurrProcessID();  // who为0时使用当前进程ID
    }

#ifdef LOSCFG_KERNEL_PLIMITS
    if (prio < OsPidLimitGetPriorityLimit()) {
        return -EINVAL;  // 优先级低于限制值
    }
#else
    if (prio < OS_USER_PROCESS_PRIORITY_HIGHEST) {
        return -EINVAL;  // 优先级低于用户进程最高优先级
    }
#endif

    ret = OsPermissionToCheck(who, LOS_GetCurrProcessID());  // 检查操作权限
    if (ret < 0) {
        return ret;  // 权限不足
    }

    return LOS_SetProcessPriority(who, prio);  // 设置进程优先级
}

/**
 * @brief 设置调度参数
 * @details 修改指定任务或进程的调度参数（不修改调度策略）
 * @param id 任务ID或进程ID
 * @param userParam 用户空间调度参数
 * @param flag 标志（<0表示任务，>=0表示进程）
 * @return 0表示成功，负数表示错误码
 */
int SysSchedSetParam(int id, const LosSchedParam *userParam, int flag)
{
    int ret, policy;  // 返回值和调度策略
    LosSchedParam param;  // 内核空间调度参数

    if (flag < 0) {
        if (LOS_ArchCopyFromUser(&param, userParam, sizeof(LosSchedParam)) != 0) {
            return -EFAULT;  // 从用户空间拷贝参数失败
        }
        return -OsUserTaskSchedulerSet(id, LOS_SCHED_RR, &param, false);  // 设置任务调度参数
    }

    if (id == 0) {
        id = (int)LOS_GetCurrProcessID();  // id为0时使用当前进程ID
    }

    ret = LOS_GetProcessScheduler(id, &policy, NULL);  // 获取当前调度策略
    if (ret < 0) {
        return ret;  // 获取失败
    }

    return SysSchedSetScheduler(id, policy, userParam, flag);  // 调用设置调度策略接口
}

/**
 * @brief 获取进程优先级
 * @details 获取指定进程的调度优先级
 * @param which 优先级类型
 * @param who 进程ID（0表示当前进程）
 * @return 优先级值，负数表示错误码
 */
int SysGetProcessPriority(int which, int who)
{
    if (who == 0) {
        who = (int)LOS_GetCurrProcessID();  // who为0时使用当前进程ID
    }

    return OsGetProcessPriority(which, who);  // 获取进程优先级
}

/**
 * @brief 获取最小优先级
 * @details 获取RR调度策略的最小优先级值
 * @param policy 调度策略
 * @return 最小优先级值，-EINVAL表示不支持的调度策略
 */
int SysSchedGetPriorityMin(int policy)
{
    if (policy != LOS_SCHED_RR) {
        return -EINVAL;  // 仅支持RR调度策略
    }

    return OS_USER_PROCESS_PRIORITY_HIGHEST;  // 返回用户进程最高优先级（数值最小）
}

/**
 * @brief 获取最大优先级
 * @details 获取RR调度策略的最大优先级值
 * @param policy 调度策略
 * @return 最大优先级值，-EINVAL表示不支持的调度策略
 */
int SysSchedGetPriorityMax(int policy)
{
    if (policy != LOS_SCHED_RR) {
        return -EINVAL;  // 仅支持RR调度策略
    }

    return OS_USER_PROCESS_PRIORITY_LOWEST;  // 返回用户进程最低优先级（数值最大）
}

/**
 * @brief 获取RR调度时间片
 * @details 获取指定进程的RR调度时间片总和
 * @param pid 进程ID
 * @param tp 存储时间片结果的timespec结构体指针
 * @return 0表示成功，负数表示错误码
 */
int SysSchedRRGetInterval(int pid, struct timespec *tp)
{
    unsigned int intSave;  // 中断状态保存
    int ret;  // 返回值
    SchedParam param = { 0 };  // 调度参数
    time_t timeSlice = 0;  // 时间片总和
    struct timespec tv = { 0 };  // 时间结构体
    LosTaskCB *taskCB = NULL;  // 任务控制块指针
    LosProcessCB *processCB = NULL;  // 进程控制块指针

    if (tp == NULL) {
        return -EINVAL;  // 参数指针为空
    }

    if (OS_PID_CHECK_INVALID(pid)) {
        return -EINVAL;  // 进程ID无效
    }

    if (pid == 0) {
        processCB = OsCurrProcessGet();  // pid为0时使用当前进程
    } else {
        processCB = OS_PCB_FROM_PID(pid);  // 获取指定进程控制块
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    /* 若进程不存在则返回ESRCH */
    if (OsProcessIsInactive(processCB)) {
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        return -ESRCH;  // 进程不存在
    }

    // 遍历进程的所有任务
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {
        if (!OsTaskIsInactive(taskCB)) {  // 跳过非活动任务
            taskCB->ops->schedParamGet(taskCB, &param);  // 获取任务调度参数
            if (param.policy == LOS_SCHED_RR) {
                timeSlice += param.timeSlice;  // 累加RR调度时间片
            }
        }
    }

    SCHEDULER_UNLOCK(intSave);  // 开调度器

    timeSlice = timeSlice * OS_NS_PER_CYCLE;  // 转换为纳秒
    tv.tv_sec = timeSlice / OS_SYS_NS_PER_SECOND;  // 计算秒数
    tv.tv_nsec = timeSlice % OS_SYS_NS_PER_SECOND;  // 计算纳秒数
    ret = LOS_ArchCopyToUser(tp, &tv, sizeof(struct timespec));  // 拷贝结果到用户空间
    if (ret != 0) {
        return -EFAULT;  // 拷贝失败
    }

    return 0;  // 成功
}

/**
 * @brief 等待进程结束
 * @details 系统调用实现，等待指定进程终止
 * @param pid 进程ID
 * @param status 存储退出状态的指针
 * @param options 等待选项
 * @param rusage 资源使用信息（未使用）
 * @return 退出进程的ID，负数表示错误
 */
int SysWait(int pid, USER int *status, int options, void *rusage)
{
    (void)rusage;  // 未使用的参数

    return LOS_Wait(pid, status, (unsigned int)options, NULL);  // 调用内核等待接口
}

/**
 * @brief 等待进程结束（扩展版）
 * @details 系统调用实现，支持按进程ID、进程组ID或任意子进程等待
 * @param type 等待类型（P_ALL/P_PID/P_PGID）
 * @param pid 进程ID或进程组ID
 * @param info 存储状态信息的siginfo结构体指针
 * @param options 等待选项
 * @param rusage 资源使用信息（未使用）
 * @return 0表示成功，负数表示错误码
 */
int SysWaitid(idtype_t type, int pid, USER siginfo_t *info, int options, void *rusage)
{
    (void)rusage;  // 未使用的参数
    int ret;  // 返回值
    int truepid = 0;  // 转换后的进程ID

    switch (type) {
        case P_ALL:
            /* 等待任意子进程；id被忽略 */
            truepid = -1;  // 使用-1表示任意子进程
            break;
        case P_PID:
            /* 等待进程ID匹配的子进程 */
            if (pid <= 0) {
                return -EINVAL;  // 无效的进程ID
            }
            truepid = pid;  // 直接使用进程ID
            break;
        case P_PGID:
            /* 等待进程组ID匹配的子进程 */
            if (pid <= 1) {
                return -EINVAL;  // 无效的进程组ID
            }
            truepid = -pid;  // 使用负数表示进程组ID
            break;
        default:
            return -EINVAL;  // 不支持的等待类型
    }

    ret = LOS_Waitid(truepid, info, (unsigned int)options, NULL);  // 调用内核等待接口
    if (ret > 0) {
        ret = 0;  // 成功时返回0
    }
    return ret;  // 返回结果
}

/**
 * @brief 创建子进程（fork系统调用）
 * @details 通过拷贝当前进程创建新的子进程
 * @return 子进程中返回0，父进程中返回子进程ID，负数表示错误
 */
int SysFork(void)
{
    return OsClone(0, 0, 0);  // 调用克隆接口，不设置任何标志
}

/**
 * @brief 创建子进程（vfork系统调用）
 * @details 创建子进程并挂起父进程，直到子进程退出或执行exec
 * @return 子进程中返回0，父进程中返回子进程ID，负数表示错误
 */
int SysVfork(void)
{
    return OsClone(CLONE_VFORK, 0, 0);  // 调用克隆接口，设置CLONE_VFORK标志
}

/**
 * @brief 创建子进程（clone系统调用）
 * @details 灵活的进程创建接口，支持多种克隆标志
 * @param flags 克隆标志
 * @param stack 子进程栈指针
 * @param parentTid 父进程TID存储地址（未使用）
 * @param tls 线程本地存储（未使用）
 * @param childTid 子进程TID存储地址（未使用）
 * @return 子进程中返回0，父进程中返回子进程ID，负数表示错误
 */
int SysClone(int flags, void *stack, int *parentTid, unsigned long tls, int *childTid)
{
    (void)parentTid;  // 未使用的参数
    (void)tls;  // 未使用的参数
    (void)childTid;  // 未使用的参数

    return OsClone((UINT32)flags, (UINTPTR)stack, 0);  // 调用克隆接口
}

/**
 * @brief 取消共享资源
 * @details 取消当前进程与其他进程的资源共享
 * @param flags 要取消共享的资源标志
 * @return 0表示成功，-ENOSYS表示不支持
 */
int SysUnshare(int flags)
{
#ifdef LOSCFG_KERNEL_CONTAINER
    return OsUnshare(flags);  // 支持容器时调用取消共享接口
#else
    return -ENOSYS;  // 不支持容器功能
#endif
}

/**
 * @brief 设置命名空间
 * @details 将当前进程加入指定的命名空间
 * @param fd 命名空间文件描述符
 * @param type 命名空间类型
 * @return 0表示成功，-ENOSYS表示不支持
 */
int SysSetns(int fd, int type)
{
#ifdef LOSCFG_KERNEL_CONTAINER
    return OsSetNs(fd, type);  // 支持容器时调用设置命名空间接口
#else
    return -ENOSYS;  // 不支持容器功能
#endif
}

/**
 * @brief 获取父进程ID
 * @details 获取当前进程的父进程ID
 * @return 父进程ID
 */
unsigned int SysGetPPID(void)
{
#ifdef LOSCFG_PID_CONTAINER
    if (OsCurrProcessGet()->processID == OS_USER_ROOT_PROCESS_ID) {
        return 0;  // 根进程的父进程ID为0
    }
#endif
    return OsCurrProcessGet()->parentProcess->processID;  // 返回父进程ID
}

/**
 * @brief 获取进程ID
 * @details 获取当前进程的ID
 * @return 当前进程ID
 */
unsigned int SysGetPID(void)
{
    return LOS_GetCurrProcessID();  // 调用内核接口获取当前进程ID
}

/**
 * @brief 设置进程组ID
 * @details 将指定进程加入指定进程组
 * @param pid 进程ID（0表示当前进程）
 * @param gid 进程组ID（0表示使用进程ID）
 * @return 0表示成功，负数表示错误码
 */
int SysSetProcessGroupID(unsigned int pid, unsigned int gid)
{
    int ret;  // 返回值

    if (pid == 0) {
        pid = LOS_GetCurrProcessID();  // pid为0时使用当前进程ID
    }

    if (gid == 0) {
        gid = pid;  // gid为0时使用进程ID作为进程组ID
    }

    ret = OsPermissionToCheck(pid, gid);  // 检查操作权限
    if (ret < 0) {
        return ret;  // 权限不足
    }

    return OsSetProcessGroupID(pid, gid);  // 设置进程组ID
}

/**
 * @brief 获取进程组ID
 * @details 获取指定进程的进程组ID
 * @param pid 进程ID（0表示当前进程）
 * @return 进程组ID，负数表示错误码
 */
int SysGetProcessGroupID(unsigned int pid)
{
    if (pid == 0) {
        pid = LOS_GetCurrProcessID();  // pid为0时使用当前进程ID
    }

    return LOS_GetProcessGroupID(pid);  // 获取进程组ID
}

/**
 * @brief 获取当前进程组ID
 * @details 获取当前进程的进程组ID
 * @return 当前进程组ID
 */
int SysGetCurrProcessGroupID(void)
{
    return LOS_GetCurrProcessGroupID();  // 获取当前进程组ID
}

/**
 * @brief 获取用户ID
 * @details 获取当前进程的实际用户ID
 * @return 实际用户ID
 */
int SysGetUserID(void)
{
    return LOS_GetUserID();  // 获取实际用户ID
}

/**
 * @brief 获取有效用户ID
 * @details 获取当前进程的有效用户ID
 * @return 有效用户ID
 */
int SysGetEffUserID(void)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32 intSave;  // 中断状态保存
    int euid;  // 有效用户ID

    SCHEDULER_LOCK(intSave);  // 关调度器
#ifdef LOSCFG_USER_CONTAINER
    euid = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->euid);  // 容器环境下转换有效用户ID
#else
    euid = (int)OsCurrUserGet()->effUserID;  // 非容器环境直接获取有效用户ID
#endif
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return euid;  // 返回有效用户ID
#else
    return 0;  // 不支持能力时返回0
#endif
}

/**
 * @brief 获取有效组ID
 * @details 获取当前进程的有效组ID
 * @return 有效组ID
 */
int SysGetEffGID(void)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32 intSave;  // 中断状态保存
    int egid;  // 有效组ID

    SCHEDULER_LOCK(intSave);  // 关调度器
#ifdef LOSCFG_USER_CONTAINER
    egid = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->egid);  // 容器环境下转换有效组ID
#else
    egid = (int)OsCurrUserGet()->effGid;  // 非容器环境直接获取有效组ID
#endif
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return egid;  // 返回有效组ID
#else
    return 0;  // 不支持能力时返回0
#endif
}

/**
 * @brief 获取实际、有效和保存的用户ID
 * @details 将当前进程的实际用户ID、有效用户ID和保存的用户ID拷贝到用户空间
 * @param ruid 存储实际用户ID的指针
 * @param euid 存储有效用户ID的指针
 * @param suid 存储保存的用户ID的指针
 * @return 0表示成功，负数表示错误码
 */
int SysGetRealEffSaveUserID(int *ruid, int *euid, int *suid)
{
    int ret;  // 返回值
    int realUserID, effUserID, saveUserID;  // 实际、有效和保存的用户ID
#ifdef LOSCFG_SECURITY_CAPABILITY
    unsigned int intSave;  // 中断状态保存

    SCHEDULER_LOCK(intSave);  // 关调度器
#ifdef LOSCFG_USER_CONTAINER
    realUserID = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->uid);  // 容器环境下转换实际用户ID
    effUserID = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->euid);  // 容器环境下转换有效用户ID
    saveUserID = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->euid);  // 容器环境下转换保存的用户ID
#else
    realUserID = OsCurrUserGet()->userID;  // 非容器环境获取实际用户ID
    effUserID = OsCurrUserGet()->effUserID;  // 非容器环境获取有效用户ID
    saveUserID = OsCurrUserGet()->effUserID;  // 非容器环境获取保存的用户ID
#endif
    SCHEDULER_UNLOCK(intSave);  // 开调度器
#else
    realUserID = 0;  // 不支持能力时返回0
    effUserID = 0;  // 不支持能力时返回0
    saveUserID = 0;  // 不支持能力时返回0
#endif

    ret = LOS_ArchCopyToUser(ruid, &realUserID, sizeof(int));  // 拷贝实际用户ID到用户空间
    if (ret != 0) {
        return -EFAULT;  // 拷贝失败
    }

    ret = LOS_ArchCopyToUser(euid, &effUserID, sizeof(int));  // 拷贝有效用户ID到用户空间
    if (ret != 0) {
        return -EFAULT;  // 拷贝失败
    }

    ret = LOS_ArchCopyToUser(suid, &saveUserID, sizeof(int));  // 拷贝保存的用户ID到用户空间
    if (ret != 0) {
        return -EFAULT;  // 拷贝失败
    }

    return 0;  // 成功
}

#ifdef LOSCFG_USER_CONTAINER
long SysSetUserID(int uid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UserContainer *userContainer = CurrentCredentials()->userContainer;  // 用户容器
    int retval = -EPERM;  // 返回值，默认权限拒绝
    unsigned int intSave;  // 中断状态保存

    if (uid < 0) {
        return -EINVAL;  // 用户ID无效
    }

    UINT32 kuid = OsMakeKuid(userContainer, uid);  // 转换为内核用户ID
    if (kuid == (UINT32)-1) {
        return -EINVAL;  // 转换失败
    }

    Credentials *newCredentials = PrepareCredential(OsCurrProcessGet());  // 准备新的凭证
    if (newCredentials == NULL) {
        return -ENOMEM;  // 内存分配失败
    }

    Credentials *oldCredentials = CurrentCredentials();  // 当前凭证

    SCHEDULER_LOCK(intSave);  // 关调度器
    User *user = OsCurrUserGet();  // 获取当前用户
    if (IsCapPermit(CAP_SETUID)) {  // 检查是否有CAP_SETUID能力
        newCredentials->uid = kuid;  // 设置新的用户ID
        if (kuid != oldCredentials->uid) {
            user->userID = kuid;  // 更新用户ID
            user->effUserID = kuid;  // 更新有效用户ID
        }
        retval = LOS_OK;  // 成功
    } else if (kuid != oldCredentials->uid) {  // 没有权限且用户ID不匹配
        goto ERROR;  // 跳转错误处理
    }
    newCredentials->euid = kuid;  // 设置有效用户ID

    retval = CommitCredentials(newCredentials);  // 提交新凭证
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return retval;  // 返回结果

ERROR:
    FreeCredential(newCredentials);  // 释放新凭证
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return retval;  // 返回错误
#else
    if (uid != 0) {
        return -EPERM;  // 不支持能力时只能设置为0
    }
    return 0;  // 成功
#endif
}

#else

/**
 * @brief 设置用户ID
 * @details 修改当前进程的用户ID
 * @param uid 目标用户ID
 * @return 0表示成功，负数表示错误码
 */
int SysSetUserID(int uid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    int ret = -EPERM;  // 返回值，默认权限拒绝
    unsigned int intSave;  // 中断状态保存

    if (uid < 0) {
        return -EINVAL;  // 用户ID无效
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    User *user = OsCurrUserGet();  // 获取当前用户
    if (IsCapPermit(CAP_SETUID)) {  // 检查是否有CAP_SETUID能力
        user->userID = uid;  // 设置用户ID
        user->effUserID = uid;  // 设置有效用户ID
        /* 将进程添加到用户 */
    } else if (user->userID != uid) {  // 没有权限且用户ID不匹配
        goto EXIT;  // 跳转退出
    }

    ret = LOS_OK;  // 成功
    /* 将进程添加到用户 */
EXIT:
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
#else
    if (uid != 0) {
        return -EPERM;  // 不支持能力时只能设置为0
    }

    return 0;  // 成功
#endif
}
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY
/**
 * @brief 检查用户ID设置参数
 * @details 验证实际、有效和保存的用户ID参数合法性
 * @param ruid 实际用户ID
 * @param euid 有效用户ID
 * @param suid 保存的用户ID
 * @return 0表示合法，-EINVAL表示无效
 */
static int SetRealEffSaveUserIDCheck(int ruid, int euid, int suid)
{
    if ((ruid < 0) && (ruid != -1)) {
        return -EINVAL;  // 实际用户ID无效
    }

    if ((euid < 0) && (euid != -1)) {
        return -EINVAL;  // 有效用户ID无效
    }

    if ((suid < 0) && (suid != -1)) {
        return -EINVAL;  // 保存的用户ID无效
    }

    return 0;  // 合法
}
#endif

/**
 * @brief 设置实际、有效和保存的用户ID
 * @details 同时修改进程的实际用户ID、有效用户ID和保存的用户ID
 * @param ruid 实际用户ID（-1表示不修改）
 * @param euid 有效用户ID（-1表示不修改）
 * @param suid 保存的用户ID（-1表示不修改）
 * @return 0表示成功，负数表示错误码
 */
int SysSetRealEffSaveUserID(int ruid, int euid, int suid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    int ret;  // 返回值

    if ((ruid == -1) && (euid == -1) && (suid == -1)) {
        return 0;  // 所有ID都不修改
    }

    ret = SetRealEffSaveUserIDCheck(ruid, euid, suid);  // 检查参数合法性
    if (ret != 0) {
        return ret;  // 参数无效
    }

    if (ruid >= 0) {
        if (((euid != -1) && (euid != ruid)) || ((suid != -1) && (suid != ruid))) {
            return -EPERM;  // 实际用户ID修改时，其他ID必须相同或不修改
        }
        return SysSetUserID(ruid);  // 设置用户ID
    } else if (euid >= 0) {
        if ((suid != -1) && (suid != euid)) {
            return -EPERM;  // 有效用户ID修改时，保存的用户ID必须相同或不修改
        }
        return SysSetUserID(euid);  // 设置用户ID
    } else {
        return SysSetUserID(suid);  // 设置用户ID
    }
#else
    if ((ruid != 0) || (euid != 0) || (suid != 0)) {
        return -EPERM;  // 不支持能力时只能设置为0
    }
    return 0;  // 成功
#endif
}

/**
 * @brief 设置实际和有效用户ID
 * @details 修改进程的实际用户ID和有效用户ID
 * @param ruid 实际用户ID（-1表示不修改）
 * @param euid 有效用户ID（-1表示不修改）
 * @return 0表示成功，负数表示错误码
 */
int SysSetRealEffUserID(int ruid, int euid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    return SysSetRealEffSaveUserID(ruid, euid, -1);  // 调用设置用户ID接口，保存的用户ID不修改
#else
    if ((ruid != 0) || (euid != 0)) {
        return -EPERM;  // 不支持能力时只能设置为0
    }
    return 0;  // 成功
#endif
}

#ifdef LOSCFG_USER_CONTAINER
int SysSetGroupID(int gid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UserContainer *userContainer = CurrentCredentials()->userContainer;  // 用户容器
    int retval = -EPERM;  // 返回值，默认权限拒绝
    unsigned int oldGid;  // 旧组ID
    unsigned int intSave;  // 中断状态保存
    int count;  // 循环计数

    if (gid < 0) {
        return -EINVAL;  // 组ID无效
    }

    unsigned int kgid = OsMakeKgid(userContainer, gid);  // 转换为内核组ID
    if (kgid == (UINT32)-1) {
        return -EINVAL;  // 转换失败
    }

    Credentials *newCredentials = PrepareCredential(OsCurrProcessGet());  // 准备新的凭证
    if (newCredentials == NULL) {
        return -ENOMEM;  // 内存分配失败
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    User *user = OsCurrUserGet();  // 获取当前用户
    if (IsCapPermit(CAP_SETGID)) {  // 检查是否有CAP_SETGID能力
        newCredentials->gid = kgid;  // 设置新的组ID
        newCredentials->egid = kgid;  // 设置新的有效组ID
        oldGid = user->gid;  // 保存旧组ID
        user->gid = kgid;  // 更新组ID
        user->effGid = kgid;  // 更新有效组ID
        // 更新用户组列表中的旧组ID
        for (count = 0; count < user->groupNumber; count++) {
            if (user->groups[count] == oldGid) {
                user->groups[count] = kgid;  // 替换旧组ID
                retval = LOS_OK;  // 成功
                break;
            }
        }
    } else if (user->gid != kgid) {  // 没有权限且组ID不匹配
        goto ERROR;  // 跳转错误处理
    }

    retval = CommitCredentials(newCredentials);  // 提交新凭证
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return retval;  // 返回结果

ERROR:
    FreeCredential(newCredentials);  // 释放新凭证
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return retval;  // 返回错误

#else
    if (gid != 0) {
        return -EPERM;  // 不支持能力时只能设置为0
    }
    return 0;  // 成功
#endif
}
#else
/**
 * @brief 设置组ID
 * @details 修改当前进程的组ID
 * @param gid 目标组ID
 * @return 0表示成功，负数表示错误码
 */
int SysSetGroupID(int gid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    int ret = -EPERM;  // 返回值，默认权限拒绝
    unsigned int intSave;  // 中断状态保存
    unsigned int count;  // 循环计数
    unsigned int oldGid;  // 旧组ID
    User *user = NULL;  // 用户指针

    if (gid < 0) {
        return -EINVAL;  // 组ID无效
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    user = OsCurrUserGet();  // 获取当前用户
    if (IsCapPermit(CAP_SETGID)) {  // 检查是否有CAP_SETGID能力
        oldGid = user->gid;  // 保存旧组ID
        user->gid = gid;  // 设置组ID
        user->effGid = gid;  // 设置有效组ID
        // 更新用户组列表中的旧组ID
        for (count = 0; count < user->groupNumber; count++) {
            if (user->groups[count] == oldGid) {
                user->groups[count] = gid;  // 替换旧组ID
                ret = LOS_OK;  // 成功
                goto EXIT;  // 跳转退出
            }
        }
    } else if (user->gid != gid) {  // 没有权限且组ID不匹配
        goto EXIT;  // 跳转退出
    }

    ret = LOS_OK;  // 成功
    /* 将进程添加到用户 */
EXIT:
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果

#else
    if (gid != 0) {
        return -EPERM;  // 不支持能力时只能设置为0
    }

    return 0;  // 成功
#endif
}
#endif

/**
 * @brief 获取实际、有效和保存的组ID
 * @details 将当前进程的实际组ID、有效组ID和保存的组ID拷贝到用户空间
 * @param rgid 存储实际组ID的指针
 * @param egid 存储有效组ID的指针
 * @param sgid 存储保存的组ID的指针
 * @return 0表示成功，负数表示错误码
 */
int SysGetRealEffSaveGroupID(int *rgid, int *egid, int *sgid)
{
    int ret;  // 返回值
    int realGroupID, effGroupID, saveGroupID;  // 实际、有效和保存的组ID
#ifdef LOSCFG_SECURITY_CAPABILITY
    unsigned int intSave;  // 中断状态保存

    SCHEDULER_LOCK(intSave);  // 关调度器
#ifdef LOSCFG_USER_CONTAINER
    realGroupID = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->gid);  // 容器环境下转换实际组ID
    effGroupID = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->egid);  // 容器环境下转换有效组ID
    saveGroupID = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->egid);  // 容器环境下转换保存的组ID
#else
    realGroupID = OsCurrUserGet()->gid;  // 非容器环境获取实际组ID
    effGroupID = OsCurrUserGet()->effGid;  // 非容器环境获取有效组ID
    saveGroupID = OsCurrUserGet()->effGid;  // 非容器环境获取保存的组ID
#endif
    SCHEDULER_UNLOCK(intSave);  // 开调度器
#else
    realGroupID = 0;  // 不支持能力时返回0
    effGroupID = 0;  // 不支持能力时返回0
    saveGroupID = 0;  // 不支持能力时返回0
#endif

    ret = LOS_ArchCopyToUser(rgid, &realGroupID, sizeof(int));  // 拷贝实际组ID到用户空间
    if (ret != 0) {
        return -EFAULT;  // 拷贝失败
    }

    ret = LOS_ArchCopyToUser(egid, &effGroupID, sizeof(int));  // 拷贝有效组ID到用户空间
    if (ret != 0) {
        return -EFAULT;  // 拷贝失败
    }

    ret = LOS_ArchCopyToUser(sgid, &saveGroupID, sizeof(int));  // 拷贝保存的组ID到用户空间
    if (ret != 0) {
        return -EFAULT;  // 拷贝失败
    }

    return 0;  // 成功
}

#ifdef LOSCFG_SECURITY_CAPABILITY
/**
 * @brief 检查组ID设置参数
 * @details 验证实际、有效和保存的组ID参数合法性
 * @param rgid 实际组ID
 * @param egid 有效组ID
 * @param sgid 保存的组ID
 * @return 0表示合法，-EINVAL表示无效
 */
static int SetRealEffSaveGroupIDCheck(int rgid, int egid, int sgid)
{
    if ((rgid < 0) && (rgid != -1)) {
        return -EINVAL;  // 实际组ID无效
    }

    if ((egid < 0) && (egid != -1)) {
        return -EINVAL;  // 有效组ID无效
    }

    if ((sgid < 0) && (sgid != -1)) {
        return -EINVAL;  // 保存的组ID无效
    }

    return 0;  // 合法
}
#endif
/**
 * @brief 设置进程的真实、有效和保存的组ID
 * @param rgid 真实组ID，-1表示不修改
 * @param egid 有效组ID，-1表示不修改
 * @param sgid 保存的组ID，-1表示不修改
 * @return 成功返回0，失败返回错误码
 */
int SysSetRealEffSaveGroupID(int rgid, int egid, int sgid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    int ret;  // 函数返回值

    // 如果所有ID都设为-1，则不做修改直接返回成功
    if ((rgid == -1) && (egid == -1) && (sgid == -1)) {
        return 0;
    }

    // 检查组ID设置的合法性
    ret = SetRealEffSaveGroupIDCheck(rgid, egid, sgid);
    if (ret != 0) {
        return ret;  // 检查失败，返回错误码
    }

    // 根据不同的ID设置情况进行处理
    if (rgid >= 0) {
        // 如果设置真实组ID，有效组ID和保存组ID必须与真实组ID相同或不修改
        if (((egid != -1) && (egid != rgid)) || ((sgid != -1) && (sgid != rgid))) {
            return -EPERM;  // 权限不足错误
        }
        return SysSetGroupID(rgid);  // 设置组ID为rgid
    } else if (egid >= 0) {
        // 如果设置有效组ID，保存组ID必须与有效组ID相同或不修改
        if ((sgid != -1) && (sgid != egid)) {
            return -EPERM;  // 权限不足错误
        }
        return SysSetGroupID(egid);  // 设置组ID为egid
    } else {
        return SysSetGroupID(sgid);  // 设置组ID为sgid
    }

#else
    // 未启用安全能力时，只允许设置为0
    if ((rgid != 0) || (egid != 0) || (sgid != 0)) {
        return -EPERM;  // 权限不足错误
    }
    return 0;  // 成功返回
#endif
}

/**
 * @brief 设置进程的真实和有效组ID
 * @param rgid 真实组ID
 * @param egid 有效组ID
 * @return 成功返回0，失败返回错误码
 */
int SysSetRealEffGroupID(int rgid, int egid)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    return SysSetRealEffSaveGroupID(rgid, egid, -1);  // 调用保存组ID为-1的版本
#else
    // 未启用安全能力时，只允许设置为0
    if ((rgid != 0) || (egid != 0)) {
        return -EPERM;  // 权限不足错误
    }
    return 0;  // 成功返回
#endif
}

/**
 * @brief 获取当前进程的组ID
 * @return 当前进程的组ID
 */
int SysGetGroupID(void)
{
    return LOS_GetGroupID();  // 调用内核接口获取组ID
}

#ifdef LOSCFG_SECURITY_CAPABILITY
/**
 * @brief 设置用户组列表
 * @param listSize 组列表大小
 * @param safeList 安全的组列表指针
 * @param size 原始组列表大小
 * @return 成功返回0，失败返回错误码
 */
static int SetGroups(int listSize, const int *safeList, int size)
{
    User *oldUser = NULL;  // 旧用户结构体指针
    unsigned int intSave;  // 中断状态保存变量

    // 分配新用户结构体内存，包含组列表
    User *newUser = LOS_MemAlloc(m_aucSysMem1, sizeof(User) + listSize * sizeof(int));
    if (newUser == NULL) {
        return -ENOMEM;  // 内存分配失败
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    oldUser = OsCurrUserGet();  // 获取当前用户
    // 复制旧用户信息到新用户
    (VOID)memcpy_s(newUser, sizeof(User), oldUser, sizeof(User));
    if (safeList != NULL) {
        // 复制安全的组列表
        (VOID)memcpy_s(newUser->groups, size * sizeof(int), safeList, size * sizeof(int));
    }
    // 如果列表大小相同，添加当前组ID
    if (listSize == size) {
        newUser->groups[listSize] = oldUser->gid;
    }

    newUser->groupNumber = listSize + 1;  // 更新组数量
    OsCurrProcessGet()->user = newUser;  // 设置当前进程用户为新用户
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    (void)LOS_MemFree(m_aucSysMem1, oldUser);  // 释放旧用户内存
    return 0;  // 成功返回
}

/**
 * @brief 获取用户组列表
 * @param size 缓冲区大小
 * @param list 存储组列表的缓冲区指针
 * @return 成功返回组数量，失败返回错误码
 */
static int GetGroups(int size, int list[])
{
    unsigned int intSave;  // 中断状态保存变量
    int groupCount;  // 组数量
    int ret;  // 函数返回值
    int *safeList = NULL;  // 安全的组列表指针
    unsigned int listSize;  // 列表大小

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    groupCount = OsCurrUserGet()->groupNumber;  // 获取组数量

    listSize = groupCount * sizeof(int);  // 计算列表大小
    if (size == 0) {  // 如果size为0，仅返回组数量
        SCHEDULER_UNLOCK(intSave);
        return groupCount;
    } else if (list == NULL) {  // 如果列表指针为空，返回错误
        SCHEDULER_UNLOCK(intSave);
        return -EFAULT;
    } else if (size < groupCount) {  // 如果缓冲区太小，返回错误
        SCHEDULER_UNLOCK(intSave);
        return -EINVAL;
    }

    // 分配安全列表内存
    safeList = LOS_MemAlloc(m_aucSysMem1, listSize);
    if (safeList == NULL) {  // 内存分配失败
        SCHEDULER_UNLOCK(intSave);
        return -ENOMEM;
    }

    // 复制组列表到安全缓冲区
    (void)memcpy_s(safeList, listSize, &OsCurrProcessGet()->user->groups[0], listSize);
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    // 将组列表复制到用户空间
    ret = LOS_ArchCopyToUser(list, safeList, listSize);
    if (ret != 0) {
        groupCount = -EFAULT;  // 复制失败
    }

    (void)LOS_MemFree(m_aucSysMem1, safeList);  // 释放安全列表内存
    return groupCount;  // 返回组数量或错误码
}
#endif

/**
 * @brief 系统调用：获取用户组列表
 * @param size 缓冲区大小
 * @param list 存储组列表的缓冲区指针
 * @return 成功返回组数量，失败返回错误码
 */
int SysGetGroups(int size, int list[])
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    return GetGroups(size, list);  // 调用内部实现函数
#else
    int group = 0;  // 默认组ID
    int groupCount = 1;  // 默认组数量
    int ret;  // 函数返回值

    if (size == 0) {  // 如果size为0，返回组数量
        return groupCount;
    } else if (list == NULL) {  // 如果列表指针为空，返回错误
        return -EFAULT;
    } else if (size < groupCount) {  // 如果缓冲区太小，返回错误
        return -EINVAL;
    }

    // 将组ID复制到用户空间
    ret = LOS_ArchCopyToUser(list, &group, sizeof(int));
    if (ret != 0) {
        return -EFAULT;  // 复制失败
    }

    return groupCount;  // 返回组数量
#endif
}

/**
 * @brief 系统调用：设置用户组列表
 * @param size 组列表大小
 * @param list 组列表指针
 * @return 成功返回0，失败返回错误码
 */
int SysSetGroups(int size, const int list[])
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    int ret;  // 函数返回值
    int gid;  // 当前组ID
    int listSize = size;  // 列表大小
    unsigned int count;  // 循环计数器
    int *safeList = NULL;  // 安全的组列表指针
#endif

    // 如果size不为0但列表指针为空，返回错误
    if ((size != 0) && (list == NULL)) {
        return -EFAULT;
    }

    // 检查size是否在有效范围内
    if ((size < 0) || (size > OS_GROUPS_NUMBER_MAX)) {
        return -EINVAL;
    }

#ifdef LOSCFG_SECURITY_CAPABILITY
    // 检查是否有设置组ID的权限
    if (!IsCapPermit(CAP_SETGID)) {
        return -EPERM;
    }

    if (size != 0) {
        // 分配安全列表内存
        safeList = LOS_MemAlloc(m_aucSysMem1, size * sizeof(int));
        if (safeList == NULL) {
            return -ENOMEM;  // 内存分配失败
        }

        // 从用户空间复制组列表
        ret = LOS_ArchCopyFromUser(safeList, list, size * sizeof(int));
        if (ret != 0) {
            ret = -EFAULT;  // 复制失败
            goto EXIT;  // 跳转到退出处理
        }
        gid = OsCurrUserGet()->gid;  // 获取当前组ID
        // 检查组列表中的每个组ID
        for (count = 0; count < size; count++) {
            if (safeList[count] == gid) {
                listSize = size - 1;  // 调整列表大小
            } else if (safeList[count] < 0) {
                ret = -EINVAL;  // 组ID无效
                goto EXIT;  // 跳转到退出处理
            }
        }
    }

    ret = SetGroups(listSize, safeList, size);  // 设置组列表
EXIT:
    if (safeList != NULL) {
        (void)LOS_MemFree(m_aucSysMem1, safeList);  // 释放安全列表内存
    }

    return ret;  // 返回结果
#else
    return 0;  // 未启用安全能力时直接返回成功
#endif
}

/**
 * @brief 创建用户线程
 * @param func 线程入口函数
 * @param userParam 用户线程参数
 * @param joinable 是否可连接
 * @return 成功返回线程ID，失败返回无效值
 */
unsigned int SysCreateUserThread(const TSK_ENTRY_FUNC func, const UserTaskParam *userParam, bool joinable)
{
    TSK_INIT_PARAM_S param = { 0 };  // 线程初始化参数
    int ret;  // 函数返回值

    // 从用户空间复制线程参数
    ret = LOS_ArchCopyFromUser(&(param.userParam), userParam, sizeof(UserTaskParam));
    if (ret != 0) {
        return OS_INVALID_VALUE;  // 复制失败
    }

    param.pfnTaskEntry = func;  // 设置线程入口函数
    // 设置线程可连接属性
    if (joinable == TRUE) {
        param.uwResved = LOS_TASK_ATTR_JOINABLE;
    } else {
        param.uwResved = LOS_TASK_STATUS_DETACHED;
    }

    return OsCreateUserTask(OS_INVALID_VALUE, &param);  // 创建用户线程
}

/**
 * @brief 设置线程局部存储区域
 * @param area 存储区域地址
 * @return 成功返回0，失败返回错误码
 */
int SysSetThreadArea(const char *area)
{
    unsigned int intSave;  // 中断状态保存变量
    int ret = LOS_OK;  // 函数返回值

    // 检查地址是否为用户空间地址
    if (!LOS_IsUserAddress((unsigned long)(uintptr_t)area)) {
        return EINVAL;
    }

    LosTaskCB *taskCB = OsCurrTaskGet();  // 获取当前任务控制块
    SCHEDULER_LOCK(intSave);  // 关闭调度器
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);  // 获取进程控制块
    if (processCB->processMode != OS_USER_MODE) {  // 检查是否为用户模式
        ret = EPERM;  // 权限不足
        goto OUT;
    }

    taskCB->userArea = (unsigned long)(uintptr_t)area;  // 设置用户区域
OUT:
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return ret;  // 返回结果
}

/**
 * @brief 获取线程局部存储区域
 * @return 存储区域地址
 */
char *SysGetThreadArea(void)
{
    return (char *)(OsCurrTaskGet()->userArea);  // 返回用户区域地址
}

/**
 * @brief 设置用户线程为分离状态
 * @param taskID 线程ID
 * @return 成功返回0，失败返回错误码
 */
int SysUserThreadSetDetach(unsigned int taskID)
{
    unsigned int intSave;  // 中断状态保存变量
    int ret;  // 函数返回值

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查线程ID是否有效
        return EINVAL;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    SCHEDULER_LOCK(intSave);  // 关闭调度器
    ret = OsUserTaskOperatePermissionsCheck(taskCB);  // 检查操作权限
    if (ret != LOS_OK) {
        goto EXIT;
    }

    ret = (int)OsTaskSetDetachUnsafe(taskCB);  // 设置分离状态

EXIT:
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return ret;  // 返回结果
}

/**
 * @brief 分离用户线程
 * @param taskID 线程ID
 * @return 成功返回0，失败返回错误码
 */
int SysUserThreadDetach(unsigned int taskID)
{
    unsigned int intSave;  // 中断状态保存变量
    int ret;  // 函数返回值

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查线程ID是否有效
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    // 检查操作权限
    ret = OsUserTaskOperatePermissionsCheck(OS_TCB_FROM_TID(taskID));
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    if (ret != LOS_OK) {
        return ret;
    }

    if (LOS_TaskDelete(taskID) != LOS_OK) {  // 删除线程
        return ESRCH;  // 线程不存在
    }

    return LOS_OK;  // 成功返回
}

/**
 * @brief 连接用户线程
 * @param taskID 线程ID
 * @return 成功返回0，失败返回错误码
 */
int SysThreadJoin(unsigned int taskID)
{
    unsigned int intSave;  // 中断状态保存变量
    int ret;  // 函数返回值

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查线程ID是否有效
        return EINVAL;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    SCHEDULER_LOCK(intSave);  // 关闭调度器
    ret = OsUserTaskOperatePermissionsCheck(taskCB);  // 检查操作权限
    if (ret != LOS_OK) {
        goto EXIT;
    }

    ret = (int)OsTaskJoinPendUnsafe(OS_TCB_FROM_TID(taskID));  // 等待线程结束

EXIT:
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return ret;  // 返回结果
}

/**
 * @brief 用户线程组退出
 * @param status 退出状态
 */
void SysUserExitGroup(int status)
{
    (void)status;  // 未使用的参数
    OsProcessThreadGroupDestroy();  // 销毁进程线程组
}

/**
 * @brief 线程退出
 * @param status 退出状态
 */
void SysThreadExit(int status)
{
    OsRunningTaskToExit(OsCurrTaskGet(), (unsigned int)status);  // 运行中任务退出
}

/**
 * @brief futex系统调用实现
 * @param uAddr 用户空间地址
 * @param flags 操作标志
 * @param val 值
 * @param absTime 绝对时间
 * @param newUserAddr 新用户空间地址
 * @return 成功返回0，失败返回错误码
 */
int SysFutex(const unsigned int *uAddr, unsigned int flags, int val, unsigned int absTime, const unsigned int *newUserAddr)
{
    if ((flags & FUTEX_MASK) == FUTEX_REQUEUE) {  // 重新排队操作
        return -OsFutexRequeue(uAddr, flags, val, absTime, newUserAddr);
    }

    if ((flags & FUTEX_MASK) == FUTEX_WAKE) {  // 唤醒操作
        return -OsFutexWake(uAddr, flags, val);
    }

    return -OsFutexWait(uAddr, flags, val, absTime);  // 等待操作
}

/**
 * @brief 获取当前线程ID
 * @return 当前线程ID
 */
unsigned int SysGetTid(void)
{
    return OsCurrTaskGet()->taskID;  // 返回当前任务ID
}

/**
 * @brief 调度亲和性参数预处理
 * @param id 进程ID或线程ID
 * @param flag 标志：>=0表示进程模式，<0表示线程模式
 * @param taskID 输出参数，任务ID
 * @param processID 输出参数，进程ID
 * @return 成功返回0，失败返回错误码
 */
static int SchedAffinityParameterPreprocess(int id, int flag, unsigned int *taskID, unsigned int *processID)
{
    if (flag >= 0) {  // 进程模式
        if (OS_PID_CHECK_INVALID(id)) {  // 检查进程ID是否有效
            return -ESRCH;
        }
        LosProcessCB *ProcessCB = OS_PCB_FROM_PID((UINT32)id);  // 获取进程控制块
        if (ProcessCB->threadGroup == NULL) {  // 检查线程组是否存在
            return -ESRCH;
        }
        // 设置任务ID：0表示当前任务，否则为进程的主线程
        *taskID = (id == 0) ? (OsCurrTaskGet()->taskID) : (ProcessCB->threadGroup->taskID);
        // 设置进程ID：0表示当前进程，否则为指定进程
        *processID = (id == 0) ? (OS_PCB_FROM_TID(*taskID)->processID) : id;
    } else {  // 线程模式
        if (OS_TID_CHECK_INVALID(id)) {  // 检查线程ID是否有效
            return -ESRCH;
        }
        *taskID = id;  // 设置任务ID
        *processID = OS_INVALID_VALUE;  // 进程ID无效
    }
    return LOS_OK;  // 成功返回
}

/**
 * @brief 获取调度亲和性
 * @param id 进程ID或线程ID
 * @param cpuset 输出参数，CPU亲和性掩码
 * @param flag 标志：>=0表示进程模式，<0表示线程模式
 * @return 成功返回0，失败返回错误码
 */
int SysSchedGetAffinity(int id, unsigned int *cpuset, int flag)
{
    int ret;  // 函数返回值
    unsigned int processID;  // 进程ID
    unsigned int taskID;  // 任务ID
    unsigned int intSave;  // 中断状态保存变量
    unsigned int cpuAffiMask;  // CPU亲和性掩码

    // 参数预处理
    ret = SchedAffinityParameterPreprocess(id, flag, &taskID, &processID);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    if (flag >= 0) {  // 进程模式
        if (OsProcessIsInactive(OS_PCB_FROM_PID(processID))) {  // 检查进程是否活跃
            SCHEDULER_UNLOCK(intSave);
            return -ESRCH;
        }
    } else {  // 线程模式
        // 检查操作权限
        ret = OsUserTaskOperatePermissionsCheck(OS_TCB_FROM_TID(taskID));
        if (ret != LOS_OK) {
            SCHEDULER_UNLOCK(intSave);
            if (ret == EINVAL) {
                return -ESRCH;
            }
            return -ret;
        }
    }

#ifdef LOSCFG_KERNEL_SMP
    cpuAffiMask = (unsigned int)OS_TCB_FROM_TID(taskID)->cpuAffiMask;  // 获取CPU亲和性掩码
#else
    cpuAffiMask = 1;  // 非SMP系统，CPU掩码为1
#endif /* LOSCFG_KERNEL_SMP */

    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    // 将CPU亲和性掩码复制到用户空间
    ret = LOS_ArchCopyToUser(cpuset, &cpuAffiMask, sizeof(unsigned int));
    if (ret != LOS_OK) {
        return -EFAULT;  // 复制失败
    }

    return LOS_OK;  // 成功返回
}

/**
 * @brief 设置调度亲和性
 * @param id 进程ID或线程ID
 * @param cpuset CPU亲和性掩码
 * @param flag 标志：>=0表示进程模式，<0表示线程模式
 * @return 成功返回0，失败返回错误码
 */
int SysSchedSetAffinity(int id, const unsigned short cpuset, int flag)
{
    int ret;  // 函数返回值
    unsigned int processID;  // 进程ID
    unsigned int taskID;  // 任务ID
    unsigned int intSave;  // 中断状态保存变量
    unsigned short currCpuMask;  // 当前CPU掩码
    bool needSched = FALSE;  // 是否需要调度

    // 检查CPU掩码是否有效
    if (cpuset > LOSCFG_KERNEL_CPU_MASK) {
        return -EINVAL;
    }

    // 参数预处理
    ret = SchedAffinityParameterPreprocess(id, flag, &taskID, &processID);
    if (ret != LOS_OK) {
        return ret;
    }

    if (flag >= 0) {  // 进程模式
        // 检查权限
        ret = OsPermissionToCheck(processID, LOS_GetCurrProcessID());
        if (ret != LOS_OK) {
            return ret;
        }
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        if (OsProcessIsInactive(OS_PCB_FROM_PID(processID))) {  // 检查进程是否活跃
            SCHEDULER_UNLOCK(intSave);
            return -ESRCH;
        }
    } else {  // 线程模式
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        // 检查操作权限
        ret = OsUserTaskOperatePermissionsCheck(OS_TCB_FROM_TID(taskID));
        if (ret != LOS_OK) {
            SCHEDULER_UNLOCK(intSave);
            if (ret == EINVAL) {
                return -ESRCH;
            }
            return -ret;
        }
    }

    // 设置任务CPU亲和性
    needSched = OsTaskCpuAffiSetUnsafe(taskID, cpuset, &currCpuMask);
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    // 如果需要调度且调度器活跃
    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_MpSchedule(currCpuMask);  // 多处理器调度
        LOS_Schedule();  // 触发调度
    }

    return LOS_OK;  // 成功返回
}
