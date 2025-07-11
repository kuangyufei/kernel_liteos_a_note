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

#ifndef _LOS_SCHED_PRI_H
#define _LOS_SCHED_PRI_H

#include "los_sortlink_pri.h"
#include "los_sys_pri.h"
#include "los_hwi.h"
#include "hal_timer.h"
#ifdef LOSCFG_SCHED_DEBUG
#include "los_statistics_pri.h"
#endif
#include "los_stackinfo_pri.h"
#include "los_futex_pri.h"
#ifdef LOSCFG_KERNEL_PM
#include "los_pm_pri.h"
#endif
#include "los_signal.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#ifdef LOSCFG_KERNEL_LITEIPC
#include "hm_liteipc.h"
#endif
#include "los_mp.h"
#ifdef LOSCFG_KERNEL_CONTAINER
#include "los_container_pri.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @ingroup los_sched
 * @brief 调度最小周期（单位：系统时钟周期）
 * @details 计算公式：系统时钟频率 / 最小滴答频率配置项
 */
#define OS_SCHED_MINI_PERIOD          (OS_SYS_CLOCK / LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI)

/**
 * @ingroup los_sched
 * @brief 滴答响应精度（单位：系统时钟周期）
 * @details 取值为调度最小周期的75%，用于确保调度响应的时间精度
 */
#define OS_TICK_RESPONSE_PRECISION    (UINT32)((OS_SCHED_MINI_PERIOD * 75) / 100)

/**
 * @ingroup los_sched
 * @brief 最大调度响应时间
 * @details 使用无效排序链表时间作为最大响应时间阈值
 */
#define OS_SCHED_MAX_RESPONSE_TIME    OS_SORT_LINK_INVALID_TIME

/**
 * @ingroup los_sched
 * @brief 将滴答数转换为系统时钟周期数
 * @param ticks 滴答数
 * @return 转换后的系统时钟周期数（64位无符号整数）
 */
#define OS_SCHED_TICK_TO_CYCLE(ticks) ((UINT64)ticks * OS_CYCLE_PER_TICK)

/**
 * @ingroup los_sched
 * @brief 将CPU亲和性掩码转换为CPU ID
 * @param mask CPU亲和性掩码（二进制位表示）
 * @return CPU ID（从0开始计数）
 */
#define AFFI_MASK_TO_CPUID(mask)      ((UINT16)((mask) - 1))

/** @ingroup los_sched EDF调度最小运行时间（单位：微秒） */
#define OS_SCHED_EDF_MIN_RUNTIME    100 /* 100 us */
/** @ingroup los_sched EDF调度最小截止时间（单位：微秒） */
#define OS_SCHED_EDF_MIN_DEADLINE   400 /* 400 us */
/** @ingroup los_sched EDF调度最大截止时间（单位：微秒） */
#define OS_SCHED_EDF_MAX_DEADLINE   5000000 /* 5 s，十进制5000000微秒 */

/**
 * @ingroup los_sched
 * @brief 任务调度状态全局变量
 * @details 每一位代表一个CPU的调度状态，bit N对应CPU N的调度激活状态
 */
extern UINT32 g_taskScheduled;

/**
 * @ingroup los_sched
 * @brief 判断当前CPU调度器是否激活
 * @return 非0表示激活，0表示未激活
 */
#define OS_SCHEDULER_ACTIVE (g_taskScheduled & (1U << ArchCurrCpuid()))

/**
 * @ingroup los_sched
 * @brief 判断所有CPU调度器是否都已激活
 * @return 非0表示全部激活，0表示未全部激活
 */
#define OS_SCHEDULER_ALL_ACTIVE (g_taskScheduled == LOSCFG_KERNEL_CPU_MASK)

/**
 * @ingroup los_sched
 * @brief 任务控制块结构体前向声明
 */
typedef struct TagTaskCB LosTaskCB;

/**
 * @ingroup los_sched
 * @brief 调度链表查找函数指针类型
 * @param[in] 参数1：查找关键字
 * @param[in] 参数2：比较对象
 * @return BOOL：TRUE表示找到匹配项，FALSE表示未找到
 */
typedef BOOL (*SCHED_TL_FIND_FUNC)(UINTPTR, UINTPTR);

/**
 * @ingroup los_sched
 * @brief 获取当前调度时间周期数
 * @return 当前系统时钟周期数（64位）
 */
STATIC INLINE UINT64 OsGetCurrSchedTimeCycle(VOID)
{
    return HalClockGetCycles();
}

/**
 * @ingroup los_sched
 * @brief 调度标志枚举
 * @details 定义调度器的不同状态标志，用于控制调度行为
 */
typedef enum {
    INT_NO_RESCH = 0x0,   /**< 无需重新调度 */
    INT_PEND_RESCH = 0x1, /**< 挂起调度标志，需要触发调度 */
    INT_PEND_TICK = 0x2,   /**< 挂起滴答标志，需要处理滴答事件 */
} SchedFlag;

/** @ingroup los_sched 优先级队列数量 */
#define OS_PRIORITY_QUEUE_NUM 32

/**
 * @ingroup los_sched
 * @brief 高优先级优先(HPF)队列结构
 * @details 包含特定优先级的任务队列、就绪任务计数和队列位图
 */
typedef struct {
    LOS_DL_LIST priQueList[OS_PRIORITY_QUEUE_NUM]; /**< 优先级队列链表数组 */
    UINT32      readyTasks[OS_PRIORITY_QUEUE_NUM]; /**< 就绪任务计数数组 */
    UINT32      queueBitmap;                       /**< 队列位图，用于快速查找非空队列 */
} HPFQueue;

/**
 * @ingroup los_sched
 * @brief HPF调度运行队列结构
 * @details 包含多级HPF队列和全局队列位图
 */
typedef struct {
    HPFQueue queueList[OS_PRIORITY_QUEUE_NUM]; /**< HPF队列数组 */
    UINT32   queueBitmap;                      /**< 队列位图，用于标识非空HPF队列 */
} HPFRunqueue;

/**
 * @ingroup los_sched
 * @brief EDF调度运行队列结构
 * @details 用于实现最早截止时间优先调度算法的队列管理
 */
typedef struct {
    LOS_DL_LIST root;    /**< EDF队列根节点 */
    LOS_DL_LIST waitList;/**< 等待队列 */
    UINT64 period;       /**< 调度周期（单位：系统时钟周期） */
} EDFRunqueue;

