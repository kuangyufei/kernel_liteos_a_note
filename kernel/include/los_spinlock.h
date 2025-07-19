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

extern VOID ArchSpinLock(size_t *lock);
extern VOID ArchSpinUnlock(size_t *lock);
extern INT32 ArchSpinTrylock(size_t *lock);
/**
 * @brief 自旋锁结构体定义
 * @details 用于实现多处理器环境下的同步机制，确保临界区资源的互斥访问
 */
typedef struct Spinlock {
    size_t      rawLock;                  // 自旋锁原始状态值，0表示未锁定，1表示已锁定
#ifdef LOSCFG_KERNEL_SMP                  // 若启用SMP（对称多处理）配置
    UINT32      cpuid;                    // 持有锁的CPU核心ID
    VOID        *owner;                   // 持有锁的任务/线程指针
    const CHAR  *name;                    // 自旋锁名称，用于调试和跟踪
#endif
} SPIN_LOCK_S;                            // 自旋锁结构体类型定义

#ifdef LOSCFG_KERNEL_SMP_LOCKDEP          // 若启用SMP锁依赖检查功能
/**
 * @brief 锁依赖检查进入函数
 * @param[in] lock 自旋锁指针
 */
#define LOCKDEP_CHECK_IN(lock)  OsLockDepCheckIn(lock)  // 记录锁的获取
/**
 * @brief 锁依赖记录函数
 * @param[in] lock 自旋锁指针
 */
#define LOCKDEP_RECORD(lock)    OsLockDepRecord(lock)    // 记录锁的依赖关系
/**
 * @brief 锁依赖检查退出函数
 * @param[in] lock 自旋锁指针
 */
#define LOCKDEP_CHECK_OUT(lock) OsLockDepCheckOut(lock)  // 验证锁的释放
/**
 * @brief 清除所有锁依赖记录
 */
#define LOCKDEP_CLEAR_LOCKS()   OsLockdepClearSpinlocks() // 清除自旋锁的依赖记录
#else                                     // 未启用锁依赖检查时
#define LOCKDEP_CHECK_IN(lock)                           // 空宏，不执行任何操作
#define LOCKDEP_RECORD(lock)                             // 空宏，不执行任何操作
#define LOCKDEP_CHECK_OUT(lock)                          // 空宏，不执行任何操作
#define LOCKDEP_CLEAR_LOCKS()                            // 空宏，不执行任何操作
#endif

#ifdef LOSCFG_KERNEL_SMP                  // 若启用SMP配置
#define SPINLOCK_OWNER_INIT     NULL                      // 自旋锁持有者初始值（无持有者）

/**
 * @brief 自旋锁初始化宏
 * @param[in] lockName 自旋锁变量名
 * @details 初始化自旋锁结构体成员，设置初始状态为未锁定
 */
#define SPIN_LOCK_INITIALIZER(lockName) \
{                                       \
    .rawLock    = 0U,                   \ // 初始化为未锁定状态
    .cpuid      = (UINT32)(-1),         \ // 初始CPU ID为无效值 
    .owner      = SPINLOCK_OWNER_INIT,  \ // 初始无持有者 
    .name       = #lockName,            \ // 设置锁名称为变量名 
}

/**
 * @ingroup  los_spinlock
 * @brief 锁定自旋锁
 *
 * @par 功能描述
 * 该API用于获取指定的自旋锁，若锁已被占用则忙等待（自旋）直到获取成功
 *
 * @attention
 * <ul><li>自旋锁用于SMP系统中短时间的临界区保护，持有期间不可睡眠</li>
 * <li>调用者必须确保在获取锁后尽快释放，避免长时间阻塞其他CPU</li></ul>
 *
 * @param  lock [IN] 类型 #SPIN_LOCK_S*，自旋锁结构体指针
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_spinlock.h: 包含该API声明的头文件</li></ul>
 * @see LOS_SpinUnlock
 */
extern VOID LOS_SpinLock(SPIN_LOCK_S *lock);  // 锁定自旋锁

