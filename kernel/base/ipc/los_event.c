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
 * @file    los_event.c
 * @brief
 * @link
   @verbatim
    事件（Event）是一种任务间通信的机制，可用于任务间的同步。
        多任务环境下，任务之间往往需要同步操作，一个等待即是一个同步。事件可以提供一对多、多对多的同步操作。
        一对多同步模型：一个任务等待多个事件的触发。可以是任意一个事件发生时唤醒任务处理事件，也可以是几个事件都发生后才唤醒任务处理事件。
        多对多同步模型：多个任务等待多个事件的触发。

    事件特点
        任务通过创建事件控制块来触发事件或等待事件。
        事件间相互独立，内部实现为一个32位无符号整型，每一位标识一种事件类型。第25位不可用，因此最多可支持31种事件类型。
        事件仅用于任务间的同步，不提供数据传输功能。
        多次向事件控制块写入同一事件类型，在被清零前等效于只写入一次。
        多个任务可以对同一事件进行读写操作。
        支持事件读写超时机制。

    事件读取模式
        在读事件时，可以选择读取模式。读取模式如下：
        所有事件（LOS_WAITMODE_AND）：逻辑与，基于接口传入的事件类型掩码eventMask，
            只有这些事件都已经发生才能读取成功，否则该任务将阻塞等待或者返回错误码。
        任一事件（LOS_WAITMODE_OR）：逻辑或，基于接口传入的事件类型掩码eventMask，
            只要这些事件中有任一种事件发生就可以读取成功，否则该任务将阻塞等待或者返回错误码。
        清除事件（LOS_WAITMODE_CLR）：这是一种附加读取模式，需要与所有事件模式或任一事件模式结合
            使用（LOS_WAITMODE_AND | LOS_WAITMODE_CLR或 LOS_WAITMODE_OR | LOS_WAITMODE_CLR）。在这种模式下，
            当设置的所有事件模式或任一事件模式读取成功后，会自动清除事件控制块中对应的事件类型位。

    运作机制
        任务在调用LOS_EventRead接口读事件时，可以根据入参事件掩码类型eventMask读取事件的单个或者多个事件类型。
            事件读取成功后，如果设置LOS_WAITMODE_CLR会清除已读取到的事件类型，反之不会清除已读到的事件类型，需显式清除。
            可以通过入参选择读取模式，读取事件掩码类型中所有事件还是读取事件掩码类型中任意事件。
        任务在调用LOS_EventWrite接口写事件时，对指定事件控制块写入指定的事件类型，
            可以一次同时写多个事件类型。写事件会触发任务调度。
        任务在调用LOS_EventClear接口清除事件时，根据入参事件和待清除的事件类型，
            对事件对应位进行清0操作。		
            
    使用场景
        事件可应用于多种任务同步场景，在某些同步场景下可替代信号量。

    注意事项
        在系统初始化之前不能调用读写事件接口。如果调用，系统运行会不正常。
        在中断中，可以对事件对象进行写操作，但不能进行读操作。
        在锁任务调度状态下，禁止任务阻塞于读事件。
        LOS_EventClear 入参值是：要清除的指定事件类型的反码（~events）。
        为了区别LOS_EventRead接口返回的是事件还是错误码，事件掩码的第25位不能使用。
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-06
 */
#include "los_event_pri.h"
#include "los_task_pri.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_percpu_pri.h"
#include "los_sched_pri.h"
#include "los_hook.h"
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
#include "los_exc.h"
#endif

