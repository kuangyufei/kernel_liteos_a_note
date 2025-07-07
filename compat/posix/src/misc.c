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

#include "sys/types.h"
#include "sys/resource.h"
#include "unistd.h"
#include "stdio.h"
#include "pthread.h"
#include "sys/utsname.h"
#include "mqueue.h"
#include "semaphore.h"
#include "los_process_pri.h"
#include "los_hw.h"

/*
 * Supply some suitable values for constants that may not be present
 * in all configurations.
 */
// 定义系统配置项启用/禁用状态常量
#define SC_ENABLE                       1  // 功能启用状态
#define SC_DISABLE                      (-1)  // 功能禁用状态

// 宏定义：用于switch-case结构中返回特定配置值
#define CONF_CASE_RETURN(name, val) \
    case (name):                    \
        return (val)  // 返回配置项name对应的值val

/**
 * @brief 获取系统信息并填充到utsname结构体
 * @param[in,out] name 指向utsname结构体的指针，用于存储系统信息
 * @return 成功返回0，失败返回-EFAULT
 * @note 区分容器环境和普通环境的系统信息获取方式
 */
int uname(struct utsname *name)
{
    if (name == NULL) {  // 检查输入参数合法性
        return -EFAULT;  // 参数为空，返回错误码
    }

#ifdef LOSCFG_UTS_CONTAINER  // 容器环境配置
    struct utsname *currentUtsName = OsGetCurrUtsName();  // 获取当前容器的utsname信息
    if (currentUtsName == NULL) {  // 检查获取结果
        return -EFAULT;  // 获取失败，返回错误码
    }
    (VOID)memcpy_s(name, sizeof(struct utsname), currentUtsName, sizeof(struct utsname));  // 拷贝容器环境系统信息
#else  // 非容器环境

    (VOID)strcpy_s(name->sysname, sizeof(name->sysname), KERNEL_NAME);  // 设置系统名称
    (VOID)strcpy_s(name->nodename, sizeof(name->nodename), KERNEL_NODE_NAME);  // 设置节点名称
    // 格式化版本信息字符串（内核名称、版本号、编译日期时间）
    INT32 ret = sprintf_s(name->version, sizeof(name->version), "%s %u.%u.%u.%u %s %s",
                          KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, __DATE__, __TIME__);
    if (ret < 0) {  // 检查格式化结果
        return -EIO;  // 格式化失败，返回I/O错误码
    }

    const char *cpuInfo = LOS_CpuInfo();  // 获取CPU信息
    (VOID)strcpy_s(name->machine, sizeof(name->machine), cpuInfo);  // 设置CPU架构信息
    // 格式化发布版本号字符串
    ret = sprintf_s(name->release, sizeof(name->release), "%u.%u.%u.%u",
                     KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE);
    if (ret < 0) {  // 检查格式化结果
        return -EIO;  // 格式化失败，返回I/O错误码
    }

    name->domainname[0] = '\0';  // 初始化域名（未使用）
#endif
    return 0;  // 成功返回0
}

/**
 * @brief 获取系统配置参数值
 * @param[in] name 配置参数标识符（如_SC_ARG_MAX、_SC_PAGESIZE等）
 * @return 成功返回配置值，失败返回-1并设置errno
 * @note 根据不同配置项返回预定义常量或动态计算值
 */
