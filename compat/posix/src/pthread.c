/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "pprivate.h"
#include "pthread.h"
#include "sched.h"

#include "stdio.h"
#include "map_error.h"
#include "los_process_pri.h"
#include "los_sched_pri.h"

/*
 * 线程控制结构数组。pthread_t对象本质上是该数组的索引
 */
STATIC _pthread_data g_pthreadData[LOSCFG_BASE_CORE_TSK_LIMIT + 1];

/* 已退出但未被回收的线程计数 */
STATIC INT32 g_pthreadsExited = 0;

/* 用于保护pthread数据的互斥锁 */
STATIC pthread_mutex_t g_pthreadsDataMutex = PTHREAD_MUTEX_INITIALIZER;

/* PTHREAD_CANCELED宏指向的虚拟变量 */
UINTPTR g_pthreadCanceledDummyVar;

/**
 * @brief 获取当前线程的内部控制结构指针
 * @return 指向当前线程_pthread_data结构的指针
 * @note 私有接口，通过当前任务ID索引全局线程数组
 */
_pthread_data *pthread_get_self_data(void)
{
    UINT32 runningTaskPID = ((LosTaskCB *)(OsCurrTaskGet()))->taskID;  // 获取当前任务ID
    _pthread_data *data = &g_pthreadData[runningTaskPID];  // 索引到对应的线程控制结构

    return data;  // 返回线程控制结构指针
}

/**
 * @brief 根据线程ID获取线程控制结构
 * @param[in] id 线程ID
 * @return 成功返回线程控制结构指针，失败返回NULL
 * @note 会验证线程状态和ID有效性
 */
_pthread_data *pthread_get_data(pthread_t id)
{
    _pthread_data *data = NULL;

    if (OS_TID_CHECK_INVALID(id)) {  // 检查线程ID是否有效
        return NULL;  // 无效ID返回NULL
    }

    data = &g_pthreadData[id];  // 获取线程控制结构
    /* 检查是否为有效条目 */
    if ((data->state == PTHREAD_STATE_FREE) || (data->state == PTHREAD_STATE_EXITED)) {
        return NULL;  // 空闲或已退出状态返回NULL
    }

    /* 检查条目是否与ID匹配 */
    if (data->id != id) {
        return NULL;  // ID不匹配返回NULL
    }

    /* 返回指针 */
    return data;
}

/**
 * @brief 检查是否有挂起的取消请求且取消是否启用
 * @return 有取消请求且启用返回1，否则返回0
 * @note 内联函数优化常见情况（无取消请求时减少检查）
 */
STATIC INT32 CheckForCancel(VOID)
{
    _pthread_data *self = pthread_get_self_data();  // 获取当前线程数据
    if (self->canceled && (self->cancelstate == PTHREAD_CANCEL_ENABLE)) {  // 检查取消标志和状态
        return 1;  // 需要取消
    }
    return 0;  // 无需取消
}

/**
 * @brief 处理未使用状态的线程
 * @param[in,out] data 线程控制结构指针
 * @note 将线程状态设为空闲并清空数据
 */
STATIC VOID ProcessUnusedStatusTask(_pthread_data *data)
{
    data->state = PTHREAD_STATE_FREE;  // 设置为空闲状态
    (VOID)memset_s(data, sizeof(_pthread_data), 0, sizeof(_pthread_data));  // 清空线程数据
}

/**
 * @brief 清理并释放已退出的线程资源
 * @note 必须在持有pthread_mutex锁的情况下调用
 * @warning 必须从非退出线程中调用
 */
STATIC VOID PthreadReap(VOID)
{
    UINT32 i;
    _pthread_data *data = NULL;
    /*
     * 遍历线程表查找已退出线程
     * g_pthreadsExited计数器用于在找到所有退出线程后退出循环（无退出线程时也会退出）
     */
    for (i = 0; g_pthreadsExited && (i < g_taskMaxNum); i++) {
        data = &g_pthreadData[i];
        if (data->state == PTHREAD_STATE_EXITED) {  // 找到已退出线程
            /* Huawei LiteOS不会自动删除死亡TCB，需要手动删除 */
            (VOID)LOS_TaskDelete(data->task->taskID);  // 删除任务
            if (data->task->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
                ProcessUnusedStatusTask(data);  // 处理未使用任务
                g_pthreadsExited--;  // 减少退出线程计数
            }
        }
    }
}