/**
 * @ingroup los_sched
 * @brief 调度运行队列结构
 * @details 包含系统调度所需的各类队列和调度控制参数
 */
typedef struct {
    SortLinkAttribute timeoutQueue;  /**< 任务超时队列，用于管理任务超时事件 */
    HPFRunqueue       *hpfRunqueue;  /**< 指向HPF调度队列的指针 */
    EDFRunqueue       *edfRunqueue;  /**< 指向EDF调度队列的指针 */
    UINT64            responseTime;  /**< 当前CPU滴答中断的响应时间（单位：系统时钟周期） */
    UINT32            responseID;    /**< 当前CPU滴答中断的响应ID */
    LosTaskCB         *idleTask;     /**< 空闲任务指针 */
    UINT32            taskLockCnt;   /**< 任务锁计数器，0表示未锁定，>0表示锁定调度 */
    UINT32            schedFlag;     /**< 调度挂起标志，取值为SchedFlag枚举类型 */
} SchedRunqueue;

extern SchedRunqueue g_schedRunqueue[LOSCFG_KERNEL_CORE_NUM];

VOID OsSchedExpireTimeUpdate(VOID);

/**
 * @ingroup los_sched
 * @brief 获取当前CPU的调度运行队列
 * @return 指向当前CPU调度运行队列的指针
 */
STATIC INLINE SchedRunqueue *OsSchedRunqueue(VOID)
{
    return &g_schedRunqueue[ArchCurrCpuid()];
}

/**
 * @ingroup los_sched
 * @brief 通过CPU ID获取调度运行队列
 * @param id CPU ID
 * @return 指向指定CPU调度运行队列的指针
 */
STATIC INLINE SchedRunqueue *OsSchedRunqueueByID(UINT16 id)
{
    return &g_schedRunqueue[id];
}

/**
 * @ingroup los_sched
 * @brief 获取调度锁计数器值
 * @return 当前调度锁计数
 */
STATIC INLINE UINT32 OsSchedLockCountGet(VOID)
{
    return OsSchedRunqueue()->taskLockCnt;
}

/**
 * @ingroup los_sched
 * @brief 设置调度锁计数器值
 * @param count 要设置的锁计数值
 */
STATIC INLINE VOID OsSchedLockSet(UINT32 count)
{
    OsSchedRunqueue()->taskLockCnt = count;
}

/**
 * @ingroup los_sched
 * @brief 增加调度锁计数（加锁）
 * @details 禁止任务抢占，每调用一次计数加1，需与OsSchedUnlock配对使用
 */
STATIC INLINE VOID OsSchedLock(VOID)
{
    OsSchedRunqueue()->taskLockCnt++;
}

/**
 * @ingroup los_sched
 * @brief 减少调度锁计数（解锁）
 * @details 允许任务抢占，每调用一次计数减1，需与OsSchedLock配对使用
 */
STATIC INLINE VOID OsSchedUnlock(VOID)
{
    OsSchedRunqueue()->taskLockCnt--;
}

/**
 * @ingroup los_sched
 * @brief 解锁并检查是否需要调度
 * @return TRUE表示需要调度，FALSE表示不需要
 */
