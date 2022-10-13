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

#include "los_base.h"
#include "los_spinlock.h"
#include "los_task_pri.h"
#include "los_printf_pri.h"
#include "los_atomic.h"
#include "los_exc.h"

#ifdef LOSCFG_KERNEL_SMP_LOCKDEP

#define PRINT_BUF_SIZE 256

#define LOCKDEP_GET_NAME(lockDep, index)    (((SPIN_LOCK_S *)((lockDep)->heldLocks[(index)].lockPtr))->name)
#define LOCKDEP_GET_ADDR(lockDep, index)    ((lockDep)->heldLocks[(index)].lockAddr)

STATIC Atomic g_lockdepAvailable = 1;

/* atomic insurance for lockdep check */
STATIC INLINE VOID OsLockDepRequire(UINT32 *intSave)
{
    *intSave = LOS_IntLock();
    while (LOS_AtomicCmpXchg32bits(&g_lockdepAvailable, 0, 1)) {
        /* busy waiting */
    }
}

STATIC INLINE VOID OsLockDepRelease(UINT32 intSave)
{
    LOS_AtomicSet(&g_lockdepAvailable, 1);
    LOS_IntRestore(intSave);
}

STATIC INLINE UINT64 OsLockDepGetCycles(VOID)
{
    UINT32 high, low;

    LOS_GetCpuCycle(&high, &low);
    /* combine cycleHi and cycleLo into 8 bytes cycles */
    return (((UINT64)high << 32) + low); // 32 bits for lower half of UINT64
}

STATIC INLINE CHAR *OsLockDepErrorStringGet(enum LockDepErrType type)
{
    CHAR *errorString = NULL;

    switch (type) {
        case LOCKDEP_ERR_DOUBLE_LOCK:
            errorString = "double lock";
            break;
        case LOCKDEP_ERR_DEAD_LOCK:
            errorString = "dead lock";
            break;
        case LOCKDEP_ERR_UNLOCK_WITOUT_LOCK:
            errorString = "unlock without lock";
            break;
        case LOCKDEP_ERR_OVERFLOW:
            errorString = "lockdep overflow";
            break;
        default:
            errorString = "unknown error code";
            break;
    }

    return errorString;
}

WEAK VOID OsLockDepPanic(enum LockDepErrType errType)
{
    /* halt here */
    (VOID)errType;
    (VOID)LOS_IntLock();
    OsBackTrace();
    while (1) {}
}

STATIC VOID OsLockDepPrint(const CHAR *fmt, va_list ap)
{
    UINT32 len;
    CHAR buf[PRINT_BUF_SIZE] = {0};

    len = vsnprintf_s(buf, PRINT_BUF_SIZE, PRINT_BUF_SIZE - 1, fmt, ap);
    if ((len == -1) && (*buf == '\0')) {
        /* parameter is illegal or some features in fmt dont support */
        UartPuts("OsLockDepPrint is error\n", strlen("OsLockDepPrint is error\n"), 0);
        return;
    }
    *(buf + len) = '\0';

    UartPuts(buf, len, 0);
}

STATIC VOID OsPrintLockDepInfo(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsLockDepPrint(fmt, ap);
    va_end(ap);
}

STATIC VOID OsLockDepDumpLock(const LosTaskCB *task, const SPIN_LOCK_S *lock,
                              const VOID *requestAddr, enum LockDepErrType errType)
{
    INT32 i;
    const LockDep *lockDep = &task->lockDep;
    const LosTaskCB *temp = task;

    OsPrintLockDepInfo("lockdep check failed\n");
    OsPrintLockDepInfo("error type   : %s\n", OsLockDepErrorStringGet(errType));
    OsPrintLockDepInfo("request addr : 0x%x\n", requestAddr);

    while (1) {
        OsPrintLockDepInfo("task name    : %s\n", temp->taskName);
        OsPrintLockDepInfo("task id      : %u\n", temp->taskID);
        OsPrintLockDepInfo("cpu num      : %u\n", temp->currCpu);
        OsPrintLockDepInfo("start dumping lockdep information\n");
        for (i = 0; i < lockDep->lockDepth; i++) {
            if (lockDep->heldLocks[i].lockPtr == lock) {
                OsPrintLockDepInfo("[%d] %s <-- addr:0x%x\n", i, LOCKDEP_GET_NAME(lockDep, i),
                                   LOCKDEP_GET_ADDR(lockDep, i));
            } else {
                OsPrintLockDepInfo("[%d] %s \n", i, LOCKDEP_GET_NAME(lockDep, i));
            }
        }
        OsPrintLockDepInfo("[%d] %s <-- now\n", i, lock->name);

        if (errType == LOCKDEP_ERR_DEAD_LOCK) {
            temp = lock->owner;
            lock = temp->lockDep.waitLock;
            lockDep = &temp->lockDep;
        }

        if (temp == task) {
            break;
        }
    }
}

STATIC BOOL OsLockDepCheckDependency(const LosTaskCB *current, LosTaskCB *lockOwner)
{
    BOOL checkResult = TRUE;
    SPIN_LOCK_S *lockTemp = NULL;

    do {
        if (current == lockOwner) {
            checkResult = FALSE;
            return checkResult;
        }
        if (lockOwner->lockDep.waitLock != NULL) {
            lockTemp = lockOwner->lockDep.waitLock;
            lockOwner = lockTemp->owner;
        } else {
            break;
        }
    } while (lockOwner != SPINLOCK_OWNER_INIT);

    return checkResult;
}

