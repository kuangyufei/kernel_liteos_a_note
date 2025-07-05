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

/**
 * @def PRINT_BUF_SIZE
 * @brief 打印缓冲区大小
 * @note 用于存储锁依赖检查相关的日志信息
 */
#define PRINT_BUF_SIZE 256

/**
 * @def LOCKDEP_GET_NAME
 * @brief 获取锁名称的宏
 * @param lockDep 锁依赖结构指针
 * @param index 锁数组索引
 * @return 锁的名称字符串
 */
#define LOCKDEP_GET_NAME(lockDep, index)    (((SPIN_LOCK_S *)((lockDep)->heldLocks[(index)].lockPtr))->name)

/**
 * @def LOCKDEP_GET_ADDR
 * @brief 获取锁地址的宏
 * @param lockDep 锁依赖结构指针
 * @param index 锁数组索引
 * @return 锁的地址
 */
#define LOCKDEP_GET_ADDR(lockDep, index)    ((lockDep)->heldLocks[(index)].lockAddr)

/** 锁依赖检查可用状态原子变量 (1-可用, 0-繁忙) */
STATIC Atomic g_lockdepAvailable = 1;

/**
 * @brief 获取锁依赖检查的原子操作权限
 * @param intSave 用于保存中断状态的变量指针
 * @return VOID
 * @note 禁用中断并通过忙等待确保原子访问
 */
STATIC INLINE VOID OsLockDepRequire(UINT32 *intSave)
{
    *intSave = LOS_IntLock();  // 禁用中断并保存状态
    // 原子比较交换：当g_lockdepAvailable为1时设置为0，否则忙等待
    while (LOS_AtomicCmpXchg32bits(&g_lockdepAvailable, 0, 1)) {
        /* busy waiting */  // 忙等待直到获取权限
    }
}

/**
 * @brief 释放锁依赖检查的原子操作权限
 * @param intSave 之前保存的中断状态
 * @return VOID
 * @note 恢复中断状态并标记锁依赖检查为可用
 */
STATIC INLINE VOID OsLockDepRelease(UINT32 intSave)
{
    LOS_AtomicSet(&g_lockdepAvailable, 1);  // 设置为可用状态
    LOS_IntRestore(intSave);                // 恢复中断状态
}

/**
 * @brief 获取当前CPU周期计数
 * @return UINT64 64位CPU周期计数值
 * @note 用于测量锁等待和持有时间
 */
STATIC INLINE UINT64 OsLockDepGetCycles(VOID)
{
    UINT32 high, low;                       // 高32位和低32位周期值

    LOS_GetCpuCycle(&high, &low);           // 获取CPU周期计数
    /* combine cycleHi and cycleLo into 8 bytes cycles */
    return (((UINT64)high << 32) + low);    // 组合为64位周期值
}

/**
 * @brief 将锁依赖错误类型转换为字符串
 * @param type 错误类型枚举值
 * @return CHAR* 错误描述字符串
 */
STATIC INLINE CHAR *OsLockDepErrorStringGet(enum LockDepErrType type)
{
    CHAR *errorString = NULL;               // 错误描述字符串指针

    switch (type) {
        case LOCKDEP_ERR_DOUBLE_LOCK:
            errorString = "double lock";  // 双重加锁错误
            break;
        case LOCKDEP_ERR_DEAD_LOCK:
            errorString = "dead lock";    // 死锁错误
            break;
        case LOCKDEP_ERR_UNLOCK_WITOUT_LOCK:
            errorString = "unlock without lock";  // 未加锁却解锁错误
            break;
        case LOCKDEP_ERR_OVERFLOW:
            errorString = "lockdep overflow";     // 锁依赖深度溢出
            break;
        default:
            errorString = "unknown error code";   // 未知错误
            break;
    }

    return errorString;                     // 返回错误描述字符串
}

/**
 * @brief 锁依赖错误处理函数（弱定义）
 * @param errType 错误类型
 * @return VOID
 * @note 默认实现：关中断、打印回溯并进入死循环
 */
WEAK VOID OsLockDepPanic(enum LockDepErrType errType)
{
    /* halt here */
    (VOID)errType;                          // 未使用的参数
    (VOID)LOS_IntLock();                    // 关闭中断
    OsBackTrace();                          // 打印函数调用回溯
    while (1) {}                            // 死循环挂起系统
}

/**
 * @brief 格式化输出锁依赖信息
 * @param fmt 格式化字符串
 * @param ap 可变参数列表
 * @return VOID
 * @note 使用UART输出格式化日志
 */
