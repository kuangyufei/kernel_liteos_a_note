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

BOOL LOS_RwlockIsValid(const LosRwlock *rwlock)
{
    if ((rwlock != NULL) && ((rwlock->magic & RWLOCK_COUNT_MASK) == OS_RWLOCK_MAGIC)) {
        return TRUE;
    }

    return FALSE;
}

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

STATIC UINT32 OsRwlockCheck(LosRwlock *rwlock)
{
    if (rwlock == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    /* DO NOT Call blocking API in system tasks */
    LosTaskCB *runTask = (LosTaskCB *)OsCurrTaskGet();
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        return LOS_EPERM;
    }

    return LOS_OK;
}

STATIC BOOL OsRwlockPriCompare(LosTaskCB *runTask, LOS_DL_LIST *rwList)
{
    if (!LOS_ListEmpty(rwList)) {
        LosTaskCB *highestTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(rwList));
        if (runTask->priority < highestTask->priority) {
            return TRUE;
        }
        return FALSE;
    }
    return TRUE;
}

STATIC UINT32 OsRwlockRdPendOp(LosTaskCB *runTask, LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 ret;

    /*
     * When the rwlock mode is read mode or free mode and the priority of the current read task
     * is higher than the first pended write task. current read task can obtain this rwlock.
     */
    if (rwlock->rwCount >= 0) {
        if (OsRwlockPriCompare(runTask, &(rwlock->writeList))) {
            if (rwlock->rwCount == INT8_MAX) {
                return LOS_EINVAL;
            }
            rwlock->rwCount++;
            return LOS_OK;
        }
    }

    if (!timeout) {
        return LOS_EINVAL;
    }

    if (!OsPreemptableInSched()) {
        return LOS_EDEADLK;
    }

    /* The current task is not allowed to obtain the write lock when it obtains the read lock. */
    if ((LosTaskCB *)(rwlock->writeOwner) == runTask) {
        return LOS_EINVAL;
    }

    /*
     * When the rwlock mode is write mode or the priority of the current read task
     * is lower than the first pended write task, current read task will be pended.
     */
    LOS_DL_LIST *node = OsSchedLockPendFindPos(runTask, &(rwlock->readList));
    ret = OsSchedTaskWait(node, timeout, TRUE);
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {
        return LOS_ETIMEDOUT;
    }

    return ret;
}

STATIC UINT32 OsRwlockWrPendOp(LosTaskCB *runTask, LosRwlock *rwlock, UINT32 timeout)
{
    UINT32 ret;

    /* When the rwlock is free mode, current write task can obtain this rwlock. */
    if (rwlock->rwCount == 0) {
        rwlock->rwCount = -1;
        rwlock->writeOwner = (VOID *)runTask;
        return LOS_OK;
    }

    /* Current write task can use one rwlock once again if the rwlock owner is it. */
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) == runTask)) {
        if (rwlock->rwCount == INT8_MIN) {
            return LOS_EINVAL;
        }
        rwlock->rwCount--;
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
     * write task will be pended.
     */
    LOS_DL_LIST *node =  OsSchedLockPendFindPos(runTask, &(rwlock->writeList));
    ret = OsSchedTaskWait(node, timeout, TRUE);
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

    /* When the rwlock is read mode, current write task will be pended. */
    if (rwlock->rwCount > 0) {
        return LOS_EBUSY;
    }

    /* When other write task obtains this rwlock, current write task will be pended. */
    LosTaskCB *runTask = OsCurrTaskGet();
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) != runTask)) {
        return LOS_EBUSY;
    }

    return OsRwlockWrPendOp(runTask, rwlock, timeout);
}

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