/**
 * @brief   初始化事件控制块
 * @details 初始化事件ID为0并初始化事件等待链表
 * @param   eventCB [IN] 事件控制块指针
 * @return  UINT32 - 操作结果
 *          LOS_OK：初始化成功
 *          LOS_ERRNO_EVENT_PTR_NULL：事件控制块指针为空
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_EventInit(PEVENT_CB_S eventCB)
{
    UINT32 intSave;  // 用于保存中断状态

    if (eventCB == NULL) {  // 检查事件控制块指针是否为空
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    intSave = LOS_IntLock();  // 关中断，保护临界区
    eventCB->uwEventID = 0;  // 初始化事件ID为0
    LOS_ListInit(&eventCB->stEventList);  // 初始化事件等待链表
    LOS_IntRestore(intSave);  // 恢复中断状态
    OsHookCall(LOS_HOOK_TYPE_EVENT_INIT, eventCB);  // 调用事件初始化钩子函数
    return LOS_OK;
}

/**
 * @brief   事件参数检查
 * @details 检查事件控制块指针、事件掩码和等待模式的有效性
 * @param   ptr [IN] 事件控制块指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @return  UINT32 - 操作结果
 *          LOS_OK：参数有效
 *          LOS_ERRNO_EVENT_PTR_NULL：指针为空
 *          LOS_ERRNO_EVENT_EVENTMASK_INVALID：事件掩码无效
 *          LOS_ERRNO_EVENT_SETBIT_INVALID：事件位设置无效
 *          LOS_ERRNO_EVENT_FLAGS_INVALID：等待模式标志无效
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsEventParamCheck(const VOID *ptr, UINT32 eventMask, UINT32 mode)
{
    if (ptr == NULL) {  // 检查指针是否为空
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    if (eventMask == 0) {  // 检查事件掩码是否为0
        return LOS_ERRNO_EVENT_EVENTMASK_INVALID;
    }

    if (eventMask & LOS_ERRTYPE_ERROR) {  // 检查事件掩码是否包含错误位(第25位)
        return LOS_ERRNO_EVENT_SETBIT_INVALID;
    }

    // 检查等待模式是否合法：只能是AND、OR、CLR的组合，且必须包含AND或OR之一
    if (((mode & LOS_WAITMODE_OR) && (mode & LOS_WAITMODE_AND)) ||
        (mode & ~(LOS_WAITMODE_OR | LOS_WAITMODE_AND | LOS_WAITMODE_CLR)) ||
        !(mode & (LOS_WAITMODE_OR | LOS_WAITMODE_AND))) {
        return LOS_ERRNO_EVENT_FLAGS_INVALID;
    }
    return LOS_OK;
}

/**
 * @brief   事件轮询检查
 * @details 根据事件掩码和等待模式检查事件是否满足条件，支持清除事件标志
 * @param   eventID [IN/OUT] 事件ID指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @return  UINT32 - 满足条件的事件掩码，0表示不满足
 */
LITE_OS_SEC_TEXT UINT32 OsEventPoll(UINT32 *eventID, UINT32 eventMask, UINT32 mode)
{
    UINT32 ret = 0;  // 初始化返回值为0

    LOS_ASSERT(OsIntLocked());  // 断言：当前处于中断锁定状态
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));  // 断言：任务自旋锁已被持有

    if (mode & LOS_WAITMODE_OR) {  // 逻辑或模式：任一事件满足即可
        if ((*eventID & eventMask) != 0) {
            ret = *eventID & eventMask;  // 返回满足条件的事件掩码
        }
    } else {  // 逻辑与模式：所有事件都需满足
        if ((eventMask != 0) && (eventMask == (*eventID & eventMask))) {
            ret = *eventID & eventMask;  // 返回满足条件的事件掩码
        }
    }

    if (ret && (mode & LOS_WAITMODE_CLR)) {  // 如果设置了清除标志，则清除已满足的事件位
        *eventID = *eventID & ~ret;
    }

    return ret;
}