VOID OsLockDepCheckIn(SPIN_LOCK_S *lock)
{
    UINT32 intSave;
    enum LockDepErrType checkResult = LOCKDEP_SUCCESS;
    VOID *requestAddr = (VOID *)__builtin_return_address(1);
    LosTaskCB *current = OsCurrTaskGet();
    LockDep *lockDep = &current->lockDep;
    LosTaskCB *lockOwner = NULL;

    OsLockDepRequire(&intSave);

    if (lockDep->lockDepth >= (INT32)MAX_LOCK_DEPTH) {
        checkResult = LOCKDEP_ERR_OVERFLOW;
        goto OUT;
    }

    lockOwner = lock->owner;
    /* not owned by any tasks yet, not doing following checks */
    if (lockOwner == SPINLOCK_OWNER_INIT) {
        goto OUT;
    }

    if (current == lockOwner) {
        checkResult = LOCKDEP_ERR_DOUBLE_LOCK;
        goto OUT;
    }

    if (OsLockDepCheckDependency(current, lockOwner) != TRUE) {
        checkResult = LOCKDEP_ERR_DEAD_LOCK;
        goto OUT;
    }

OUT:
    if (checkResult == LOCKDEP_SUCCESS) {
        /*
         * though the check may succeed, the waitLock still need to be set.
         * because the OsLockDepCheckIn and OsLockDepRecord is not strictly multi-core
         * sequential, there would be more than two tasks can pass the checking, but
         * only one task can successfully obtain the lock.
         */
        lockDep->waitLock = lock;
        lockDep->heldLocks[lockDep->lockDepth].lockAddr = requestAddr;
        lockDep->heldLocks[lockDep->lockDepth].waitTime = OsLockDepGetCycles(); /* start time */
        OsLockDepRelease(intSave);
        return;
    }
    OsLockDepDumpLock(current, lock, requestAddr, checkResult);
    OsLockDepRelease(intSave);
    OsLockDepPanic(checkResult);
}

VOID OsLockDepRecord(SPIN_LOCK_S *lock)
{
    UINT32 intSave;
    UINT64 cycles;
    LosTaskCB *current = OsCurrTaskGet();
    LockDep *lockDep = &current->lockDep;
    HeldLocks *heldlock = &lockDep->heldLocks[lockDep->lockDepth];

    OsLockDepRequire(&intSave);

    /*
     * OsLockDepCheckIn records start time t1, after the lock is obtained, we
     * get the time t2, (t2 - t1) is the time of waiting for the lock
     */
    cycles = OsLockDepGetCycles();
    heldlock->waitTime = cycles - heldlock->waitTime;
    heldlock->holdTime = cycles;

    /* record lock info */
    lock->owner = current;
    lock->cpuid = ArchCurrCpuid();

    /* record lockdep info */
    heldlock->lockPtr = lock;
    lockDep->lockDepth++;
    lockDep->waitLock = NULL;

    OsLockDepRelease(intSave);
}

VOID OsLockDepCheckOut(SPIN_LOCK_S *lock)
{
    UINT32 intSave;
    INT32 depth;
    enum LockDepErrType checkResult;
    VOID *requestAddr = (VOID *)__builtin_return_address(1);
    LosTaskCB *current = OsCurrTaskGet();
    LosTaskCB *owner = NULL;
    LockDep *lockDep = NULL;
    HeldLocks *heldlocks = NULL;

    OsLockDepRequire(&intSave);

    owner = lock->owner;
    if (owner == SPINLOCK_OWNER_INIT) {
        checkResult = LOCKDEP_ERR_UNLOCK_WITOUT_LOCK;
        OsLockDepDumpLock(current, lock, requestAddr, checkResult);
        OsLockDepRelease(intSave);
        OsLockDepPanic(checkResult);
        return;
    }

    lockDep = &owner->lockDep;
    heldlocks = &lockDep->heldLocks[0];
    depth = lockDep->lockDepth;

    /* find the lock position */
    while (--depth >= 0) {
        if (heldlocks[depth].lockPtr == lock) {
            break;
        }
    }

    LOS_ASSERT(depth >= 0);

    /* record lock holding time */
    heldlocks[depth].holdTime = OsLockDepGetCycles() - heldlocks[depth].holdTime;

    /* if unlock an older lock, needs move heldLock records */
    while (depth < lockDep->lockDepth - 1) {
        lockDep->heldLocks[depth] = lockDep->heldLocks[depth + 1];
        depth++;
    }

    lockDep->lockDepth--;
    lock->cpuid = (UINT32)(-1);
    lock->owner = SPINLOCK_OWNER_INIT;

    OsLockDepRelease(intSave);
}

VOID OsLockdepClearSpinlocks(VOID)
{
    LosTaskCB *task = OsCurrTaskGet();
    LockDep *lockDep = &task->lockDep;
    SPIN_LOCK_S *lock = NULL;

    /*
     * Unlock spinlocks that running task has held.
     * lockDepth will decrease after each spinlock is unlockded.
     */
    while (lockDep->lockDepth) {
        lock = lockDep->heldLocks[lockDep->lockDepth - 1].lockPtr;
        LOS_SpinUnlock(lock);
    }
}

#else /* LOSCFG_KERNEL_SMP_LOCKDEP */

VOID OsLockDepCheckIn(SPIN_LOCK_S *lock)
{
    (VOID)lock;

    return;
}

VOID OsLockDepRecord(SPIN_LOCK_S *lock)
{
    (VOID)lock;

    return;
}

VOID OsLockDepCheckOut(SPIN_LOCK_S *lock)
{
    (VOID)lock;

    return;
}

VOID OsLockdepClearSpinlocks(VOID)
{
    return;
}

#endif