/**
 * @brief 设置线程属性
 * @param[in] self 当前线程控制结构
 * @param[in] attr 用户提供的线程属性（可为NULL）
 * @param[out] outAttr 输出的线程属性
 * @note 若attr为NULL则使用默认属性，否则复制用户属性
 */
STATIC VOID SetPthreadAttr(const _pthread_data *self, const pthread_attr_t *attr, pthread_attr_t *outAttr)
{
    /*
     * 设置要使用的属性集
     * 要么使用传入的属性，要么使用默认属性集
     */
    if (attr == NULL) {
        (VOID)pthread_attr_init(outAttr);  // 初始化默认属性
    } else {
        (VOID)memcpy_s(outAttr, sizeof(pthread_attr_t), attr, sizeof(pthread_attr_t));  // 复制用户属性
    }

    /*
     * 如果栈大小无效，我们假设它至少为PTHREAD_STACK_MIN字节
     */
    if (!outAttr->stacksize_set) {
        outAttr->stacksize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;  // 设置默认栈大小
    }
    if (outAttr->inheritsched == PTHREAD_INHERIT_SCHED) {  // 检查是否继承调度属性
        if (self->task == NULL) {
            outAttr->schedparam.sched_priority = LOS_TaskPriGet(OsCurrTaskGet()->taskID);  // 获取当前任务优先级
        } else {
            outAttr->schedpolicy = self->attr.schedpolicy;  // 继承调度策略
            outAttr->schedparam  = self->attr.schedparam;   // 继承调度参数
            outAttr->scope       = self->attr.scope;        // 继承作用域
        }
    }
}

/**
 * @brief 设置线程数据属性
 * @param[in] userAttr 用户线程属性
 * @param[in] threadID 线程ID
 * @param[in] taskCB 任务控制块指针
 * @param[out] created 要初始化的线程控制结构
 * @note 初始化线程ID、任务指针、状态和取消相关属性
 */
STATIC VOID SetPthreadDataAttr(const pthread_attr_t *userAttr, const pthread_t threadID,
                               LosTaskCB *taskCB, _pthread_data *created)
{
    created->attr         = *userAttr;  // 设置线程属性
    created->id           = threadID;   // 设置线程ID
    created->task         = taskCB;     // 设置任务控制块指针
    // 根据分离状态设置初始状态
    created->state        = (userAttr->detachstate == PTHREAD_CREATE_JOINABLE) ?
                            PTHREAD_STATE_RUNNING : PTHREAD_STATE_DETACHED;
    /* 需要确认 */
    created->cancelstate  = PTHREAD_CANCEL_ENABLE;    // 默认启用取消
    created->canceltype   = PTHREAD_CANCEL_DEFERRED;  // 默认延迟取消
    created->cancelbuffer = NULL;                     // 取消缓冲区为空
    created->canceled     = 0;                        // 取消标志未设置
    created->freestack    = 0; /* 未使用，默认值：0 */
    created->stackmem     = taskCB->topOfStack;       // 设置栈内存地址
    created->thread_data  = NULL;                     // 线程私有数据为空
}

/**
 * @brief 初始化线程数据
 * @param[in] threadID 线程ID
 * @param[in,out] userAttr 用户线程属性
 * @param[in] name 线程名称
 * @param[in] len 名称长度
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @note 设置线程名称、栈大小和CPU亲和性（SMP配置下）
 */
STATIC UINT32 InitPthreadData(pthread_t threadID, pthread_attr_t *userAttr,
                              const CHAR name[], size_t len)
{
    errno_t err;
    UINT32 ret = LOS_OK;
    LosTaskCB *taskCB = OS_TCB_FROM_TID(threadID);  // 从线程ID获取任务控制块
    _pthread_data *created = &g_pthreadData[threadID];  // 获取线程控制结构

    err = strncpy_s(created->name, sizeof(created->name), name, len);  // 复制线程名称
    if (err != EOK) {
        PRINT_ERR("%s: %d, err: %d\n", __FUNCTION__, __LINE__, err);
        return LOS_NOK;
    }
    userAttr->stacksize   = taskCB->stackSize;  // 设置栈大小
    err = OsSetTaskName(taskCB, created->name, FALSE);  // 设置任务名称
    if (err != LOS_OK) {
        PRINT_ERR("%s: %d, err: %d\n", __FUNCTION__, __LINE__, err);
        return LOS_NOK;
    }
#ifdef LOSCFG_KERNEL_SMP  // SMP配置下设置CPU亲和性
    if (userAttr->cpuset.__bits[0] > 0) {
        taskCB->cpuAffiMask = (UINT16)userAttr->cpuset.__bits[0];  // 设置CPU亲和性掩码
    }
#endif

    SetPthreadDataAttr(userAttr, threadID, taskCB, created);  // 设置线程数据属性
    return ret;
}

