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

#include "pprivate.h"
#include "pthread.h"
#include "sched.h"

#include "stdio.h"
#include "map_error.h"
#include "los_process_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/*
 * Array of pthread control structures. A pthread_t object is
 * "just" an index into this array.
 */ //pthread控制结构的数组。pthread_t对象只是这个数组的一个索引
STATIC _pthread_data g_pthreadData[LOSCFG_BASE_CORE_TSK_LIMIT + 1]; //128

/* Count of number of threads that have exited and not been reaped. */ //已退出但尚未获取的线程数的计数
STATIC INT32 g_pthreadsExited = 0;

/* this is to protect the pthread data */	//这是为了保护pthread数据
STATIC pthread_mutex_t g_pthreadsDataMutex = PTHREAD_MUTEX_INITIALIZER; //posix 模块的互斥锁

/* pointed to by PTHREAD_CANCELED */
UINTPTR g_pthreadCanceledDummyVar;

/*
 * Private version of pthread_self() that returns a pointer to our internal
 * control structure.
 */ //获取当前线程的数据，返回指向内部控制结构的指针
_pthread_data *pthread_get_self_data(void)
{
    UINT32 runningTaskPID = ((LosTaskCB *)(OsCurrTaskGet()))->taskID;//获取当前taskID
    _pthread_data *data = &g_pthreadData[runningTaskPID];//获取当前线程的 posix 数据

    return data;
}
//以posix 方式获取线程的数据 参数为线程(task) id
_pthread_data *pthread_get_data(pthread_t id)
{
    _pthread_data *data = NULL;

    if (OS_TID_CHECK_INVALID(id)) {
        return NULL;
    }

    data = &g_pthreadData[id];
    /* Check that this is a valid entry */ //检查是否是一个有效的实体
    if ((data->state == PTHREAD_STATE_FREE) || (data->state == PTHREAD_STATE_EXITED)) { 
        return NULL;
    }

    /* Check that the entry matches the id */ //检查实体ID和参数ID是否一致
    if (data->id != id) {
        return NULL;
    }

    /* Return the pointer */
    return data;
}

/*
 * Check whether there is a cancel pending and if so, whether
 * cancellations are enabled. We do it in this order to reduce the
 * number of tests in the common case - when no cancellations are
 * pending. We make this inline so it can be called directly below for speed
 */
STATIC INT32 CheckForCancel(VOID)
{
    _pthread_data *self = pthread_get_self_data();
    if (self->canceled && (self->cancelstate == PTHREAD_CANCEL_ENABLE)) {
        return 1;
    }
    return 0;
}

STATIC VOID ProcessUnusedStatusTask(_pthread_data *data)
{
    data->state = PTHREAD_STATE_FREE;
    (VOID)memset_s(data, sizeof(_pthread_data), 0, sizeof(_pthread_data));
}

/*
 * This function is called to tidy up and dispose of any threads that have
 * exited. This work must be done from a thread other than the one exiting.
 * Note: this function must be called with pthread_mutex locked.
 */ //这个函数必须在 持有 pthread_mutex 锁的情况下调用 ,该函数主要用于 posix 线程池和内核task池对退出情况的同步
STATIC VOID PthreadReap(VOID)
{
    UINT32 i;
    _pthread_data *data = NULL;
    /*
     * Loop over the thread table looking for exited threads. The //在线程池上循环，查找已退出的线程。
     * g_pthreadsExited counter springs us out of this once we have
     * found them all (and keeps us out if there are none to do).
     */
    for (i = 0; g_pthreadsExited && (i < g_taskMaxNum); i++) {
        data = &g_pthreadData[i];
        if (data->state == PTHREAD_STATE_EXITED) { //找到一个退出线程
            /* the Huawei LiteOS not delete the dead TCB,so need to delete the TCB */
            (VOID)LOS_TaskDelete(data->task->taskID);//删除这个task
            if (data->task->taskStatus & OS_TASK_STATUS_UNUSED) {//如果task贴有未使用的标签
                ProcessUnusedStatusTask(data);//posix线程数据清0,状态改为 free
                g_pthreadsExited--;//退出线程计数器减少一个
            }
        }
    }
}

