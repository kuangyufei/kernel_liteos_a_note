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
/*!
 * @file    los_rwlock.c
 * @brief
 * @link rwlock https://weharmony.github.io/openharmony/zh-cn/device-dev/kernel/kernel-small-basic-trans-rwlock.html @endlink
   @verbatim
   基本概念
	   读写锁与互斥锁类似，可用来同步同一进程中的各个任务，但与互斥锁不同的是，其允许多个读操作并发重入，而写操作互斥。
	   相对于互斥锁的开锁或闭锁状态，读写锁有三种状态：读模式下的锁，写模式下的锁，无锁。
	   读写锁的使用规则：
			保护区无写模式下的锁，任何任务均可以为其增加读模式下的锁。
			保护区处于无锁状态下，才可增加写模式下的锁。
			多任务环境下往往存在多个任务访问同一共享资源的应用场景，读模式下的锁以共享状态对保护区访问，
			而写模式下的锁可被用于对共享资源的保护从而实现独占式访问。
			这种共享-独占的方式非常适合多任务中读数据频率远大于写数据频率的应用中，提高应用多任务并发度。
   运行机制
	   相较于互斥锁，读写锁如何实现读模式下的锁及写模式下的锁来控制多任务的读写访问呢？
	   若A任务首次获取了写模式下的锁，有其他任务来获取或尝试获取读模式下的锁，均无法再上锁。
	   若A任务获取了读模式下的锁，当有任务来获取或尝试获取读模式下的锁时，读写锁计数均加一。
   @endverbatim
   @image html 
 * @attention  
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2022-02-18
 */
#include "los_rwlock_pri.h"
#include "stdint.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_task_pri.h"
#include "los_exc.h"
#include "los_sched_pri.h"

#ifdef LOSCFG_BASE_IPC_RWLOCK
#define RWLOCK_COUNT_MASK 0x00FFFFFFU
/**
 * @brief 检查读写锁是否有效
 * @param rwlock 读写锁指针
 * @return 有效返回TRUE，无效返回FALSE
 */
BOOL LOS_RwlockIsValid(const LosRwlock *rwlock)
{
    if ((rwlock != NULL) && ((rwlock->magic & RWLOCK_COUNT_MASK) == OS_RWLOCK_MAGIC)) {  // 验证锁指针和魔术字
        return TRUE;
    }

    return FALSE;  // 锁无效
}

/**
 * @brief 初始化读写锁
 * @param rwlock 读写锁指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_RwlockInit(LosRwlock *rwlock)
{
    UINT32 intSave;  // 中断状态保存变量

    if (rwlock == NULL) {  // 检查锁指针是否为空
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    if ((rwlock->magic & RWLOCK_COUNT_MASK) == OS_RWLOCK_MAGIC) {  // 检查是否已初始化
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        return LOS_EPERM;  // 返回权限错误
    }

    rwlock->rwCount = 0;  // 初始化读写计数器
    rwlock->writeOwner = NULL;  // 初始化写锁持有者
    LOS_ListInit(&(rwlock->readList));  // 初始化读等待链表
    LOS_ListInit(&(rwlock->writeList));  // 初始化写等待链表
    rwlock->magic = OS_RWLOCK_MAGIC;  // 设置魔术字
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return LOS_OK;  // 返回成功
}

/**
 * @brief 销毁读写锁
 * @param rwlock 读写锁指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_RwlockDestroy(LosRwlock *rwlock)
{
    UINT32 intSave;  // 中断状态保存变量

    if (rwlock == NULL) {  // 检查锁指针是否为空
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {  // 检查锁是否已初始化
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        return LOS_EBADF;  // 返回文件描述符错误
    }

    if (rwlock->rwCount != 0) {  // 检查锁是否正被使用
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        return LOS_EBUSY;  // 返回忙错误
    }

    (VOID)memset_s(rwlock, sizeof(LosRwlock), 0, sizeof(LosRwlock));  // 清零锁结构
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return LOS_OK;  // 返回成功
}

/**
 * @brief 读写锁参数检查
 * @param rwlock 读写锁指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsRwlockCheck(const LosRwlock *rwlock)
{
    if (rwlock == NULL) {  // 检查锁指针是否为空
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
        return LOS_EINTR;  // 返回中断错误
    }

    /* DO NOT Call blocking API in system tasks */
    LosTaskCB *runTask = (LosTaskCB *)OsCurrTaskGet();  // 获取当前任务
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        return LOS_EPERM;  // 返回权限错误
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 比较当前任务与等待队列中最高优先级任务的优先级
 * @param runTask 当前任务
 * @param rwList 等待链表
 * @return 当前任务优先级高返回TRUE，否则返回FALSE
 */
