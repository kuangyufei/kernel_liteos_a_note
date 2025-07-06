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
/*!
 * @file    los_mux.c
 * @brief
 * @link kernel-mini-basic-ipc-mutex-guide http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-mini-basic-ipc-mutex-guide.html @endlink
   @verbatim
   基本概念
		互斥锁又称互斥型信号量，是一种特殊的二值性信号量，用于实现对共享资源的独占式处理。
		任意时刻互斥锁的状态只有两种，开锁或闭锁。当有任务持有时，互斥锁处于闭锁状态，这个任务获得该互斥锁的所有权。
		当该任务释放它时，该互斥锁被开锁，任务失去该互斥锁的所有权。当一个任务持有互斥锁时，其他任务将不能再对该互斥锁进行开锁或持有。
		多任务环境下往往存在多个任务竞争同一共享资源的应用场景，互斥锁可被用于对共享资源的保护从而实现独占式访问。
		另外互斥锁可以解决信号量存在的优先级翻转问题。
   
   运作机制
	   多任务环境下会存在多个任务访问同一公共资源的场景，而有些公共资源是非共享的临界资源，
	   只能被独占使用。互斥锁怎样来避免这种冲突呢？
	   用互斥锁处理临界资源的同步访问时，如果有任务访问该资源，则互斥锁为加锁状态。此时其他任务
	   如果想访问这个临界资源则会被阻塞，直到互斥锁被持有该锁的任务释放后，其他任务才能重新访问
	   该公共资源，此时互斥锁再次上锁，如此确保同一时刻只有一个任务正在访问这个临界资源，保证了
	   临界资源操作的完整性。
   
   使用场景
	   多任务环境下往往存在多个任务竞争同一临界资源的应用场景，互斥锁可以提供任务间的互斥机制，
	   防止两个任务在同一时刻访问相同的临界资源，从而实现独占式访问。
   
   申请互斥锁有三种模式：无阻塞模式、永久阻塞模式、定时阻塞模式。
	   无阻塞模式：任务需要申请互斥锁，若该互斥锁当前没有任务持有，或者持有该互斥锁的任务和申请
	   		该互斥锁的任务为同一个任务，则申请成功。
	   永久阻塞模式：任务需要申请互斥锁，若该互斥锁当前没有被占用，则申请成功。否则，该任务进入阻塞态，
	   		系统切换到就绪任务中优先级高者继续执行。任务进入阻塞态后，直到有其他任务释放该互斥锁，阻塞任务才会重新得以执行。
	   定时阻塞模式：任务需要申请互斥锁，若该互斥锁当前没有被占用，则申请成功。否则该任务进入阻塞态，
	   		系统切换到就绪任务中优先级高者继续执行。任务进入阻塞态后，指定时间超时前有其他任务释放该互斥锁，
	   		或者用户指定时间超时后，阻塞任务才会重新得以执行。 
	   释放互斥锁：
			如果有任务阻塞于该互斥锁，则唤醒被阻塞任务中优先级最高的，该任务进入就绪态，并进行任务调度。
			如果没有任务阻塞于该互斥锁，则互斥锁释放成功。
   
   互斥锁典型场景的开发流程：
	   通过make menuconfig配置互斥锁模块。
	   创建互斥锁LOS_MuxCreate。
	   申请互斥锁LOS_MuxPend。
	   释放互斥锁LOS_MuxPost。
	   删除互斥锁LOS_MuxDelete。

   @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/27/mux.png
 * @attention 两个任务不能对同一把互斥锁加锁。如果某任务对已被持有的互斥锁加锁，则该任务会被挂起，直到持有该锁的任务对互斥锁解锁，才能执行对这把互斥锁的加锁操作。
			  \n 互斥锁不能在中断服务程序中使用。
			  \n LiteOS-M内核作为实时操作系统需要保证任务调度的实时性，尽量避免任务的长时间阻塞，因此在获得互斥锁之后，应该尽快释放互斥锁。
			  \n 持有互斥锁的过程中，不得再调用LOS_TaskPriSet等接口更改持有互斥锁任务的优先级。  
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-18
 */
#include "los_mux_pri.h"
#include "los_bitmap.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_task_pri.h"
#include "los_exc.h"
#include "los_sched_pri.h"