STATIC VOID SetPthreadAttr(const _pthread_data *self, const pthread_attr_t *attr, pthread_attr_t *outAttr)
{
    /*
     * Set use_attr to the set of attributes we are going to
     * actually use. Either those passed in, or the default set.
     */
    if (attr == NULL) {
        (VOID)pthread_attr_init(outAttr);
    } else {
        (VOID)memcpy_s(outAttr, sizeof(pthread_attr_t), attr, sizeof(pthread_attr_t));
    }

    /*
     * If the stack size is not valid, we can assume that it is at
     * least PTHREAD_STACK_MIN bytes.
     */
    if (!outAttr->stacksize_set) {
        outAttr->stacksize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    }
    if (outAttr->inheritsched == PTHREAD_INHERIT_SCHED) {
        if (self->task == NULL) {
            outAttr->schedparam.sched_priority = ((LosTaskCB *)(OsCurrTaskGet()))->priority;
        } else {
            outAttr->schedpolicy = self->attr.schedpolicy;
            outAttr->schedparam  = self->attr.schedparam;
            outAttr->scope       = self->attr.scope;
        }
    }
}

STATIC VOID SetPthreadDataAttr(const pthread_attr_t *userAttr, const pthread_t threadID,
                               LosTaskCB *taskCB, _pthread_data *created)
{
    created->attr         = *userAttr;
    created->id           = threadID;
    created->task         = taskCB;
    created->state        = (userAttr->detachstate == PTHREAD_CREATE_JOINABLE) ?
                            PTHREAD_STATE_RUNNING : PTHREAD_STATE_DETACHED;
    /* need to confirmation */
    created->cancelstate  = PTHREAD_CANCEL_ENABLE;
    created->canceltype   = PTHREAD_CANCEL_DEFERRED;
    created->cancelbuffer = NULL;
    created->canceled     = 0;
    created->freestack    = 0; /* no use default : 0 */
    created->stackmem     = taskCB->topOfStack;
    created->thread_data  = NULL;
}
//posix 初始化线程数据
STATIC UINT32 InitPthreadData(pthread_t threadID, pthread_attr_t *userAttr,
                              const CHAR name[], size_t len)
{
    errno_t err;
    UINT32 ret = LOS_OK;
    LosTaskCB *taskCB = OS_TCB_FROM_TID(threadID);//取出task实体
    _pthread_data *created = &g_pthreadData[threadID];//取出posix 对应的 线程实体

    err = strncpy_s(created->name, sizeof(created->name), name, len);//线程的名称用参数名称
    if (err != EOK) {
        PRINT_ERR("%s: %d, err: %d\n", __FUNCTION__, __LINE__, err);
        return LOS_NOK;
    }
    userAttr->stacksize   = taskCB->stackSize;//栈大小用 task的
    err = memcpy_s(taskCB->taskName, OS_TCB_NAME_LEN, created->name, strlen(created->name));//task名称用参数名称
    if (err != EOK) {
        PRINT_ERR("%s: %d, err: %d\n", __FUNCTION__, __LINE__, err);
        taskCB->taskName[0] = '\0';
        return LOS_NOK;
    }
#if (LOSCFG_KERNEL_SMP == YES)
    if (userAttr->cpuset.__bits[0] > 0) {
        taskCB->cpuAffiMask = (UINT16)userAttr->cpuset.__bits[0];//指定task的CPU亲和力掩码
    }
#endif

    SetPthreadDataAttr(userAttr, threadID, taskCB, created);//设置线程数据具体属性
    return ret;
}
//POSIX接口之 创建一个线程
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*startRoutine)(void *), void *arg)
{
    pthread_attr_t userAttr;
    UINT32 ret;
    CHAR name[PTHREAD_DATA_NAME_MAX];//线程名称不能超过 20个字符
    STATIC UINT16 pthreadNumber = 1;
    TSK_INIT_PARAM_S taskInitParam = {0};
    UINT32 taskHandle;
    _pthread_data *self = pthread_get_self_data();

    if ((thread == NULL) || (startRoutine == NULL)) {
        return EINVAL;
    }

    SetPthreadAttr(self, attr, &userAttr);

    (VOID)memset_s(name, sizeof(name), 0, sizeof(name));
    (VOID)snprintf_s(name, sizeof(name), sizeof(name) - 1, "pth%02d", pthreadNumber);
    pthreadNumber++;
	//参数的转化,适配
    taskInitParam.pcName       = name;	//线程名称
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)startRoutine;//入口函数
    taskInitParam.auwArgs[0]   = (UINTPTR)arg;	//入口函数的参数
    taskInitParam.usTaskPrio   = (UINT16)userAttr.schedparam.sched_priority;//线程优先级
    taskInitParam.uwStackSize  = userAttr.stacksize;//栈大小
    if (OsProcessIsUserMode(OsCurrProcessGet())) {//当前进程是用户进程吗
        taskInitParam.processID = OsGetKernelInitProcessID();// 赋予内核初始化进程ID 即:"Kprocess"进程
    } else {
        taskInitParam.processID = OsCurrProcessGet()->processID;// 内核某个进程的ID
    }
    if (userAttr.detachstate == PTHREAD_CREATE_DETACHED) {//线程间关系为 分离模式
        taskInitParam.uwResved = LOS_TASK_STATUS_DETACHED;
    } else {////线程间关系为 联结模式
        /* Set the pthread default joinable */
        taskInitParam.uwResved = 0;
    }

    PthreadReap();// posix <--> liteos 数据同步
    ret = LOS_TaskCreateOnly(&taskHandle, &taskInitParam);//创建一个任务
    if (ret == LOS_OK) {
        *thread = (pthread_t)taskHandle;//任务ID
        ret = InitPthreadData(*thread, &userAttr, name, PTHREAD_DATA_NAME_MAX);//初始化线程数据
        if (ret != LOS_OK) {
            goto ERROR_OUT_WITH_TASK;
        }
        (VOID)LOS_SetTaskScheduler(taskHandle, SCHED_RR, taskInitParam.usTaskPrio);//设置任务的调度参数
    }

    if (ret == LOS_OK) {
        return ENOERR;
    } else {
        goto ERROR_OUT;
    }