/**
 * @brief 创建一个新线程
 * @param[out] thread 输出参数，返回新创建线程的ID
 * @param[in] attr 线程属性（可为NULL，使用默认属性）
 * @param[in] startRoutine 线程入口函数
 * @param[in] arg 传递给线程入口函数的参数
 * @return 成功返回ENOERR，失败返回错误码
 * @note 内部使用LOS_TaskCreateOnly创建任务，支持分离状态和继承调度属性
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*startRoutine)(void *), void *arg)
{
    pthread_attr_t userAttr;  // 用户线程属性
    UINT32 ret;               // 返回值
    CHAR name[PTHREAD_DATA_NAME_MAX] = {0};  // 线程名称
    STATIC UINT16 pthreadNumber = 1;         // 线程编号计数器
    TSK_INIT_PARAM_S taskInitParam = {0};    // 任务初始化参数
    UINT32 taskHandle;                       // 任务句柄
    _pthread_data *self = pthread_get_self_data();  // 获取当前线程数据

    if ((thread == NULL) || (startRoutine == NULL)) {  // 检查必要参数
        return EINVAL;  // 参数无效
    }

    SetPthreadAttr(self, attr, &userAttr);  // 设置线程属性

    (VOID)snprintf_s(name, sizeof(name), sizeof(name) - 1, "pth%02d", pthreadNumber);  // 生成线程名称
    pthreadNumber++;  // 增加线程编号

    // 初始化任务参数
    taskInitParam.pcName       = name;                           // 任务名称
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)startRoutine;   // 任务入口函数
    taskInitParam.auwArgs[0]   = (UINTPTR)arg;                   // 传递参数
    taskInitParam.usTaskPrio   = (UINT16)userAttr.schedparam.sched_priority;  // 任务优先级
    taskInitParam.uwStackSize  = userAttr.stacksize;             // 栈大小
    if (OsProcessIsUserMode(OsCurrProcessGet())) {  // 检查是否为用户模式进程
        taskInitParam.processID = (UINTPTR)OsGetKernelInitProcess();  // 设置内核进程ID
    } else {
        taskInitParam.processID = (UINTPTR)OsCurrProcessGet();  // 设置当前进程ID
    }
    if (userAttr.detachstate == PTHREAD_CREATE_DETACHED) {  // 分离状态
        taskInitParam.uwResved = LOS_TASK_STATUS_DETACHED;  // 设置分离状态标志
    } else {
        /* 设置pthread默认可连接 */
        taskInitParam.uwResved = LOS_TASK_ATTR_JOINABLE;  // 设置可连接标志
    }

    PthreadReap();  // 清理已退出线程
    ret = LOS_TaskCreateOnly(&taskHandle, &taskInitParam);  // 创建任务（仅创建不调度）
    if (ret == LOS_OK) {
        *thread = (pthread_t)taskHandle;  // 设置线程ID
        ret = InitPthreadData(*thread, &userAttr, name, PTHREAD_DATA_NAME_MAX);  // 初始化线程数据
        if (ret != LOS_OK) {
            goto ERROR_OUT_WITH_TASK;  // 初始化失败，跳转到错误处理
        }
        (VOID)LOS_SetTaskScheduler(taskHandle, SCHED_RR, taskInitParam.usTaskPrio);  // 设置调度器
    }

    if (ret == LOS_OK) {
        return ENOERR;  // 创建成功
    } else {
        goto ERROR_OUT;  // 创建失败
    }

ERROR_OUT_WITH_TASK:
    (VOID)LOS_TaskDelete(taskHandle);  // 删除已创建的任务
ERROR_OUT:
    *thread = (pthread_t)-1;  // 设置无效线程ID

    return map_errno(ret);  // 映射错误码并返回
}

/**
 * @brief 终止当前线程并返回值
 * @param[in] retVal 线程退出返回值
 * @note 会设置线程状态为已退出或可连接，并唤醒等待的线程
 */