STATIC VOID OsLockDepPrint(const CHAR *fmt, va_list ap)
{
    UINT32 len;                             // 输出字符串长度
    CHAR buf[PRINT_BUF_SIZE] = {0};         // 打印缓冲区

    // 格式化输出到缓冲区
    len = vsnprintf_s(buf, PRINT_BUF_SIZE, PRINT_BUF_SIZE - 1, fmt, ap);
    if ((len == -1) && (*buf == '\0')) {
        /* 参数非法或不支持的格式化特性 */
        UartPuts("OsLockDepPrint is error\n", strlen("OsLockDepPrint is error\n"), 0);
        return;
    }
    *(buf + len) = '\0';                   // 添加字符串结束符

    UartPuts(buf, len, 0);                  // 通过UART输出
}

/**
 * @brief 打印锁依赖信息（可变参数包装）
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @return VOID
 */
STATIC VOID OsPrintLockDepInfo(const CHAR *fmt, ...)
{
    va_list ap;                             // 可变参数列表
    va_start(ap, fmt);                      // 初始化可变参数
    OsLockDepPrint(fmt, ap);                // 调用实际打印函数
    va_end(ap);                             // 清理可变参数
}

/**
 * @brief 转储锁依赖错误详细信息
 * @param task 任务控制块指针
 * @param lock 发生错误的锁指针
 * @param requestAddr 请求锁的代码地址
 * @param errType 错误类型
 * @return VOID
 * @note 打印任务信息、锁序列和错误详情，死锁时显示循环等待链
 */
STATIC VOID OsLockDepDumpLock(const LosTaskCB *task, const SPIN_LOCK_S *lock,
                              const VOID *requestAddr, enum LockDepErrType errType)
{
    INT32 i;                                // 循环计数器
    const LockDep *lockDep = &task->lockDep;// 锁依赖结构指针
    const LosTaskCB *temp = task;           // 临时任务指针（用于死锁链遍历）

    OsPrintLockDepInfo("lockdep check failed\n");
    OsPrintLockDepInfo("error type   : %s\n", OsLockDepErrorStringGet(errType));
    OsPrintLockDepInfo("request addr : 0x%x\n", requestAddr);

    while (1) {
        OsPrintLockDepInfo("task name    : %s\n", temp->taskName);
        OsPrintLockDepInfo("task id      : %u\n", temp->taskID);
        OsPrintLockDepInfo("cpu num      : %u\n", temp->currCpu);
        OsPrintLockDepInfo("start dumping lockdep information\n");
        // 打印当前持有的锁序列
        for (i = 0; i < lockDep->lockDepth; i++) {
            if (lockDep->heldLocks[i].lockPtr == lock) {
                OsPrintLockDepInfo("[%d] %s <-- addr:0x%x\n", i, LOCKDEP_GET_NAME(lockDep, i),
                                   LOCKDEP_GET_ADDR(lockDep, i));
            } else {
                OsPrintLockDepInfo("[%d] %s \n", i, LOCKDEP_GET_NAME(lockDep, i));
            }
        }
        OsPrintLockDepInfo("[%d] %s <-- now\n", i, lock->name);

        // 如果是死锁错误，遍历等待链
        if (errType == LOCKDEP_ERR_DEAD_LOCK) {
            temp = lock->owner;              // 获取当前锁的持有者
            lock = temp->lockDep.waitLock;   // 获取持有者等待的下一个锁
            lockDep = &temp->lockDep;        // 更新锁依赖结构指针
        }

        if (temp == task) {                  // 回到起始任务，退出循环
            break;
        }
    }
}

/**
 * @brief 检查锁依赖关系是否存在循环等待（死锁）
 * @param current 当前任务指针
 * @param lockOwner 锁持有者任务指针
 * @return BOOL TRUE-无死锁，FALSE-检测到死锁
 * @note 通过遍历锁持有者的等待链检测循环依赖
 */
STATIC BOOL OsLockDepCheckDependency(const LosTaskCB *current, LosTaskCB *lockOwner)
{
    BOOL checkResult = TRUE;                // 检查结果
    SPIN_LOCK_S *lockTemp = NULL;           // 临时锁指针

    do {
        if (current == lockOwner) {         // 检测到循环等待（死锁）
            checkResult = FALSE;
            return checkResult;
        }
        if (lockOwner->lockDep.waitLock != NULL) {
            lockTemp = lockOwner->lockDep.waitLock;  // 获取持有者等待的锁
            lockOwner = lockTemp->owner;             // 获取该锁的持有者
        } else {
            break;                          // 没有更多等待的锁，退出循环
        }
    } while (lockOwner != SPINLOCK_OWNER_INIT);

    return checkResult;                     // 返回检查结果
}