STATIC INLINE BOOL OsSchedUnlockResch(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    if (rq->taskLockCnt > 0) {
        rq->taskLockCnt--;
        if ((rq->taskLockCnt == 0) && (rq->schedFlag & INT_PEND_RESCH) && OS_SCHEDULER_ACTIVE) {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @ingroup los_sched
 * @brief 检查调度是否被锁定
 * @return TRUE表示已锁定，FALSE表示未锁定
 */
STATIC INLINE BOOL OsSchedIsLock(VOID)
{
    return (OsSchedRunqueue()->taskLockCnt != 0);
}

/* Check if preemptible with counter flag */
/**
 * @ingroup los_sched
 * @brief 检查当前是否可抢占
 * @details 需禁用中断以防止任务迁移导致的状态错误
 * @return TRUE表示可抢占，FALSE表示不可抢占
 */
STATIC INLINE BOOL OsPreemptable(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    /*
     * Unlike OsPreemptableInSched, the int may be not disabled when OsPreemptable
     * is called, needs manually disable interrupt, to prevent current task from
     * being migrated to another core, and get the wrong preemptable status.
     */
    UINT32 intSave = LOS_IntLock();
    BOOL preemptible = (rq->taskLockCnt == 0);
    if (!preemptible) {
        /* Set schedule flag if preemption is disabled */
        rq->schedFlag |= INT_PEND_RESCH;
    }
    LOS_IntRestore(intSave);
    return preemptible;
}

/**
 * @ingroup los_sched
 * @brief 调度器内部检查是否可抢占
 * @details SMP系统和UP系统有不同的判断条件
 * @return TRUE表示可抢占，FALSE表示不可抢占
 */
STATIC INLINE BOOL OsPreemptableInSched(VOID)
{
    BOOL preemptible = FALSE;
    SchedRunqueue *rq = OsSchedRunqueue();

#ifdef LOSCFG_KERNEL_SMP
    /*
     * For smp systems, schedule must hold the task spinlock, and this counter
     * will increase by 1 in that case.
     */
    preemptible = (rq->taskLockCnt == 1);

#else
    preemptible = (rq->taskLockCnt == 0);
#endif
    if (!preemptible) {
        /* Set schedule flag if preemption is disabled */
        rq->schedFlag |= INT_PEND_RESCH;
    }

    return preemptible;
}

/**
 * @ingroup los_sched
 * @brief 获取当前CPU的空闲任务
 * @return 指向空闲任务控制块的指针
 */
STATIC INLINE LosTaskCB *OsSchedRunqueueIdleGet(VOID)
{
    return OsSchedRunqueue()->idleTask;
}

/**
 * @ingroup los_sched
 * @brief 设置调度挂起标志
 * @details 标记需要进行任务调度
 */
STATIC INLINE VOID OsSchedRunqueuePendingSet(VOID)
{
    OsSchedRunqueue()->schedFlag |= INT_PEND_RESCH;
}

/** @ingroup los_sched 普通调度策略 */
#define LOS_SCHED_NORMAL    0U
/** @ingroup los_sched 先进先出调度策略 */
#define LOS_SCHED_FIFO      1U
/** @ingroup los_sched 时间片轮转调度策略 */
#define LOS_SCHED_RR        2U
/** @ingroup los_sched 空闲任务调度策略 */
#define LOS_SCHED_IDLE      3U
/** @ingroup los_sched 截止时间优先调度策略 */
#define LOS_SCHED_DEADLINE  6U

/**
 * @ingroup los_sched
 * @brief 调度参数结构体
 * @details 包含任务调度相关的所有参数，适用于不同调度策略
 */
typedef struct {
    UINT16 policy;          /**< 调度策略，取值为LOS_SCHED_xxx系列宏 */
    /* HPF scheduling parameters */
    UINT16 basePrio;        /**< 基础优先级 */
    UINT16 priority;        /**< 当前优先级 */
    UINT32 timeSlice;       /**< 时间片大小（单位：系统时钟周期） */

    /* EDF scheduling parameters */
    INT32  runTimeUs;       /**< 运行时间（单位：微秒） */
    UINT32 deadlineUs;      /**< 截止时间（单位：微秒） */
    UINT32 periodUs;        /**< 周期（单位：微秒） */
} SchedParam;

/**
 * @ingroup los_sched
 * @brief HPF调度策略参数结构体
 * @details 用于高优先级优先调度策略的参数管理
 */
typedef struct {
    UINT16  policy;             /* This field must be present for all scheduling policies and must be the first in the structure */
    UINT16  basePrio;           /**< 基础优先级 */
    UINT16  priority;           /**< 当前优先级 */
    UINT32  initTimeSlice;      /* cycle */ /**< 初始时间片大小（单位：系统时钟周期） */
    UINT32  priBitmap;          /* Bitmap for recording the change of task priority, the priority can not be greater than 31 */ /**< 优先级变化记录位图，优先级最大不超过31 */
} SchedHPF;

/** @ingroup los_sched EDF未使用状态 */
#define EDF_UNUSED       0
/** @ingroup los_sched EDF下一周期状态 */
#define EDF_NEXT_PERIOD  1
/** @ingroup los_sched EDF永久等待状态 */
#define EDF_WAIT_FOREVER 2
/** @ingroup los_sched EDF初始化状态 */
#define EDF_INIT         3

/**
 * @ingroup los_sched
 * @brief EDF调度策略参数结构体
 * @details 用于最早截止时间优先调度策略的参数管理
 */
typedef struct {
    UINT16 policy;           /**< 调度策略 */
    UINT16 cpuid;            /**< CPU ID */
    UINT32 flags;            /**< 状态标志，取值为EDF_xxx系列宏 */
    INT32  runTime;          /* cycle */ /**< 运行时间（单位：系统时钟周期） */
    UINT64 deadline;         /* deadline >> runTime */ /**< 截止时间（单位：系统时钟周期） */
    UINT64 period;           /* period >= deadline */ /**< 周期（单位：系统时钟周期），必须大于等于截止时间 */
    UINT64 finishTime;       /* startTime + deadline */ /**< 完成时间（单位：系统时钟周期） */
} SchedEDF;

/**
 * @ingroup los_sched
 * @brief 调度策略联合体
 * @details 包含不同调度策略的参数结构体，使用联合体节省内存
 */
typedef struct {
    union {
        SchedEDF edf;        /**< EDF调度策略参数 */
        SchedHPF hpf;        /**< HPF调度策略参数 */
    };
} SchedPolicy;

/**
 * @ingroup los_sched
 * @brief 调度操作函数集结构体
 * @details 定义调度器的各种操作接口，实现不同调度策略的多态性
 */
typedef struct {
    VOID (*dequeue)(SchedRunqueue *rq, LosTaskCB *taskCB);                      /**< 将任务从就绪队列中移除 */
    VOID (*enqueue)(SchedRunqueue *rq, LosTaskCB *taskCB);                      /**< 将任务加入就绪队列 */
    VOID (*start)(SchedRunqueue *rq, LosTaskCB *taskCB);                        /**< 启动任务调度 */
    VOID (*exit)(LosTaskCB *taskCB);                                            /**< 任务退出调度 */
    UINT64 (*waitTimeGet)(LosTaskCB *taskCB);                                   /**< 获取任务等待时间 */
    UINT32 (*wait)(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 timeout);      /**< 任务等待事件 */
    VOID (*wake)(LosTaskCB *taskCB);                                            /**< 唤醒任务 */
    BOOL (*schedParamModify)(LosTaskCB *taskCB, const SchedParam *param);       /**< 修改调度参数 */
    UINT32 (*schedParamGet)(const LosTaskCB *taskCB, SchedParam *param);        /**< 获取调度参数 */
    UINT32 (*delay)(LosTaskCB *taskCB, UINT64 waitTime);                        /**< 延迟任务执行 */
    VOID (*yield)(LosTaskCB *taskCB);                                           /**< 任务让出CPU */
    UINT32 (*suspend)(LosTaskCB *taskCB);                                       /**< 挂起任务 */
    UINT32 (*resume)(LosTaskCB *taskCB, BOOL *needSched);                       /**< 恢复任务 */
    UINT64 (*deadlineGet)(const LosTaskCB *taskCB);                             /**< 获取任务截止时间 */
    VOID (*timeSliceUpdate)(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime); /**< 更新任务时间片 */
    INT32 (*schedParamCompare)(const SchedPolicy *sp1, const SchedPolicy *sp2); /**< 比较两个调度参数 */
    VOID (*priorityInheritance)(LosTaskCB *owner, const SchedParam *param);     /**< 优先级继承 */
    VOID (*priorityRestore)(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param); /**< 优先级恢复 */
} SchedOps;

/**
 * @ingroup los_sched
 * @brief 定义可用的任务优先级 - 最高优先级
 * @note 优先级数值越小表示优先级越高，0为系统最高任务优先级
 *       该优先级通常保留给内核关键任务使用，用户任务不应使用此优先级
 */
#define OS_TASK_PRIORITY_HIGHEST    0

/**
 * @ingroup los_sched
 * @brief 定义可用的任务优先级 - 最低优先级
 * @note 31为系统最低任务优先级，数值越大表示优先级越低
 *       用户任务创建时若未指定优先级，默认使用此优先级
 */
#define OS_TASK_PRIORITY_LOWEST     31

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 初始化状态
 * @note 十六进制值：0x0001U (十进制：1)
 *       任务刚创建但尚未启动时的初始状态
 *       此状态与其他状态互斥，任务启动后会自动清除此标志
 */
#define OS_TASK_STATUS_INIT         0x0001U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 就绪状态
 * @note 十六进制值：0x0002U (十进制：2)
 *       任务已准备好运行，等待CPU调度
 *       处于就绪队列中的任务会被调度器考虑执行
 */
#define OS_TASK_STATUS_READY        0x0002U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 运行状态
 * @note 十六进制值：0x0004U (十进制：4)
 *       任务正在CPU上执行
 *       在单CPU系统中，任何时刻只有一个任务处于此状态
 */
#define OS_TASK_STATUS_RUNNING      0x0004U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 挂起状态
 * @note 十六进制值：0x0008U (十进制：8)
 *       任务被主动挂起，暂停执行
 *       需通过LOS_TaskResume接口恢复为就绪状态
 *       挂起状态可与阻塞状态叠加
 */
#define OS_TASK_STATUS_SUSPENDED    0x0008U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 阻塞状态
 * @note 十六进制值：0x0010U (十进制：16)
 *       任务因等待资源或事件而阻塞
 *       当等待条件满足时由内核自动唤醒
 *       常见于等待信号量、互斥锁等同步机制
 */
#define OS_TASK_STATUS_PENDING      0x0010U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 延迟状态
 * @note 十六进制值：0x0020U (十进制：32)
 *       任务通过LOS_TaskDelay主动延迟执行
 *       延迟时间到达后自动转为就绪状态
 */
#define OS_TASK_STATUS_DELAY        0x0020U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 超时状态
 * @note 十六进制值：0x0040U (十进制：64)
 *       任务等待事件超时
 *       通常作为等待操作的返回状态，而非持久状态
 */
#define OS_TASK_STATUS_TIMEOUT      0x0040U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 定时等待状态
 * @note 十六进制值：0x0080U (十进制：128)
 *       任务在等待事件的同时设置了超时时间
 *       是OS_TASK_STATUS_PENDING和超时属性的组合状态
 */
#define OS_TASK_STATUS_PEND_TIME    0x0080U

/**
 * @ingroup los_sched
 * @brief 任务或任务控制块状态标志 - 退出状态
 * @note 十六进制值：0x0100U (十进制：256)
 *       任务执行完毕或被强制终止
 *       处于此状态的任务资源可被系统回收
 */
#define OS_TASK_STATUS_EXIT         0x0100U

/**
 * @ingroup los_sched
 * @brief 任务阻塞状态组合宏
 * @details 包含初始化、挂起、延迟和等待时间四种阻塞状态
 */
#define OS_TASK_STATUS_BLOCKED     (OS_TASK_STATUS_INIT | OS_TASK_STATUS_PENDING | \
                                    OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)

/**
 * @ingroup los_task
 * @brief 任务控制块状态标志：任务延迟操作被冻结
 * @details 当任务处于休眠或延迟状态时，若系统进入低功耗模式，此标志会被置位以暂停计时
 *          系统退出低功耗模式后需通过SchedTaskUnfreeze函数恢复计时
 * @value 0x0200U - 十六进制值，对应十进制512
 * @note 与OS_TASK_STATUS_DELAY等状态标志组合使用
 */
#define OS_TASK_STATUS_FROZEN       0x0200U

/**
 * @ingroup los_sched
 * @brief 任务控制块名称长度
 * @details 定义任务名称的最大字符数，包含字符串终止符'\0'
 * @value 32 - 固定长度为32字节
 * @attention 任务名称过长会被自动截断，建议保持在31字符以内
 */
#define OS_TCB_NAME_LEN             32

/**
 * @ingroup los_task
 * @brief 任务控制块(Task Control Block, TCB)
 * @details 记录任务的所有属性和运行状态，是操作系统内核管理任务的核心数据结构
 *          每个任务对应一个LosTaskCB实例，由内核统一创建和管理
 * @attention 应用层不应直接修改此结构体成员，需通过内核提供的API进行任务操作
 * @see LOS_TaskCreate 用于创建任务并初始化TCB
 */
typedef struct TagTaskCB {
    VOID            *stackPointer;      /**< 任务栈指针 - 指向当前任务栈顶位置，上下文切换时保存/恢复 */
    UINT16          taskStatus;         /**< 任务状态 - 由OS_TASK_STATUS_xxx系列标志位组合而成（如运行/就绪/阻塞） */

    UINT64          startTime;          /**< 任务各阶段开始时间 - 记录任务在不同调度阶段的启动时间，单位：系统时钟周期 */
    UINT64          waitTime;           /**< 任务延迟时间 - 任务等待的滴答数(tick)，1 tick = 1/LOSCFG_BASE_CORE_TICK_PER_SECOND秒 */
    UINT64          irqStartTime;       /**< 中断开始时间 - 记录当前任务被中断时的系统时间，用于计算中断耗时 */
    UINT32          irqUsedTime;        /**< 中断消耗时间 - 中断处理占用的CPU时间，单位：系统时钟周期 */
    INT32           timeSlice;          /**< 任务剩余时间片 - 为负数时表示使用默认时间片（由LOSCFG_BASE_CORE_TIMESLICE定义） */
    SortLinkList    sortList;           /**< 任务排序链表节点 - 用于任务在超时队列中的排序和管理 */
    const SchedOps  *ops;               /**< 调度操作函数集 - 指向特定调度策略（如HPF/EDF）的操作接口表 */
    SchedPolicy     sp;                 /**< 调度策略参数 - 存储优先级、截止时间等调度相关参数，具体结构与调度策略相关 */

    UINT32          stackSize;          /**< 任务栈大小 - 单位：字节，创建任务时指定的栈空间总大小 */
    UINTPTR         topOfStack;         /**< 任务栈顶地址 - 栈空间的最高地址，栈向下生长时的起始位置 */
    UINT32          taskID;             /**< 任务ID - 系统内唯一标识，由内核自动分配，范围：0~LOSCFG_BASE_CORE_TSK_LIMIT-1 */
    TSK_ENTRY_FUNC  taskEntry;          /**< 任务入口函数 - 任务启动时执行的函数指针，原型为：VOID (*TSK_ENTRY_FUNC)(VOID *) */
    VOID            *joinRetval;        /**< 线程连接返回值 - pthread适配用，存储线程退出状态，通过pthread_join获取 */
    VOID            *taskMux;           /**< 任务持有的互斥锁 - 指向当前任务锁定的互斥锁结构，用于死锁检测 */
    VOID            *taskEvent;         /**< 任务持有的事件 - 指向当前任务等待的事件控制块，事件触发时唤醒任务 */
    UINTPTR         args[4];            /**< 任务参数数组 - 最多支持4个参数传递给任务入口函数，通过LOS_TaskCreate指定 */
    CHAR            taskName[OS_TCB_NAME_LEN]; /**< 任务名称 - 以null结尾的字符串，最长31个可见字符，用于调试和任务识别 */
    LOS_DL_LIST     pendList;           /**< 任务等待链表节点 - 任务挂起时加入等待队列（如信号量/互斥锁队列）的节点 */
    LOS_DL_LIST     threadList;         /**< 线程链表 - 进程内线程链表节点，用于进程管理中的线程遍历 */
    UINT32          eventMask;          /**< 事件掩码 - 记录任务感兴趣的事件组合，与eventMode配合使用 */
    UINT32          eventMode;          /**< 事件模式 - 事件触发模式（LOS_WAITMODE_AND/LOS_WAITMODE_OR/LOS_WAITMODE_CLR） */
#ifdef LOSCFG_KERNEL_CPUP
    OsCpupBase      taskCpup;           /**< 任务CPU使用率统计 - 包含CPU占用时间、调度次数等性能监控数据 */
#endif
    INT32           errorNo;            /**< 错误号 - 记录任务执行过程中的错误状态码，类似POSIX的errno */
    UINT32          signal;             /**< 任务信号 - 待处理的信号集合，每一位代表一种信号（如SIGINT/SIGTERM） */
    sig_cb          sig;                /**< 信号回调函数结构 - 存储信号处理函数、信号掩码等信号相关信息 */
#ifdef LOSCFG_KERNEL_SMP
    UINT16          currCpu;            /**< 当前CPU核心号 - 任务正在运行的CPU核心ID（0~LOSCFG_KERNEL_CORE_NUM-1） */
    UINT16          lastCpu;            /**< 上次运行CPU核心号 - 任务上一次调度时的CPU核心ID，用于负载均衡 */
    UINT16          cpuAffiMask;        /**< CPU亲和性掩码 - 每一位代表一个可调度的CPU核心，支持最多16核（bit0对应CPU0） */
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    UINT32          syncSignal;         /**< 信号同步标志 - 用于多核心间信号处理同步，避免信号丢失或重复处理 */
#endif
#ifdef LOSCFG_KERNEL_SMP_LOCKDEP
    LockDep         lockDep;            /**< 锁依赖调试信息 - 用于检测死锁、锁顺序颠倒等锁相关问题（仅调试版本有效） */
#endif
#endif
#ifdef LOSCFG_SCHED_DEBUG
    SchedStat       schedStat;          /**< 调度统计信息 - 包含任务切换次数、运行时间等调度相关统计数据 */
#endif
#ifdef LOSCFG_KERNEL_VM
    UINTPTR         archMmu;            /**< 架构相关MMU信息 - 存储MMU页表等内存管理单元相关数据 */
    UINTPTR         userArea;           /**< 用户空间区域 - 指向用户进程地址空间描述结构 */
    UINTPTR         userMapBase;        /**< 用户映射基地址 - 用户线程栈的映射起始地址 */
    UINT32          userMapSize;        /**< 用户映射大小 - 用户线程栈大小，实际大小为userMapSize + USER_STACK_MIN_SIZE */
    FutexNode       futex;              /**< 快速用户空间互斥体节点 - 用于用户态线程同步机制 */
#endif
    UINTPTR         processCB;          /**< 所属进程控制块 - 指向该任务所属进程的PCB（进程控制块），NULL表示内核任务 */
    LOS_DL_LIST     joinList;           /**< 连接等待链表 - 等待该任务退出的线程链表，用于pthread_join实现 */
    LOS_DL_LIST     lockList;           /**< 持有的锁列表 - 任务当前持有的所有锁的链表，用于死锁检测和资源管理 */
    UINTPTR         waitID;             /**< 等待的ID - 等待的子进程PID或进程组GID，用于wait/waitpid系统调用 */
    UINT16          waitFlag;           /**< 等待标志 - 等待的子进程类型（如WEXITED/WUNTRACED/WNOHANG等标志组合） */
#ifdef LOSCFG_KERNEL_LITEIPC
    IpcTaskInfo     *ipcTaskInfo;       /**< IPC任务信息 - 轻量级进程间通信相关的任务信息 */
#endif
#ifdef LOSCFG_KERNEL_PERF
    UINTPTR         pc;                 /**< 程序计数器 - 记录任务切换时的PC值，用于性能分析 */
    UINTPTR         fp;                 /**< 帧指针 - 记录任务切换时的FP值，用于栈回溯和性能分析 */
#endif
#ifdef LOSCFG_PID_CONTAINER
    PidContainer    *pidContainer;      /**< PID容器 - 指向该任务所属的PID命名空间容器 */
#endif
#ifdef LOSCFG_IPC_CONTAINER
    BOOL            cloneIpc;           /**< IPC克隆标志 - 标识任务是否继承父进程的IPC命名空间 */
#endif
} LosTaskCB;

/**
 * @ingroup los_task
 * @brief 判断任务是否处于运行状态
 * @param taskCB 任务控制块指针，指向要判断的任务
 * @retval TRUE 任务正在运行（OS_TASK_STATUS_RUNNING标志置位）
 * @retval FALSE 任务未运行
 * @note 运行状态表示任务当前正在CPU上执行
 */
STATIC INLINE BOOL OsTaskIsRunning(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_RUNNING) != 0);
}

