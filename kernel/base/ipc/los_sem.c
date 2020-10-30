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

#include "los_sem_pri.h"
#include "los_sem_debug_pri.h"
#include "los_err_pri.h"
#include "los_task_pri.h"
#include "los_exc.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_percpu_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#if (LOSCFG_BASE_IPC_SEM == YES)

#if (LOSCFG_BASE_IPC_SEM_LIMIT <= 0)
#error "sem maxnum cannot be zero"
#endif /* LOSCFG_BASE_IPC_SEM_LIMIT <= 0 */

LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_unusedSemList;//可用的信号量列表,干嘛不用freeList? 可以看出这里是另一个人写的代码
LITE_OS_SEC_BSS LosSemCB *g_allSem = NULL;//信号池,一次分配 LOSCFG_BASE_IPC_SEM_LIMIT 个信号量

/*
 * Description  : Initialize the semaphore doubly linked list
 * Return       : LOS_OK on success, or error code on failure
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsSemInit(VOID)//信号量初始化
{
    LosSemCB *semNode = NULL;
    UINT32 index;

    LOS_ListInit(&g_unusedSemList);//初始
    /* system resident memory, don't free */
    g_allSem = (LosSemCB *)LOS_MemAlloc(m_aucSysMem0, (LOSCFG_BASE_IPC_SEM_LIMIT * sizeof(LosSemCB)));//分配信号池
    if (g_allSem == NULL) {
        return LOS_ERRNO_SEM_NO_MEMORY;
    }

    for (index = 0; index < LOSCFG_BASE_IPC_SEM_LIMIT; index++) {
        semNode = ((LosSemCB *)g_allSem) + index;//拿信号控制块, 可以直接g_allSem[index]来嘛
        semNode->semID = SET_SEM_ID(0, index);//保存ID
        semNode->semStat = OS_SEM_UNUSED;//标记未使用
        LOS_ListTailInsert(&g_unusedSemList, &semNode->semList);//通过semList把 信号块挂到空闲链表上
    }

    if (OsSemDbgInitHook() != LOS_OK) {
        return LOS_ERRNO_SEM_NO_MEMORY;
    }
    return LOS_OK;
}

/*
 * Description  : Create a semaphore,
 * Input        : count     --- semaphore count,
 *                maxCount  --- Max number of available semaphores,
 *                semHandle --- Index of semaphore,
 * Return       : LOS_OK on success ,or error code on failure
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsSemCreate(UINT16 count, UINT16 maxCount, UINT32 *semHandle)
{
    UINT32 intSave;
    LosSemCB *semCreated = NULL;
    LOS_DL_LIST *unusedSem = NULL;
    UINT32 errNo;
    UINT32 errLine;

    if (semHandle == NULL) {
        return LOS_ERRNO_SEM_PTR_NULL;
    }

    if (count > maxCount) {//信号量不能大于最大值,两参数都是外面给的
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_OVERFLOW);
    }

    SCHEDULER_LOCK(intSave);//进入临界区了,拿自旋锁

    if (LOS_ListEmpty(&g_unusedSemList)) {//没有可分配的空闲信号提供
        SCHEDULER_UNLOCK(intSave);
        OsSemInfoGetFullDataHook();
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_ALL_BUSY);
    }

    unusedSem = LOS_DL_LIST_FIRST(&g_unusedSemList);//拿第一个出来创建
    LOS_ListDelete(unusedSem);//从空闲链表上摘除
    semCreated = GET_SEM_LIST(unusedSem);//通过semList挂到链表上的,这里也要通过它把LosSemCB头查到. 进程,线程等结构体也都是这么干的.
    semCreated->semCount = count;//设置数量
    semCreated->semStat = OS_SEM_USED;//设置可用状态
    semCreated->maxSemCount = maxCount;//设置最大信号数量
    LOS_ListInit(&semCreated->semList);//初始化链表,后续阻塞任务通过task->pendList挂到semList链表上,就知道哪些任务在等它了.
    *semHandle = semCreated->semID;//参数带走 semID

    OsSemDbgUpdateHook(semCreated->semID, OsCurrTaskGet()->taskEntry, count);

    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;

ERR_HANDLER:
    OS_RETURN_ERROR_P2(errLine, errNo);
}

LITE_OS_SEC_TEXT_INIT UINT32 LOS_SemCreate(UINT16 count, UINT32 *semHandle)
{
    return OsSemCreate(count, OS_SEM_COUNT_MAX, semHandle);
}

LITE_OS_SEC_TEXT_INIT UINT32 LOS_BinarySemCreate(UINT16 count, UINT32 *semHandle)
{
    return OsSemCreate(count, OS_SEM_BINARY_COUNT_MAX, semHandle);
}
//删除信号量,参数就是 semID 
LITE_OS_SEC_TEXT_INIT UINT32 LOS_SemDelete(UINT32 semHandle)
{
    UINT32 intSave;
    LosSemCB *semDeleted = NULL;
    UINT32 errNo;
    UINT32 errLine;

    if (GET_SEM_INDEX(semHandle) >= (UINT32)LOSCFG_BASE_IPC_SEM_LIMIT) {
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_INVALID);
    }

    semDeleted = GET_SEM(semHandle);//通过ID拿到信号量实体

    SCHEDULER_LOCK(intSave);

    if ((semDeleted->semStat == OS_SEM_UNUSED) || (semDeleted->semID != semHandle)) {//参数判断
        SCHEDULER_UNLOCK(intSave);
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_INVALID);
    }

    if (!LOS_ListEmpty(&semDeleted->semList)) {//当前还有任务挂在这个信号上面,当然不能删除
        SCHEDULER_UNLOCK(intSave);
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_PENDED);//这个宏很有意思,里面goto到ERR_HANDLER
    }

    LOS_ListTailInsert(&g_unusedSemList, &semDeleted->semList);//通过semList从尾部插入空闲链表
    semDeleted->semStat = OS_SEM_UNUSED;//状态变成了未使用
    semDeleted->semID = SET_SEM_ID(GET_SEM_COUNT(semDeleted->semID) + 1, GET_SEM_INDEX(semDeleted->semID));//设置ID

    OsSemDbgUpdateHook(semDeleted->semID, NULL, 0);

    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;

ERR_HANDLER:
    OS_RETURN_ERROR_P2(errLine, errNo);
}
//挂起信号
LITE_OS_SEC_TEXT UINT32 LOS_SemPend(UINT32 semHandle, UINT32 timeout)
{
    UINT32 intSave;
    LosSemCB *semPended = GET_SEM(semHandle);//通过ID拿到信号体
    UINT32 retErr = LOS_OK;
    LosTaskCB *runTask = NULL;

    if (GET_SEM_INDEX(semHandle) >= (UINT32)LOSCFG_BASE_IPC_SEM_LIMIT) {
        OS_RETURN_ERROR(LOS_ERRNO_SEM_INVALID);
    }

    if (OS_INT_ACTIVE) {
        PRINT_ERR("!!!LOS_ERRNO_SEM_PEND_INTERR!!!\n");
        OsBackTrace();
        return LOS_ERRNO_SEM_PEND_INTERR;
    }

    runTask = OsCurrTaskGet();//获取当前任务
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        OsBackTrace();
        return LOS_ERRNO_SEM_PEND_IN_SYSTEM_TASK;
    }

    SCHEDULER_LOCK(intSave);

    if ((semPended->semStat == OS_SEM_UNUSED) || (semPended->semID != semHandle)) {
        retErr = LOS_ERRNO_SEM_INVALID;
        goto OUT;
    }

    /* Update the operate time, no matter the actual Pend success or not */
    OsSemDbgTimeUpdateHook(semHandle);

    if (semPended->semCount > 0) {//还有资源可用,返回肯定得成功,semCount=0时代表没资源了,task会必须去睡眠了
        semPended->semCount--;//资源少了一个
        goto OUT;//注意这里 retErr = LOS_OK ,所以返回是OK的 
    } else if (!timeout) {
        retErr = LOS_ERRNO_SEM_UNAVAILABLE;
        goto OUT;
    }

    if (!OsPreemptableInSched()) {//不能申请调度 (不能调度的原因是因为没有持有调度任务自旋锁)
        PRINT_ERR("!!!LOS_ERRNO_SEM_PEND_IN_LOCK!!!\n");
        OsBackTrace();
        retErr = LOS_ERRNO_SEM_PEND_IN_LOCK;
        goto OUT;
    }

    runTask->taskSem = (VOID *)semPended;//标记当前任务在等这个信号量
    retErr = OsTaskWait(&semPended->semList, timeout, TRUE);//任务进入等待状态,当前任务会挂到semList上,并在其中切换任务上下文
    if (retErr == LOS_ERRNO_TSK_TIMEOUT) {//注意:这里是涉及到task切换的,把自己挂起,唤醒其他task 
        runTask->taskSem = NULL;
        retErr = LOS_ERRNO_SEM_TIMEOUT;
    }

