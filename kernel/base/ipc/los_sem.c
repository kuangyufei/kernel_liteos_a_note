/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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
 * @file    los_sem.c
 * @brief
 * @link  kernel-mini-basic-ipc-sem-basic http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-mini-basic-ipc-sem-basic.html @endlink
 @verbatim
	 信号量（Semaphore）是一种实现任务间通信的机制，可以实现任务间同步或共享资源的互斥访问。
	 
	 一个信号量的数据结构中，通常有一个计数值，用于对有效资源数的计数，表示剩下的可被使用的共享资源数，其值的含义分两种情况：
	 
	 	0，表示该信号量当前不可获取，因此可能存在正在等待该信号量的任务。
	 	正值，表示该信号量当前可被获取。
	 以同步为目的的信号量和以互斥为目的的信号量在使用上有如下不同：
	 
	 	用作互斥时，初始信号量计数值不为0，表示可用的共享资源个数。在需要使用共享资源前，先获取信号量，
		 	然后使用一个共享资源，使用完毕后释放信号量。这样在共享资源被取完，即信号量计数减至0时，其他需要获取信号量的任务将被阻塞，
		 	从而保证了共享资源的互斥访问。另外，当共享资源数为1时，建议使用二值信号量，一种类似于互斥锁的机制。
		用作同步时，初始信号量计数值为0。任务1获取信号量而阻塞，直到任务2或者某中断释放信号量，任务1才得以进入Ready或Running态，从而达到了任务间的同步。

	 信号量运作原理
		信号量初始化，为配置的N个信号量申请内存（N值可以由用户自行配置，通过LOSCFG_BASE_IPC_SEM_LIMIT宏实现），并把所有信号量初始化成未使用，
		加入到未使用链表中供系统使用。

		信号量创建，从未使用的信号量链表中获取一个信号量，并设定初值。

		信号量申请，若其计数器值大于0，则直接减1返回成功。否则任务阻塞，等待其它任务释放该信号量，等待的超时时间可设定。
		当任务被一个信号量阻塞时，将该任务挂到信号量等待任务队列的队尾。

		信号量释放，若没有任务等待该信号量，则直接将计数器加1返回。否则唤醒该信号量等待任务队列上的第一个任务。

		信号量删除，将正在使用的信号量置为未使用信号量，并挂回到未使用链表。

		信号量允许多个任务在同一时刻访问共享资源，但会限制同一时刻访问此资源的最大任务数目。当访问资源的任务数达到该资源允许的最大数量时，
		会阻塞其他试图获取该资源的任务，直到有任务释放该信号量。

	开发流程
		创建信号量LOS_SemCreate，若要创建二值信号量则调用LOS_BinarySemCreate。
		申请信号量LOS_SemPend。
		释放信号量LOS_SemPost。
		删除信号量LOS_SemDelete。
 @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/29/sem_run.png
 * @attention 由于中断不能被阻塞，因此不能在中断中使用阻塞模式申请信号量。
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-18
 */
#include "los_sem_pri.h"
#include "los_sem_debug_pri.h"
#include "los_err_pri.h"
#include "los_task_pri.h"
#include "los_exc.h"
#include "los_sched_pri.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_percpu_pri.h"
#include "los_hook.h"

#ifdef LOSCFG_BASE_IPC_SEM

#if (LOSCFG_BASE_IPC_SEM_LIMIT <= 0)  // 如果信号量最大数量配置小于等于0
#error "sem maxnum cannot be zero"  // 编译错误：信号量最大数量不能为零
#endif /* LOSCFG_BASE_IPC_SEM_LIMIT <= 0 */  // 条件编译结束
// @note 可用的信号量列表,其他地方用free，这里不用freeSemList? 可以看出这里是另一个人写的代码
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_unusedSemList;  // 未使用信号量链表，静态全局变量
LITE_OS_SEC_BSS LosSemCB *g_allSem = NULL;  // 所有信号量控制块数组指针，静态全局变量