/**
 * @ingroup los_task
 * @brief 判断任务是否处于就绪状态
 * @param taskCB 任务控制块指针
 * @retval TRUE 任务已就绪（OS_TASK_STATUS_READY标志置位）
 * @retval FALSE 任务未就绪
 * @note 就绪状态表示任务等待调度器分配CPU时间
 */
STATIC INLINE BOOL OsTaskIsReady(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_READY) != 0);
}

/**
 * @ingroup los_task
 * @brief 判断任务是否处于非活动状态
 * @param taskCB 任务控制块指针
 * @retval TRUE 任务处于初始化或退出状态
 * @retval FALSE 任务处于活动状态
 * @details 非活动状态包括：
 *          - OS_TASK_STATUS_INIT：任务刚创建尚未启动
 *          - OS_TASK_STATUS_EXIT：任务已退出但资源未释放
 */
STATIC INLINE BOOL OsTaskIsInactive(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & (OS_TASK_STATUS_INIT | OS_TASK_STATUS_EXIT)) != 0);
}

/**
 * @ingroup los_task
 * @brief 判断任务是否处于等待状态
 * @param taskCB 任务控制块指针
 * @retval TRUE 任务处于等待状态（OS_TASK_STATUS_PENDING标志置位）
 * @retval FALSE 任务未处于等待状态
 * @note 等待状态表示任务正在等待某个资源（如信号量、互斥锁）
 */
