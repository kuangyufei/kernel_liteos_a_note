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

#include "los_spinlock.h"
#ifdef LOSCFG_KERNEL_SMP
#include "los_sched_pri.h"


VOID LOS_SpinInit(SPIN_LOCK_S *lock)
{
    lock->rawLock = 0;
    lock->cpuid   = (UINT32)-1;
    lock->owner   = SPINLOCK_OWNER_INIT;
    lock->name    = "spinlock";
}

BOOL LOS_SpinHeld(const SPIN_LOCK_S *lock)
{
    return (lock->rawLock != 0);
}

VOID LOS_SpinLock(SPIN_LOCK_S *lock)
{
    UINT32 intSave = LOS_IntLock();
    OsSchedLock();
    LOS_IntRestore(intSave);

    LOCKDEP_CHECK_IN(lock);
    ArchSpinLock(&lock->rawLock);
    LOCKDEP_RECORD(lock);
}

INT32 LOS_SpinTrylock(SPIN_LOCK_S *lock)
{
    UINT32 intSave = LOS_IntLock();
    OsSchedLock();
    LOS_IntRestore(intSave);

    INT32 ret = ArchSpinTrylock(&lock->rawLock);
    if (ret == LOS_OK) {
        LOCKDEP_CHECK_IN(lock);
        LOCKDEP_RECORD(lock);
        return ret;
    }

    intSave = LOS_IntLock();
    BOOL needSched = OsSchedUnlockResch();
    LOS_IntRestore(intSave);
    if (needSched) {
        LOS_Schedule();
    }

    return ret;
}

VOID LOS_SpinUnlock(SPIN_LOCK_S *lock)
{
    UINT32 intSave;
    LOCKDEP_CHECK_OUT(lock);
    ArchSpinUnlock(&lock->rawLock);

    intSave = LOS_IntLock();
    BOOL needSched = OsSchedUnlockResch();
    LOS_IntRestore(intSave);
    if (needSched) {
        LOS_Schedule();
    }
}

VOID LOS_SpinLockSave(SPIN_LOCK_S *lock, UINT32 *intSave)
{
    *intSave = LOS_IntLock();
    OsSchedLock();

    LOCKDEP_CHECK_IN(lock);
    ArchSpinLock(&lock->rawLock);
    LOCKDEP_RECORD(lock);
}

VOID LOS_SpinUnlockRestore(SPIN_LOCK_S *lock, UINT32 intSave)
{
    LOCKDEP_CHECK_OUT(lock);
    ArchSpinUnlock(&lock->rawLock);

    BOOL needSched = OsSchedUnlockResch();
    LOS_IntRestore(intSave);
    if (needSched) {
        LOS_Schedule();
    }
}
#endif