/**
 * @brief 锁获取前的依赖检查
 * @param lock 要获取的自旋锁指针
 * @return VOID
 * @note 检查双重加锁、死锁和深度溢出，通过后记录等待信息
 */
VOID OsLockDepCheckIn(SPIN_LOCK_S *lock)
{
    UINT32 intSave;                         // 中断状态保存变量
    enum LockDepErrType checkResult = LOCKDEP_SUCCESS;  // 检查结果
    VOID *requestAddr = (VOID *)__builtin_return_address(1);  // 获取调用者地址
    LosTaskCB *current = OsCurrTaskGet();  // 获取当前任务
    LockDep *lockDep = &current->lockDep;  // 当前任务的锁依赖结构
    LosTaskCB *lockOwner = NULL;            // 锁持有者任务

    OsLockDepRequire(&intSave);             // 获取锁依赖检查权限

    // 检查锁依赖深度是否溢出
    if (lockDep->lockDepth >= (INT32)MAX_LOCK_DEPTH) {
        checkResult = LOCKDEP_ERR_OVERFLOW;
        goto OUT;
    }

    lockOwner = lock->owner;                // 获取当前锁持有者
    /* 如果锁未被任何任务持有，无需进一步检查 */
    if (lockOwner == SPINLOCK_OWNER_INIT) {
        goto OUT;
    }

    // 检查是否为当前任务重复加锁
    if (current == lockOwner) {
        checkResult = LOCKDEP_ERR_DOUBLE_LOCK;
        goto OUT;
    }

    // 检查是否存在死锁依赖
    if (OsLockDepCheckDependency(current, lockOwner) != TRUE) {
        checkResult = LOCKDEP_ERR_DEAD_LOCK;
        goto OUT;
    }

OUT:
    if (checkResult == LOCKDEP_SUCCESS) {
        /*
         * 即使检查成功，仍需设置waitLock
         * 因为多核心环境下OsLockDepCheckIn和OsLockDepRecord并非严格顺序执行
         * 可能有多个任务通过检查，但只有一个能成功获取锁
         */
        lockDep->waitLock = lock;          // 记录等待的锁
        // 记录请求地址和开始等待时间
        lockDep->heldLocks[lockDep->lockDepth].lockAddr = requestAddr;
        lockDep->heldLocks[lockDep->lockDepth].waitTime = OsLockDepGetCycles(); /* start time */
        OsLockDepRelease(intSave);          // 释放检查权限
        return;
    }
    // 检查失败，转储错误信息并触发panic
    OsLockDepDumpLock(current, lock, requestAddr, checkResult);
    OsLockDepRelease(intSave);              // 释放检查权限
    OsLockDepPanic(checkResult);            // 处理错误（默认死循环）
}

/**
 * @brief 记录锁获取信息
 * @param lock 已获取的自旋锁指针
 * @return VOID
 * @note 更新锁持有者信息、计算等待时间并记录锁依赖
 */
VOID OsLockDepRecord(SPIN_LOCK_S *lock)
{
    UINT32 intSave;                         // 中断状态保存变量
    UINT64 cycles;                          // CPU周期计数
    LosTaskCB *current = OsCurrTaskGet();  // 获取当前任务
    LockDep *lockDep = &current->lockDep;  // 当前任务的锁依赖结构
    HeldLocks *heldlock = &lockDep->heldLocks[lockDep->lockDepth];  // 锁记录项

    OsLockDepRequire(&intSave);             // 获取锁依赖检查权限

    /*
     * OsLockDepCheckIn记录开始时间t1，获取锁后获取时间t2，(t2 - t1)为等待时间
     */
    cycles = OsLockDepGetCycles();          // 获取当前周期计数
    heldlock->waitTime = cycles - heldlock->waitTime;  // 计算等待时间
    heldlock->holdTime = cycles;            // 记录持有锁的开始时间

    /* 更新锁的持有者信息 */
    lock->owner = current;                  // 设置锁持有者为当前任务
    lock->cpuid = ArchCurrCpuid();          // 记录持有锁的CPU ID

    /* 更新锁依赖记录 */
    heldlock->lockPtr = lock;               // 记录锁指针
    lockDep->lockDepth++;                   // 增加锁深度计数
    lockDep->waitLock = NULL;               // 清除等待锁标记

    OsLockDepRelease(intSave);              // 释放检查权限
}