STATIC INLINE BOOL OsTaskIsPending(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_PENDING) != 0);
}

/**
 * @ingroup los_task
 * @brief 判断任务是否处于挂起状态
 * @param taskCB 任务控制块指针
 * @retval TRUE 任务处于挂起状态（OS_TASK_STATUS_SUSPENDED标志置位）
 * @retval FALSE 任务未挂起
 * @see LOS_TaskSuspend/LOS_TaskResume 用于挂起/恢复任务
 */
STATIC INLINE BOOL OsTaskIsSuspended(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_SUSPENDED) != 0);
}

/**
 * @ingroup los_task
 * @brief 判断任务是否处于阻塞状态
 * @param taskCB 任务控制块指针
 * @retval TRUE 任务处于挂起、等待或延迟状态
 * @retval FALSE 任务处于活动状态（运行或就绪）
 * @details 阻塞状态是以下状态的组合：
 *          - OS_TASK_STATUS_SUSPENDED：任务被挂起
 *          - OS_TASK_STATUS_PENDING：任务等待资源
 *          - OS_TASK_STATUS_DELAY：任务延迟执行
 */
STATIC INLINE BOOL OsTaskIsBlocked(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & (OS_TASK_STATUS_SUSPENDED | OS_TASK_STATUS_PENDING | OS_TASK_STATUS_DELAY)) != 0);
}