STATIC BOOL OsRwlockPriCompare(LosTaskCB *runTask, LOS_DL_LIST *rwList)
{
    if (!LOS_ListEmpty(rwList)) {  // 检查等待链表是否为空
        LosTaskCB *highestTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(rwList));  // 获取等待队列中优先级最高的任务
        if (OsSchedParamCompare(runTask, highestTask) < 0) {  // 比较优先级
            return TRUE;  // 当前任务优先级低
        }
        return FALSE;  // 当前任务优先级高
    }
    return TRUE;  // 等待队列为空
}

/**
 * @brief 读锁等待操作
 * @param runTask 当前任务
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsRwlockRdPendOp(LosTaskCB *runTask, LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 ret;  // 返回值

    /*
     * When the rwlock mode is read mode or free mode and the priority of the current read task
     * is higher than the first pended write task. current read task can obtain this rwlock.
     */
    if (rwlock->rwCount >= 0) {  // 读模式或空闲模式
        if (OsRwlockPriCompare(runTask, &(rwlock->writeList))) {  // 比较优先级
            if (rwlock->rwCount == INT8_MAX) {  // 检查读计数是否达到上限
                return LOS_EINVAL;  // 返回无效参数错误
            }
            rwlock->rwCount++;  // 增加读计数
            return LOS_OK;  // 返回成功
        }
    }

    if (!timeout) {  // 非阻塞模式
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (!OsPreemptableInSched()) {  // 检查是否允许抢占
        return LOS_EDEADLK;  // 返回死锁错误
    }

    /* The current task is not allowed to obtain the write lock when it obtains the read lock. */
    if ((LosTaskCB *)(rwlock->writeOwner) == runTask) {  // 检查当前任务是否持有写锁
        return LOS_EINVAL;  // 返回无效参数错误
    }

    /*
     * When the rwlock mode is write mode or the priority of the current read task
     * is lower than the first pended write task, current read task will be pended.
     */
    LOS_DL_LIST *node = OsSchedLockPendFindPos(runTask, &(rwlock->readList));  // 查找等待位置
    ret = runTask->ops->wait(runTask, node, timeout);  // 等待读锁
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {  // 等待超时
        return LOS_ETIMEDOUT;  // 返回超时错误
    }

    return ret;  // 返回结果
}

/**
 * @brief 写锁等待操作
 * @param runTask 当前任务
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsRwlockWrPendOp(LosTaskCB *runTask, LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 ret;  // 返回值

    /* When the rwlock is free mode, current write task can obtain this rwlock. */
    if (rwlock->rwCount == 0) {  // 空闲模式
        rwlock->rwCount = -1;  // 设置为写模式
        rwlock->writeOwner = (VOID *)runTask;  // 设置写锁持有者
        return LOS_OK;  // 返回成功
    }

    /* Current write task can use one rwlock once again if the rwlock owner is it. */
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) == runTask)) {  // 当前任务已持有写锁
        if (rwlock->rwCount == INT8_MIN) {  // 检查写计数是否达到下限
            return LOS_EINVAL;  // 返回无效参数错误
        }
        rwlock->rwCount--;  // 减少写计数（重入）
        return LOS_OK;  // 返回成功
    }

    if (!timeout) {  // 非阻塞模式
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (!OsPreemptableInSched()) {  // 检查是否允许抢占
        return LOS_EDEADLK;  // 返回死锁错误
    }

    /*
     * When the rwlock is read mode or other write task obtains this rwlock, current
     * write task will be pended.
     */
    LOS_DL_LIST *node =  OsSchedLockPendFindPos(runTask, &(rwlock->writeList));  // 查找等待位置
    ret = runTask->ops->wait(runTask, node, timeout);  // 等待写锁
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {  // 等待超时
        ret = LOS_ETIMEDOUT;  // 设置超时错误
    }

    return ret;  // 返回结果
}

/**
 * @brief 不安全的读锁获取（无调度保护）
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsRwlockRdUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {  // 检查锁是否有效
        return LOS_EBADF;  // 返回文件描述符错误
    }

    return OsRwlockRdPendOp(OsCurrTaskGet(), rwlock, timeout);  // 执行读锁等待操作
}

/**
 * @brief 不安全的尝试获取读锁（无调度保护）
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsRwlockTryRdUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {  // 检查锁是否有效
        return LOS_EBADF;  // 返回文件描述符错误
    }

    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前任务
    if ((LosTaskCB *)(rwlock->writeOwner) == runTask) {  // 检查当前任务是否持有写锁
        return LOS_EINVAL;  // 返回无效参数错误
    }

    /*
     * When the rwlock mode is read mode or free mode and the priority of the current read task
     * is lower than the first pended write task, current read task can not obtain the rwlock.
     */
    if ((rwlock->rwCount >= 0) && !OsRwlockPriCompare(runTask, &(rwlock->writeList))) {  // 优先级检查
        return LOS_EBUSY;  // 返回忙错误
    }

    /*
     * When the rwlock mode is write mode, current read task can not obtain the rwlock.
     */
    if (rwlock->rwCount < 0) {  // 写模式
        return LOS_EBUSY;  // 返回忙错误
    }

    return OsRwlockRdPendOp(runTask, rwlock, timeout);  // 执行读锁等待操作
}