#ifdef LOSCFG_BASE_IPC_MUX
#define MUTEXATTR_TYPE_MASK 0x0FU  // 互斥锁属性类型掩码，用于提取type字段的低4位

/**
 * @brief 初始化互斥锁属性结构
 * @details 将互斥锁属性初始化为默认值：优先级继承协议、最低优先级上限、默认类型
 * @param attr 互斥锁属性指针
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrInit(LosMuxAttr *attr)
{
    if (attr == NULL) {  // 检查输入参数有效性
        return LOS_EINVAL;
    }

    attr->protocol    = LOS_MUX_PRIO_INHERIT;    // 设置优先级继承协议
    attr->prioceiling = OS_TASK_PRIORITY_LOWEST; // 设置最低优先级上限
    attr->type        = LOS_MUX_DEFAULT;         // 设置默认互斥锁类型
    return LOS_OK;
}

/**
 * @brief 销毁互斥锁属性结构
 * @details 目前为空实现，仅检查输入参数有效性
 * @param attr 互斥锁属性指针
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrDestroy(LosMuxAttr *attr)
{
    if (attr == NULL) {  // 检查输入参数有效性
        return LOS_EINVAL;
    }

    return LOS_OK;  // 销毁成功
}

/**
 * @brief 获取互斥锁属性的类型
 * @details 从属性结构中提取并验证互斥锁类型
 * @param attr 互斥锁属性指针
 * @param outType 输出参数，用于存储获取到的类型
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrGetType(const LosMuxAttr *attr, INT32 *outType)
{
    INT32 type;

    if ((attr == NULL) || (outType == NULL)) {  // 检查输入参数有效性
        return LOS_EINVAL;
    }

    type = (INT32)(attr->type & MUTEXATTR_TYPE_MASK);  // 提取类型字段（低4位）
    if ((type < LOS_MUX_NORMAL) || (type > LOS_MUX_ERRORCHECK)) {  // 验证类型范围
        return LOS_EINVAL;
    }

    *outType = type;  // 设置输出参数

    return LOS_OK;
}

/**
 * @brief 设置互斥锁属性的类型
 * @details 验证并设置互斥锁的类型（普通、递归或错误检查）
 * @param attr 互斥锁属性指针
 * @param type 要设置的类型
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrSetType(LosMuxAttr *attr, INT32 type)
{
    if ((attr == NULL) || (type < LOS_MUX_NORMAL) || (type > LOS_MUX_ERRORCHECK)) {  // 检查参数有效性
        return LOS_EINVAL;
    }

    // 保留其他位，仅更新类型字段
    attr->type = (UINT8)((attr->type & ~MUTEXATTR_TYPE_MASK) | (UINT32)type);
    return LOS_OK;
}

/**
 * @brief 获取互斥锁属性的优先级协议
 * @details 从属性结构中获取优先级协议（无、继承或保护）
 * @param attr 互斥锁属性指针
 * @param protocol 输出参数，用于存储获取到的协议
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrGetProtocol(const LosMuxAttr *attr, INT32 *protocol)
{
    if ((attr != NULL) && (protocol != NULL)) {  // 检查输入参数有效性
        *protocol = attr->protocol;  // 设置输出参数
    } else {
        return LOS_EINVAL;
    }

    return LOS_OK;
}

/**
 * @brief 设置互斥锁属性的优先级协议
 * @details 验证并设置互斥锁的优先级协议（无、继承或保护）
 * @param attr 互斥锁属性指针
 * @param protocol 要设置的协议
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrSetProtocol(LosMuxAttr *attr, INT32 protocol)
{
    if (attr == NULL) {  // 检查输入参数有效性
        return LOS_EINVAL;
    }

    switch (protocol) {  // 验证协议类型
        case LOS_MUX_PRIO_NONE:      // 无优先级协议
        case LOS_MUX_PRIO_INHERIT:   // 优先级继承协议
        case LOS_MUX_PRIO_PROTECT:   // 优先级保护协议
            attr->protocol = (UINT8)protocol;  // 设置协议
            return LOS_OK;
        default:  // 无效协议
            return LOS_EINVAL;
    }
}

/**
 * @brief 获取互斥锁属性的优先级上限
 * @details 从属性结构中获取优先级上限值
 * @param attr 互斥锁属性指针
 * @param prioceiling 输出参数，用于存储获取到的优先级上限
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrGetPrioceiling(const LosMuxAttr *attr, INT32 *prioceiling)
{
    if (attr == NULL) {  // 检查输入参数有效性
        return LOS_EINVAL;
    }

    if (prioceiling != NULL) {  // 设置输出参数（允许为NULL不获取值）
        *prioceiling = attr->prioceiling;
    }

    return LOS_OK;
}

/**
 * @brief 设置互斥锁属性的优先级上限
 * @details 验证并设置互斥锁的优先级上限值
 * @param attr 互斥锁属性指针
 * @param prioceiling 要设置的优先级上限
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrSetPrioceiling(LosMuxAttr *attr, INT32 prioceiling)
{
    if ((attr == NULL) ||
        (prioceiling < OS_TASK_PRIORITY_HIGHEST) ||  // 检查优先级范围
        (prioceiling > OS_TASK_PRIORITY_LOWEST)) {
        return LOS_EINVAL;
    }

    attr->prioceiling = (UINT8)prioceiling;  // 设置优先级上限

    return LOS_OK;
}

/**
 * @brief 设置互斥锁的优先级上限
 * @details 锁定互斥锁后更新其优先级上限，并返回旧的优先级上限
 * @param mutex 互斥锁指针
 * @param prioceiling 新的优先级上限
 * @param oldPrioceiling 输出参数，用于存储旧的优先级上限
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxSetPrioceiling(LosMux *mutex, INT32 prioceiling, INT32 *oldPrioceiling)
{
    INT32 ret;          // 函数返回值
    INT32 retLock;      // 锁定操作返回值
    if ((mutex == NULL) ||
        (prioceiling < OS_TASK_PRIORITY_HIGHEST) ||  // 检查参数有效性
        (prioceiling > OS_TASK_PRIORITY_LOWEST)) {
        return LOS_EINVAL;
    }

    retLock = LOS_MuxLock(mutex, LOS_WAIT_FOREVER);  // 永久等待锁定互斥锁
    if (retLock != LOS_OK) {
        return retLock;  // 锁定失败，返回错误
    }

    if (oldPrioceiling != NULL) {  // 保存旧的优先级上限（如果需要）
        *oldPrioceiling = mutex->attr.prioceiling;
    }

    ret = LOS_MuxAttrSetPrioceiling(&mutex->attr, prioceiling);  // 设置新的优先级上限

    retLock = LOS_MuxUnlock(mutex);  // 解锁互斥锁
    if ((ret == LOS_OK) && (retLock != LOS_OK)) {  // 如果设置成功但解锁失败，返回解锁错误
        return retLock;
    }

    return ret;  // 返回设置结果
}

/**
 * @brief 获取互斥锁的优先级上限
 * @details 从互斥锁结构中获取当前的优先级上限值
 * @param mutex 互斥锁指针
 * @param prioceiling 输出参数，用于存储获取到的优先级上限
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxGetPrioceiling(const LosMux *mutex, INT32 *prioceiling)
{
    // 检查互斥锁有效性和输出参数
    if ((mutex != NULL) && (prioceiling != NULL) && (mutex->magic == OS_MUX_MAGIC)) {
        *prioceiling = mutex->attr.prioceiling;  // 设置输出参数
        return LOS_OK;
    }

    return LOS_EINVAL;  // 参数无效或互斥锁未初始化
}

/**
 * @brief 检查互斥锁是否有效
 * @details 通过魔术字验证互斥锁是否已正确初始化
 * @param mutex 互斥锁指针
 * @return 有效返回TRUE，无效返回FALSE
 */