/**
 * @ingroup los_sched
 * @brief 判断任务是否采用EDF调度策略
 * @param taskCB 任务控制块指针
 * @retval TRUE 任务使用EDF（最早截止时间优先）调度策略
 * @retval FALSE 任务使用其他调度策略（如HPF）
 * @details 通过将SchedPolicy类型强制转换为SchedEDF类型来访问调度策略字段
 * @note EDF调度策略由LOS_SCHED_DEADLINE标识
 */
STATIC INLINE BOOL OsSchedPolicyIsEDF(const LosTaskCB *taskCB)
{
    const SchedEDF *sched = (const SchedEDF *)&taskCB->sp;
    return (sched->policy == LOS_SCHED_DEADLINE);
}

/**
 * @ingroup los_sched
 * @brief 获取当前运行的任务控制块
 * @return LosTaskCB* 当前运行任务的控制块指针
 * @details 调用架构相关的ArchCurrTaskGet函数实现
 * @note 仅内核态可调用此函数
 */
STATIC INLINE LosTaskCB *OsCurrTaskGet(VOID)
{
    return (LosTaskCB *)ArchCurrTaskGet();
}

/**
 * @ingroup los_sched
 * @brief 设置当前运行的任务控制块
 * @param task 要设置为当前运行任务的控制块指针
 * @details 调用架构相关的ArchCurrTaskSet函数实现
 * @note 仅调度器内部使用，用于上下文切换
 */
STATIC INLINE VOID OsCurrTaskSet(LosTaskCB *task)
{
    ArchCurrTaskSet(task);
}

/**
 * @ingroup los_sched
 * @brief 设置当前用户任务
 * @param thread 用户任务线程指针
 * @details 调用架构相关的ArchCurrUserTaskSet函数实现
 * @note 用于用户态线程切换
 */
STATIC INLINE VOID OsCurrUserTaskSet(UINTPTR thread)
{
    ArchCurrUserTaskSet(thread);
}

/**
 * @ingroup los_sched
 * @brief 更新中断使用时间
 * @details 计算从irqStartTime到当前的时间差，更新任务的irqUsedTime
 * @note 中断处理结束时调用，用于性能统计
 */
STATIC INLINE VOID OsSchedIrqUsedTimeUpdate(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqUsedTime = OsGetCurrSchedTimeCycle() - runTask->irqStartTime;
}

/**
 * @ingroup los_sched
 * @brief 记录中断开始时间
 * @details 将当前系统时间记录到当前任务的irqStartTime字段
 * @note 中断处理开始时调用，与OsSchedIrqUsedTimeUpdate配合使用
 */
STATIC INLINE VOID OsSchedIrqStartTime(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqStartTime = OsGetCurrSchedTimeCycle();
}

#ifdef LOSCFG_KERNEL_SMP
/**
 * @ingroup los_sched
 * @brief 查找最空闲的CPU运行队列
 * @param idleCpuid 输出参数，返回最空闲CPU的ID
 * @details 遍历所有CPU核心，比较超时队列中的节点数量，选择节点最少的CPU
 * @note 仅SMP（对称多处理）配置下有效
 * @attention CPU从1开始遍历，0作为初始比较基准
 */
STATIC INLINE VOID IdleRunqueueFind(UINT16 *idleCpuid)
{
    SchedRunqueue *idleRq = OsSchedRunqueueByID(0);
    UINT32 nodeNum = OsGetSortLinkNodeNum(&idleRq->timeoutQueue);
    UINT16 cpuid = 1;
    do {
        SchedRunqueue *rq = OsSchedRunqueueByID(cpuid);
        UINT32 temp = OsGetSortLinkNodeNum(&rq->timeoutQueue);
        if (nodeNum > temp) {
            *idleCpuid = cpuid;
            nodeNum = temp;
        }
        cpuid++;
    } while (cpuid < LOSCFG_KERNEL_CORE_NUM);
}
#endif

/**
 * @ingroup los_sched
 * @brief 将任务添加到超时队列
 * @param taskCB 要添加的任务控制块指针
 * @param responseTime 任务超时响应时间（绝对时间），单位：系统时钟周期
 * @details 根据任务CPU亲和性选择目标CPU，将任务添加到对应CPU的超时队列
 * @note SMP模式下：
 *       - 若亲和性掩码无效，自动选择最空闲CPU
 *       - 若目标CPU非当前CPU且响应时间更早，触发跨核心调度
 */