void pthread_exit(void *retVal)
{
    _pthread_data *self = pthread_get_self_data();  // 获取当前线程数据
    UINT32 intSave;                                 // 中断保存变量

    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, (int *)0) != ENOERR) {  // 禁用取消
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    if (pthread_mutex_lock(&g_pthreadsDataMutex) != ENOERR) {  // 加锁保护线程数据
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    self->task->joinRetval = retVal;  // 设置线程返回值
    /*
     * 如果已经分离，进入EXITED状态，否则进入JOIN状态
     */
    if (self->state == PTHREAD_STATE_DETACHED) {
        self->state = PTHREAD_STATE_EXITED;  // 分离状态线程设置为已退出
        g_pthreadsExited++;  // 增加退出线程计数
    } else {
        self->state = PTHREAD_STATE_JOIN;  // 可连接线程设置为等待连接
    }

    if (pthread_mutex_unlock(&g_pthreadsDataMutex) != ENOERR) {  // 解锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }
    SCHEDULER_LOCK(intSave);  // 关调度器
    /* 如果线程是最高优先级线程，在LOS_SemPost中无法调度 */
    OsTaskJoinPostUnsafe(self->task);  // 唤醒等待连接的线程
    if (self->task->taskStatus & OS_TASK_STATUS_RUNNING) {
        OsSchedResched();  // 触发调度
    }
    SCHEDULER_UNLOCK(intSave);  // 开调度器
}

/**
 * @brief 根据被连接线程的状态进行处理
 * @param[in,out] joined 被连接线程的控制结构
 * @return 成功返回0，失败返回错误码
 * @note 处理不同状态线程的连接逻辑
 */
STATIC INT32 ProcessByJoinState(_pthread_data *joined)
{
    UINT32 intSave;  // 中断保存变量
    INT32 err = 0;   // 错误码
    UINT32 ret;      // 返回值
    switch (joined->state) {
        case PTHREAD_STATE_RUNNING:  // 线程仍在运行
            /* 线程仍在运行，必须等待它 */
            SCHEDULER_LOCK(intSave);  // 关调度器
            ret = OsTaskJoinPendUnsafe(joined->task);  // 等待任务退出
            SCHEDULER_UNLOCK(intSave);  // 开调度器
            if (ret != LOS_OK) {
                err = (INT32)ret;  // 设置错误码
                break;
            }

            joined->state = PTHREAD_STATE_ALRDY_JOIN;  // 设置为已连接状态
            break;
           /*
            * 线程在等待期间变得不可连接，因此我们
            * 继续执行以报告错误
            */
        case PTHREAD_STATE_FREE:      // 空闲状态
        case PTHREAD_STATE_DETACHED:  // 分离状态
        case PTHREAD_STATE_EXITED:    // 已退出状态
            /* 这些状态都不能被连接 */
            err = EINVAL;  // 返回无效参数错误
            break;
        case PTHREAD_STATE_ALRDY_JOIN:  // 已连接状态
            err = EINVAL;  // 返回无效参数错误
            break;
        case PTHREAD_STATE_JOIN:  // 等待连接状态
            break;  // 无需处理
        default:
            PRINT_ERR("state: %u is not supported\n", (UINT32)joined->state);  // 不支持的状态
            break;
    }
    return err;  // 返回错误码
}

/**
 * @brief 等待线程终止并获取返回值
 * @param[in] thread 要等待的线程ID
 * @param[out] retVal 输出参数，用于存储线程返回值（可为NULL）
 * @return 成功返回ENOERR，失败返回错误码
 * @note 会阻塞当前线程直到目标线程终止，支持取消点
 */
int pthread_join(pthread_t thread, void **retVal)
{
    INT32 err;  // 错误码
    UINT8 status;  // 线程状态
    _pthread_data *self = NULL;    // 当前线程数据
    _pthread_data *joined = NULL;  // 被连接线程数据

    /* 首先检查取消 */
    pthread_testcancel();  // 检查取消请求

    /* 清理任何死亡线程 */
    (VOID)pthread_mutex_lock(&g_pthreadsDataMutex);
    PthreadReap();
    (VOID)pthread_mutex_unlock(&g_pthreadsDataMutex);

    self   = pthread_get_self_data();  // 获取当前线程数据
    joined = pthread_get_data(thread);  // 获取被连接线程数据
    if (joined == NULL) {
        return ESRCH;  // 线程不存在
    }
    status = joined->state;  // 保存当前状态

    if (joined == self) {  // 检查是否连接自身
        return EDEADLK;  // 死锁错误
    }

    err = ProcessByJoinState(joined);  // 根据状态处理
    (VOID)pthread_mutex_lock(&g_pthreadsDataMutex);

    if (!err) {  // 无错误
        /*
         * 此处，我们知道被连接线程已退出并准备好被连接
         */
        if (retVal != NULL) {
            /* 获取返回值 */
            *retVal = joined->task->joinRetval;  // 设置返回值
        }

        /* 设置状态为已退出 */
        joined->state = PTHREAD_STATE_EXITED;  // 更新状态
        g_pthreadsExited++;  // 增加退出线程计数

        /* 清理任何死亡线程 */
        PthreadReap();
    } else {
        joined->state = status;  // 恢复原始状态
    }

    (VOID)pthread_mutex_unlock(&g_pthreadsDataMutex);
    /* 返回前检查取消 */
    pthread_testcancel();  // 检查取消请求

    return err;  // 返回错误码
}

/**
 * @brief 设置线程为分离状态
 * @param[in] thread 要设置的线程ID
 * @return 成功返回ENOERR，失败返回错误码
 * @note 分离状态的线程退出后会自动释放资源，无需pthread_join
 */
int pthread_detach(pthread_t thread)
{
    int ret = 0;  // 返回值
    UINT32 intSave;  // 中断保存变量

    _pthread_data *detached = NULL;  // 被分离线程数据

    if (pthread_mutex_lock(&g_pthreadsDataMutex) != ENOERR) {  // 加锁
        ret = ESRCH;  // 锁定失败
    }
    detached = pthread_get_data(thread);  // 获取线程数据
    if (detached == NULL) {
        ret = ESRCH; /* 无此线程 */
    } else if (detached->state == PTHREAD_STATE_DETACHED) {
        ret = EINVAL; /* 已经分离！ */
    } else if (detached->state == PTHREAD_STATE_JOIN) {  // 等待连接状态
        detached->state = PTHREAD_STATE_EXITED;  // 设置为已退出
        g_pthreadsExited++;  // 增加退出计数
    } else {
        /* 设置状态为分离并唤醒任何等待连接的线程 */
        SCHEDULER_LOCK(intSave);  // 关调度器
        if (!(detached->task->taskStatus & OS_TASK_STATUS_EXIT)) {  // 线程未退出
            ret = OsTaskSetDetachUnsafe(detached->task);  // 设置分离状态
            if (ret == ESRCH) {
                ret = LOS_OK;  // 忽略ESRCH错误
            } else if (ret == LOS_OK) {
                detached->state = PTHREAD_STATE_DETACHED;  // 更新状态
            }
        } else {
            detached->state = PTHREAD_STATE_EXITED;  // 已退出线程设置为已退出状态
            g_pthreadsExited++;  // 增加退出计数
        }
        SCHEDULER_UNLOCK(intSave);  // 开调度器
    }

    /* 清理任何死亡线程 */
    PthreadReap();
    if (pthread_mutex_unlock(&g_pthreadsDataMutex) != ENOERR) {  // 解锁
        ret = ESRCH;  // 解锁失败
    }

    return ret;  // 返回结果
}

/**
 * @brief 设置线程的调度参数
 * @param[in] thread 线程ID
 * @param[in] policy 调度策略（当前仅支持SCHED_RR）
 * @param[in] param 调度参数（包含优先级）
 * @return 成功返回ENOERR，失败返回错误码
 * @note 优先级必须在[0, OS_TASK_PRIORITY_LOWEST]范围内
 */
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param)
{
    _pthread_data *data = NULL;  // 线程数据
    int ret;                     // 返回值

    if ((param == NULL) || (param->sched_priority > OS_TASK_PRIORITY_LOWEST)) {  // 检查参数
        return EINVAL;  // 参数无效
    }

    if (policy != SCHED_RR) {  // 检查调度策略
        return EINVAL;  // 仅支持RR调度
    }

    /* 参数似乎有效，修改线程 */
    ret = pthread_mutex_lock(&g_pthreadsDataMutex);  // 加锁
    if (ret != ENOERR) {
        return ret;  // 锁定失败
    }

    data = pthread_get_data(thread);  // 获取线程数据
    if (data == NULL) {
        ret = pthread_mutex_unlock(&g_pthreadsDataMutex);  // 解锁
        if (ret != ENOERR) {
            return ret;  // 解锁失败
        }
        return ESRCH;  // 线程不存在
    }

    /* 现在仅支持一种策略 */
    data->attr.schedpolicy = SCHED_RR;  // 设置调度策略
    data->attr.schedparam  = *param;    // 设置调度参数

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);  // 解锁
    if (ret != ENOERR) {
        return ret;  // 解锁失败
    }
    (VOID)LOS_TaskPriSet((UINT32)thread, (UINT16)param->sched_priority);  // 设置任务优先级

    return ENOERR;  // 成功
}