LITE_OS_SEC_TEXT BOOL LOS_MuxIsValid(const LosMux *mutex)
{
    if ((mutex != NULL) && (mutex->magic == OS_MUX_MAGIC)) {  // 检查指针和魔术字
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief 检查互斥锁属性的有效性
 * @details 验证互斥锁属性中的类型、优先级上限和协议是否在有效范围内
 * @param attr 互斥锁属性指针
 * @return 有效返回LOS_OK，无效返回LOS_NOK
 */
STATIC UINT32 OsCheckMutexAttr(const LosMuxAttr *attr)
{
    // 检查类型范围
    if (((INT8)(attr->type) < LOS_MUX_NORMAL) || (attr->type > LOS_MUX_ERRORCHECK)) {
        return LOS_NOK;
    }
    // 检查优先级上限范围
    if (((INT8)(attr->prioceiling) < OS_TASK_PRIORITY_HIGHEST) || (attr->prioceiling > OS_TASK_PRIORITY_LOWEST)) {
        return LOS_NOK;
    }
    // 检查协议范围
    if (((INT8)(attr->protocol) < LOS_MUX_PRIO_NONE) || (attr->protocol > LOS_MUX_PRIO_PROTECT)) {
        return LOS_NOK;
    }
    return LOS_OK;  // 属性有效
}

/**
 * @brief 初始化互斥锁
 * @details 根据属性初始化互斥锁结构，设置初始状态和魔术字
 * @param mutex 互斥锁指针
 * @param attr 互斥锁属性指针（NULL表示使用默认属性）
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxInit(LosMux *mutex, const LosMuxAttr *attr)
{
    UINT32 intSave;  // 中断状态保存变量

    if (mutex == NULL) {  // 检查互斥锁指针有效性
        return LOS_EINVAL;
    }

    if (attr == NULL) {  // 使用默认属性
        (VOID)LOS_MuxAttrInit(&mutex->attr);
    } else {  // 复制用户提供的属性
        (VOID)memcpy_s(&mutex->attr, sizeof(LosMuxAttr), attr, sizeof(LosMuxAttr));
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {  // 检查属性有效性
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    mutex->muxCount = 0;       // 初始化计数器
    mutex->owner = NULL;       // 初始无持有者
    LOS_ListInit(&mutex->muxList);  // 初始化等待列表
    mutex->magic = OS_MUX_MAGIC;    // 设置魔术字（标记为已初始化）
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return LOS_OK;
}

/**
 * @brief 销毁互斥锁
 * @details 验证互斥锁状态并清除其内容，仅允许销毁未锁定的互斥锁
 * @param mutex 互斥锁指针
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxDestroy(LosMux *mutex)
{
    UINT32 intSave;  // 中断状态保存变量

    if (mutex == NULL) {  // 检查互斥锁指针有效性
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (mutex->magic != OS_MUX_MAGIC) {  // 检查是否已初始化
        SCHEDULER_UNLOCK(intSave);
        return LOS_EBADF;
    }

    if (mutex->muxCount != 0) {  // 检查是否已锁定
        SCHEDULER_UNLOCK(intSave);
        return LOS_EBUSY;
    }

    (VOID)memset_s(mutex, sizeof(LosMux), 0, sizeof(LosMux));  // 清除互斥锁内容
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return LOS_OK;
}

/**
 * @brief 设置互斥锁的优先级继承位图
 * @details 当使用优先级继承协议时，提高持有互斥锁任务的优先级
 * @param mutex 互斥锁指针
 * @param runTask 当前运行任务控制块
 */
STATIC VOID OsMuxBitmapSet(const LosMux *mutex, const LosTaskCB *runTask)
{
    if (mutex->attr.protocol != LOS_MUX_PRIO_INHERIT) {  // 仅处理优先级继承协议
        return;
    }

    SchedParam param = { 0 };  // 调度参数
    LosTaskCB *owner = (LosTaskCB *)mutex->owner;  // 互斥锁持有者
    INT32 ret = OsSchedParamCompare(owner, runTask);  // 比较优先级
    if (ret > 0) {  // 如果持有者优先级低于当前任务
        runTask->ops->schedParamGet(runTask, &param);  // 获取当前任务优先级
        owner->ops->priorityInheritance(owner, &param);  // 提高持有者优先级
    }
}

/**
 * @brief 恢复互斥锁的优先级继承位图
 * @details 当使用优先级继承协议时，恢复持有互斥锁任务的原始优先级
 * @param mutex 互斥锁指针
 * @param list 任务列表
 * @param runTask 当前运行任务控制块
 */
VOID OsMuxBitmapRestore(const LosMux *mutex, const LOS_DL_LIST *list, const LosTaskCB *runTask)
{
    if (mutex->attr.protocol != LOS_MUX_PRIO_INHERIT) {  // 仅处理优先级继承协议
        return;
    }

    SchedParam param = { 0 };  // 调度参数
    LosTaskCB *owner = (LosTaskCB *)mutex->owner;  // 互斥锁持有者
    runTask->ops->schedParamGet(runTask, &param);  // 获取当前任务优先级
    owner->ops->priorityRestore(owner, list, &param);  // 恢复持有者原始优先级
}

/**
 * @brief 执行互斥锁等待操作
 * @details 处理互斥锁的获取逻辑，包括立即获取、递归获取和阻塞等待
 * @param runTask 当前运行任务控制块
 * @param mutex 互斥锁指针
 * @param timeout 超时时间（滴答数）
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
STATIC UINT32 OsMuxPendOp(LosTaskCB *runTask, LosMux *mutex, UINT32 timeout)
{
    UINT32 ret;  // 返回值

    // 检查等待列表是否未初始化（宏定义初始化情况）
    if ((mutex->muxList.pstPrev == NULL) || (mutex->muxList.pstNext == NULL)) {
        /* 这是针对互斥锁宏初始化的处理 */
        mutex->muxCount = 0;       // 重置计数器
        mutex->owner = NULL;       // 重置持有者
        LOS_ListInit(&mutex->muxList);  // 初始化等待列表
    }

    if (mutex->muxCount == 0) {  // 互斥锁未被持有
        mutex->muxCount++;         // 增加持有计数
        mutex->owner = (VOID *)runTask;  // 设置当前任务为持有者
        // 将互斥锁添加到任务的持有列表
        LOS_ListTailInsert(&runTask->lockList, &mutex->holdList);
        if (mutex->attr.protocol == LOS_MUX_PRIO_PROTECT) {  // 优先级保护协议
            SchedParam param = { 0 };
            runTask->ops->schedParamGet(runTask, &param);  // 获取当前调度参数
            param.priority = mutex->attr.prioceiling;  // 设置为优先级上限
            runTask->ops->priorityInheritance(runTask, &param);  // 应用优先级
        }
        return LOS_OK;  // 获取成功
    }

    // 递归互斥锁且当前任务为持有者
    if (((LosTaskCB *)mutex->owner == runTask) && (mutex->attr.type == LOS_MUX_RECURSIVE)) {
        mutex->muxCount++;  // 增加递归计数
        return LOS_OK;      // 获取成功
    }

    if (!timeout) {  // 非阻塞模式且获取失败
        return LOS_EINVAL;
    }

    if (!OsPreemptableInSched()) {  // 检查是否可抢占（避免死锁）
        return LOS_EDEADLK;
    }

    OsMuxBitmapSet(mutex, runTask);  // 设置优先级继承位图

    runTask->taskMux = (VOID *)mutex;  // 记录当前等待的互斥锁
    // 查找插入等待队列的位置
    LOS_DL_LIST *node = OsSchedLockPendFindPos(runTask, &mutex->muxList);
    if (node == NULL) {
        ret = LOS_NOK;
        return ret;
    }

    // 设置任务等待掩码和超时
    OsTaskWaitSetPendMask(OS_TASK_WAIT_MUTEX, (UINTPTR)mutex, timeout);
    ret = runTask->ops->wait(runTask, node, timeout);  // 阻塞等待
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {  // 等待超时
        OsMuxBitmapRestore(mutex, NULL, runTask);  // 恢复优先级位图
        runTask->taskMux = NULL;  // 清除等待的互斥锁
        ret = LOS_ETIMEDOUT;      // 返回超时错误
    }

    return ret;
}