STATIC INLINE VOID OsSchedTimeoutQueueAdd(LosTaskCB *taskCB, UINT64 responseTime)
{
#ifdef LOSCFG_KERNEL_SMP
    UINT16 cpuid = AFFI_MASK_TO_CPUID(taskCB->cpuAffiMask);
    if (cpuid >= LOSCFG_KERNEL_CORE_NUM) {
        cpuid = 0;
        IdleRunqueueFind(&cpuid);  /**< 查找负载最轻的CPU核心 */
    }
#else
    UINT16 cpuid = 0;  /**< UP系统固定使用CPU 0 */
#endif

    SchedRunqueue *rq = OsSchedRunqueueByID(cpuid);
    OsAdd2SortLink(&rq->timeoutQueue, &taskCB->sortList, responseTime, cpuid);
#ifdef LOSCFG_KERNEL_SMP
    if ((cpuid != ArchCurrCpuid()) && (responseTime < rq->responseTime)) {
        rq->schedFlag |= INT_PEND_TICK;  /**< 设置调度标志，触发目标CPU调度 */
        LOS_MpSchedule(CPUID_TO_AFFI_MASK(cpuid));  /**< 发送跨核心调度IPI */
    }
#endif
}

/**
 * @ingroup los_sched
 * @brief 从超时队列中删除任务
 * @param taskCB 要删除的任务控制块指针
 * @details 从任务当前所在的CPU超时队列中移除任务节点
 * @note 若删除的是最早超时任务，需重置队列的responseTime为最大值
 */
STATIC INLINE VOID OsSchedTimeoutQueueDelete(LosTaskCB *taskCB)
{
    SortLinkList *node = &taskCB->sortList;
#ifdef LOSCFG_KERNEL_SMP
    SchedRunqueue *rq = OsSchedRunqueueByID(node->cpuid);
#else
    SchedRunqueue *rq = OsSchedRunqueueByID(0);
#endif
    UINT64 oldResponseTime = GET_SORTLIST_VALUE(node);
    OsDeleteFromSortLink(&rq->timeoutQueue, node);
    if (oldResponseTime <= rq->responseTime) {
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;  /**< 重置响应时间为最大值 */
    }
}

/**
 * @brief 调整任务在超时队列中的响应时间
 * @param taskCB 任务控制块指针，指向需要调整超时时间的任务
 * @param responseTime 新的响应时间（单位：系统时钟周期）
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 * @note 1. 该函数会更新任务在超时队列中的位置，并在成功时设置调度标志位
 *       2. SMP系统中使用任务当前关联的CPU ID，UP系统固定使用CPU 0
 */
STATIC INLINE UINT32 OsSchedTimeoutQueueAdjust(LosTaskCB *taskCB, UINT64 responseTime)
{
    UINT32 ret;
    SortLinkList *node = &taskCB->sortList;  /* 获取任务的排序链表节点 */
#ifdef LOSCFG_KERNEL_SMP
    UINT16 cpuid = node->cpuid;              /* SMP：使用节点中记录的CPU ID */
#else
    UINT16 cpuid = 0;                        /* UP：固定使用CPU 0 */
#endif
    SchedRunqueue *rq = OsSchedRunqueueByID(cpuid); /* 获取对应CPU的调度运行队列 */
    ret = OsSortLinkAdjustNodeResponseTime(&rq->timeoutQueue, node, responseTime);
    if (ret == LOS_OK) {
        rq->schedFlag |= INT_PEND_TICK;      /* 设置调度标志位，触发滴答中断处理 */
    }
    return ret;
}

/**
 * @brief 冻结任务（暂停超时等待）
 * @param taskCB 任务控制块指针，指向需要冻结的任务
 * @note 1. 仅当系统启用电源管理(LOSCFG_KERNEL_PM)且处于PM模式时有效
 *       2. 仅对处于时间等待或延迟状态的任务进行冻结
 *       3. 冻结时会从超时队列中移除任务，但保留原始响应时间
 */
STATIC INLINE VOID SchedTaskFreeze(LosTaskCB *taskCB)
{
    UINT64 responseTime;

#ifdef LOSCFG_KERNEL_PM
    if (!OsIsPmMode()) {                     /* 检查是否处于电源管理模式 */
        return;
    }
#endif

    /* 仅处理处于时间等待或延迟状态的任务 */
    if (!(taskCB->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY))) {
        return;
    }

    responseTime = GET_SORTLIST_VALUE(&taskCB->sortList); /* 保存原始响应时间 */
    OsSchedTimeoutQueueDelete(taskCB);                   /* 从超时队列中删除任务 */
    SET_SORTLIST_VALUE(&taskCB->sortList, responseTime); /* 恢复原始响应时间值 */
    taskCB->taskStatus |= OS_TASK_STATUS_FROZEN;         /* 设置任务冻结状态标志 */
    return;
}

/**
 * @brief 解冻任务（恢复超时等待）
 * @param taskCB 任务控制块指针，指向需要解冻的任务
 * @note 1. 解冻后根据响应时间判断是否需要重新加入超时队列
 *       2. 若响应时间已过期，则清除阻塞状态并设置无效时间值
 */
STATIC INLINE VOID SchedTaskUnfreeze(LosTaskCB *taskCB)
{
    UINT64 currTime, responseTime;

    if (!(taskCB->taskStatus & OS_TASK_STATUS_FROZEN)) {  /* 检查任务是否处于冻结状态 */
        return;
    }

    taskCB->taskStatus &= ~OS_TASK_STATUS_FROZEN;        /* 清除冻结状态标志 */
    currTime = OsGetCurrSchedTimeCycle();                /* 获取当前调度时间周期 */
    responseTime = GET_SORTLIST_VALUE(&taskCB->sortList); /* 获取保存的响应时间 */
    if (responseTime > currTime) {                       /* 响应时间未过期 */
        OsSchedTimeoutQueueAdd(taskCB, responseTime);    /* 重新加入超时队列 */
        return;
    }

    SET_SORTLIST_VALUE(&taskCB->sortList, OS_SORT_LINK_INVALID_TIME); /* 设置无效时间值 */
    if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {   /* 任务处于等待状态 */
        LOS_ListDelete(&taskCB->pendList);               /* 从等待链表中删除 */
    }
    taskCB->taskStatus &= ~OS_TASK_STATUS_BLOCKED;       /* 清除阻塞状态标志 */
    return;
}

