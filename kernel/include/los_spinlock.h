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

#ifndef _LOS_SPINLOCK_H
#define _LOS_SPINLOCK_H
#include "los_typedef.h"
#include "los_config.h"
#include "los_hwi.h"
#include "los_task.h"
#include "los_lockdep.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/******************************************************************************
概述
	在多核环境中，由于使用相同的内存空间，存在对同一资源进行访问的情况，所以需要互斥
	访问机制来保证同一时刻只有一个核进行操作。自旋锁就是这样的一种机制。

	自旋锁是指当一个线程在获取锁时，如果锁已经被其它线程获取，那么该线程将循环等待，
	并不断判断是否能够成功获取锁，直到获取到锁才会退出循环。因此建议保护耗时较短的操作，
	防止对系统整体性能有明显的影响。

	自旋锁与互斥锁比较类似，它们都是为了解决对共享资源的互斥使用问题。无论是互斥锁，
	还是自旋锁，在任何时刻，最多只能有一个持有者。但是两者在调度机制上略有不同，对于互斥锁，
	如果锁已经被占用，锁申请者会被阻塞；但是自旋锁不会引起调用者阻塞，会一直循环检测自旋锁是否已经被释放。
	
使用场景
	自旋锁可以提供任务之间的互斥访问机制，用来防止两个任务在同一时刻访问相同的共享资源。

自旋锁的开发典型流程
	自旋锁依赖于SMP，可以通过make menuconfig配置。
	创建自旋锁：使用LOS_SpinInit初始化自旋锁，或者使用SPIN_LOCK_INIT初始化静态内存的自旋锁。
	申请自旋锁：使用接口LOS_SpinLock/LOS_SpinTrylock/LOS_SpinLockSave申请指定的自旋锁，
		申请成功就继续往后执行锁保护的代码；申请失败在自旋锁申请中忙等，直到申请到自旋锁为止。
	释放自旋锁：使用LOS_SpinUnlock/LOS_SpinUnlockRestore接口释放自旋锁。锁保护代码执行完毕后，
		释放对应的自旋锁，以便其他核申请自旋锁。

注意事项
	同一个任务不能对同一把自旋锁进行多次加锁，否则会导致死锁。
	自旋锁中会执行本核的锁任务操作，因此需要等到最外层完成解锁后本核才会进行任务调度。
	LOS_SpinLock与LOS_SpinUnlock允许单独使用，即可以不进行关中断，但是用户需要保证使用的接口
	只会在任务或中断中使用。如果接口同时会在任务和中断中被调用，请使用LOS_SpinLockSave
	与LOS_SpinUnlockRestore，因为在未关中断的情况下使用LOS_SpinLock可能会导致死锁。
	耗时的操作谨慎选用自旋锁，可使用互斥锁进行保护。
	未开启SMP的单核场景下，自旋锁功能无效，只有LOS_SpinLockSave与LOS_SpinUnlockRestore接口有关闭恢复中断功能。
	建议LOS_SpinLock和LOS_SpinUnlock，LOS_SpinLockSave和LOS_SpinUnlockRestore配对使用，避免出错。
参考
	https://gitee.com/LiteOS/LiteOS/blob/master/doc/Huawei_LiteOS_Kernel_Developer_Guide_zh.md#setup
******************************************************************************/

extern VOID ArchSpinLock(size_t *lock);
extern VOID ArchSpinUnlock(size_t *lock);
extern INT32 ArchSpinTrylock(size_t *lock);

typedef struct Spinlock {
    size_t      rawLock;
#if (LOSCFG_KERNEL_SMP_LOCKDEP == YES)
    UINT32      cpuid;
    VOID        *owner;
    const CHAR  *name;
#endif
} SPIN_LOCK_S;

#if (LOSCFG_KERNEL_SMP_LOCKDEP == YES)
#define SPINLOCK_OWNER_INIT     NULL

#define LOCKDEP_CHECK_IN(lock)  OsLockDepCheckIn(lock)
#define LOCKDEP_RECORD(lock)    OsLockDepRecord(lock)
#define LOCKDEP_CHECK_OUT(lock) OsLockDepCheckOut(lock)
#define LOCKDEP_CLEAR_LOCKS()   OsLockdepClearSpinlocks()