long sysconf(int name)
{
    switch (name) {  // 根据配置项标识符分支处理
        CONF_CASE_RETURN(_SC_AIO_LISTIO_MAX,                    SC_DISABLE);  // AIO列表I/O最大数：禁用
        CONF_CASE_RETURN(_SC_AIO_MAX,                           SC_DISABLE);  // AIO最大数：禁用
        CONF_CASE_RETURN(_SC_AIO_PRIO_DELTA_MAX,                SC_DISABLE);  // AIO优先级增量最大值：禁用
        CONF_CASE_RETURN(_SC_ARG_MAX,                           ARG_MAX);  // 命令行参数最大长度
        CONF_CASE_RETURN(_SC_ASYNCHRONOUS_IO,                   SC_DISABLE);  // 异步I/O：禁用
        CONF_CASE_RETURN(_SC_CHILD_MAX,                         CHILD_MAX);  // 最大子进程数
        CONF_CASE_RETURN(_SC_CLK_TCK,                           SYS_CLK_TCK);  // 系统时钟频率
        CONF_CASE_RETURN(_SC_DELAYTIMER_MAX,                    DELAYTIMER_MAX);  // 延迟定时器最大值
        CONF_CASE_RETURN(_SC_FSYNC,                             SC_DISABLE);  // 文件同步功能：禁用
        CONF_CASE_RETURN(_SC_GETGR_R_SIZE_MAX,                  GETGR_R_SIZE_MAX);  // 组信息缓存大小
        CONF_CASE_RETURN(_SC_GETPW_R_SIZE_MAX,                  GETPW_R_SIZE_MAX);  // 密码信息缓存大小
        CONF_CASE_RETURN(_SC_JOB_CONTROL,                       SC_DISABLE);  // 作业控制：禁用
        CONF_CASE_RETURN(_SC_LOGIN_NAME_MAX,                    LOGIN_NAME_MAX);  // 登录名最大长度
        CONF_CASE_RETURN(_SC_MAPPED_FILES,                      SC_DISABLE);  // 映射文件支持：禁用
        CONF_CASE_RETURN(_SC_MEMLOCK,                           SC_DISABLE);  // 内存锁定：禁用
        CONF_CASE_RETURN(_SC_MEMLOCK_RANGE,                     SC_DISABLE);  // 内存区域锁定：禁用
        CONF_CASE_RETURN(_SC_MEMORY_PROTECTION,                 SC_DISABLE);  // 内存保护：禁用
        CONF_CASE_RETURN(_SC_MESSAGE_PASSING,                   SC_DISABLE);  // 消息传递：禁用
#ifdef LOSCFG_BASE_IPC_QUEUE  // 消息队列配置启用时
        CONF_CASE_RETURN(_SC_MQ_OPEN_MAX,                       MQ_OPEN_MAX);  // 最大打开消息队列数
        CONF_CASE_RETURN(_SC_MQ_PRIO_MAX,                       MQ_PRIO_MAX);  // 消息最大优先级
#endif
        CONF_CASE_RETURN(_SC_NGROUPS_MAX,                       NGROUPS_MAX);  // 最大附属组数量
        CONF_CASE_RETURN(_SC_OPEN_MAX,                          OPEN_MAX);  // 最大打开文件描述符数
        CONF_CASE_RETURN(_SC_PAGESIZE,                          0x1000);  // 页面大小（4KB）
        CONF_CASE_RETURN(_SC_PRIORITIZED_IO,                    SC_DISABLE);  // 优先级I/O：禁用
        CONF_CASE_RETURN(_SC_PRIORITY_SCHEDULING,               SC_DISABLE);  // 优先级调度：禁用
        CONF_CASE_RETURN(_SC_REALTIME_SIGNALS,                  SC_DISABLE);  // 实时信号：禁用
        CONF_CASE_RETURN(_SC_RTSIG_MAX,                         RTSIG_MAX);  // 实时信号最大数
        CONF_CASE_RETURN(_SC_SAVED_IDS,                         SC_DISABLE);  // 保存的用户ID：禁用

#ifdef LOSCFG_BASE_IPC_SEM  // 信号量配置启用时
        CONF_CASE_RETURN(_SC_SEMAPHORES,                        SC_ENABLE);  // 信号量：启用
        CONF_CASE_RETURN(_SC_SEM_NSEMS_MAX,                     SEM_NSEMS_MAX);  // 最大信号量集数
        CONF_CASE_RETURN(_SC_SEM_VALUE_MAX,                     SEM_VALUE_MAX);  // 信号量最大值
#endif

        CONF_CASE_RETURN(_SC_SHARED_MEMORY_OBJECTS,             SC_DISABLE);  // 共享内存对象：禁用
        CONF_CASE_RETURN(_SC_SIGQUEUE_MAX,                      SIGQUEUE_MAX);  // 信号队列最大长度
        CONF_CASE_RETURN(_SC_STREAM_MAX,                        STREAM_MAX);  // 流数量最大值
        CONF_CASE_RETURN(_SC_SYNCHRONIZED_IO,                   SC_DISABLE);  // 同步I/O：禁用
        CONF_CASE_RETURN(_SC_THREADS,                           SC_ENABLE);  // 线程支持：启用
        CONF_CASE_RETURN(_SC_THREAD_ATTR_STACKADDR,             SC_ENABLE);  // 线程栈地址属性：启用
        CONF_CASE_RETURN(_SC_THREAD_ATTR_STACKSIZE,             PTHREAD_ATTR_STACKSIZE);  // 线程栈大小属性
        CONF_CASE_RETURN(_SC_THREAD_DESTRUCTOR_ITERATIONS,      PTHREAD_DESTRUCTOR_ITERATIONS);  // 线程析构函数迭代次数
        CONF_CASE_RETURN(_SC_THREAD_KEYS_MAX,                   PTHREAD_KEYS_MAX);  // 线程私有数据键最大数
        CONF_CASE_RETURN(_SC_THREAD_PRIO_INHERIT,               PTHREAD_PRIO_INHERIT);  // 线程优先级继承
        CONF_CASE_RETURN(_SC_THREAD_PRIO_PROTECT,               PTHREAD_PRIO_PROTECT);  // 线程优先级保护
        CONF_CASE_RETURN(_SC_THREAD_PRIORITY_SCHEDULING,        PTHREAD_PRIORITY_SCHEDULING);  // 线程优先级调度
        CONF_CASE_RETURN(_SC_THREAD_PROCESS_SHARED,             PTHREAD_PROCESS_SHARED);  // 线程进程共享属性
        CONF_CASE_RETURN(_SC_THREAD_SAFE_FUNCTIONS,             SC_DISABLE);  // 线程安全函数：禁用
        CONF_CASE_RETURN(_SC_THREAD_STACK_MIN,                  PTHREAD_STACK_MIN);  // 线程最小栈大小
        CONF_CASE_RETURN(_SC_THREAD_THREADS_MAX,                PTHREAD_THREADS_MAX);  // 最大线程数
        CONF_CASE_RETURN(_SC_TIMERS,                            TIMERS);  // 定时器支持
        CONF_CASE_RETURN(_SC_TIMER_MAX,                         TIMER_MAX);  // 最大定时器数
        CONF_CASE_RETURN(_SC_TTY_NAME_MAX,                      TTY_NAME_MAX);  // 终端设备名最大长度
        CONF_CASE_RETURN(_SC_TZNAME_MAX,                        TZNAME_MAX);  // 时区名称最大长度
        CONF_CASE_RETURN(_SC_VERSION,                           POSIX_VERSION);  // POSIX版本号

        default:  // 未知配置项
            set_errno(EINVAL);  // 设置参数无效错误
            return -1;  // 返回错误
    }
}