/**
 * @brief 设置调度器标志位（每bit代表一个CPU核心）
 * @param cpuid CPU核心ID
 * @note 该标志用于在OSStartToRun之前防止内核调度，确保系统初始化完成
 */
#define OS_SCHEDULER_SET(cpuid) do {     \
    g_taskScheduled |= (1U << (cpuid));  \
} while (0);

/**
 * @brief 清除调度器标志位
 * @param cpuid CPU核心ID
 * @note 与OS_SCHEDULER_SET配合使用，管理各CPU核心的调度使能状态
 */
#define OS_SCHEDULER_CLR(cpuid) do {     \
    g_taskScheduled &= ~(1U << (cpuid)); \
} while (0);

#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
/**
 * @brief 调度优先级限制时间检查（声明）
 * @param task 任务控制块指针
 * @return BOOL 检查结果，TRUE表示通过检查，FALSE表示未通过
 * @note 仅当启用调度优先级限制功能(LOSCFG_KERNEL_SCHED_PLIMIT)时有效
 */
BOOL OsSchedLimitCheckTime(LosTaskCB *task);
#endif

/**
 * @brief 获取EDF（最早截止时间优先）运行队列中的最高优先级任务
 * @param rq EDF运行队列指针
 * @return LosTaskCB* 最高优先级任务控制块指针，队列为空时返回NULL
 * @note EDF调度算法根据任务的截止时间选择最早到期的任务优先执行
 */
STATIC INLINE LosTaskCB *EDFRunqueueTopTaskGet(EDFRunqueue *rq)
{
    LOS_DL_LIST *root = &rq->root;         /* 获取EDF队列的根节点 */
    if (LOS_ListEmpty(root)) {             /* 检查队列是否为空 */
        return NULL;
    }

    /* 返回队列中的第一个任务（EDF队列按截止时间排序） */
    return LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(root), LosTaskCB, pendList);
}

/**
 * @brief 获取HPF（最高优先级优先）运行队列中的最高优先级任务
 * @param rq HPF运行队列指针
 * @return LosTaskCB* 最高优先级任务控制块指针，队列为空时返回NULL
 * @note 1. 优先级数值越小表示优先级越高（0为最高优先级）
 *       2. SMP系统中会考虑任务的CPU亲和性掩码
 *       3. 支持两级优先级查找（基础优先级+子优先级）
 */
STATIC INLINE LosTaskCB *HPFRunqueueTopTaskGet(HPFRunqueue *rq)
{
    LosTaskCB *newTask = NULL;
    UINT32 baseBitmap = rq->queueBitmap;   /* 基础优先级位图，每bit代表一级基础优先级 */
#ifdef LOSCFG_KERNEL_SMP
    UINT32 cpuid = ArchCurrCpuid();        /* SMP：获取当前CPU ID，用于CPU亲和性检查 */
#endif

    /* 遍历基础优先级位图，从高到低查找有任务的优先级组 */
    while (baseBitmap) {
        UINT32 basePrio = CLZ(baseBitmap); /* 计算最高置位bit的位置（最高基础优先级） */
        HPFQueue *queueList = &rq->queueList[basePrio]; /* 获取该基础优先级对应的队列 */
        UINT32 bitmap = queueList->queueBitmap;         /* 子优先级位图 */
        while (bitmap) {
            UINT32 priority = CLZ(bitmap); /* 计算最高置位bit的位置（最高子优先级） */
            /* 遍历该优先级下的所有任务 */
            LOS_DL_LIST_FOR_EACH_ENTRY(newTask, &queueList->priQueList[priority], LosTaskCB, pendList) {
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
                /* 检查任务是否超过优先级限制时间 */
                if (!OsSchedLimitCheckTime(newTask)) {
                    bitmap &= ~(1U << (OS_PRIORITY_QUEUE_NUM - priority - 1));
                    continue;
                }
#endif
#ifdef LOSCFG_KERNEL_SMP
                /* SMP：检查任务是否允许在当前CPU上运行 */
                if (newTask->cpuAffiMask & (1U << cpuid)) {
#endif
                    return newTask;       /* 返回找到的最高优先级任务 */
#ifdef LOSCFG_KERNEL_SMP
                }
#endif
            }
            /* 清除当前子优先级bit，继续查找下一优先级 */
            bitmap &= ~(1U << (OS_PRIORITY_QUEUE_NUM - priority - 1));
        }
        /* 清除当前基础优先级bit，继续查找下一优先级组 */
        baseBitmap &= ~(1U << (OS_PRIORITY_QUEUE_NUM - basePrio - 1));
    }

    return NULL;                           /* 未找到可运行任务 */
}

VOID EDFProcessDefaultSchedParamGet(SchedParam *param);
VOID EDFSchedPolicyInit(SchedRunqueue *rq);
UINT32 EDFTaskSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                             const SchedParam *parentParam,
                             const LosSchedParam *param);

VOID HPFSchedPolicyInit(SchedRunqueue *rq);
VOID HPFTaskSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                           const SchedParam *parentParam,
                           const LosSchedParam *param);
VOID HPFProcessDefaultSchedParamGet(SchedParam *param);

VOID IdleTaskSchedParamInit(LosTaskCB *taskCB);

INT32 OsSchedParamCompare(const LosTaskCB *task1, const LosTaskCB *task2);
VOID OsSchedPriorityInheritance(LosTaskCB *owner, const SchedParam *param);
UINT32 OsSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                        const SchedParam *parentParam,
                        const LosSchedParam *param);
VOID OsSchedProcessDefaultSchedParamGet(UINT16 policy, SchedParam *param);

VOID OsSchedResponseTimeReset(UINT64 responseTime);
VOID OsSchedToUserReleaseLock(VOID);
VOID OsSchedTick(VOID);
UINT32 OsSchedInit(VOID);
VOID OsSchedStart(VOID);

VOID OsSchedRunqueueIdleInit(LosTaskCB *idleTask);
VOID OsSchedRunqueueInit(VOID);

/*
 * This function simply picks the next task and switches to it.
 * Current task needs to already be in the right state or the right
 * queues it needs to be in.
 */
VOID OsSchedResched(VOID);
VOID OsSchedIrqEndCheckNeedSched(VOID);

/*
* This function inserts the runTask to the lock pending list based on the
* task priority.
*/
LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SCHED_PRI_H */