/**
 * @brief 获取线程的调度参数
 * @param[in] thread 线程ID
 * @param[out] policy 输出参数，返回调度策略
 * @param[out] param 输出参数，返回调度参数
 * @return 成功返回ENOERR，失败返回错误码
 */
int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param)
{
    _pthread_data *data = NULL;  // 线程数据
    int ret;                     // 返回值

    if ((policy == NULL) || (param == NULL)) {  // 检查输出参数
        return EINVAL;  // 参数无效
    }

    ret = pthread_mutex_lock(&g_pthreadsDataMutex);  // 加锁
    if (ret != ENOERR) {
        return ret;  // 锁定失败
    }

    data = pthread_get_data(thread);  // 获取线程数据
    if (data == NULL) {
        goto ERR_OUT;  // 线程不存在，跳转错误处理
    }

    *policy = data->attr.schedpolicy;  // 获取调度策略
    *param = data->attr.schedparam;    // 获取调度参数

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);  // 解锁
    return ret;  // 返回结果
ERR_OUT:
    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);  // 解锁
    if (ret != ENOERR) {
        return ret;  // 解锁失败
    }
    return ESRCH;  // 线程不存在
}

/**
 * @brief 确保初始化例程只被调用一次
 * @param[in,out] onceControl 控制变量，确保初始化只执行一次
 * @param[in] initRoutine 要执行的初始化函数
 * @return 成功返回ENOERR，失败返回错误码
 * @note 使用互斥锁保证线程安全
 */