/**
 * @brief 获取当前进程ID
 * @return 返回当前任务的ID作为进程ID
 * @note LiteOS中任务ID等同于进程ID
 */
pid_t getpid(void)
{
    return ((LosTaskCB *)(OsCurrTaskGet()))->taskID;  // 获取当前任务控制块并返回其taskID
}

/**
 * @brief 获取进程资源限制
 * @param[in] resource 资源类型（RLIMIT_NOFILE或RLIMIT_FSIZE）
 * @param[out] rlim 指向rlimit结构体的指针，用于存储资源限制值
 * @return 成功返回0，失败返回-EINVAL
 * @note 仅支持文件描述符数量和文件大小限制
 */
int getrlimit(int resource, struct rlimit *rlim)
{
    unsigned int intSave;  // 用于保存调度器锁定状态
    LosProcessCB *pcb = OsCurrProcessGet();  // 获取当前进程控制块
    struct rlimit *resourceLimit = pcb->resourceLimit;  // 获取进程资源限制数组

    switch (resource) {  // 检查资源类型合法性
        case RLIMIT_NOFILE:  // 文件描述符数量限制
        case RLIMIT_FSIZE:  // 文件大小限制
            break;  // 合法资源类型，继续执行
        default:  // 不支持的资源类型
            return -EINVAL;  // 返回参数无效错误
    }

    if (resourceLimit == NULL) {  // 资源限制未初始化
        rlim->rlim_cur = 0;  // 当前限制设为0
        rlim->rlim_max = 0;  // 最大限制设为0

        return 0;  // 返回成功
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器，防止并发修改
    rlim->rlim_cur = resourceLimit[resource].rlim_cur;  // 获取当前资源限制值
    rlim->rlim_max = resourceLimit[resource].rlim_max;  // 获取最大资源限制值
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    return 0;  // 返回成功
}

#define FSIZE_RLIMIT 0XFFFFFFFF  // 文件大小限制最大值（无限制）
#ifndef NR_OPEN_DEFAULT
#define NR_OPEN_DEFAULT 1024  // 默认最大打开文件描述符数
#endif
/**
 * @brief 设置进程资源限制
 * @param[in] resource 资源类型（RLIMIT_NOFILE或RLIMIT_FSIZE）
 * @param[in] rlim 指向rlimit结构体的指针，包含新的资源限制值
 * @return 成功返回0，失败返回-EINVAL或-EPERM
 * @note 仅支持文件描述符数量和文件大小限制的设置
 */
int setrlimit(int resource, const struct rlimit *rlim)
{
    unsigned int intSave;  // 用于保存调度器锁定状态
    struct rlimit *resourceLimit = NULL;  // 临时资源限制数组指针
    LosProcessCB *pcb = OsCurrProcessGet();  // 获取当前进程控制块

    if (rlim->rlim_cur > rlim->rlim_max) {  // 检查当前限制是否大于最大限制
        return -EINVAL;  // 参数无效，返回错误
    }
    switch (resource) {  // 检查资源类型并验证限制值
        case RLIMIT_NOFILE:  // 文件描述符数量限制
            if (rlim->rlim_max > NR_OPEN_DEFAULT) {  // 最大限制超过默认值
                return -EPERM;  // 权限不足，返回错误
            }
            break;
        case RLIMIT_FSIZE:  // 文件大小限制
            if (rlim->rlim_max > FSIZE_RLIMIT) {  // 最大限制超过允许值
                return -EPERM;  // 权限不足，返回错误
            }
            break;
        default:  // 不支持的资源类型
            return -EINVAL;  // 参数无效，返回错误
    }

    if (pcb->resourceLimit == NULL) {  // 资源限制未初始化
        // 分配资源限制数组内存（系统内存池）
        resourceLimit = LOS_MemAlloc((VOID *)m_aucSysMem0, RLIM_NLIMITS * sizeof(struct rlimit));
        if (resourceLimit == NULL) {  // 内存分配失败
            return -EINVAL;  // 返回错误
        }
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器，防止并发修改
    if (pcb->resourceLimit == NULL) {  // 首次初始化资源限制
        pcb->resourceLimit = resourceLimit;  // 设置进程资源限制指针
        resourceLimit = NULL;  // 清除临时指针（避免后续释放）
    }
    // 更新资源限制值
    pcb->resourceLimit[resource].rlim_cur = rlim->rlim_cur;
    pcb->resourceLimit[resource].rlim_max = rlim->rlim_max;
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, resourceLimit);  // 释放临时分配的内存（如未使用则为NULL安全）
    return 0;  // 返回成功
}