/**
 * @ingroup  los_spinlock
 * @brief 尝试锁定自旋锁
 *
 * @par 功能描述
 * 该API尝试获取自旋锁，若锁未被占用则立即获取，否则返回失败，不会阻塞等待
 *
 * @attention 与LOS_SpinLock不同，此函数不会自旋等待，适合非阻塞场景
 *
 * @param  lock [IN] 类型 #SPIN_LOCK_S*，自旋锁结构体指针
 *
 * @retval LOS_OK   成功获取自旋锁
 * @retval LOS_NOK  自旋锁已被占用，获取失败
 * @par 依赖
 * <ul><li>los_spinlock.h: 包含该API声明的头文件</li></ul>
 * @see LOS_SpinLock
 */
extern INT32 LOS_SpinTrylock(SPIN_LOCK_S *lock);  // 尝试锁定自旋锁

/**
 * @ingroup  los_spinlock
 * @brief 解锁自旋锁
 *
 * @par 功能描述
 * 该API用于释放指定的自旋锁，允许其他等待的CPU获取该锁
 *
 * @attention
 * <ul><li>必须由持有锁的CPU释放，否则会导致未定义行为</li>
 * <li>释放前需确保已退出临界区，避免数据竞争</li></ul>
 *
 * @param  lock [IN] 类型 #SPIN_LOCK_S*，自旋锁结构体指针
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_spinlock.h: 包含该API声明的头文件</li></ul>
 * @see LOS_SpinLock
 */
extern VOID LOS_SpinUnlock(SPIN_LOCK_S *lock);  // 解锁自旋锁

/**
 * @ingroup  los_spinlock
 * @brief 锁定自旋锁并禁用所有中断
 *
 * @par 功能描述
 * 该API先禁用当前CPU的所有中断，然后获取自旋锁，提供最高级别的临界区保护
 *
 * @attention
 * <ul><li>中断禁用会影响系统响应性，必须最小化持有时间</li>
 * <li>intSave参数用于保存中断状态，必须在解锁时传入LOS_SpinUnlockRestore恢复</li></ul>
 *
 * @param  lock     [IN]  类型 #SPIN_LOCK_S*，自旋锁结构体指针
 * @param  intSave  [OUT] 类型 #UINT32*，用于保存中断标志的变量指针
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_spinlock.h: 包含该API声明的头文件</li></ul>
 * @see LOS_SpinLock | LOS_SpinUnlockRestore
 */
extern VOID LOS_SpinLockSave(SPIN_LOCK_S *lock, UINT32 *intSave);  // 锁定自旋锁并禁用中断

/**
 * @ingroup  los_spinlock
 * @brief 解锁自旋锁并恢复中断标志
 *
 * @par 功能描述
 * 该API释放自旋锁，并恢复之前由LOS_SpinLockSave保存的中断状态
 *
 * @attention
 * <ul><li>intSave参数必须是由LOS_SpinLockSave获取的值，不可随意修改</li>
 * <li>必须与LOS_SpinLockSave配对使用，确保中断正确恢复</li></ul>
 *
 * @param  lock     [IN] 类型 #SPIN_LOCK_S*，自旋锁结构体指针
 * @param  intSave  [IN] 类型 #UINT32，由LOS_SpinLockSave保存的中断标志
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_spinlock.h: 包含该API声明的头文件</li></ul>
 * @see LOS_SpinUnlock | LOS_SpinLockSave
 */
extern VOID LOS_SpinUnlockRestore(SPIN_LOCK_S *lock, UINT32 intSave);  // 解锁自旋锁并恢复中断

/**
 * @ingroup  los_spinlock
 * @brief 检查当前CPU是否持有自旋锁
 *
 * @par 功能描述
 * 该API判断当前CPU是否持有指定的自旋锁，用于死锁检测和调试
 *
 * @attention 仅在SMP配置下有效，非SMP系统始终返回TRUE
 *
 * @param  lock [IN] 类型 const #SPIN_LOCK_S*，自旋锁结构体指针
 *
 * @retval TRUE   当前CPU持有该自旋锁
 * @retval FALSE  当前CPU未持有该自旋锁
 * @par 依赖
 * <ul><li>los_spinlock.h: 包含该API声明的头文件</li></ul>
 */
extern BOOL LOS_SpinHeld(const SPIN_LOCK_S *lock);  // 检查是否持有自旋锁