/**
 * @brief 初始化信号量双向链表
 * @return LOS_OK 初始化成功；错误码 初始化失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsSemInit(VOID)  // 信号量初始化函数
{
    LosSemCB *semNode = NULL;  // 信号量控制块指针
    UINT32 index;  // 循环索引

    LOS_ListInit(&g_unusedSemList);  // 初始化未使用信号量链表
    /* system resident memory, don't free */  // 系统常驻内存，不释放
    g_allSem = (LosSemCB *)LOS_MemAlloc(m_aucSysMem0, (LOSCFG_BASE_IPC_SEM_LIMIT * sizeof(LosSemCB)));  // 分配信号量控制块数组内存
    if (g_allSem == NULL) {  // 如果内存分配失败
        return LOS_ERRNO_SEM_NO_MEMORY;  // 返回信号量无内存错误码
    }

    for (index = 0; index < LOSCFG_BASE_IPC_SEM_LIMIT; index++) {  // 遍历所有信号量控制块
        semNode = ((LosSemCB *)g_allSem) + index;  // 获取当前索引的信号量控制块
        semNode->semID = SET_SEM_ID(0, index);  // 设置信号量ID
        semNode->semStat = OS_SEM_UNUSED;  // 设置信号量状态为未使用
        LOS_ListTailInsert(&g_unusedSemList, &semNode->semList);  // 将信号量控制块插入未使用链表尾部
    }

    if (OsSemDbgInitHook() != LOS_OK) {  // 如果调试钩子初始化失败
        return LOS_ERRNO_SEM_NO_MEMORY;  // 返回信号量无内存错误码
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 创建信号量
 * @param count 信号量初始计数
 * @param maxCount 信号量最大计数
 * @param semHandle 输出参数，返回创建的信号量句柄
 * @return LOS_OK 创建成功；错误码 创建失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsSemCreate(UINT16 count, UINT16 maxCount, UINT32 *semHandle)  // 创建信号量函数
{
    UINT32 intSave;  // 中断状态保存变量
    LosSemCB *semCreated = NULL;  // 要创建的信号量控制块指针
    LOS_DL_LIST *unusedSem = NULL;  // 未使用信号量节点指针
    UINT32 errNo;  // 错误码
    UINT32 errLine;  // 错误行号

    if (semHandle == NULL) {  // 如果信号量句柄指针为空
        return LOS_ERRNO_SEM_PTR_NULL;  // 返回信号量指针为空错误码
    }

    if (count > maxCount) {  // 如果初始计数大于最大计数
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_OVERFLOW);  // 跳转到错误处理，溢出错误
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁，保存中断状态

    if (LOS_ListEmpty(&g_unusedSemList)) {  // 如果未使用信号量链表为空
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁
        OsSemInfoGetFullDataHook();  // 获取信号量已满信息钩子
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_ALL_BUSY);  // 跳转到错误处理，所有信号量繁忙
    }

    unusedSem = LOS_DL_LIST_FIRST(&g_unusedSemList);  // 获取未使用链表的第一个节点
    LOS_ListDelete(unusedSem);  // 从链表中删除该节点
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    semCreated = GET_SEM_LIST(unusedSem);  // 将节点指针转换为信号量控制块指针
    semCreated->semCount = count;  // 设置信号量初始计数
    semCreated->semStat = OS_SEM_USED;  // 设置信号量状态为已使用
    semCreated->maxSemCount = maxCount;  // 设置信号量最大计数
    LOS_ListInit(&semCreated->semList);  // 初始化信号量的等待任务链表
    *semHandle = semCreated->semID;  // 设置输出参数信号量句柄
    OsHookCall(LOS_HOOK_TYPE_SEM_CREATE, semCreated);  // 调用信号量创建钩子
    OsSemDbgUpdateHook(semCreated->semID, OsCurrTaskGet()->taskEntry, count);  // 更新调试信息
    return LOS_OK;  // 返回成功

ERR_HANDLER:  // 错误处理标签
    OS_RETURN_ERROR_P2(errLine, errNo);  // 返回错误码和错误行号
}

