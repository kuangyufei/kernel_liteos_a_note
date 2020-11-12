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

#include "los_event_pri.h"
#include "los_task_pri.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_percpu_pri.h"

#if (LOSCFG_BASE_CORE_SWTMR == YES)
#include "los_exc.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
//初始化一个事件控制块
LITE_OS_SEC_TEXT_INIT UINT32 LOS_EventInit(PEVENT_CB_S eventCB)
{
    UINT32 intSave;

    if (eventCB == NULL) {
        return LOS_ERRNO_EVENT_PTR_NULL;
    }

    intSave = LOS_IntLock();
    eventCB->uwEventID = 0;
    LOS_ListInit(&eventCB->stEventList);//事件链表初始化
    LOS_IntRestore(intSave);
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
////根据用户传入的事件值、事件掩码及校验模式，返回用户传入的事件是否符合预期
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

        runTask->eventMask = eventMask;
        runTask->eventMode = mode;
        runTask->taskEvent = eventCB;//事件控制块
        ret = OsTaskWait(&eventCB->stEventList, timeout, TRUE);//任务进入等待状态，挂入阻塞链表
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {//如果返回超时
            runTask->taskEvent = NULL;
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
    UINT8 exitFlag = 0;

    if (((resumedTask->eventMode & LOS_WAITMODE_OR) && ((resumedTask->eventMask & events) != 0)) ||
        ((resumedTask->eventMode & LOS_WAITMODE_AND) &&
        ((resumedTask->eventMask & eventCB->uwEventID) == resumedTask->eventMask))) {
        exitFlag = 1;

        resumedTask->taskEvent = NULL;
        OsTaskWake(resumedTask);//唤醒任务
    }

    return exitFlag;
}

LITE_OS_SEC_TEXT VOID OsEventWriteUnsafe(PEVENT_CB_S eventCB, UINT32 events, BOOL once, UINT8 *exitFlag)
{
    LosTaskCB *resumedTask = NULL;
    LosTaskCB *nextTask = NULL;
    BOOL schedFlag = FALSE;

    eventCB->uwEventID |= events;
    if (!LOS_ListEmpty(&eventCB->stEventList)) {
        for (resumedTask = LOS_DL_LIST_ENTRY((&eventCB->stEventList)->pstNext, LosTaskCB, pendList);
             &resumedTask->pendList != &eventCB->stEventList;) {
            nextTask = LOS_DL_LIST_ENTRY(resumedTask->pendList.pstNext, LosTaskCB, pendList);
            if (OsEventResume(resumedTask, eventCB, events)) {
                schedFlag = TRUE;
            }
            if (once == TRUE) {
                break;
            }
            resumedTask = nextTask;
        }
    }

    if ((exitFlag != NULL) && (schedFlag == TRUE)) {
        *exitFlag = 1;
    }
}

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

    SCHEDULER_LOCK(intSave);
    OsEventWriteUnsafe(eventCB, events, once, &exitFlag);
    SCHEDULER_UNLOCK(intSave);

    if (exitFlag == 1) {
        LOS_MpSchedule(OS_MP_CPU_ALL);
        LOS_Schedule();
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
        return LOS_ERRNO_EVENT_SHOULD_NOT_DESTORY;
    }

    eventCB->uwEventID = 0;
    LOS_ListDelInit(&eventCB->stEventList);
    SCHEDULER_UNLOCK(intSave);

    return LOS_OK;
}
//清除指定的事件类型
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_EventClear(PEVENT_CB_S eventCB, UINT32 events)
{
    UINT32 intSave;

    if (eventCB == NULL) {
        return LOS_ERRNO_EVENT_PTR_NULL;
    }
    SCHEDULER_LOCK(intSave);
    eventCB->uwEventID &= events;
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

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