#define SPIN_LOCK_INITIALIZER(lockName) \
{                                       \
    .rawLock    = 0U,                   \
    .cpuid      = (UINT32)(-1),         \
    .owner      = SPINLOCK_OWNER_INIT,  \
    .name       = #lockName,            \
}
#else
#define LOCKDEP_CHECK_IN(lock)
#define LOCKDEP_RECORD(lock)
#define LOCKDEP_CHECK_OUT(lock)
#define LOCKDEP_CLEAR_LOCKS()
#define SPIN_LOCK_INITIALIZER(lockName) \
{                                       \
    .rawLock    = 0U,                   \
}
#endif
//静态初始化自旋锁
#define SPIN_LOCK_INIT(lock)  SPIN_LOCK_S lock = SPIN_LOCK_INITIALIZER(lock)

#if (LOSCFG_KERNEL_SMP == YES)
/**
 * @ingroup  los_spinlock
 * @brief Lock the spinlock.
 *
 * @par Description:
 * This API is used to lock the spin lock.
 *
 * @attention None.
 *
 * @param  lock [IN] Type #SPIN_LOCK_S spinlock pointer.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_spinlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SpinUnlock
 */	//申请指定的自旋锁，如果无法获取锁，会一直循环等待
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinLock(SPIN_LOCK_S *lock)
{
    /*
     * disable the scheduler, so it won't do schedule untill
     * scheduler is reenabled. The LOS_TaskUnlock should not
     * be directly called along this critic area.
     */
    LOS_TaskLock();

    LOCKDEP_CHECK_IN(lock);
    ArchSpinLock(&lock->rawLock);
    LOCKDEP_RECORD(lock);
}

/**
 * @ingroup  los_spinlock
 * @brief Trying lock the spinlock.
 *
 * @par Description:
 * This API is used to trying lock the spin lock.
 *
 * @attention None.
 *
 * @param  lock [IN] Type #SPIN_LOCK_S spinlock pointer.
 *
 * @retval LOS_OK   Got the spinlock.
 * @retval LOS_NOK  Not getting the spinlock.
 * @par Dependency:
 * <ul><li>los_spinlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SpinLock
 */	//尝试申请指定的自旋锁，如果无法获取锁，直接返回失败，而不会一直循环等待
LITE_OS_SEC_ALW_INLINE STATIC INLINE INT32 LOS_SpinTrylock(SPIN_LOCK_S *lock)
{
    LOS_TaskLock();

    INT32 ret = ArchSpinTrylock(&lock->rawLock);
    if (ret == LOS_OK) {
        LOCKDEP_CHECK_IN(lock);
        LOCKDEP_RECORD(lock);
    } else {
        LOS_TaskUnlock();
    }

    return ret;
}

/**
 * @ingroup  los_spinlock
 * @brief Unlock the spinlock.
 *
 * @par Description:
 * This API is used to unlock the spin lock.
 *
 * @attention None.
 *
 * @param  lock [IN] Type #SPIN_LOCK_S spinlock pointer.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_spinlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SpinLock
 */	//释放指定的自旋锁
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinUnlock(SPIN_LOCK_S *lock)
{
    LOCKDEP_CHECK_OUT(lock);
    ArchSpinUnlock(&lock->rawLock);//注意ArchSpinUnlock是一个汇编函数 见于 los_dispatch.s

    /* restore the scheduler flag */
    LOS_TaskUnlock();
}

/**
 * @ingroup  los_spinlock
 * @brief Lock the spinlock and disable all interrupts.
 *
 * @par Description:
 * This API is used to lock the spin lock and disable all interrupts.
 *
 * @attention None.
 *
 * @param  lock     [IN]    Type #SPIN_LOCK_S spinlock pointer.
 * @param  intSave  [OUT]   Type #UINT32 Saved interrupt flag for latter restored.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_spinlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SpinLock
 */	//关中断后，再申请指定的自旋锁
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinLockSave(SPIN_LOCK_S *lock, UINT32 *intSave)
{//自旋锁是不切换任务上下文的,忙等状态.锁的是细颗粒的操作,代码量小,运行时间极短的临界区
    *intSave = LOS_IntLock();
    LOS_SpinLock(lock);
}