/**
 * @brief 不安全的写锁获取（无调度保护）
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsRwlockWrUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {  // 检查锁是否有效
        return LOS_EBADF;  // 返回文件描述符错误
    }

    return OsRwlockWrPendOp(OsCurrTaskGet(), rwlock, timeout);  // 执行写锁等待操作
}

/**
 * @brief 不安全的尝试获取写锁（无调度保护）
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsRwlockTryWrUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {  // 检查锁是否有效
        return LOS_EBADF;  // 返回文件描述符错误
    }

    /* When the rwlock is read mode, current write task will be pended. */
    if (rwlock->rwCount > 0) {  // 读模式
        return LOS_EBUSY;  // 返回忙错误
    }

    /* When other write task obtains this rwlock, current write task will be pended. */
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前任务
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) != runTask)) {  // 其他任务持有写锁
        return LOS_EBUSY;  // 返回忙错误
    }

    return OsRwlockWrPendOp(runTask, rwlock, timeout);  // 执行写锁等待操作
}

/**
 * @brief 获取读锁
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_RwlockRdLock(LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 intSave;  // 中断状态保存变量

    UINT32 ret = OsRwlockCheck(rwlock);  // 参数检查
    if (ret != LOS_OK) {  // 检查失败
        return ret;  // 返回错误码
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    ret = OsRwlockRdUnsafe(rwlock, timeout);  // 不安全的读锁获取
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
}

/**
 * @brief 尝试获取读锁
 * @param rwlock 读写锁指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_RwlockTryRdLock(LosRwlock *rwlock)
{
    UINT32 intSave;  // 中断状态保存变量

    UINT32 ret = OsRwlockCheck(rwlock);  // 参数检查
    if (ret != LOS_OK) {  // 检查失败
        return ret;  // 返回错误码
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    ret = OsRwlockTryRdUnsafe(rwlock, 0);  // 不安全的尝试获取读锁
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
}

/**
 * @brief 获取写锁
 * @param rwlock 读写锁指针
 * @param timeout 超时时间
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_RwlockWrLock(LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 intSave;  // 中断状态保存变量

    UINT32 ret = OsRwlockCheck(rwlock);  // 参数检查
    if (ret != LOS_OK) {  // 检查失败
        return ret;  // 返回错误码
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    ret = OsRwlockWrUnsafe(rwlock, timeout);  // 不安全的写锁获取
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
}

/**
 * @brief 尝试获取写锁
 * @param rwlock 读写锁指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_RwlockTryWrLock(LosRwlock *rwlock)
{
    UINT32 intSave;  // 中断状态保存变量

    UINT32 ret = OsRwlockCheck(rwlock);  // 参数检查
    if (ret != LOS_OK) {  // 检查失败
        return ret;  // 返回错误码
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    ret = OsRwlockTryWrUnsafe(rwlock, 0);  // 不安全的尝试获取写锁
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return ret;  // 返回结果
}

/**
 * @brief 获取读写锁模式
 * @param readList 读等待链表
 * @param writeList 写等待链表
 * @return 锁模式（无/读/写/写优先/读优先）
 */
STATIC UINT32 OsRwlockGetMode(LOS_DL_LIST *readList, LOS_DL_LIST *writeList)
{
    BOOL isReadEmpty = LOS_ListEmpty(readList);  // 读等待链表是否为空
    BOOL isWriteEmpty = LOS_ListEmpty(writeList);  // 写等待链表是否为空
    if (isReadEmpty && isWriteEmpty) {  // 均为空
        return RWLOCK_NONE_MODE;  // 无模式
    }
    if (!isReadEmpty && isWriteEmpty) {  // 读非空，写空
        return RWLOCK_READ_MODE;  // 读模式
    }
    if (isReadEmpty && !isWriteEmpty) {  // 读空，写非空
        return RWLOCK_WRITE_MODE;  // 写模式
    }
    LosTaskCB *pendedReadTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(readList));  // 获取第一个读等待任务
    LosTaskCB *pendedWriteTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(writeList));  // 获取第一个写等待任务
    if (OsSchedParamCompare(pendedWriteTask, pendedReadTask) <= 0) {  // 写任务优先级高
        return RWLOCK_WRITEFIRST_MODE;  // 写优先模式
    }
    return RWLOCK_READFIRST_MODE;  // 读优先模式
}