ERROR_OUT_WITH_TASK:
    (VOID)LOS_TaskDelete(taskHandle);
ERROR_OUT:
    *thread = (pthread_t)-1;

    return map_errno(ret);
}
//POSIX接口之 终止当前线程
void pthread_exit(void *retVal)
{
    _pthread_data *self = pthread_get_self_data();//获取当前线程实体
    UINT32 intSave;

    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, (int *)0) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    if (pthread_mutex_lock(&g_pthreadsDataMutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    self->task->joinRetval = retVal;
    /*
     * If we are already detached, go to EXITED state, otherwise
     * go into JOIN state.
     */
    if (self->state == PTHREAD_STATE_DETACHED) {
        self->state = PTHREAD_STATE_EXITED;
        g_pthreadsExited++;
    } else {
        self->state = PTHREAD_STATE_JOIN;
    }

    if (pthread_mutex_unlock(&g_pthreadsDataMutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }
    SCHEDULER_LOCK(intSave);
    /* If the thread is the highest thread,it can't schedule in LOS_SemPost. */
    OsTaskJoinPostUnsafe(self->task);
    if (self->task->taskStatus & OS_TASK_STATUS_RUNNING) {
        OsSchedResched();
    }
    SCHEDULER_UNLOCK(intSave);
}

STATIC INT32 ProcessByJoinState(_pthread_data *joined)
{
    UINT32 intSave;
    INT32 err = 0;
    UINT32 ret;
    switch (joined->state) {
        case PTHREAD_STATE_RUNNING:
            /* The thread is still running, we must wait for it. */
            SCHEDULER_LOCK(intSave);
            ret = OsTaskJoinPendUnsafe(joined->task);
            SCHEDULER_UNLOCK(intSave);
            if (ret != LOS_OK) {
                err = (INT32)ret;
                break;
            }

            joined->state = PTHREAD_STATE_ALRDY_JOIN;
            break;
           /*
            * The thread has become unjoinable while we waited, so we
            * fall through to complain.
            */
        case PTHREAD_STATE_FREE:
        case PTHREAD_STATE_DETACHED:
        case PTHREAD_STATE_EXITED:
            /* None of these may be joined. */
            err = EINVAL;
            break;
        case PTHREAD_STATE_ALRDY_JOIN:
            err = EINVAL;
            break;
        case PTHREAD_STATE_JOIN:
            break;
        default:
            PRINT_ERR("state: %u is not supported\n", (UINT32)joined->state);
            break;
    }
    return err;
}
//阻塞当前的线程，直到另外一个线程运行结束
int pthread_join(pthread_t thread, void **retVal)
{
    INT32 err;
    UINT8 status;
    _pthread_data *self = NULL;
    _pthread_data *joined = NULL;

    /* Check for cancellation first. */
    pthread_testcancel();

    /* Dispose of any dead threads */
    (VOID)pthread_mutex_lock(&g_pthreadsDataMutex);
    PthreadReap();
    (VOID)pthread_mutex_unlock(&g_pthreadsDataMutex);

    self   = pthread_get_self_data();
    joined = pthread_get_data(thread);
    if (joined == NULL) {
        return ESRCH;
    }
    status = joined->state;

    if (joined == self) {
        return EDEADLK;
    }

    err = ProcessByJoinState(joined);
    (VOID)pthread_mutex_lock(&g_pthreadsDataMutex);

    if (!err) {
        /*
         * Here, we know that joinee is a thread that has exited and is
         * ready to be joined.
         */
        if (retVal != NULL) {
            /* Get the retVal */
            *retVal = joined->task->joinRetval;
        }

        /* Set state to exited. */
        joined->state = PTHREAD_STATE_EXITED;
        g_pthreadsExited++;

        /* Dispose of any dead threads */
        PthreadReap();
    } else {
        joined->state = status;
    }

    (VOID)pthread_mutex_unlock(&g_pthreadsDataMutex);
    /* Check for cancellation before returning */
    pthread_testcancel();

    return err;
}

/*
 * Set the detachstate of the thread to "detached". The thread then does not
 * need to be joined and its resources will be freed when it exits.
 */
int pthread_detach(pthread_t thread)
{
    int ret = 0;
    UINT32 intSave;

    _pthread_data *detached = NULL;

    if (pthread_mutex_lock(&g_pthreadsDataMutex) != ENOERR) {
        ret = ESRCH;
    }
    detached = pthread_get_data(thread);
    if (detached == NULL) {
        ret = ESRCH; /* No such thread */
    } else if (detached->state == PTHREAD_STATE_DETACHED) {
        ret = EINVAL; /* Already detached! */
    } else if (detached->state == PTHREAD_STATE_JOIN) {
        detached->state = PTHREAD_STATE_EXITED;
        g_pthreadsExited++;
    } else {
        /* Set state to detached and kick any joinees to make them return. */
        SCHEDULER_LOCK(intSave);
        if (!(detached->task->taskStatus & OS_TASK_STATUS_EXIT)) {
            ret = OsTaskSetDeatchUnsafe(detached->task);
            if (ret == ESRCH) {
                ret = LOS_OK;
            } else if (ret == LOS_OK) {
                detached->state = PTHREAD_STATE_DETACHED;
            }
        } else {
            detached->state = PTHREAD_STATE_EXITED;
            g_pthreadsExited++;
        }
        SCHEDULER_UNLOCK(intSave);
    }

    /* Dispose of any dead threads */
    PthreadReap();
    if (pthread_mutex_unlock(&g_pthreadsDataMutex) != ENOERR) {
        ret = ESRCH;
    }

    return ret;
}
//posix 设置线程的调度参数
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param)
{
    _pthread_data *data = NULL;
    int ret;

    if ((param == NULL) || (param->sched_priority > OS_TASK_PRIORITY_LOWEST)) {//优先级不能过 31级
        return EINVAL;
    }

    if (policy != SCHED_RR) {//必须是抢占式调度方式
        return EINVAL;
    }

    /* The parameters seem OK, change the thread. */
    ret = pthread_mutex_lock(&g_pthreadsDataMutex);//持有posix 线程互斥锁
    if (ret != ENOERR) {
        return ret;
    }

    data = pthread_get_data(thread);//获取线程实体数据
    if (data == NULL) {
        ret = pthread_mutex_unlock(&g_pthreadsDataMutex);//异常情况要释放锁
        if (ret != ENOERR) {
            return ret;
        }
        return ESRCH;
    }

    /* Only support one policy now */
    data->attr.schedpolicy = SCHED_RR; 	//鸿蒙目前仅支持 抢占式调度
    data->attr.schedparam  = *param;	//调度参数

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);//释放锁
    if (ret != ENOERR) {
        return ret;
    }
    (VOID)LOS_TaskPriSet((UINT32)thread, (UINT16)param->sched_priority);//设置调度的优先级

    return ENOERR;
}
//posix 获取线程的调度参数
int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param)
{
    _pthread_data *data = NULL;
    int ret;

    if ((policy == NULL) || (param == NULL)) {
        return EINVAL;
    }

    ret = pthread_mutex_lock(&g_pthreadsDataMutex);//持有posix 线程互斥锁
    if (ret != ENOERR) {
        return ret;
    }

    data = pthread_get_data(thread);//获取线程实体数据
    if (data == NULL) {
        goto ERR_OUT;
    }

    *policy = data->attr.schedpolicy;//拿到调度方式
    *param = data->attr.schedparam;//拿到调度参数

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);//释放锁
    return ret;