/**
 * @brief   事件读取前检查
 * @details 检查事件读取的前置条件，包括参数合法性、中断状态和任务类型
 * @param   eventCB [IN] 事件控制块指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @return  UINT32 - 操作结果
 *          LOS_OK：检查通过
 *          其他错误码：检查失败
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsEventReadCheck(const PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode)
{
    UINT32 ret;  // 保存检查结果
    LosTaskCB *runTask = NULL;  // 当前运行任务控制块指针
    ret = OsEventParamCheck(eventCB, eventMask, mode);  // 检查事件参数
    if (ret != LOS_OK) {
        return ret;
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断中调用
        return LOS_ERRNO_EVENT_READ_IN_INTERRUPT;
    }

    runTask = OsCurrTaskGet();  // 获取当前运行任务
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        OsBackTrace();  // 打印回溯信息
        return LOS_ERRNO_EVENT_READ_IN_SYSTEM_TASK;
    }
    return LOS_OK;
}

/**
 * @brief   事件读取实现函数
 * @details 实现事件读取的核心逻辑，支持阻塞等待和超时机制
 * @param   eventCB [IN] 事件控制块指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @param   timeout [IN] 超时时间(节拍数)
 * @param   once [IN] 是否只唤醒一个等待任务
 * @return  UINT32 - 满足条件的事件掩码或错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsEventReadImp(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode,
                                              UINT32 timeout, BOOL once)
{
    UINT32 ret = 0;  // 初始化返回值为0
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    OsHookCall(LOS_HOOK_TYPE_EVENT_READ, eventCB, eventMask, mode, timeout);  // 调用事件读取钩子函数

    if (once == FALSE) {  // 如果不是只读取一次，则先轮询检查事件
        ret = OsEventPoll(&eventCB->uwEventID, eventMask, mode);
    }

    if (ret == 0) {  // 如果事件未满足
        if (timeout == 0) {  // 非阻塞模式，直接返回0
            return ret;
        }

        if (!OsPreemptableInSched()) {  // 检查是否在不可抢占区域
            return LOS_ERRNO_EVENT_READ_IN_LOCK;
        }

        runTask->eventMask = eventMask;  // 保存事件掩码到任务控制块
        runTask->eventMode = mode;  // 保存等待模式到任务控制块
        runTask->taskEvent = eventCB;  // 保存事件控制块指针到任务控制块
        OsTaskWaitSetPendMask(OS_TASK_WAIT_EVENT, eventMask, timeout);  // 设置任务等待掩码
        ret = runTask->ops->wait(runTask, &eventCB->stEventList, timeout);  // 将任务加入等待队列
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {  // 等待超时
            return LOS_ERRNO_EVENT_READ_TIMEOUT;
        }

        ret = OsEventPoll(&eventCB->uwEventID, eventMask, mode);  // 再次轮询检查事件
    }
    return ret;
}

/**
 * @brief   事件读取函数
 * @details 读取指定事件，支持阻塞等待和超时机制
 * @param   eventCB [IN] 事件控制块指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @param   timeout [IN] 超时时间(节拍数)
 * @param   once [IN] 是否只唤醒一个等待任务
 * @return  UINT32 - 满足条件的事件掩码或错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsEventRead(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode, UINT32 timeout,
                                           BOOL once)
{
    UINT32 ret;  // 保存返回值
    UINT32 intSave;  // 用于保存中断状态

    ret = OsEventReadCheck(eventCB, eventMask, mode);  // 检查事件读取条件
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    ret = OsEventReadImp(eventCB, eventMask, mode, timeout, once);  // 调用事件读取实现函数
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return ret;
}

/**
 * @brief   恢复等待事件的任务
 * @details 检查等待任务是否满足事件条件，如果满足则唤醒任务
 * @param   resumedTask [IN] 等待任务控制块指针
 * @param   eventCB [IN] 事件控制块指针
 * @param   events [IN] 触发的事件掩码
 * @return  UINT8 - 是否唤醒任务
 *          1：唤醒成功
 *          0：唤醒失败
 */
LITE_OS_SEC_TEXT STATIC UINT8 OsEventResume(LosTaskCB *resumedTask, const PEVENT_CB_S eventCB, UINT32 events)
{
    UINT8 exitFlag = 0;  // 初始化唤醒标志为0

    // 检查任务等待模式和事件是否满足条件
    if (((resumedTask->eventMode & LOS_WAITMODE_OR) && ((resumedTask->eventMask & events) != 0)) ||
        ((resumedTask->eventMode & LOS_WAITMODE_AND) &&
        ((resumedTask->eventMask & eventCB->uwEventID) == resumedTask->eventMask))) {
        exitFlag = 1;  // 设置唤醒标志

        resumedTask->taskEvent = NULL;  // 清除任务的事件控制块指针
        OsTaskWakeClearPendMask(resumedTask);  // 清除任务的等待掩码
        resumedTask->ops->wake(resumedTask);  // 唤醒任务
    }

    return exitFlag;
}

/**
 * @brief   不安全的事件写入函数(无锁)
 * @details 在无锁保护的情况下写入事件并唤醒等待任务
 * @param   eventCB [IN] 事件控制块指针
 * @param   events [IN] 要写入的事件掩码
 * @param   once [IN] 是否只唤醒一个等待任务
 * @param   exitFlag [OUT] 是否需要调度的标志
 */