/**
 * @brief 读写锁释放后唤醒等待任务
 * @param rwlock 读写锁指针
 * @param needSched 是否需要调度的输出标志
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsRwlockPostOp(LosRwlock *rwlock, BOOL *needSched)
{
    UINT32 rwlockMode;  // 锁模式
    LosTaskCB *resumedTask = NULL;  // 要唤醒的任务

    rwlock->rwCount = 0;  // 重置读写计数器
    rwlock->writeOwner = NULL;  // 重置写锁持有者
    rwlockMode = OsRwlockGetMode(&(rwlock->readList), &(rwlock->writeList));  // 获取锁模式
    if (rwlockMode == RWLOCK_NONE_MODE) {  // 无等待任务
        return LOS_OK;  // 返回成功
    }
    /* In this case, rwlock will wake the first pended write task. */
    if ((rwlockMode == RWLOCK_WRITE_MODE) || (rwlockMode == RWLOCK_WRITEFIRST_MODE)) {  // 写模式或写优先
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->writeList)));  // 获取第一个写等待任务
        rwlock->rwCount = -1;  // 设置为写模式
        rwlock->writeOwner = (VOID *)resumedTask;  // 设置写锁持有者
        resumedTask->ops->wake(resumedTask);  // 唤醒任务
        if (needSched != NULL) {  // 需要调度标志有效
            *needSched = TRUE;  // 设置需要调度
        }
        return LOS_OK;  // 返回成功
    }

    rwlock->rwCount = 1;  // 设置读计数为1
    resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->readList)));  // 获取第一个读等待任务
    resumedTask->ops->wake(resumedTask);  // 唤醒任务
    while (!LOS_ListEmpty(&(rwlock->readList))) {  // 遍历读等待链表
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->readList)));  // 获取下一个读等待任务
        if (rwlockMode == RWLOCK_READFIRST_MODE) {  // 读优先模式
            LosTaskCB *pendedWriteTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->writeList)));  // 获取第一个写等待任务
            if (OsSchedParamCompare(resumedTask, pendedWriteTask) >= 0) {  // 读任务优先级不低于写任务
                break;  // 停止唤醒
            }
        }
        if (rwlock->rwCount == INT8_MAX) {  // 读计数达到上限
            return EINVAL;  // 返回无效参数错误
        }
        rwlock->rwCount++;  // 增加读计数
        resumedTask->ops->wake(resumedTask);  // 唤醒任务
    }
    if (needSched != NULL) {  // 需要调度标志有效
        *needSched = TRUE;  // 设置需要调度
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 不安全的释放读写锁（无调度保护）
 * @param rwlock 读写锁指针
 * @param needSched 是否需要调度的输出标志
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsRwlockUnlockUnsafe(LosRwlock *rwlock, BOOL *needSched)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {  // 检查锁是否有效
        return LOS_EBADF;  // 返回文件描述符错误
    }

    if (rwlock->rwCount == 0) {  // 锁未被持有
        return LOS_EPERM;  // 返回权限错误
    }

    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前任务
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) != runTask)) {  // 非写锁持有者释放写锁
        return LOS_EPERM;  // 返回权限错误
    }

    /*
     * When the rwCount of the rwlock more than 1 or less than -1, the rwlock mode will
     * not changed after current unlock operation, so pended tasks can not be waken.
     */
    if (rwlock->rwCount > 1) {  // 读锁重入
        rwlock->rwCount--;  // 减少读计数
        return LOS_OK;  // 返回成功
    }

    if (rwlock->rwCount < -1) {  // 写锁重入
        rwlock->rwCount++;  // 增加写计数
        return LOS_OK;  // 返回成功
    }

    return OsRwlockPostOp(rwlock, needSched);  // 唤醒等待任务
}

/**
 * @brief 释放读写锁
 * @param rwlock 读写锁指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_RwlockUnLock(LosRwlock *rwlock)
{
    UINT32 intSave;  // 中断状态保存变量
    BOOL needSched = FALSE;  // 是否需要调度标志

    UINT32 ret = OsRwlockCheck(rwlock);  // 参数检查
    if (ret != LOS_OK) {  // 检查失败
        return ret;  // 返回错误码
    }

    SCHEDULER_LOCK(intSave);  // 关调度器
    ret = OsRwlockUnlockUnsafe(rwlock, &needSched);  // 不安全的释放锁
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    LOS_MpSchedule(OS_MP_CPU_ALL);  // 多处理器调度
    if (needSched == TRUE) {  // 需要调度
        LOS_Schedule();  // 任务调度
    }
    return ret;  // 返回结果
}

#endif /* LOSCFG_BASE_IPC_RWLOCK */