ERR_OUT:
    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);//异常情况下释放锁
    if (ret != ENOERR) {
        return ret;
    }
    return ESRCH;
}

/* Call initRoutine just the once per control variable. */
int pthread_once(pthread_once_t *onceControl, void (*initRoutine)(void))
{
    pthread_once_t old;
    int ret;

    if ((onceControl == NULL) || (initRoutine == NULL)) {
        return EINVAL;
    }

    /* Do a test and set on the onceControl object. */
    ret = pthread_mutex_lock(&g_pthreadsDataMutex);
    if (ret != ENOERR) {
        return ret;
    }

    old = *onceControl;
    *onceControl = 1;

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);
    if (ret != ENOERR) {
        return ret;
    }
    /* If the onceControl was zero, call the initRoutine(). */
    if (!old) {
        initRoutine();
    }

    return ENOERR;
}

/* Thread specific data */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
    (VOID)key;
    (VOID)destructor;
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);
    return 0;
}

/* Store the pointer value in the thread-specific data slot addressed by the key. */
int pthread_setspecific(pthread_key_t key, const void *pointer)
{
    (VOID)key;
    (VOID)pointer;
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);
    return 0;
}

/* Retrieve the pointer value in the thread-specific data slot addressed by the key. */
void *pthread_getspecific(pthread_key_t key)
{
    (VOID)key;
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);
    return NULL;
}

