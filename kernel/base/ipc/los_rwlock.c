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

#include "los_rwlock_pri.h"
#include "stdint.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_task_pri.h"
#include "los_exc.h"
#include "los_sched_pri.h"


#ifdef LOSCFG_BASE_IPC_RWLOCK
#define RWLOCK_COUNT_MASK 0x00FFFFFFU
/// 判断读写锁有效性
BOOL LOS_RwlockIsValid(const LosRwlock *rwlock)
{
    if ((rwlock != NULL) && ((rwlock->magic & RWLOCK_COUNT_MASK) == OS_RWLOCK_MAGIC)) {
        return TRUE;
    }

    return FALSE;
}
/// 创建读写锁,初始化锁信息
UINT32 LOS_RwlockInit(LosRwlock *rwlock)
{
    UINT32 intSave;

    if (rwlock == NULL) {
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    if ((rwlock->magic & RWLOCK_COUNT_MASK) == OS_RWLOCK_MAGIC) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EPERM;
    }

    rwlock->rwCount = 0;
    rwlock->writeOwner = NULL;
    LOS_ListInit(&(rwlock->readList));
    LOS_ListInit(&(rwlock->writeList));
    rwlock->magic = OS_RWLOCK_MAGIC;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
/// 删除指定的读写锁
UINT32 LOS_RwlockDestroy(LosRwlock *rwlock)
{
    UINT32 intSave;

    if (rwlock == NULL) {
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EBADF;
    }

    if (rwlock->rwCount != 0) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EBUSY;
    }

    (VOID)memset_s(rwlock, sizeof(LosRwlock), 0, sizeof(LosRwlock));
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
/// 读写锁检查
STATIC UINT32 OsRwlockCheck(LosRwlock *rwlock)
{
    if (rwlock == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) { // 读写锁不能在中断服务程序中使用。请想想为什么 ?
        return LOS_EINTR;
    }

    /* DO NOT Call blocking API in system tasks | 系统任务不能使用读写锁 */
    LosTaskCB *runTask = (LosTaskCB *)OsCurrTaskGet();
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        return LOS_EPERM;
    }

    return LOS_OK;
}
/// 指定任务优先级优先级是否低于 写锁任务最高优先级   
STATIC BOOL OsRwlockPriCompare(LosTaskCB *runTask, LOS_DL_LIST *rwList)
{
    if (!LOS_ListEmpty(rwList)) {
        LosTaskCB *highestTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(rwList));//首个写锁任务优先级是最高的
        if (runTask->priority < highestTask->priority) {//如果当前任务优先级低于等待写锁任务
            return TRUE;
        }
        return FALSE;
    }
    return TRUE;
}
/* 申请读模式下的锁,分三种情况：  
1. 若无人持有锁，读任务可获得锁。
2. 若有人持有锁，读任务可获得锁，读取顺序按照任务优先级。
3. 若有人（非自己）持有写模式下的锁，则当前任务无法获得锁，直到写模式下的锁释放。
*/
STATIC UINT32 OsRwlockRdPendOp(LosTaskCB *runTask, LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 ret;

    /*
     * When the rwlock mode is read mode or free mode and the priority of the current read task
     * is higher than the first pended write task. current read task can obtain this rwlock.
     */
    if (rwlock->rwCount >= 0) {//第一和第二种情况
        if (OsRwlockPriCompare(runTask, &(rwlock->writeList))) {//读优先级低于写优先级,意思就是必须先写再读
            if (rwlock->rwCount == INT8_MAX) {//读锁任务达到上限
                return LOS_EINVAL;
            }
            rwlock->rwCount++;//拿读锁成功
            return LOS_OK;
        }
    }

    if (!timeout) {
        return LOS_EINVAL;
    }

    if (!OsPreemptableInSched()) {//不可抢占时
        return LOS_EDEADLK;
    }

    /* The current task is not allowed to obtain the write lock when it obtains the read lock. 
    | 当前任务在获得读锁时不允许获得写锁 */
    if ((LosTaskCB *)(rwlock->writeOwner) == runTask) { //拥有写锁任务是否为当前任务
        return LOS_EINVAL;
    }

    /*
     * When the rwlock mode is write mode or the priority of the current read task
     * is lower than the first pended write task, current read task will be pended.
     | 当 rwlock 模式为写模式或当前读任务的优先级低于第一个挂起的写任务时，当前读任务将被挂起。
     反正就是写锁任务优先
     */
    LOS_DL_LIST *node = OsSchedLockPendFindPos(runTask, &(rwlock->readList));//找到要挂入的位置
    //例如现有链表内任务优先级为 0 3 8 9 23 当前为 10 时, 返回的是 9 这个节点 
    ret = OsSchedTaskWait(node, timeout, TRUE);//从尾部插入读锁链表 由此变成了 0 3 8 9 10 23
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {
        return LOS_ETIMEDOUT;
    }

    return ret;
}
/// 申请写模式下的锁
STATIC UINT32 OsRwlockWrPendOp(LosTaskCB *runTask, LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 ret;

    /* When the rwlock is free mode, current write task can obtain this rwlock. 
	| 若该锁当前没有任务持有，或者持有该读模式下的锁的任务和申请该锁的任务为同一个任务，则申请成功，可立即获得写模式下的锁。*/
    if (rwlock->rwCount == 0) {
        rwlock->rwCount = -1;
        rwlock->writeOwner = (VOID *)runTask;//直接给当前进程锁
        return LOS_OK;
    }

    /* Current write task can use one rwlock once again if the rwlock owner is it. 
	| 如果 rwlock 拥有者是当前写入任务，则它可以再次使用该锁。*/
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) == runTask)) {
        if (rwlock->rwCount == INT8_MIN) {
            return LOS_EINVAL;
        }
        rwlock->rwCount--;//注意再次拥有算是两把写锁了.
        return LOS_OK;
    }

    if (!timeout) {
        return LOS_EINVAL;
    }

    if (!OsPreemptableInSched()) {
        return LOS_EDEADLK;
    }

    /*
     * When the rwlock is read mode or other write task obtains this rwlock, current
     * write task will be pended. | 当 rwlock 为读模式或其他写任务获得该 rwlock 时，当前的写任务将被挂起。直到读模式下的锁释放
     */
    LOS_DL_LIST *node =  OsSchedLockPendFindPos(runTask, &(rwlock->writeList));//找到要挂入的位置
    ret = OsSchedTaskWait(node, timeout, TRUE);//从尾部插入写锁链表
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {
        ret = LOS_ETIMEDOUT;
    }

    return ret;
}