int pthread_once(pthread_once_t *onceControl, void (*initRoutine)(void))
{
    pthread_once_t old;  // 旧值
    int ret;             // 返回值

    if ((onceControl == NULL) || (initRoutine == NULL)) {  // 检查参数
        return EINVAL;  // 参数无效
    }

    /* 对onceControl对象执行测试并设置操作 */
    ret = pthread_mutex_lock(&g_pthreadsDataMutex);  // 加锁
    if (ret != ENOERR) {
        return ret;  // 锁定失败
    }

    old = *onceControl;  // 保存旧值
    *onceControl = 1;    // 设置为已初始化

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);  // 解锁
    if (ret != ENOERR) {
        return ret;  // 解锁失败
    }
    /* 如果onceControl之前为零，则调用initRoutine() */
    if (!old) {
        initRoutine();  // 执行初始化
    }

    return ENOERR;  // 成功
}

/**
 * @brief 创建线程私有数据键
 * @param[out] key 输出参数，返回创建的键
 * @param[in] destructor 线程退出时的析构函数
 * @return 始终返回0
 * @note 本实现未支持线程私有数据功能
 */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
    (VOID)key;       // 未使用参数
    (VOID)destructor;  // 未使用参数
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);  // 打印不支持信息
    return 0;  // 返回0
}

/**
 * @brief 设置线程私有数据
 * @param[in] key 私有数据键
 * @param[in] pointer 要存储的指针值
 * @return 始终返回0
 * @note 本实现未支持线程私有数据功能
 */
int pthread_setspecific(pthread_key_t key, const void *pointer)
{
    (VOID)key;     // 未使用参数
    (VOID)pointer;  // 未使用参数
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);  // 打印不支持信息
    return 0;  // 返回0
}

/**
 * @brief 获取线程私有数据
 * @param[in] key 私有数据键
 * @return 始终返回NULL
 * @note 本实现未支持线程私有数据功能
 */
void *pthread_getspecific(pthread_key_t key)
{
    (VOID)key;  // 未使用参数
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);  // 打印不支持信息
    return NULL;  // 返回NULL
}

/**
 * @brief 设置当前线程的取消状态
 * @param[in] state 取消状态：PTHREAD_CANCEL_ENABLE或PTHREAD_CANCEL_DISABLE
 * @param[out] oldState 输出参数，返回旧的取消状态（可为NULL）
 * @return 成功返回ENOERR，失败返回错误码
 */