/**
 * @brief 不安全的互斥锁锁定操作（无调度器锁）
 * @details 内部使用的互斥锁锁定函数，不进行调度器锁定
 * @param mutex 互斥锁指针
 * @param timeout 超时时间（滴答数）
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
UINT32 OsMuxLockUnsafe(LosMux *mutex, UINT32 timeout)
{
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前任务

    if (mutex->magic != OS_MUX_MAGIC) {  // 检查互斥锁是否已初始化
        return LOS_EBADF;
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {  // 检查属性有效性
        return LOS_EINVAL;
    }

    // 错误检查类型且当前任务已持有互斥锁（避免死锁）
    if ((mutex->attr.type == LOS_MUX_ERRORCHECK) && (mutex->owner == (VOID *)runTask)) {
        return LOS_EDEADLK;
    }

    return OsMuxPendOp(runTask, mutex, timeout);  // 执行等待操作
}

/**
 * @brief 不安全的互斥锁尝试锁定操作（无调度器锁）
 * @details 内部使用的互斥锁尝试锁定函数，不进行调度器锁定
 * @param mutex 互斥锁指针
 * @param timeout 超时时间（滴答数）
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
UINT32 OsMuxTrylockUnsafe(LosMux *mutex, UINT32 timeout)
{
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前任务

    if (mutex->magic != OS_MUX_MAGIC) {  // 检查互斥锁是否已初始化
        return LOS_EBADF;
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {  // 检查属性有效性
        return LOS_EINVAL;
    }

    // 如果互斥锁已被持有且不是当前任务或非递归类型
    if ((mutex->owner != NULL) &&
        (((LosTaskCB *)mutex->owner != runTask) || (mutex->attr.type != LOS_MUX_RECURSIVE))) {
        return LOS_EBUSY;  // 返回忙
    }

    return OsMuxPendOp(runTask, mutex, timeout);  // 执行等待操作
}

/**
 * @brief 锁定互斥锁
 * @details 外部接口，获取互斥锁，支持超时等待
 * @param mutex 互斥锁指针
 * @param timeout 超时时间（滴答数）
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxLock(LosMux *mutex, UINT32 timeout)
{
    LosTaskCB *runTask = NULL;  // 当前任务控制块
    UINT32 intSave;             // 中断状态保存变量
    UINT32 ret;                 // 返回值

    if (mutex == NULL) {  // 检查互斥锁指针有效性
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {  // 不允许在中断上下文中调用
        return LOS_EINTR;
    }

    runTask = (LosTaskCB *)OsCurrTaskGet();  // 获取当前任务
    /* 不允许在系统任务中调用阻塞API */
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        PRINTK("Warning: DO NOT call %s in system tasks.\n", __FUNCTION__);
        OsBackTrace();  // 打印回溯信息
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    ret = OsMuxLockUnsafe(mutex, timeout);  // 执行不安全锁定操作
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return ret;
}