LITE_OS_SEC_TEXT VOID OsEventWriteUnsafe(PEVENT_CB_S eventCB, UINT32 events, BOOL once, UINT8 *exitFlag)
{
    LosTaskCB *resumedTask = NULL;  // 要恢复的任务控制块指针
    LosTaskCB *nextTask = NULL;  // 下一个任务控制块指针
    BOOL schedFlag = FALSE;  // 是否需要调度的标志
    OsHookCall(LOS_HOOK_TYPE_EVENT_WRITE, eventCB, events);  // 调用事件写入钩子函数
    eventCB->uwEventID |= events;  // 设置事件位
    if (!LOS_ListEmpty(&eventCB->stEventList)) {  // 如果事件等待链表不为空
        // 遍历等待链表
        for (resumedTask = LOS_DL_LIST_ENTRY((&eventCB->stEventList)->pstNext, LosTaskCB, pendList);
             &resumedTask->pendList != &eventCB->stEventList;) {
            nextTask = LOS_DL_LIST_ENTRY(resumedTask->pendList.pstNext, LosTaskCB, pendList);  // 获取下一个任务
            if (OsEventResume(resumedTask, eventCB, events)) {  // 尝试恢复当前任务
                schedFlag = TRUE;  // 设置需要调度标志
            }
            if (once == TRUE) {  // 如果只唤醒一个任务，则跳出循环
                break;
            }
            resumedTask = nextTask;  // 移动到下一个任务
        }
    }

    if ((exitFlag != NULL) && (schedFlag == TRUE)) {  // 如果需要调度且exitFlag有效
        *exitFlag = 1;
    }
}

/**
 * @brief   事件写入函数
 * @details 写入指定事件并唤醒等待任务，支持单次唤醒和多次唤醒
 * @param   eventCB [IN] 事件控制块指针
 * @param   events [IN] 要写入的事件掩码
 * @param   once [IN] 是否只唤醒一个等待任务
 * @return  UINT32 - 操作结果
 *          LOS_OK：写入成功
 *          LOS_ERRNO_EVENT_PTR_NULL：事件控制块指针为空
 *          LOS_ERRNO_EVENT_SETBIT_INVALID：事件位设置无效
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsEventWrite(PEVENT_CB_S eventCB, UINT32 events, BOOL once)
{
    UINT32 intSave;  // 用于保存中断状态
    UINT8 exitFlag = 0;  // 是否需要调度的标志

    if (eventCB == NULL) {  // 检查事件控制块指针是否为空
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    if (events & LOS_ERRTYPE_ERROR) {  // 检查事件掩码是否包含错误位(第25位)
        return LOS_ERRNO_EVENT_SETBIT_INVALID;
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    OsEventWriteUnsafe(eventCB, events, once, &exitFlag);  // 调用不安全的事件写入函数
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    if (exitFlag == 1) {  // 如果需要调度
        LOS_MpSchedule(OS_MP_CPU_ALL);  // 多处理器调度
        LOS_Schedule();  // 触发任务调度
    }
    return LOS_OK;
}

/**
 * @brief   事件轮询接口
 * @details 非阻塞方式检查事件是否满足条件
 * @param   eventID [IN/OUT] 事件ID指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @return  UINT32 - 满足条件的事件掩码或错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_EventPoll(UINT32 *eventID, UINT32 eventMask, UINT32 mode)
{
    UINT32 ret;  // 保存返回值
    UINT32 intSave;  // 用于保存中断状态

    ret = OsEventParamCheck((VOID *)eventID, eventMask, mode);  // 检查事件参数
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    ret = OsEventPoll(eventID, eventMask, mode);  // 调用事件轮询检查函数
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return ret;
}

/**
 * @brief   事件读取接口
 * @details 读取指定事件，支持阻塞等待和超时机制
 * @param   eventCB [IN] 事件控制块指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @param   timeout [IN] 超时时间(节拍数)
 * @return  UINT32 - 满足条件的事件掩码或错误码
 */
LITE_OS_SEC_TEXT UINT32 LOS_EventRead(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode, UINT32 timeout)
{
    return OsEventRead(eventCB, eventMask, mode, timeout, FALSE);
}

/**
 * @brief   事件写入接口
 * @details 写入指定事件并唤醒所有等待任务
 * @param   eventCB [IN] 事件控制块指针
 * @param   events [IN] 要写入的事件掩码
 * @return  UINT32 - 操作结果
 *          LOS_OK：写入成功
 *          其他错误码：写入失败
 */
LITE_OS_SEC_TEXT UINT32 LOS_EventWrite(PEVENT_CB_S eventCB, UINT32 events)
{
    return OsEventWrite(eventCB, events, FALSE);
}