UINT32 OsRwlockRdUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {
        return LOS_EBADF;
    }

    return OsRwlockRdPendOp(OsCurrTaskGet(), rwlock, timeout);
}

UINT32 OsRwlockTryRdUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {
        return LOS_EBADF;
    }

    LosTaskCB *runTask = OsCurrTaskGet();
    if ((LosTaskCB *)(rwlock->writeOwner) == runTask) {
        return LOS_EINVAL;
    }

    /*
     * When the rwlock mode is read mode or free mode and the priority of the current read task
     * is lower than the first pended write task, current read task can not obtain the rwlock.
     */
    if ((rwlock->rwCount >= 0) && !OsRwlockPriCompare(runTask, &(rwlock->writeList))) {
        return LOS_EBUSY;
    }

    /*
     * When the rwlock mode is write mode, current read task can not obtain the rwlock.
     */
    if (rwlock->rwCount < 0) {
        return LOS_EBUSY;
    }

    return OsRwlockRdPendOp(runTask, rwlock, timeout);
}

UINT32 OsRwlockWrUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {
        return LOS_EBADF;
    }

    return OsRwlockWrPendOp(OsCurrTaskGet(), rwlock, timeout);
}

UINT32 OsRwlockTryWrUnsafe(LosRwlock *rwlock, UINT32 timeout)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {
        return LOS_EBADF;
    }

    /* When the rwlock is read mode, current write task will be pended.
    | 当 rwlock 为读模式时，当前的写任务将被挂起。*/
    if (rwlock->rwCount > 0) {
        return LOS_EBUSY;
    }

    /* When other write task obtains this rwlock, current write task will be pended. 
	| 当其他写任务获得这个rwlock时，当前的写任务将被挂起。*/
    LosTaskCB *runTask = OsCurrTaskGet();
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) != runTask)) {
        return LOS_EBUSY;
    }

    return OsRwlockWrPendOp(runTask, rwlock, timeout);//
}
/// 申请指定的读模式下的锁
UINT32 LOS_RwlockRdLock(LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 intSave;

    UINT32 ret = OsRwlockCheck(rwlock);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsRwlockRdUnsafe(rwlock, timeout);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
/// 尝试申请指定的读模式下的锁
UINT32 LOS_RwlockTryRdLock(LosRwlock *rwlock)
{
    UINT32 intSave;

    UINT32 ret = OsRwlockCheck(rwlock);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsRwlockTryRdUnsafe(rwlock, 0);//所谓尝试就是没锁爷就返回,不等待,不纠结.当前任务也不会被挂起
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
/// 申请指定的写模式下的锁
UINT32 LOS_RwlockWrLock(LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 intSave;

    UINT32 ret = OsRwlockCheck(rwlock);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsRwlockWrUnsafe(rwlock, timeout);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
/// 尝试申请指定的写模式下的锁
UINT32 LOS_RwlockTryWrLock(LosRwlock *rwlock)
{
    UINT32 intSave;

    UINT32 ret = OsRwlockCheck(rwlock);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsRwlockTryWrUnsafe(rwlock, 0);//所谓尝试就是没锁爷就返回,不等待,不纠结.当前任务也不会被挂起
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
/// 获取读写锁模式
STATIC UINT32 OsRwlockGetMode(LOS_DL_LIST *readList, LOS_DL_LIST *writeList)
{
    BOOL isReadEmpty = LOS_ListEmpty(readList);
    BOOL isWriteEmpty = LOS_ListEmpty(writeList);
    if (isReadEmpty && isWriteEmpty) { //读写链表都没有内容
        return RWLOCK_NONE_MODE; //自由模式
    }
    if (!isReadEmpty && isWriteEmpty) { //读链表有数据,写链表没有数据
        return RWLOCK_READ_MODE;
    }
    if (isReadEmpty && !isWriteEmpty) { //写链表有数据,读链表没有数据
        return RWLOCK_WRITE_MODE;
    }
    LosTaskCB *pendedReadTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(readList));
    LosTaskCB *pendedWriteTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(writeList));
    if (pendedWriteTask->priority <= pendedReadTask->priority) { //都有数据比优先级
        return RWLOCK_WRITEFIRST_MODE; //写的优先级高时,为写优先模式
    }
    return RWLOCK_READFIRST_MODE; //读的优先级高时,为读优先模式
}
/// 释放锁
STATIC UINT32 OsRwlockPostOp(LosRwlock *rwlock, BOOL *needSched)
{
    UINT32 rwlockMode;
    LosTaskCB *resumedTask = NULL;
    UINT16 pendedWriteTaskPri;

    rwlock->rwCount = 0;
    rwlock->writeOwner = NULL;
    rwlockMode = OsRwlockGetMode(&(rwlock->readList), &(rwlock->writeList));//先获取模式
    if (rwlockMode == RWLOCK_NONE_MODE) {//自由模式则正常返回
        return LOS_OK;
    }
    /* In this case, rwlock will wake the first pended write task. | 在这种情况下，rwlock 将唤醒第一个挂起的写任务。 */
    if ((rwlockMode == RWLOCK_WRITE_MODE) || (rwlockMode == RWLOCK_WRITEFIRST_MODE)) {//如果当前是写模式 (有任务在等写锁涅)
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->writeList)));//获取任务实体
        rwlock->rwCount = -1;//直接干成-1,注意这里并不是 --
        rwlock->writeOwner = (VOID *)resumedTask;//有锁了则唤醒等锁的任务(写模式)
        OsSchedTaskWake(resumedTask);
        if (needSched != NULL) {
            *needSched = TRUE;
        }
        return LOS_OK;
    }
    /* In this case, rwlock will wake the valid pended read task. 在这种情况下，rwlock 将唤醒有效的挂起读取任务。 */
    if (rwlockMode == RWLOCK_READFIRST_MODE) { //如果是读优先模式
        pendedWriteTaskPri = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->writeList)))->priority;//取出写锁任务的最高优先级
    }
    resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->readList)));//获取最高优先级读锁任务
    rwlock->rwCount = 1; //直接干成1,因为是释放操作 
    OsSchedTaskWake(resumedTask);//有锁了则唤醒等锁的任务(读模式)
    while (!LOS_ListEmpty(&(rwlock->readList))) {//遍历读链表,目的是要唤醒其他读模式的任务(优先级得要高于pendedWriteTaskPri才行)
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->readList)));
        if ((rwlockMode == RWLOCK_READFIRST_MODE) && (resumedTask->priority >= pendedWriteTaskPri)) {//低于写模式的优先级
            break;//跳出循环
        }
        if (rwlock->rwCount == INT8_MAX) {
            return EINVAL;
        }
        rwlock->rwCount++;//读锁任务数量增加
        OsSchedTaskWake(resumedTask);//不断唤醒读锁任务,由此实现了允许多个读操作并发,因为在多核情况下resumedTask很大可能
        //与当前任务并不在同一个核上运行, 此处非常的有意思,点赞! @note_good
    }
    if (needSched != NULL) {
        *needSched = TRUE;
    }
    return LOS_OK;
}
/// 释放锁,唤醒任务
UINT32 OsRwlockUnlockUnsafe(LosRwlock *rwlock, BOOL *needSched)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {
        return LOS_EBADF;
    }

    if (rwlock->rwCount == 0) {
        return LOS_EPERM;
    }

    LosTaskCB *runTask = OsCurrTaskGet();
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) != runTask)) {//写模式时,当前任务未持有锁
        return LOS_EPERM;
    }

    /*
     * When the rwCount of the rwlock more than 1 or less than -1, the rwlock mode will
     * not changed after current unlock operation, so pended tasks can not be waken.
     | 当 rwlock 的 rwCount 大于 1 或小于 -1 时，当前解锁操作后 rwlock 模式不会改变，因此挂起的任务不能被唤醒。
     */
    if (rwlock->rwCount > 1) {//读模式
        rwlock->rwCount--;
        return LOS_OK;
    }

    if (rwlock->rwCount < -1) {//写模式
        rwlock->rwCount++;
        return LOS_OK;
    }

    return OsRwlockPostOp(rwlock, needSched);
}
/// 释放指定读写锁
UINT32 LOS_RwlockUnLock(LosRwlock *rwlock)
{
    UINT32 intSave;
    BOOL needSched = FALSE;

    UINT32 ret = OsRwlockCheck(rwlock);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsRwlockUnlockUnsafe(rwlock, &needSched);
    SCHEDULER_UNLOCK(intSave);
    LOS_MpSchedule(OS_MP_CPU_ALL);//设置调度CPU的方式,所有CPU参与调度
    if (needSched == TRUE) {//是否需要调度
        LOS_Schedule();//产生调度,切换任务执行
    }
    return ret;
}

#endif /* LOSCFG_BASE_IPC_RWLOCK */

