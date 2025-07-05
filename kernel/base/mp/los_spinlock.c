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


/**
 * @brief 初始化自旋锁结构
 * @param lock [IN/OUT] 指向自旋锁结构的指针
 * @note 自旋锁初始化为未锁定状态，CPU ID 设为无效值，拥有者设为初始值
 */
VOID LOS_SpinInit(SPIN_LOCK_S *lock)
{
    lock->rawLock = 0;  // 原始锁值初始化为0（未锁定状态）
    lock->cpuid   = (UINT32)-1;  // CPU ID初始化为无效值
    lock->owner   = SPINLOCK_OWNER_INIT;  // 锁拥有者初始化为默认值
    lock->name    = "spinlock";  // 锁名称设置为默认字符串
}

/**
 * @brief 检查自旋锁是否被持有
 * @param lock [IN] 指向自旋锁结构的常量指针
 * @return BOOL - 锁被持有返回TRUE，否则返回FALSE
 */
BOOL LOS_SpinHeld(const SPIN_LOCK_S *lock)
{
    return (lock->rawLock != 0);  // 通过原始锁值判断锁状态
}

/**
 * @brief 获取自旋锁（阻塞式）
 * @param lock [IN/OUT] 指向自旋锁结构的指针
 * @note 获取锁前会禁用调度，获取后记录锁依赖信息
 */
VOID LOS_SpinLock(SPIN_LOCK_S *lock)
{
    UINT32 intSave = LOS_IntLock();  // 禁用中断并保存中断状态
    OsSchedLock();  // 禁用任务调度
    LOS_IntRestore(intSave);  // 恢复中断状态

    LOCKDEP_CHECK_IN(lock);  // 锁依赖检查（进入阶段）
    ArchSpinLock(&lock->rawLock);  // 架构相关的自旋锁获取实现
    LOCKDEP_RECORD(lock);  // 记录当前锁的依赖关系
}

/**
 * @brief 尝试获取自旋锁（非阻塞式）
 * @param lock [IN/OUT] 指向自旋锁结构的指针
 * @return INT32 - 成功获取返回LOS_OK，否则返回错误码
 * @note 获取失败时会检查是否需要重新调度
 */
INT32 LOS_SpinTrylock(SPIN_LOCK_S *lock)
{
    UINT32 intSave = LOS_IntLock();  // 禁用中断并保存中断状态
    OsSchedLock();  // 禁用任务调度
    LOS_IntRestore(intSave);  // 恢复中断状态

    INT32 ret = ArchSpinTrylock(&lock->rawLock);  // 尝试获取架构相关自旋锁
    if (ret == LOS_OK) {  // 判断是否成功获取锁
        LOCKDEP_CHECK_IN(lock);  // 锁依赖检查（进入阶段）
        LOCKDEP_RECORD(lock);  // 记录当前锁的依赖关系
        return ret;  // 返回成功状态
    }

    intSave = LOS_IntLock();  // 禁用中断并保存中断状态
    BOOL needSched = OsSchedUnlockResch();  // 解锁调度并检查是否需要重新调度
    LOS_IntRestore(intSave);  // 恢复中断状态
    if (needSched) {  // 判断是否需要触发调度
        LOS_Schedule();  // 执行任务调度
    }

    return ret;  // 返回尝试结果
}

/**
 * @brief 释放自旋锁
 * @param lock [IN/OUT] 指向自旋锁结构的指针
 * @note 释放锁后会检查是否需要重新调度
 */
VOID LOS_SpinUnlock(SPIN_LOCK_S *lock)
{
    UINT32 intSave;
    LOCKDEP_CHECK_OUT(lock);  // 锁依赖检查（退出阶段）
    ArchSpinUnlock(&lock->rawLock);  // 架构相关的自旋锁释放实现

    intSave = LOS_IntLock();  // 禁用中断并保存中断状态
    BOOL needSched = OsSchedUnlockResch();  // 解锁调度并检查是否需要重新调度
    LOS_IntRestore(intSave);  // 恢复中断状态
    if (needSched) {  // 判断是否需要触发调度
        LOS_Schedule();  // 执行任务调度
    }
}

/**
 * @brief 获取自旋锁并保存中断状态
 * @param lock [IN/OUT] 指向自旋锁结构的指针
 * @param intSave [OUT] 用于保存中断状态的指针
 * @note 此函数会禁用中断并保存状态，调用者需负责后续恢复
 */
VOID LOS_SpinLockSave(SPIN_LOCK_S *lock, UINT32 *intSave)
{
    *intSave = LOS_IntLock();  // 禁用中断并保存状态到指定指针
    OsSchedLock();  // 禁用任务调度

    LOCKDEP_CHECK_IN(lock);  // 锁依赖检查（进入阶段）
    ArchSpinLock(&lock->rawLock);  // 架构相关的自旋锁获取实现
    LOCKDEP_RECORD(lock);  // 记录当前锁的依赖关系
}

/**
 * @brief 释放自旋锁并恢复中断状态
 * @param lock [IN/OUT] 指向自旋锁结构的指针
 * @param intSave [IN] 需要恢复的中断状态
 * @note 需与LOS_SpinLockSave配对使用，恢复之前保存的中断状态
 */
VOID LOS_SpinUnlockRestore(SPIN_LOCK_S *lock, UINT32 intSave)
{
    LOCKDEP_CHECK_OUT(lock);  // 锁依赖检查（退出阶段）
    ArchSpinUnlock(&lock->rawLock);  // 架构相关的自旋锁释放实现

    BOOL needSched = OsSchedUnlockResch();  // 解锁调度并检查是否需要重新调度
    LOS_IntRestore(intSave);  // 恢复之前保存的中断状态
    if (needSched) {  // 判断是否需要触发调度
        LOS_Schedule();  // 执行任务调度
    }
}
#endif