/**
 * @brief   单次事件读取接口
 * @details 读取指定事件，只唤醒一个等待任务
 * @param   eventCB [IN] 事件控制块指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @param   timeout [IN] 超时时间(节拍数)
 * @return  UINT32 - 满足条件的事件掩码或错误码
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsEventReadOnce(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode,
                                              UINT32 timeout)
{
    return OsEventRead(eventCB, eventMask, mode, timeout, TRUE);
}

/**
 * @brief   单次事件写入接口
 * @details 写入指定事件并只唤醒一个等待任务
 * @param   eventCB [IN] 事件控制块指针
 * @param   events [IN] 要写入的事件掩码
 * @return  UINT32 - 操作结果
 *          LOS_OK：写入成功
 *          其他错误码：写入失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsEventWriteOnce(PEVENT_CB_S eventCB, UINT32 events)
{
    return OsEventWrite(eventCB, events, TRUE);
}

/**
 * @brief   销毁事件控制块
 * @details 从系统中销毁指定的事件控制块，只有当没有任务等待时才能销毁
 * @param   eventCB [IN] 事件控制块指针
 * @return  UINT32 - 操作结果
 *          LOS_OK：销毁成功
 *          LOS_ERRNO_EVENT_PTR_NULL：事件控制块指针为空
 *          LOS_ERRNO_EVENT_SHOULD_NOT_DESTROY：有任务在等待该事件，无法销毁
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_EventDestroy(PEVENT_CB_S eventCB)
{
    UINT32 intSave;  // 用于保存中断状态

    if (eventCB == NULL) {  // 检查事件控制块指针是否为空
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (!LOS_ListEmpty(&eventCB->stEventList)) {  // 检查是否有任务在等待该事件
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_ERRNO_EVENT_SHOULD_NOT_DESTROY;
    }

    eventCB->uwEventID = 0;  // 清除事件ID
    LOS_ListDelInit(&eventCB->stEventList);  // 初始化事件等待链表
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    OsHookCall(LOS_HOOK_TYPE_EVENT_DESTROY, eventCB);  // 调用事件销毁钩子函数
    return LOS_OK;
}

/**
 * @brief   清除事件
 * @details 清除事件控制块中的指定事件位
 * @param   eventCB [IN] 事件控制块指针
 * @param   eventMask [IN] 要清除的事件掩码(注意：需传入要清除事件位的反码)
 * @return  UINT32 - 操作结果
 *          LOS_OK：清除成功
 *          LOS_ERRNO_EVENT_PTR_NULL：事件控制块指针为空
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_EventClear(PEVENT_CB_S eventCB, UINT32 eventMask)
{
    UINT32 intSave;  // 用于保存中断状态

    if (eventCB == NULL) {  // 检查事件控制块指针是否为空
        return LOS_ERRNO_EVENT_PTR_NULL;
    }
    OsHookCall(LOS_HOOK_TYPE_EVENT_CLEAR, eventCB, eventMask);  // 调用事件清除钩子函数
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    eventCB->uwEventID &= eventMask;  // 清除事件位(与操作)
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    return LOS_OK;
}

#ifdef LOSCFG_COMPAT_POSIX
/**
 * @brief   带条件的事件读取接口
 * @details 结合条件变量读取事件，用于POSIX兼容性
 * @param   cond [IN] 条件变量指针
 * @param   eventCB [IN] 事件控制块指针
 * @param   eventMask [IN] 事件掩码
 * @param   mode [IN] 等待模式
 * @param   timeout [IN] 超时时间(节拍数)
 * @return  UINT32 - 满足条件的事件掩码或错误码
 */
LITE_OS_SEC_TEXT UINT32 OsEventReadWithCond(const EventCond *cond, PEVENT_CB_S eventCB,
                                            UINT32 eventMask, UINT32 mode, UINT32 timeout)
{
    UINT32 ret;  // 保存返回值
    UINT32 intSave;  // 用于保存中断状态

    ret = OsEventReadCheck(eventCB, eventMask, mode);  // 检查事件读取条件
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器

    if (*cond->realValue != cond->value) {  // 检查条件是否满足
        eventCB->uwEventID &= cond->clearEvent;  // 清除事件
        goto OUT;  // 跳转到退出
    }

    ret = OsEventReadImp(eventCB, eventMask, mode, timeout, FALSE);  // 调用事件读取实现函数
OUT:
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return ret;
}
#endif