/**
 * @brief 创建计数信号量（默认最大计数为OS_SEM_COUNT_MAX）
 * @param count 信号量初始计数
 * @param semHandle 输出参数，返回创建的信号量句柄
 * @return LOS_OK 创建成功；错误码 创建失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_SemCreate(UINT16 count, UINT32 *semHandle)  // 计数信号量创建函数
{
    return OsSemCreate(count, OS_SEM_COUNT_MAX, semHandle);  // 调用通用信号量创建函数
}

/**
 * @brief 创建二进制信号量（最大计数为OS_SEM_BINARY_COUNT_MAX）
 * @param count 信号量初始计数（0或1）
 * @param semHandle 输出参数，返回创建的信号量句柄
 * @return LOS_OK 创建成功；错误码 创建失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_BinarySemCreate(UINT16 count, UINT32 *semHandle)  // 二进制信号量创建函数
{
    return OsSemCreate(count, OS_SEM_BINARY_COUNT_MAX, semHandle);  // 调用通用信号量创建函数
}

/**
 * @brief 删除信号量
 * @param semHandle 要删除的信号量句柄
 * @return LOS_OK 删除成功；错误码 删除失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_SemDelete(UINT32 semHandle)  // 信号量删除函数
{
    UINT32 intSave;  // 中断状态保存变量
    LosSemCB *semDeleted = NULL;  // 要删除的信号量控制块指针
    UINT32 errNo;  // 错误码
    UINT32 errLine;  // 错误行号

    if (GET_SEM_INDEX(semHandle) >= (UINT32)LOSCFG_BASE_IPC_SEM_LIMIT) {  // 如果信号量索引超出范围
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_INVALID);  // 跳转到错误处理，信号量无效
    }

    semDeleted = GET_SEM(semHandle);  // 获取信号量控制块指针

    SCHEDULER_LOCK(intSave);  // 调度器加锁，保存中断状态

    if ((semDeleted->semStat == OS_SEM_UNUSED) || (semDeleted->semID != semHandle)) {  // 如果信号量未使用或ID不匹配
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_INVALID);  // 跳转到错误处理，信号量无效
    }

    if (!LOS_ListEmpty(&semDeleted->semList)) {  // 如果信号量的等待任务链表非空
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁
        OS_GOTO_ERR_HANDLER(LOS_ERRNO_SEM_PENDED);  // 跳转到错误处理，信号量有等待任务
    }

    LOS_ListTailInsert(&g_unusedSemList, &semDeleted->semList);  // 将信号量控制块插入未使用链表尾部
    semDeleted->semStat = OS_SEM_UNUSED;  // 设置信号量状态为未使用
    semDeleted->semID = SET_SEM_ID(GET_SEM_COUNT(semDeleted->semID) + 1, GET_SEM_INDEX(semDeleted->semID));  // 更新信号量ID版本号

    OsHookCall(LOS_HOOK_TYPE_SEM_DELETE, semDeleted);  // 调用信号量删除钩子
    OsSemDbgUpdateHook(semDeleted->semID, NULL, 0);  // 更新调试信息

    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return LOS_OK;  // 返回成功

ERR_HANDLER:  // 错误处理标签
    OS_RETURN_ERROR_P2(errLine, errNo);  // 返回错误码和错误行号
}

/**
 * @brief 等待信号量（P操作）
 * @param semHandle 信号量句柄
 * @param timeout 超时时间（单位：系统滴答）
 * @return LOS_OK 成功获取信号量；错误码 获取失败
 */
LITE_OS_SEC_TEXT UINT32 LOS_SemPend(UINT32 semHandle, UINT32 timeout)  // 信号量等待函数
{
    UINT32 intSave;  // 中断状态保存变量
    LosSemCB *semPended = GET_SEM(semHandle);  // 信号量控制块指针
    UINT32 retErr = LOS_OK;  // 返回错误码，默认为成功
    LosTaskCB *runTask = NULL;  // 当前运行任务控制块指针

    if (GET_SEM_INDEX(semHandle) >= (UINT32)LOSCFG_BASE_IPC_SEM_LIMIT) {  // 如果信号量索引超出范围
        OS_RETURN_ERROR(LOS_ERRNO_SEM_INVALID);  // 返回信号量无效错误码
    }

    if (OS_INT_ACTIVE) {  // 如果当前在中断上下文中
        PRINT_ERR("!!!LOS_ERRNO_SEM_PEND_INTERR!!!\n");  // 打印中断中等待错误
        OsBackTrace();  // 打印调用栈
        return LOS_ERRNO_SEM_PEND_INTERR;  // 返回中断中等待错误码
    }

    runTask = OsCurrTaskGet();  // 获取当前运行任务
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 如果是系统任务
        OsBackTrace();  // 打印调用栈
        return LOS_ERRNO_SEM_PEND_IN_SYSTEM_TASK;  // 返回系统任务等待错误码
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁，保存中断状态

    if ((semPended->semStat == OS_SEM_UNUSED) || (semPended->semID != semHandle)) {  // 如果信号量未使用或ID不匹配
        retErr = LOS_ERRNO_SEM_INVALID;  // 设置返回错误码为信号量无效
        goto OUT;  // 跳转到退出标签
    }
    /* Update the operate time, no matter the actual Pend success or not */  // 更新操作时间，无论等待是否成功
    OsSemDbgTimeUpdateHook(semHandle);  // 更新调试时间戳

    if (semPended->semCount > 0) {  // 如果信号量计数大于0
        semPended->semCount--;  // 信号量计数减1
        OsHookCall(LOS_HOOK_TYPE_SEM_PEND, semPended, runTask, timeout);  // 调用信号量等待钩子
        goto OUT;  // 跳转到退出标签
    } else if (!timeout) {  // 如果超时时间为0（非阻塞）
        retErr = LOS_ERRNO_SEM_UNAVAILABLE;  // 设置返回错误码为信号量不可用
        goto OUT;  // 跳转到退出标签
    }

    if (!OsPreemptableInSched()) {  // 如果当前不可抢占
        PRINT_ERR("!!!LOS_ERRNO_SEM_PEND_IN_LOCK!!!\n");  // 打印锁中等待错误
        OsBackTrace();  // 打印调用栈
        retErr = LOS_ERRNO_SEM_PEND_IN_LOCK;  // 设置返回错误码为锁中等待
        goto OUT;  // 跳转到退出标签
    }

    OsHookCall(LOS_HOOK_TYPE_SEM_PEND, semPended, runTask, timeout);  // 调用信号量等待钩子
    OsTaskWaitSetPendMask(OS_TASK_WAIT_SEM, semPended->semID, timeout);  // 设置任务等待掩码
    retErr = runTask->ops->wait(runTask, &semPended->semList, timeout);  // 将任务加入等待链表
    if (retErr == LOS_ERRNO_TSK_TIMEOUT) {  // 如果返回任务超时错误
        retErr = LOS_ERRNO_SEM_TIMEOUT;  // 转换为信号量超时错误码
    }

OUT:  // 退出标签
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return retErr;  // 返回错误码
}