OUT:
    SCHEDULER_UNLOCK(intSave);
    return retErr;
}
//释放信号
LITE_OS_SEC_TEXT UINT32 OsSemPostUnsafe(UINT32 semHandle, BOOL *needSched)
{
    LosSemCB *semPosted = NULL;
    LosTaskCB *resumedTask = NULL;

    if (GET_SEM_INDEX(semHandle) >= LOSCFG_BASE_IPC_SEM_LIMIT) {
        return LOS_ERRNO_SEM_INVALID;
    }

    semPosted = GET_SEM(semHandle);
    if ((semPosted->semID != semHandle) || (semPosted->semStat == OS_SEM_UNUSED)) {
        return LOS_ERRNO_SEM_INVALID;
    }

    /* Update the operate time, no matter the actual Post success or not */
    OsSemDbgTimeUpdateHook(semHandle);

    if (semPosted->semCount == OS_SEM_COUNT_MAX) {//当前信号资源不能大于最大资源量
        return LOS_ERRNO_SEM_OVERFLOW;
    }
    if (!LOS_ListEmpty(&semPosted->semList)) {//当前有任务挂在semList上,要去唤醒任务
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(semPosted->semList)));//semList上面挂的都是task->pendlist节点,取第一个task下来唤醒
        resumedTask->taskSem = NULL;//任务不用等信号了,重新变成NULL值
        OsTaskWake(resumedTask);//唤醒任务,注意resumedTask一定不是当前任务,OsTaskWake里面并不会自己切换任务上下文,只是设置状态
        if (needSched != NULL) {//参数不为空,就返回需要调度的标签
            *needSched = TRUE;//TRUE代表需要调度
        }
    } else {//当前没有任务挂在semList上,
        semPosted->semCount++;//信号资源多一个
    }

    return LOS_OK;
}

LITE_OS_SEC_TEXT UINT32 LOS_SemPost(UINT32 semHandle)
{
    UINT32 intSave;
    UINT32 ret;
    BOOL needSched = FALSE;

    SCHEDULER_LOCK(intSave);
    ret = OsSemPostUnsafe(semHandle, &needSched);
        SCHEDULER_UNLOCK(intSave);
    if (needSched) {//需要调度的情况
        LOS_MpSchedule(OS_MP_CPU_ALL);//向所有CPU发送调度指令
        LOS_Schedule();////发起调度
    }

    return ret;
}
#endif /* (LOSCFG_BASE_IPC_SEM == YES) */

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