int pthread_setcancelstate(int state, int *oldState)
{
    _pthread_data *self = NULL;  // 当前线程数据
    int ret;                     // 返回值

    if ((state != PTHREAD_CANCEL_ENABLE) && (state != PTHREAD_CANCEL_DISABLE)) {  // 检查状态值
        return EINVAL;  // 参数无效
    }

    ret = pthread_mutex_lock(&g_pthreadsDataMutex);  // 加锁
    if (ret != ENOERR) {
        return ret;  // 锁定失败
    }

    self = pthread_get_self_data();  // 获取当前线程数据

    if (oldState != NULL) {
        *oldState = self->cancelstate;  // 设置旧状态
    }

    self->cancelstate = (UINT8)state;  // 设置新状态

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);  // 解锁
    if (ret != ENOERR) {
        return ret;  // 解锁失败
    }

    return ENOERR;  // 成功
}

/**
 * @brief 设置当前线程的取消类型
 * @param[in] type 取消类型：PTHREAD_CANCEL_ASYNCHRONOUS或PTHREAD_CANCEL_DEFERRED
 * @param[out] oldType 输出参数，返回旧的取消类型（可为NULL）
 * @return 成功返回ENOERR，失败返回错误码
 */
int pthread_setcanceltype(int type, int *oldType)
{
    _pthread_data *self = NULL;  // 当前线程数据
    int ret;                     // 返回值

    if ((type != PTHREAD_CANCEL_ASYNCHRONOUS) && (type != PTHREAD_CANCEL_DEFERRED)) {  // 检查类型值
        return EINVAL;  // 参数无效
    }

    ret = pthread_mutex_lock(&g_pthreadsDataMutex);  // 加锁
    if (ret != ENOERR) {
        return ret;  // 锁定失败
    }

    self = pthread_get_self_data();  // 获取当前线程数据
    if (oldType != NULL) {
        *oldType = self->canceltype;  // 设置旧类型
    }

    self->canceltype = (UINT8)type;  // 设置新类型

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);  // 解锁
    if (ret != ENOERR) {
        return ret;  // 解锁失败
    }

    return ENOERR;  // 成功
}

/**
 * @brief 执行线程取消操作
 * @param[in,out] data 线程控制结构
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @note 挂起线程并设置取消相关状态和返回值
 */