/**
 * @brief 不安全的信号量释放（V操作），需外部加锁
 * @param semHandle 信号量句柄
 * @param needSched 输出参数，是否需要调度
 * @return LOS_OK 释放成功；错误码 释放失败
 */
LITE_OS_SEC_TEXT UINT32 OsSemPostUnsafe(UINT32 semHandle, BOOL *needSched)  // 不安全的信号量释放函数
{
    LosTaskCB *resumedTask = NULL;  // 要唤醒的任务控制块指针
    LosSemCB *semPosted = GET_SEM(semHandle);  // 信号量控制块指针
    if ((semPosted->semID != semHandle) || (semPosted->semStat == OS_SEM_UNUSED)) {  // 如果信号量ID不匹配或未使用
        return LOS_ERRNO_SEM_INVALID;  // 返回信号量无效错误码
    }

    /* Update the operate time, no matter the actual Post success or not */  // 更新操作时间，无论释放是否成功
    OsSemDbgTimeUpdateHook(semHandle);  // 更新调试时间戳

    if (semPosted->semCount == OS_SEM_COUNT_MAX) {  // 如果信号量计数达到最大值
        return LOS_ERRNO_SEM_OVERFLOW;  // 返回信号量溢出错误码
    }
    if (!LOS_ListEmpty(&semPosted->semList)) {  // 如果信号量的等待任务链表非空
        resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(semPosted->semList)));  // 获取第一个等待任务
        OsTaskWakeClearPendMask(resumedTask);  // 清除任务等待掩码
        resumedTask->ops->wake(resumedTask);  // 唤醒任务
        if (needSched != NULL) {  // 如果需要调度标志指针非空
            *needSched = TRUE;  // 设置需要调度
        }
    } else {  // 如果等待任务链表为空
        semPosted->semCount++;  // 信号量计数加1
    }
    OsHookCall(LOS_HOOK_TYPE_SEM_POST, semPosted, resumedTask);  // 调用信号量释放钩子
    return LOS_OK;  // 返回成功
}

/**
 * @brief 安全的信号量释放（V操作），内部加锁
 * @param semHandle 信号量句柄
 * @return LOS_OK 释放成功；错误码 释放失败
 */
LITE_OS_SEC_TEXT UINT32 LOS_SemPost(UINT32 semHandle)  // 信号量释放函数
{
    UINT32 intSave;  // 中断状态保存变量
    UINT32 ret;  // 返回值
    BOOL needSched = FALSE;  // 是否需要调度标志

    if (GET_SEM_INDEX(semHandle) >= LOSCFG_BASE_IPC_SEM_LIMIT) {  // 如果信号量索引超出范围
        return LOS_ERRNO_SEM_INVALID;  // 返回信号量无效错误码
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁，保存中断状态
    ret = OsSemPostUnsafe(semHandle, &needSched);  // 调用不安全的信号量释放函数
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    if (needSched) {  // 如果需要调度
        LOS_MpSchedule(OS_MP_CPU_ALL);  // 多CPU调度
        LOS_Schedule();  // 触发调度
    }

    return ret;  // 返回结果
}
#endif /* LOSCFG_BASE_IPC_SEM */