/*
 * Set cancel state of current thread to ENABLE or DISABLE.
 * Returns old state in *oldState.
 */
int pthread_setcancelstate(int state, int *oldState)
{
    _pthread_data *self = NULL;
    int ret;

    if ((state != PTHREAD_CANCEL_ENABLE) && (state != PTHREAD_CANCEL_DISABLE)) {
        return EINVAL;
    }

    ret = pthread_mutex_lock(&g_pthreadsDataMutex);
    if (ret != ENOERR) {
        return ret;
    }

    self = pthread_get_self_data();

    if (oldState != NULL) {
        *oldState = self->cancelstate;
    }

    self->cancelstate = (UINT8)state;

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);
    if (ret != ENOERR) {
        return ret;
    }

    return ENOERR;
}

/*
 * Set cancel type of current thread to ASYNCHRONOUS or DEFERRED.
 * Returns old type in *oldType.
 */
int pthread_setcanceltype(int type, int *oldType)
{
    _pthread_data *self = NULL;
    int ret;

    if ((type != PTHREAD_CANCEL_ASYNCHRONOUS) && (type != PTHREAD_CANCEL_DEFERRED)) {
        return EINVAL;
    }

    ret = pthread_mutex_lock(&g_pthreadsDataMutex);
    if (ret != ENOERR) {
        return ret;
    }

    self = pthread_get_self_data();
    if (oldType != NULL) {
        *oldType = self->canceltype;
    }

    self->canceltype = (UINT8)type;

    ret = pthread_mutex_unlock(&g_pthreadsDataMutex);
    if (ret != ENOERR) {
        return ret;
    }

    return ENOERR;
}