STATIC UINT32 DoPthreadCancel(_pthread_data *data)
{
    UINT32 ret = LOS_OK;  // 返回值
    UINT32 intSave;       // 中断保存变量
    LOS_TaskLock();       // 任务锁定
    data->canceled = 0;   // 清除取消标志
    if ((data->task->taskStatus & OS_TASK_STATUS_EXIT) || (LOS_TaskSuspend(data->task->taskID) != ENOERR)) {  // 检查任务状态或挂起任务
        ret = LOS_NOK;  // 操作失败
        goto OUT;       // 跳转退出
    }

    if (data->task->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {  // 检查是否有线程等待连接
        SCHEDULER_LOCK(intSave);  // 关调度器
        OsTaskJoinPostUnsafe(data->task);  // 唤醒等待连接的线程
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        g_pthreadCanceledDummyVar = (UINTPTR)PTHREAD_CANCELED;  // 设置取消虚拟变量
        data->task->joinRetval = (VOID *)g_pthreadCanceledDummyVar;  // 设置取消返回值
    } else if (data->state && !(data->task->taskStatus & OS_TASK_STATUS_UNUSED)) {  // 检查线程状态
        data->state = PTHREAD_STATE_EXITED;  // 设置为已退出状态
        g_pthreadsExited++;  // 增加退出计数
        PthreadReap();  // 清理已退出线程
    } else {
        ret = LOS_NOK;  // 操作失败
    }
OUT:
    LOS_TaskUnlock();  // 任务解锁
    return ret;  // 返回结果
}

/**
 * @brief 请求取消一个线程
 * @param[in] thread 要取消的线程ID
 * @return 成功返回ENOERR，失败返回错误码
 * @note 仅当线程启用取消且为异步取消类型时立即取消，否则设置取消标志
 */
int pthread_cancel(pthread_t thread)
{
    _pthread_data *data = NULL;  // 线程数据

    if (pthread_mutex_lock(&g_pthreadsDataMutex)) {  // 加锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    data = pthread_get_data(thread);  // 获取线程数据
    if (data == NULL) {  // 线程不存在
        if (pthread_mutex_unlock(&g_pthreadsDataMutex)) {  // 解锁
            PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
        }
        return ESRCH;  // 返回线程不存在错误
    }

    data->canceled = 1;  // 设置取消标志

    if ((data->cancelstate == PTHREAD_CANCEL_ENABLE) &&
        (data->canceltype == PTHREAD_CANCEL_ASYNCHRONOUS)) {  // 检查取消状态和类型
        /*
         * 如果线程启用了取消，并且处于异步模式
         * 挂起它并设置相应的线程状态
         * 我们还释放线程当前的等待以使其唤醒
         */
        if (DoPthreadCancel(data) == LOS_NOK) {  // 执行取消
            goto ERROR_OUT;  // 取消失败
        }
    }

    /*
     * 否则线程禁用了取消，这种情况下
     * 由线程自行启用取消
     */
    if (pthread_mutex_unlock(&g_pthreadsDataMutex)) {  // 解锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    return ENOERR;  // 成功
ERROR_OUT:
    if (pthread_mutex_unlock(&g_pthreadsDataMutex)) {  // 解锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }
    return ESRCH;  // 返回错误
}

/**
 * @brief 测试当前线程是否有挂起的取消请求
 * @note 如果有挂起的取消请求且取消已启用，则终止线程
 */
void pthread_testcancel(void)
{
    if (CheckForCancel()) {  // 检查取消请求
        /*
         * 如果启用了取消，并且有挂起的取消请求
         * 则继续执行取消操作
         * 使用特殊返回值立即退出。pthread_exit()隐式调用取消处理程序
         */
        pthread_exit((void *)PTHREAD_CANCELLED);  // 取消线程
    }
}

/**
 * @brief 获取当前线程ID
 * @return 当前线程ID
 */
pthread_t pthread_self(void)
{
    _pthread_data *data = pthread_get_self_data();  // 获取当前线程数据

    return data->id;  // 返回线程ID
}

/**
 * @brief 比较两个线程ID是否相等
 * @param[in] thread1 第一个线程ID
 * @param[in] thread2 第二个线程ID
 * @return 相等返回非零值，否则返回0
 */
int pthread_equal(pthread_t thread1, pthread_t thread2)
{
    return thread1 == thread2;  // 比较线程ID
}

/**
 * @brief 注册线程清理处理程序（内部实现）
 * @param[in] buffer 清理缓冲区
 * @param[in] routine 清理函数
 * @param[in] arg 传递给清理函数的参数
 * @note 本实现未支持清理处理程序功能
 */
void pthread_cleanup_push_inner(struct pthread_cleanup_buffer *buffer,
                                void (*routine)(void *), void *arg)
{
    (VOID)buffer;   // 未使用参数
    (VOID)routine;  // 未使用参数
    (VOID)arg;      // 未使用参数
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);  // 打印不支持信息
    return;
}

/**
 * @brief 执行并移除线程清理处理程序（内部实现）
 * @param[in] buffer 清理缓冲区
 * @param[in] execute 是否执行清理函数
 * @note 本实现未支持清理处理程序功能
 */
void pthread_cleanup_pop_inner(struct pthread_cleanup_buffer *buffer, int execute)
{
    (VOID)buffer;   // 未使用参数
    (VOID)execute;  // 未使用参数
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);  // 打印不支持信息
    return;
}
/**
 * @brief 设置线程的CPU亲和性掩码
 * @param thread 目标线程ID
 * @param cpusetsize cpu_set_t结构的大小（字节）
 * @param cpuset 指向CPU亲和性掩码的指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t* cpuset)
{
    INT32 ret = sched_setaffinity(thread, cpusetsize, cpuset);  // 调用sched_setaffinity设置线程CPU亲和性
    if (ret == -1) {                                            // 检查系统调用是否失败
        return errno;                                           // 失败时返回错误码
    } else {
        return ENOERR;                                          // 成功时返回0
    }
}

/**
 * @brief 获取线程的CPU亲和性掩码
 * @param thread 目标线程ID
 * @param cpusetsize cpu_set_t结构的大小（字节）
 * @param cpuset 用于存储CPU亲和性掩码的指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t* cpuset)
{
    INT32 ret = sched_getaffinity(thread, cpusetsize, cpuset);  // 调用sched_getaffinity获取线程CPU亲和性
    if (ret == -1) {                                            // 检查系统调用是否失败
        return errno;                                           // 失败时返回错误码
    } else {
        return ENOERR;                                          // 成功时返回0
    }
}