/**
 * @brief 尝试锁定互斥锁
 * @details 外部接口，非阻塞方式获取互斥锁
 * @param mutex 互斥锁指针
 * @return 成功返回LOS_OK，失败返回相应错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxTrylock(LosMux *mutex)
{
    LosTaskCB *runTask = NULL;  // 当前任务控制块
    UINT32 intSave;             // 中断状态保存变量
    UINT32 ret;                 // 返回值

    if (mutex == NULL) {  // 检查互斥锁指针有效性
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {  // 不允许在中断上下文中调用
        return LOS_EINTR;
    }

    runTask = (LosTaskCB *)OsCurrTaskGet();  // 获取当前任务
    /* 不允许在系统任务中调用阻塞API */
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        PRINTK("Warning: DO NOT call %s in system tasks.\n", __FUNCTION__);
        OsBackTrace();  // 打印回溯信息
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    ret = OsMuxTrylockUnsafe(mutex, 0);  // 执行不安全尝试锁定操作（超时0）
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return ret;
}

/**
 * @brief 执行互斥锁释放操作
 * @details 处理互斥锁的释放逻辑，包括唤醒等待任务和优先级恢复
 * @param taskCB 任务控制块
 * @param mutex 互斥锁指针
 * @param needSched 是否需要调度的输出标志
 * @return 成功返回LOS_OK
 */