/**
 * @brief 锁释放时的依赖检查
 * @param lock 要释放的自旋锁指针
 * @return VOID
 * @note 检查解锁合法性，更新锁依赖记录并调整锁序列
 */
VOID OsLockDepCheckOut(SPIN_LOCK_S *lock)
{
    UINT32 intSave;                         // 中断状态保存变量
    INT32 depth;                            // 锁深度索引
    enum LockDepErrType checkResult;        // 检查结果
    VOID *requestAddr = (VOID *)__builtin_return_address(1);  // 获取调用者地址
    LosTaskCB *current = OsCurrTaskGet();  // 获取当前任务
    LosTaskCB *owner = NULL;                // 锁持有者任务
    LockDep *lockDep = NULL;                // 锁依赖结构指针
    HeldLocks *heldlocks = NULL;            // 持有锁数组指针

    OsLockDepRequire(&intSave);             // 获取锁依赖检查权限

    owner = lock->owner;                    // 获取锁持有者
    if (owner == SPINLOCK_OWNER_INIT) {     // 检查是否未加锁却解锁
        checkResult = LOCKDEP_ERR_UNLOCK_WITOUT_LOCK;
        OsLockDepDumpLock(current, lock, requestAddr, checkResult);
        OsLockDepRelease(intSave);          // 释放检查权限
        OsLockDepPanic(checkResult);        // 处理错误
        return;
    }

    lockDep = &owner->lockDep;              // 获取持有者的锁依赖结构
    heldlocks = &lockDep->heldLocks[0];     // 获取持有锁数组
    depth = lockDep->lockDepth;             // 获取当前锁深度

    /* 查找要释放的锁在数组中的位置 */
    while (--depth >= 0) {
        if (heldlocks[depth].lockPtr == lock) {
            break;
        }
    }

    LOS_ASSERT(depth >= 0);                 // 断言：必须找到对应的锁记录

    /* 计算并记录锁持有时间 */
    heldlocks[depth].holdTime = OsLockDepGetCycles() - heldlocks[depth].holdTime;

    /* 如果释放的不是最后一个锁，需要调整锁记录顺序 */
    while (depth < lockDep->lockDepth - 1) {
        lockDep->heldLocks[depth] = lockDep->heldLocks[depth + 1];  // 前移后续锁记录
        depth++;
    }

    lockDep->lockDepth--;                   // 减少锁深度计数
    lock->cpuid = (UINT32)(-1);             // 重置CPU ID
    lock->owner = SPINLOCK_OWNER_INIT;      // 重置锁持有者

    OsLockDepRelease(intSave);              // 释放检查权限
}

/**
 * @brief 释放当前任务持有的所有自旋锁
 * @return VOID
 * @note 用于任务异常终止时清理持有的锁资源
 */
VOID OsLockdepClearSpinlocks(VOID)
{
    LosTaskCB *task = OsCurrTaskGet();      // 获取当前任务
    LockDep *lockDep = &task->lockDep;      // 当前任务的锁依赖结构
    SPIN_LOCK_S *lock = NULL;               // 自旋锁指针

    /*
     * 释放当前任务持有的所有自旋锁
     * 每次解锁后lockDepth会递减
     */
    while (lockDep->lockDepth) {
        // 获取最后一个持有的锁
        lock = lockDep->heldLocks[lockDep->lockDepth - 1].lockPtr;
        LOS_SpinUnlock(lock);               // 解锁
    }
}

#else /* LOSCFG_KERNEL_SMP_LOCKDEP未定义时的空实现 */

/**
 * @brief 锁依赖检查空实现（禁用锁依赖时）
 * @param lock 自旋锁指针
 * @return VOID
 */
VOID OsLockDepCheckIn(SPIN_LOCK_S *lock)
{
    (VOID)lock;                             // 未使用的参数

    return;
}

/**
 * @brief 锁记录空实现（禁用锁依赖时）
 * @param lock 自旋锁指针
 * @return VOID
 */
VOID OsLockDepRecord(SPIN_LOCK_S *lock)
{
    (VOID)lock;                             // 未使用的参数

    return;
}

/**
 * @brief 锁释放检查空实现（禁用锁依赖时）
 * @param lock 自旋锁指针
 * @return VOID
 */
VOID OsLockDepCheckOut(SPIN_LOCK_S *lock)
{
    (VOID)lock;                             // 未使用的参数

    return;
}

/**
 * @brief 清理自旋锁空实现（禁用锁依赖时）
 * @return VOID
 */
VOID OsLockdepClearSpinlocks(VOID)
{
    return;
}

#endif