/**
 * @ingroup  los_spinlock
 * @brief 初始化自旋锁
 *
 * @par 功能描述
 * 该API用于动态初始化自旋锁结构体，设置初始状态为未锁定
 *
 * @attention 对于静态定义的自旋锁，建议使用SPIN_LOCK_INITIALIZER宏初始化
 *
 * @param  lock [IN] 类型 #SPIN_LOCK_S*，需要初始化的自旋锁结构体指针
 *
 * @retval 无
 *
 * @par 依赖
 * <ul><li>los_spinlock.h: 包含该API声明的头文件</li></ul>
 */
extern VOID LOS_SpinInit(SPIN_LOCK_S *lock);  // 初始化自旋锁

#else  // 未启用SMP（对称多处理）配置时的实现
/**
 * @brief 非SMP系统的自旋锁初始化器
 * @details 在单CPU系统中，自旋锁仅需初始化原始锁状态
 */
#define SPIN_LOCK_INITIALIZER(lockName) \
{                                       \
    .rawLock    = 0U,                   \  // 初始化为未锁定状态 
}

/*
 * 非SMP系统中，这些API不执行实际的自旋锁操作，
 * 仅为统一驱动、厂商等代码接口而存在
 */
/**
 * @brief 非SMP系统的自旋锁锁定函数（空实现）
 * @details 单CPU系统无需自旋等待，直接返回
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinLock(SPIN_LOCK_S *lock)
{
    (VOID)lock;  // 未使用参数，避免编译警告
}

/**
 * @brief 非SMP系统的自旋锁尝试锁定函数（空实现）
 * @details 单CPU系统始终返回成功
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE INT32 LOS_SpinTrylock(SPIN_LOCK_S *lock)
{
    (VOID)lock;  // 未使用参数，避免编译警告
    return LOS_OK;  // 单CPU系统中总是能获取锁
}

/**
 * @brief 非SMP系统的自旋锁解锁函数（空实现）
 * @details 单CPU系统无需实际释放操作
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinUnlock(SPIN_LOCK_S *lock)
{
    (VOID)lock;  // 未使用参数，避免编译警告
}

/**
 * @brief 非SMP系统的自旋锁锁定并保存中断函数
 * @details 单CPU系统仅需禁用中断，无需自旋
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinLockSave(SPIN_LOCK_S *lock, UINT32 *intSave)
{
    (VOID)lock;  // 未使用参数，避免编译警告
    *intSave = LOS_IntLock();  // 禁用中断并保存中断状态
}

/**
 * @brief 非SMP系统的自旋锁解锁并恢复中断函数
 * @details 单CPU系统仅需恢复中断，无需实际解锁
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinUnlockRestore(SPIN_LOCK_S *lock, UINT32 intSave)
{
    (VOID)lock;  // 未使用参数，避免编译警告
    LOS_IntRestore(intSave);  // 恢复中断状态
}

/**
 * @brief 非SMP系统的自旋锁持有检查函数
 * @details 单CPU系统始终返回TRUE
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE BOOL LOS_SpinHeld(const SPIN_LOCK_S *lock)
{
    (VOID)lock;  // 未使用参数，避免编译警告
    return TRUE;  // 单CPU系统中始终认为持有锁
}

/**
 * @brief 非SMP系统的自旋锁初始化函数（空实现）
 * @details 单CPU系统无需实际初始化操作
 */
LITE_OS_SEC_ALW_INLINE STATIC INLINE VOID LOS_SpinInit(SPIN_LOCK_S *lock)
{
    (VOID)lock;  // 未使用参数，避免编译警告
}

#endif  // LOSCFG_KERNEL_SMP条件编译结束

/**
 * @brief 自旋锁静态初始化宏
 * @param[in] lock 自旋锁变量名
 * @details 定义并初始化一个自旋锁变量，等价于：SPIN_LOCK_S lock = SPIN_LOCK_INITIALIZER(lock)
 * @note 该宏用于静态变量定义，不适用于动态分配的内存
 */
#define SPIN_LOCK_INIT(lock)  SPIN_LOCK_S lock = SPIN_LOCK_INITIALIZER(lock)  // 自旋锁静态初始化宏

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