STATIC UINT32 OsMuxPostOp(LosTaskCB *taskCB, LosMux *mutex, BOOL *needSched)
{
    if (LOS_ListEmpty(&mutex->muxList)) {  // 等待列表为空
        LOS_ListDelete(&mutex->holdList);  // 从持有者列表中删除
        mutex->owner = NULL;               // 清除持有者
        return LOS_OK;
    }

    // 获取等待队列中的第一个任务
    LosTaskCB *resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(mutex->muxList)));
    OsMuxBitmapRestore(mutex, &mutex->muxList, resumedTask);  // 恢复优先级位图

    mutex->muxCount = 1;  // 设置持有计数为1
    mutex->owner = (VOID *)resumedTask;  // 设置新的持有者
    LOS_ListDelete(&mutex->holdList);    // 从旧持有者列表中删除
    // 添加到新持有者的持有列表
    LOS_ListTailInsert(&resumedTask->lockList, &mutex->holdList);
    OsTaskWakeClearPendMask(resumedTask);  // 清除等待掩码
    resumedTask->ops->wake(resumedTask);   // 唤醒任务
    resumedTask->taskMux = NULL;           // 清除任务的互斥锁等待标记
    if (needSched != NULL) {
        *needSched = TRUE;  // 设置需要调度标志
    }

    return LOS_OK;
}
#endif
/**
 * @brief 不安全地解锁互斥锁（无调度器锁保护）
 * @details 此函数用于在已持有调度器锁的情况下执行互斥锁解锁操作，完成互斥锁状态检查、递归计数处理、优先级恢复等核心逻辑
 * @param[in] taskCB 当前任务控制块指针
 * @param[in] mutex 互斥锁指针
 * @param[out] needSched 是否需要调度的标志指针
 * @return UINT32 操作结果
 * @retval LOS_OK 解锁成功
 * @retval LOS_EBADF 互斥锁魔数校验失败，无效的互斥锁
 * @retval LOS_EINVAL 互斥锁属性检查失败
 * @retval LOS_EPERM 调用者不是互斥锁所有者或互斥锁计数为0
 */