STATIC UINT32 DoPthreadCancel(_pthread_data *data)
{
    UINT32 ret = LOS_OK;
    UINT32 intSave;
    LOS_TaskLock();
    data->canceled = 0;
    if ((data->task->taskStatus & OS_TASK_STATUS_EXIT) || (LOS_TaskSuspend(data->task->taskID) != ENOERR)) {
        ret = LOS_NOK;
        goto OUT;
    }

    if (data->task->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {
        SCHEDULER_LOCK(intSave);
        OsTaskJoinPostUnsafe(data->task);
        SCHEDULER_UNLOCK(intSave);
        g_pthreadCanceledDummyVar = (UINTPTR)PTHREAD_CANCELED;
        data->task->joinRetval = (VOID *)g_pthreadCanceledDummyVar;
    } else if (data->state && !(data->task->taskStatus & OS_TASK_STATUS_UNUSED)) {
        data->state = PTHREAD_STATE_EXITED;
        g_pthreadsExited++;
        PthreadReap();
    } else {
        ret = LOS_NOK;
    }
OUT:
    LOS_TaskUnlock();
    return ret;
}

int pthread_cancel(pthread_t thread)
{
    _pthread_data *data = NULL;

    if (pthread_mutex_lock(&g_pthreadsDataMutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    data = pthread_get_data(thread);
    if (data == NULL) {
        if (pthread_mutex_unlock(&g_pthreadsDataMutex) != ENOERR) {
            PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
        }
        return ESRCH;
    }

    data->canceled = 1;

    if ((data->cancelstate == PTHREAD_CANCEL_ENABLE) &&
        (data->canceltype == PTHREAD_CANCEL_ASYNCHRONOUS)) {
        /*
         * If the thread has cancellation enabled, and it is in
         * asynchronous mode, suspend it and set corresponding thread's status.
         * We also release the thread out of any current wait to make it wake up.
         */
        if (DoPthreadCancel(data) == LOS_NOK) {
            goto ERROR_OUT;
        }
    }

    /*
     * Otherwise the thread has cancellation disabled, in which case
     * it is up to the thread to enable cancellation
     */
    if (pthread_mutex_unlock(&g_pthreadsDataMutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    return ENOERR;
ERROR_OUT:
    if (pthread_mutex_unlock(&g_pthreadsDataMutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }
    return ESRCH;
}

/*
 * Test for a pending cancellation for the current thread and terminate
 * the thread if there is one.
 */
void pthread_testcancel(void)
{
    if (CheckForCancel()) {
        /*
         * If we have cancellation enabled, and there is a cancellation
         * pending, then go ahead and do the deed.
         * Exit now with special retVal. pthread_exit() calls the
         * cancellation handlers implicitly.
         */
        pthread_exit((void *)PTHREAD_CANCELED);
    }
}

/* Get current thread id. */ //获取当前的线程ID
pthread_t pthread_self(void)
{
    _pthread_data *data = pthread_get_self_data();//获取当前线程的实体

    return data->id;
}

/* Compare two thread identifiers. */
int pthread_equal(pthread_t thread1, pthread_t thread2)
{
    return thread1 == thread2;
}

void pthread_cleanup_push_inner(struct pthread_cleanup_buffer *buffer,
                                void (*routine)(void *), void *arg)
{
    (VOID)buffer;
    (VOID)routine;
    (VOID)arg;
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);
    return;
}

void pthread_cleanup_pop_inner(struct pthread_cleanup_buffer *buffer, int execute)
{
    (VOID)buffer;
    (VOID)execute;
    PRINT_ERR("[%s] is not support.\n", __FUNCTION__);
    return;
}

/*
 * Set the cpu affinity mask for the thread
 */
int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t* cpuset)
{
    INT32 ret = sched_setaffinity(thread, cpusetsize, cpuset);
    if (ret == -1) {
        return errno;
    } else {
        return ENOERR;
    }
}

/*
 * Get the cpu affinity mask from the thread
 */
int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t* cpuset)
{
    INT32 ret = sched_getaffinity(thread, cpusetsize, cpuset);
    if (ret == -1) {
        return errno;
    } else {
        return ENOERR;
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
