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

/******************************************************************************
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
******************************************************************************/
//初始化一个事件控制块
LITE_OS_SEC_TEXT_INIT UINT32 LOS_EventInit(PEVENT_CB_S eventCB)
{
    UINT32 intSave;

    if (eventCB == NULL) {
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    intSave = LOS_IntLock();//锁中断
    eventCB->uwEventID = 0;
    LOS_ListInit(&eventCB->stEventList);//事件链表初始化
    LOS_IntRestore(intSave);//恢复中断
    OsHookCall(LOS_HOOK_TYPE_EVENT_INIT, eventCB);
    return LOS_OK;
}
//事件参数检查
LITE_OS_SEC_TEXT STATIC UINT32 OsEventParamCheck(const VOID *ptr, UINT32 eventMask, UINT32 mode)
{
    if (ptr == NULL) {
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    if (eventMask == 0) {
        return LOS_ERRNO_EVENT_EVENTMASK_INVALID;
    }

    if (eventMask & LOS_ERRTYPE_ERROR) {
        return LOS_ERRNO_EVENT_SETBIT_INVALID;
    }

    if (((mode & LOS_WAITMODE_OR) && (mode & LOS_WAITMODE_AND)) ||
        (mode & ~(LOS_WAITMODE_OR | LOS_WAITMODE_AND | LOS_WAITMODE_CLR)) ||
        !(mode & (LOS_WAITMODE_OR | LOS_WAITMODE_AND))) {
        return LOS_ERRNO_EVENT_FLAGS_INVALID;
    }
    return LOS_OK;
}
//根据用户传入的事件值、事件掩码及校验模式，返回用户传入的事件是否符合预期
LITE_OS_SEC_TEXT UINT32 OsEventPoll(UINT32 *eventID, UINT32 eventMask, UINT32 mode)
{
    UINT32 ret = 0;

    LOS_ASSERT(OsIntLocked());//断言不允许中断了
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));//任务自旋锁

    if (mode & LOS_WAITMODE_OR) {//如果模式是读取掩码中任意事件
        if ((*eventID & eventMask) != 0) {
            ret = *eventID & eventMask;
        }
    } else {//等待全部事件发生
        if ((eventMask != 0) && (eventMask == (*eventID & eventMask))) {//必须满足全部事件发生
            ret = *eventID & eventMask;
        }
    }

    if (ret && (mode & LOS_WAITMODE_CLR)) {//读取完成后清除事件
        *eventID = *eventID & ~ret;
    }

    return ret;
}
//检查读事件
LITE_OS_SEC_TEXT STATIC UINT32 OsEventReadCheck(const PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode)
{
    UINT32 ret;
    LosTaskCB *runTask = NULL;

    ret = OsEventParamCheck(eventCB, eventMask, mode);//事件参数检查
    if (ret != LOS_OK) {
        return ret;
    }

    if (OS_INT_ACTIVE) {//中断正在进行
        return LOS_ERRNO_EVENT_READ_IN_INTERRUPT;//不能在中断发送时读事件
    }

    runTask = OsCurrTaskGet();//获取当前任务
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {//任务属于系统任务
        OsBackTrace();
        return LOS_ERRNO_EVENT_READ_IN_SYSTEM_TASK;//不能在系统任务中读取事件
    }
    return LOS_OK;
}
//读取指定事件类型的实现函数，超时时间为相对时间：单位为Tick
LITE_OS_SEC_TEXT STATIC UINT32 OsEventReadImp(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode,
                                              UINT32 timeout, BOOL once)
{
    UINT32 ret = 0;
    LosTaskCB *runTask = OsCurrTaskGet();
    OsHookCall(LOS_HOOK_TYPE_EVENT_READ, eventCB, eventMask, mode, timeout);

    if (once == FALSE) {
        ret = OsEventPoll(&eventCB->uwEventID, eventMask, mode);//检测事件是否符合预期
    }

    if (ret == 0) {//不符合预期时
        if (timeout == 0) {//不等待的情况
            return ret;
        }

        if (!OsPreemptableInSched()) {//不能抢占式调度
            return LOS_ERRNO_EVENT_READ_IN_LOCK;
        }

        runTask->eventMask = eventMask;//等待事件
        runTask->eventMode = mode;	//事件模式
        runTask->taskEvent = eventCB;//事件控制块
        OsTaskWaitSetPendMask(OS_TASK_WAIT_EVENT, eventMask, timeout);
        ret = OsSchedTaskWait(&eventCB->stEventList, timeout, TRUE);
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {
            return LOS_ERRNO_EVENT_READ_TIMEOUT;
        }

        ret = OsEventPoll(&eventCB->uwEventID, eventMask, mode);//检测事件是否符合预期
    }
    return ret;
}
//读取指定事件类型，超时时间为相对时间：单位为Tick
LITE_OS_SEC_TEXT STATIC UINT32 OsEventRead(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode, UINT32 timeout,
                                           BOOL once)
{
    UINT32 ret;
    UINT32 intSave;

    ret = OsEventReadCheck(eventCB, eventMask, mode);//读取事件检查
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsEventReadImp(eventCB, eventMask, mode, timeout, once);//读事件实现函数
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
//事件恢复操作
LITE_OS_SEC_TEXT STATIC UINT8 OsEventResume(LosTaskCB *resumedTask, const PEVENT_CB_S eventCB, UINT32 events)
{
    UINT8 exitFlag = 0;//是否唤醒

    if (((resumedTask->eventMode & LOS_WAITMODE_OR) && ((resumedTask->eventMask & events) != 0)) ||
        ((resumedTask->eventMode & LOS_WAITMODE_AND) &&
        ((resumedTask->eventMask & eventCB->uwEventID) == resumedTask->eventMask))) {//逻辑与 和 逻辑或 的处理
        exitFlag = 1; 

        resumedTask->taskEvent = NULL;
        OsTaskWakeClearPendMask(resumedTask);
        OsSchedTaskWake(resumedTask);
    }

    return exitFlag;
}
//以不安全的方式写事件
LITE_OS_SEC_TEXT VOID OsEventWriteUnsafe(PEVENT_CB_S eventCB, UINT32 events, BOOL once, UINT8 *exitFlag)
{
    LosTaskCB *resumedTask = NULL;
    LosTaskCB *nextTask = NULL;
    BOOL schedFlag = FALSE;
    OsHookCall(LOS_HOOK_TYPE_EVENT_WRITE, eventCB, events);
    eventCB->uwEventID |= events;//对应位贴上标签
    if (!LOS_ListEmpty(&eventCB->stEventList)) {//等待事件链表判断,处理等待事件的任务
        for (resumedTask = LOS_DL_LIST_ENTRY((&eventCB->stEventList)->pstNext, LosTaskCB, pendList);
             &resumedTask->pendList != &eventCB->stEventList;) {//循环获取任务链表
            nextTask = LOS_DL_LIST_ENTRY(resumedTask->pendList.pstNext, LosTaskCB, pendList);//获取任务实体
            if (OsEventResume(resumedTask, eventCB, events)) {//是否恢复任务
                schedFlag = TRUE;//任务已加至就绪队列,申请发生一次调度
            }
            if (once == TRUE) {//是否只处理一次任务
                break;//退出循环
            }
            resumedTask = nextTask;//检查链表中下一个任务
        }
    }

    if ((exitFlag != NULL) && (schedFlag == TRUE)) {//是否让外面调度
        *exitFlag = 1;
    }
}
//写入事件
LITE_OS_SEC_TEXT STATIC UINT32 OsEventWrite(PEVENT_CB_S eventCB, UINT32 events, BOOL once)
{
    UINT32 intSave;
    UINT8 exitFlag = 0;

    if (eventCB == NULL) {
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    if (events & LOS_ERRTYPE_ERROR) {
        return LOS_ERRNO_EVENT_SETBIT_INVALID;
    }

    SCHEDULER_LOCK(intSave);	//禁止调度
    OsEventWriteUnsafe(eventCB, events, once, &exitFlag);//写入事件
    SCHEDULER_UNLOCK(intSave);	//允许调度

    if (exitFlag == 1) { //需要发生调度
        LOS_MpSchedule(OS_MP_CPU_ALL);//通知所有CPU调度
        LOS_Schedule();//执行调度
    }
    return LOS_OK;
}
//根据用户传入的事件值、事件掩码及校验模式，返回用户传入的事件是否符合预期
LITE_OS_SEC_TEXT UINT32 LOS_EventPoll(UINT32 *eventID, UINT32 eventMask, UINT32 mode)
{
    UINT32 ret;
    UINT32 intSave;

    ret = OsEventParamCheck((VOID *)eventID, eventMask, mode);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsEventPoll(eventID, eventMask, mode);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
//读取指定事件类型，超时时间为相对时间：单位为Tick
LITE_OS_SEC_TEXT UINT32 LOS_EventRead(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode, UINT32 timeout)
{
    return OsEventRead(eventCB, eventMask, mode, timeout, FALSE);
}
//写指定的事件类型
LITE_OS_SEC_TEXT UINT32 LOS_EventWrite(PEVENT_CB_S eventCB, UINT32 events)
{
    return OsEventWrite(eventCB, events, FALSE);
}
//只读一次事件
LITE_OS_SEC_TEXT_MINOR UINT32 OsEventReadOnce(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode,
                                              UINT32 timeout)
{
    return OsEventRead(eventCB, eventMask, mode, timeout, TRUE);
}
//只写一次事件
LITE_OS_SEC_TEXT_MINOR UINT32 OsEventWriteOnce(PEVENT_CB_S eventCB, UINT32 events)
{
    return OsEventWrite(eventCB, events, TRUE);
}
//销毁指定的事件控制块
LITE_OS_SEC_TEXT_INIT UINT32 LOS_EventDestroy(PEVENT_CB_S eventCB)
{
    UINT32 intSave;

    if (eventCB == NULL) {
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    SCHEDULER_LOCK(intSave);
    if (!LOS_ListEmpty(&eventCB->stEventList)) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_ERRNO_EVENT_SHOULD_NOT_DESTROY;
    }

    eventCB->uwEventID = 0;
    LOS_ListDelInit(&eventCB->stEventList);
    SCHEDULER_UNLOCK(intSave);
    OsHookCall(LOS_HOOK_TYPE_EVENT_DESTROY, eventCB);
    return LOS_OK;
}
//清除指定的事件类型
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_EventClear(PEVENT_CB_S eventCB, UINT32 eventMask)
{
    UINT32 intSave;

    if (eventCB == NULL) {
        return LOS_ERRNO_EVENT_PTR_NULL;
    }
    OsHookCall(LOS_HOOK_TYPE_EVENT_CLEAR, eventCB, eventMask);
    SCHEDULER_LOCK(intSave);
    eventCB->uwEventID &= eventMask;
    SCHEDULER_UNLOCK(intSave);

    return LOS_OK;
}
//有条件式读事件
#ifdef LOSCFG_COMPAT_POSIX
LITE_OS_SEC_TEXT UINT32 OsEventReadWithCond(const EventCond *cond, PEVENT_CB_S eventCB,
                                            UINT32 eventMask, UINT32 mode, UINT32 timeout)
{
    UINT32 ret;
    UINT32 intSave;

    ret = OsEventReadCheck(eventCB, eventMask, mode);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);

    if (*cond->realValue != cond->value) {
        eventCB->uwEventID &= cond->clearEvent;
        goto OUT;
    }

    ret = OsEventReadImp(eventCB, eventMask, mode, timeout, FALSE);
OUT:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
#endif