UINT32 OsMuxUnlockUnsafe(LosTaskCB *taskCB, LosMux *mutex, BOOL *needSched)
{
    if (mutex->magic != OS_MUX_MAGIC) {  // 验证互斥锁魔数，检查互斥锁有效性
        return LOS_EBADF;  // 返回无效文件错误码
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {  // 检查互斥锁属性是否合法
        return LOS_EINVAL;  // 返回无效参数错误码
    }

    if ((LosTaskCB *)mutex->owner != taskCB) {  // 验证当前任务是否为互斥锁所有者
        return LOS_EPERM;  // 返回权限错误码
    }

    if (mutex->muxCount == 0) {  // 检查互斥锁计数是否为0（已处于未加锁状态）
        return LOS_EPERM;  // 返回权限错误码
    }

    if ((--mutex->muxCount != 0) && (mutex->attr.type == LOS_MUX_RECURSIVE)) {  // 递减互斥锁计数，若为递归锁且计数未到0则直接返回
        return LOS_OK;  // 返回成功
    }

    if (mutex->attr.protocol == LOS_MUX_PRIO_PROTECT) {  // 检查是否为优先级保护协议互斥锁
        SchedParam param = { 0 };  // 定义调度参数结构体并初始化
        taskCB->ops->schedParamGet(taskCB, &param);  // 获取任务调度参数
        taskCB->ops->priorityRestore(taskCB, NULL, &param);  // 恢复任务原始优先级
    }

    return OsMuxPostOp(taskCB, mutex, needSched);  // 执行互斥锁释放后操作，唤醒等待任务
}

/**
 * @brief 互斥锁解锁接口
 * @details 提供互斥锁的用户态解锁接口，负责参数检查、调度器锁管理和必要的任务调度
 * @param[in] mutex 互斥锁指针
 * @return UINT32 操作结果
 * @retval LOS_OK 解锁成功
 * @retval LOS_EINVAL 互斥锁指针为空
 * @retval LOS_EINTR 在中断上下文中调用
 */
LITE_OS_SEC_TEXT UINT32 LOS_MuxUnlock(LosMux *mutex)
{
    LosTaskCB *runTask = NULL;  // 当前运行任务控制块指针
    BOOL needSched = FALSE;  // 是否需要调度的标志
    UINT32 intSave;  // 中断状态保存变量
    UINT32 ret;  // 函数返回值

    if (mutex == NULL) {  // 检查互斥锁指针是否为空
        return LOS_EINVAL;  // 返回无效参数错误码
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中调用
        return LOS_EINTR;  // 返回中断错误码
    }

    runTask = (LosTaskCB *)OsCurrTaskGet();  // 获取当前运行任务
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        PRINTK("Warning: DO NOT call %s in system tasks.\n", __FUNCTION__);  // 打印警告信息
        OsBackTrace();  // 输出堆栈跟踪
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器，保存中断状态
    ret = OsMuxUnlockUnsafe(runTask, mutex, &needSched);  // 调用不安全解锁函数完成核心逻辑
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器，恢复中断状态
    if (needSched == TRUE) {  // 检查是否需要调度
        LOS_MpSchedule(OS_MP_CPU_ALL);  // 触发多处理器调度
        LOS_Schedule();  // 执行任务调度
    }
    return ret;  // 返回操作结果
}

#endif /* LOSCFG_BASE_IPC_MUX */