/**
 * @ingroup  los_spinlock
 * @brief Unlock the spinlock and restore interrupt flag.
 *
 * @par Description:
 * This API is used to unlock the spin lock and restore interrupt flag.
 *
 * @attention None.
 *
 * @param  lock     [IN]    Type #SPIN_LOCK_S spinlock pointer.
 * @param  intSave  [IN]    Type #UINT32 Interrupt flag to be restored.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_spinlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SpinUnlock
 */	//先释放指定的自旋锁，再恢复中断状态
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinUnlockRestore(SPIN_LOCK_S *lock, UINT32 intSave)
{
    LOS_SpinUnlock(lock);
    LOS_IntRestore(intSave);
}

/**
 * @ingroup  los_spinlock
 * @brief Check if holding the spinlock.
 *
 * @par Description:
 * This API is used to check if holding the spinlock.
 *
 * @attention None.
 *
 * @param  lock     [IN]    Type #SPIN_LOCK_S spinlock pointer.
 *
 * @retval TRUE   Holding the spinlock.
 * @retval FALSE  Not Holding the spinlock.
 * @par Dependency:
 * <ul><li>los_spinlock.h: the header file that contains the API declaration.</li></ul>
 */ //检查自旋锁是否已经被持有
LITE_OS_SEC_ALW_INLINE STATIC INLINE BOOL LOS_SpinHeld(const SPIN_LOCK_S *lock)
{
    return (lock->rawLock != 0);
}

/**
 * @ingroup  los_spinlock
 * @brief Spinlock initialization.
 *
 * @par Description:
 * This API is used to initialize a spinlock.
 *
 * @attention None.
 *
 * @param  lock     [IN]    Type #SPIN_LOCK_S spinlock pointer.
 *
 * @retval None.
 *
 * @par Dependency:
 * <ul><li>los_spinlock.h: the header file that contains the API declaration.</li></ul>
 */	//动态初始化自旋锁
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinInit(SPIN_LOCK_S *lock)
{
    lock->rawLock     = 0;
#if (LOSCFG_KERNEL_SMP_LOCKDEP == YES)
    lock->cpuid       = (UINT32)-1;
    lock->owner       = SPINLOCK_OWNER_INIT;
    lock->name        = "spinlock";
#endif
}

#else

/*
 * For Non-SMP system, these apis does not handle with spinlocks,
 * but for unifying the code of drivers, vendors and etc.
 */	//申请指定的自旋锁，如果无法获取锁，会一直循环等待
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinLock(SPIN_LOCK_S *lock)
{
    (VOID)lock;
}
//尝试申请指定的自旋锁，如果无法获取锁，直接返回失败，而不会一直循环等待
LITE_OS_SEC_ALW_INLINE STATIC INLINE INT32 LOS_SpinTrylock(SPIN_LOCK_S *lock)
{
    (VOID)lock;
    return LOS_OK;
}
//释放指定的自旋锁
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinUnlock(SPIN_LOCK_S *lock)
{
    (VOID)lock;
}
//自旋锁是不切换任务上下文的,忙等状态.锁的是细颗粒的操作,代码量小,运行时间极短的临界区
//关中断后，再申请指定的自旋锁
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinLockSave(SPIN_LOCK_S *lock, UINT32 *intSave)
{
    (VOID)lock;
    *intSave = LOS_IntLock();
}
//先释放指定的自旋锁，再恢复中断状态
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinUnlockRestore(SPIN_LOCK_S *lock, UINT32 intSave)
{
    (VOID)lock;
    LOS_IntRestore(intSave);
}
//检查自旋锁是否已经被持有
LITE_OS_SEC_ALW_INLINE STATIC INLINE BOOL LOS_SpinHeld(const SPIN_LOCK_S *lock)
{
    (VOID)lock;
    return TRUE;
}
////动态初始化自旋锁
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinInit(SPIN_LOCK_S *lock)
{
    (VOID)lock;
}

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