UINT32 LOS_RwlockTryRdLock(LosRwlock *rwlock)
{
    UINT32 intSave;

    UINT32 ret = OsRwlockCheck(rwlock);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsRwlockTryRdUnsafe(rwlock, 0);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

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

UINT32 LOS_RwlockTryWrLock(LosRwlock *rwlock)
{
    UINT32 intSave;

    UINT32 ret = OsRwlockCheck(rwlock);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsRwlockTryWrUnsafe(rwlock, 0);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

STATIC UINT32 OsRwlockGetMode(LOS_DL_LIST *readList, LOS_DL_LIST *writeList)
{
    BOOL isReadEmpty = LOS_ListEmpty(readList);
    BOOL isWriteEmpty = LOS_ListEmpty(writeList);
    if (isReadEmpty && isWriteEmpty) {
        return RWLOCK_NONE_MODE;
    }
    if (!isReadEmpty && isWriteEmpty) {
        return RWLOCK_READ_MODE;
    }
    if (isReadEmpty && !isWriteEmpty) {
        return RWLOCK_WRITE_MODE;
    }
    LosTaskCB *pendedReadTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(readList));
    LosTaskCB *pendedWriteTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(writeList));
    if (pendedWriteTask->priority <= pendedReadTask->priority) {
        return RWLOCK_WRITEFIRST_MODE;
    }
    return RWLOCK_READFIRST_MODE;
}

STATIC UINT32 OsRwlockPostOp(LosRwlock *rwlock, BOOL *needSched)
{
    UINT32 rwlockMode;
    LosTaskCB *resumedTask = NULL;
    UINT16 pendedWriteTaskPri;

    rwlock->rwCount = 0;
    rwlock->writeOwner = NULL;
    rwlockMode = OsRwlockGetMode(&(rwlock->readList), &(rwlock->writeList));
    if (rwlockMode == RWLOCK_NONE_MODE) {
        return LOS_OK;
    }
    /* In this case, rwlock will wake the first pended write task. */
    if ((rwlockMode == RWLOCK_WRITE_MODE) || (rwlockMode == RWLOCK_WRITEFIRST_MODE)) {
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->writeList)));
        rwlock->rwCount = -1;
        rwlock->writeOwner = (VOID *)resumedTask;
        OsSchedTaskWake(resumedTask);
        if (needSched != NULL) {
            *needSched = TRUE;
        }
        return LOS_OK;
    }
    /* In this case, rwlock will wake the valid pended read task. */
    if (rwlockMode == RWLOCK_READFIRST_MODE) {
        pendedWriteTaskPri = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->writeList)))->priority;
    }
    resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->readList)));
    rwlock->rwCount = 1;
    OsSchedTaskWake(resumedTask);
    while (!LOS_ListEmpty(&(rwlock->readList))) {
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(rwlock->readList)));
        if ((rwlockMode == RWLOCK_READFIRST_MODE) && (resumedTask->priority >= pendedWriteTaskPri)) {
            break;
        }
        if (rwlock->rwCount == INT8_MAX) {
            return EINVAL;
        }
        rwlock->rwCount++;
        OsSchedTaskWake(resumedTask);
    }
    if (needSched != NULL) {
        *needSched = TRUE;
    }
    return LOS_OK;
}

UINT32 OsRwlockUnlockUnsafe(LosRwlock *rwlock, BOOL *needSched)
{
    if ((rwlock->magic & RWLOCK_COUNT_MASK) != OS_RWLOCK_MAGIC) {
        return LOS_EBADF;
    }

    if (rwlock->rwCount == 0) {
        return LOS_EPERM;
    }

    LosTaskCB *runTask = OsCurrTaskGet();
    if ((rwlock->rwCount < 0) && ((LosTaskCB *)(rwlock->writeOwner) != runTask)) {
        return LOS_EPERM;
    }

    /*
     * When the rwCount of the rwlock more than 1 or less than -1, the rwlock mode will
     * not changed after current unlock operation, so pended tasks can not be waken.
     */
    if (rwlock->rwCount > 1) {
        rwlock->rwCount--;
        return LOS_OK;
    }

    if (rwlock->rwCount < -1) {
        rwlock->rwCount++;
        return LOS_OK;
    }

    return OsRwlockPostOp(rwlock, needSched);
}

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
    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (needSched == TRUE) {
        LOS_Schedule();
    }
    return ret;
}

#endif /* LOSCFG_BASE_IPC_RWLOCK */

