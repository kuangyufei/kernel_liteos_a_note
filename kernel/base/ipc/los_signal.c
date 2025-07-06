/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_signal.h"
#include "pthread.h"
#include "los_process_pri.h"
#include "los_sched_pri.h"
#include "los_hw_pri.h"
#include "user_copy.h"
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_api.h"
#endif
#include "los_atomic.h"

#ifdef LOSCFG_KERNEL_VM
/**
 * @brief 发送信号给当前进程（未实现）
 * @details 该函数用于向当前进程发送指定信号，但当前版本未支持此功能
 * @param sig 要发送的信号编号
 * @return 始终返回-1表示不支持
 */
int raise(int sig)
{
    (VOID)sig;  // 未使用的参数，避免编译警告
    PRINT_ERR("%s NOT SUPPORT\n", __FUNCTION__);  // 打印不支持的错误信息
    errno = ENOSYS;  // 设置错误号为功能未实现
    return -1;  // 返回-1表示失败
}

#define GETUNMASKSET(procmask, pendFlag) ((~(procmask)) & (sigset_t)(pendFlag))  // 计算未屏蔽的待处理信号集
#define UINT64_BIT_SIZE 64  // 64位无符号整数的位数

/**
 * @brief 检查信号是否在信号集中
 * @details 判断指定的信号是否存在于给定的信号集中
 * @param set 指向信号集的指针
 * @param signo 要检查的信号编号
 * @return 1表示信号在集中，0表示不在，LOS_NOK表示信号无效
 */
int OsSigIsMember(const sigset_t *set, int signo)
{
    int ret = LOS_NOK;  // 初始化返回值为失败
    /* In musl, sig No bits 00000100 present sig No 3, but  1<< 3 = 00001000, so signo needs minus 1 */
    signo -= 1;  // 调整信号编号以匹配musl库的位表示
    /* Verify the signal */
    if (GOOD_SIGNO(signo)) {  // 检查信号编号是否有效
        /* Check if the signal is in the set */
        ret = ((*set & SIGNO2SET((unsigned int)signo)) != 0);  // 检查信号对应的位是否被设置
    }

    return ret;  // 返回检查结果
}

/**
 * @brief 将临时信号信息移动到未阻塞信息
 * @details 从临时信号信息列表中查找指定信号并移动到未阻塞信号信息结构
 * @param sigcb 指向信号控制块的指针
 * @param signo 要查找的信号编号
 */
STATIC VOID OsMoveTmpInfoToUnbInfo(sig_cb *sigcb, INT32 signo)
{
    SigInfoListNode *tmpInfoNode = sigcb->tmpInfoListHead;  // 临时信号信息列表头指针
    SigInfoListNode **prevHook = &sigcb->tmpInfoListHead;  // 前一个节点的钩子指针
    while (tmpInfoNode != NULL) {  // 遍历临时信号信息列表
        if (tmpInfoNode->info.si_signo == signo) {  // 找到匹配的信号
            /* copy tmpinfo to unbinfo. */
            (VOID)memcpy_s(&sigcb->sigunbinfo, sizeof(siginfo_t), &tmpInfoNode->info, sizeof(siginfo_t));  // 复制信号信息
            /* delete tmpinfo from tmpList. */
            *prevHook = tmpInfoNode->next;  // 从列表中移除节点
            (VOID)LOS_MemFree(m_aucSysMem0, tmpInfoNode);  // 释放节点内存
            break;  // 跳出循环
        }
        prevHook = &tmpInfoNode->next;  // 更新前一个节点钩子
        tmpInfoNode = tmpInfoNode->next;  // 移动到下一个节点
    }
}

/**
 * @brief 添加信号信息到临时列表
 * @details 将信号信息添加到临时信号信息列表，如果信号已存在则更新
 * @param sigcb 指向信号控制块的指针
 * @param info 指向要添加的信号信息的指针
 * @return LOS_OK表示成功，LOS_NOK表示内存分配失败
 */
STATIC INT32 OsAddSigInfoToTmpList(sig_cb *sigcb, siginfo_t *info)
{
    /* try to find the old siginfo */
    SigInfoListNode *tmp = sigcb->tmpInfoListHead;  // 临时信号信息列表头指针
    while (tmp != NULL) {  // 遍历列表查找已有信号
        if (tmp->info.si_signo == info->si_signo) {  // 找到相同信号
            /* found it, break. */
            break;  // 跳出循环
        }
        tmp = tmp->next;  // 移动到下一个节点
    }

    if (tmp == NULL) {  // 如果未找到信号
        /* none, alloc new one */
        tmp = (SigInfoListNode *)LOS_MemAlloc(m_aucSysMem0, sizeof(SigInfoListNode));  // 分配新节点内存
        if (tmp == NULL) {  // 内存分配失败
            return LOS_NOK;  // 返回失败
        }
        tmp->next = sigcb->tmpInfoListHead;  // 新节点指向当前列表头
        sigcb->tmpInfoListHead = tmp;  // 更新列表头为新节点
    }

    (VOID)memcpy_s(&tmp->info, sizeof(siginfo_t), info, sizeof(siginfo_t));  // 复制信号信息到节点

    return LOS_OK;  // 返回成功
}

/**
 * @brief 清除临时信号信息列表
 * @details 释放临时信号信息列表中的所有节点内存
 * @param sigcb 指向信号控制块的指针
 */
VOID OsClearSigInfoTmpList(sig_cb *sigcb)
{
    while (sigcb->tmpInfoListHead != NULL) {  // 遍历临时信号信息列表
        SigInfoListNode *tmpInfoNode = sigcb->tmpInfoListHead;  // 当前节点指针
        sigcb->tmpInfoListHead = sigcb->tmpInfoListHead->next;  // 更新列表头
        (VOID)LOS_MemFree(m_aucSysMem0, tmpInfoNode);  // 释放当前节点内存
    }
}

/**
 * @brief 唤醒等待信号的任务
 * @details 如果任务正在等待指定信号，则唤醒任务并清除等待状态
 * @param taskCB 指向任务控制块的指针
 * @param signo 要处理的信号编号
 */
STATIC INLINE VOID OsSigWaitTaskWake(LosTaskCB *taskCB, INT32 signo)
{
    sig_cb *sigcb = &taskCB->sig;  // 获取任务的信号控制块

    if (!LOS_ListEmpty(&sigcb->waitList) && OsSigIsMember(&sigcb->sigwaitmask, signo)) {  // 如果任务正在等待信号且信号在等待集中
        OsMoveTmpInfoToUnbInfo(sigcb, signo);  // 移动信号信息到未阻塞信息
        OsTaskWakeClearPendMask(taskCB);  // 清除任务的等待掩码并唤醒
        taskCB->ops->wake(taskCB);  // 调用唤醒操作
        OsSigEmptySet(&sigcb->sigwaitmask);  // 清空信号等待集
    }
}

/**
 * @brief 唤醒等待中的任务
 * @details 根据信号类型和任务等待状态唤醒相应的等待任务
 * @param taskCB 指向任务控制块的指针
 * @param signo 要处理的信号编号
 * @return 0表示成功
 */
STATIC UINT32 OsPendingTaskWake(LosTaskCB *taskCB, INT32 signo)
{
    if (!OsTaskIsPending(taskCB) || !OsProcessIsUserMode(OS_PCB_FROM_TCB(taskCB))) {  // 如果任务未在等待或不是用户模式进程
        return 0;  // 返回0
    }

    if ((signo != SIGKILL) && (taskCB->waitFlag != OS_TASK_WAIT_SIGNAL)) {  // 如果不是SIGKILL信号且任务不是等待信号
        return 0;  // 返回0
    }

    switch (taskCB->waitFlag) {  // 根据任务等待标志处理
        case OS_TASK_WAIT_PROCESS:  // 等待进程
        case OS_TASK_WAIT_GID:  // 等待组ID
        case OS_TASK_WAIT_ANYPROCESS:  // 等待任意进程
            OsWaitWakeTask(taskCB, OS_INVALID_VALUE);  // 唤醒等待任务
            break;  // 跳出switch
        case OS_TASK_WAIT_JOIN:  // 等待连接
            OsTaskWakeClearPendMask(taskCB);  // 清除等待掩码并唤醒
            taskCB->ops->wake(taskCB);  // 调用唤醒操作
            break;  // 跳出switch
        case OS_TASK_WAIT_SIGNAL:  // 等待信号
            OsSigWaitTaskWake(taskCB, signo);  // 唤醒等待信号的任务
            break;  // 跳出switch
        case OS_TASK_WAIT_LITEIPC:  // 等待轻量级IPC
            OsTaskWakeClearPendMask(taskCB);  // 清除等待掩码并唤醒
            taskCB->ops->wake(taskCB);  // 调用唤醒操作
            break;  // 跳出switch
        case OS_TASK_WAIT_FUTEX:  // 等待Futex
            OsFutexNodeDeleteFromFutexHash(&taskCB->futex, TRUE, NULL, NULL);  // 从Futex哈希中删除节点
            OsTaskWakeClearPendMask(taskCB);  // 清除等待掩码并唤醒
            taskCB->ops->wake(taskCB);  // 调用唤醒操作
            break;  // 跳出switch
        default:  // 默认情况
            break;  // 跳出switch
    }

    return 0;  // 返回0
}

/**
 * @brief 向任务控制块分发信号
 * @details 处理信号分发逻辑，包括信号屏蔽检查和等待任务唤醒
 * @param stcb 指向任务控制块的指针
 * @param info 指向信号信息的指针
 * @return 0表示成功，-ENOMEM表示内存分配失败
 */
int OsTcbDispatch(LosTaskCB *stcb, siginfo_t *info)
{
    bool masked = FALSE;  // 信号是否被屏蔽的标志
    sig_cb *sigcb = &stcb->sig;  // 获取任务的信号控制块

    OS_RETURN_IF_NULL(sigcb);  // 如果信号控制块为空则返回
    /* If signo is 0, not send signal, just check process or pthread exist */
    if (info->si_signo == 0) {  // 如果信号编号为0
        return 0;  // 返回0，不发送信号
    }
    masked = (bool)OsSigIsMember(&sigcb->sigprocmask, info->si_signo);  // 检查信号是否被屏蔽
    if (masked) {  // 如果信号被屏蔽
        /* If signal is in wait list and mask list, need unblock it */
        if (LOS_ListEmpty(&sigcb->waitList)  ||  // 如果等待列表为空
            (!LOS_ListEmpty(&sigcb->waitList) && !OsSigIsMember(&sigcb->sigwaitmask, info->si_signo))) {  // 或等待列表非空但信号不在等待集中
            OsSigAddSet(&sigcb->sigPendFlag, info->si_signo);  // 将信号添加到待处理标志
        }
    } else {  // 如果信号未被屏蔽
        /* unmasked signal actions */
        OsSigAddSet(&sigcb->sigFlag, info->si_signo);  // 将信号添加到信号标志
    }

    if (OsAddSigInfoToTmpList(sigcb, info) == LOS_NOK) {  // 添加信号信息到临时列表
        return -ENOMEM;  // 如果失败返回内存不足错误
    }

    return OsPendingTaskWake(stcb, info->si_signo);  // 唤醒等待任务并返回结果
}

/**
 * @brief 切换信号掩码
 * @details 更新任务的信号掩码并处理未屏蔽的待处理信号
 * @param rtcb 指向当前任务控制块的指针
 * @param set 新的信号掩码
 */
void OsSigMaskSwitch(LosTaskCB * const rtcb, sigset_t set)
{
    sigset_t unmaskset;  // 未屏蔽的信号集

    rtcb->sig.sigprocmask = set;  // 更新信号掩码
    unmaskset = GETUNMASKSET(rtcb->sig.sigprocmask, rtcb->sig.sigPendFlag);  // 计算未屏蔽的待处理信号
    if (unmaskset != NULL_SIGNAL_SET) {  // 如果存在未屏蔽的待处理信号
        /* pendlist do */
        rtcb->sig.sigFlag |= unmaskset;  // 将未屏蔽信号添加到信号标志
        rtcb->sig.sigPendFlag ^= unmaskset;  // 从未处理标志中清除这些信号
    }
}

/**
 * @brief 设置或获取信号掩码
 * @details 更改或检查当前进程的信号掩码
 * @param how 操作类型(SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK)
 * @param setl 指向新信号集的指针，NULL表示不更改
 * @param oldsetl 指向用于存储旧信号集的指针，NULL表示不获取
 * @return LOS_OK表示成功，-EINVAL表示参数无效
 */
int OsSigprocMask(int how, const sigset_t_l *setl, sigset_t_l *oldsetl)
{
    LosTaskCB *spcb = NULL;  // 当前任务控制块指针
    int ret = LOS_OK;  // 返回值
    unsigned int intSave;  // 中断保存变量
    sigset_t set;  // 信号集

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    spcb = OsCurrTaskGet();  // 获取当前任务
    /* If requested, copy the old mask to user. */
    if (oldsetl != NULL) {  // 如果需要获取旧信号掩码
        *(sigset_t *)oldsetl = spcb->sig.sigprocmask;  // 复制旧信号掩码
    }
    /* If requested, modify the current signal mask. */
    if (setl != NULL) {  // 如果需要设置新信号掩码
        set = *(sigset_t *)setl;  // 获取新信号集
        /* Okay, determine what we are supposed to do */
        switch (how) {  // 根据操作类型处理
            /* Set the union of the current set and the signal
             * set pointed to by set as the new sigprocmask. */
            case SIG_BLOCK:  // 阻塞信号
                spcb->sig.sigprocmask |= set;  // 当前掩码与新掩码取或
                break;  // 跳出switch
            /* Set the intersection of the current set and the
             * signal set pointed to by set as the new sigprocmask. */
            case SIG_UNBLOCK:  // 解除阻塞
                spcb->sig.sigprocmask &= ~(set);  // 当前掩码与新掩码的非取与
                break;  // 跳出switch
            /* Set the signal set pointed to by set as the new sigprocmask. */
            case SIG_SETMASK:  // 设置掩码
                spcb->sig.sigprocmask = set;  // 直接设置为新掩码
                break;  // 跳出switch
            default:  // 无效操作
                ret = -EINVAL;  // 返回参数无效错误
                break;  // 跳出switch
        }
        /* If pending mask not in sigmask, need set sigflag. */
        OsSigMaskSwitch(spcb, spcb->sig.sigprocmask);  // 切换信号掩码
    }
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁

    return ret;  // 返回结果
}

/**
 * @brief 遍历进程的所有子任务
 * @details 对进程的每个子任务调用指定的处理函数
 * @param spcb 指向进程控制块的指针
 * @param handler 处理函数指针
 * @param arg 传递给处理函数的参数
 * @return LOS_OK表示成功，其他值表示失败
 */
int OsSigProcessForeachChild(LosProcessCB *spcb, ForEachTaskCB handler, void *arg)
{
    int ret;  // 返回值

    /* Visit the main thread last (if present) */
    LosTaskCB *taskCB = NULL;  // 任务控制块指针
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &(spcb->threadSiblingList), LosTaskCB, threadList) {  // 遍历线程列表
        ret = handler(taskCB, arg);  // 调用处理函数
        OS_RETURN_IF(ret != 0, ret);  // 如果处理函数返回非0则返回
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 进程信号处理函数
 * @details 处理发送给进程的信号，唤醒等待信号的任务
 * @param tcb 指向任务控制块的指针
 * @param arg 指向ProcessSignalInfo结构的指针
 * @return 0表示继续遍历，其他值表示停止遍历
 */
static int SigProcessSignalHandler(LosTaskCB *tcb, void *arg)
{
    struct ProcessSignalInfo *info = (struct ProcessSignalInfo *)arg;  // 信号信息结构指针
    int ret;  // 返回值
    int isMember;  // 是否为成员的标志

    if (tcb == NULL) {  // 如果任务控制块为空
        return 0;  // 返回0
    }

    /* If the default tcb is not set, then set this one as default. */
    if (!info->defaultTcb) {  // 如果默认任务控制块未设置
        info->defaultTcb = tcb;  // 设置为当前任务
    }

    isMember = OsSigIsMember(&tcb->sig.sigwaitmask, info->sigInfo->si_signo);  // 检查信号是否在等待集中
    if (isMember && (!info->awakenedTcb)) {  // 如果信号在等待集中且未唤醒任务
        /* This means the task is waiting for this signal. Stop looking for it and use this tcb.
         * The requirement is: if more than one task in this task group is waiting for the signal,
         * then only one indeterminate task in the group will receive the signal. */
        ret = OsTcbDispatch(tcb, info->sigInfo);  // 分发信号到任务
        OS_RETURN_IF(ret < 0, ret);  // 如果失败返回错误

        /* set this tcb as awakenedTcb */
        info->awakenedTcb = tcb;  // 设置唤醒的任务
        OS_RETURN_IF(info->receivedTcb != NULL, SIG_STOP_VISIT);  // 如果已接收任务则停止访问
    }
    /* Is this signal unblocked on this thread? */
    isMember = OsSigIsMember(&tcb->sig.sigprocmask, info->sigInfo->si_signo);  // 检查信号是否未被屏蔽
    if ((!isMember) && (!info->receivedTcb) && (tcb != info->awakenedTcb)) {  // 如果信号未被屏蔽且未接收任务且不是已唤醒任务
        /* if unblockedTcb of this signal is not set, then set it. */
        if (!info->unblockedTcb) {  // 如果未设置未屏蔽任务
            info->unblockedTcb = tcb;  // 设置为当前任务
        }

        ret = OsTcbDispatch(tcb, info->sigInfo);  // 分发信号到任务
        OS_RETURN_IF(ret < 0, ret);  // 如果失败返回错误
        /* set this tcb as receivedTcb */
        info->receivedTcb = tcb;  // 设置接收任务
        OS_RETURN_IF(info->awakenedTcb != NULL, SIG_STOP_VISIT);  // 如果已唤醒任务则停止访问
    }
    return 0;  /* Keep searching */  // 返回0继续搜索
}

/**
 * @brief SIGKILL信号处理函数
 * @details 处理SIGKILL信号，唤醒相关等待任务
 * @param tcb 指向任务控制块的指针
 * @param arg 指向ProcessSignalInfo结构的指针
 * @return 0表示成功
 */
static int SigProcessKillSigHandler(LosTaskCB *tcb, void *arg)
{
    struct ProcessSignalInfo *info = (struct ProcessSignalInfo *)arg;  // 信号信息结构指针

    return OsPendingTaskWake(tcb, info->sigInfo->si_signo);  // 唤醒等待任务并返回结果
}

/**
 * @brief 加载任务控制块处理信号
 * @details 选择合适的任务控制块并分发信号
 * @param info 指向ProcessSignalInfo结构的指针
 * @param sigInfo 指向信号信息的指针
 */
static void SigProcessLoadTcb(struct ProcessSignalInfo *info, siginfo_t *sigInfo)
{
    LosTaskCB *tcb = NULL;  // 任务控制块指针

    if (info->awakenedTcb == NULL && info->receivedTcb == NULL) {  // 如果未唤醒任务且未接收任务
        if (info->unblockedTcb) {  // 如果存在未屏蔽任务
            tcb = info->unblockedTcb;  // 使用未屏蔽任务
        } else if (info->defaultTcb) {  // 如果存在默认任务
            tcb = info->defaultTcb;  // 使用默认任务
        } else {  // 否则
            return;  // 返回
        }
        /* Deliver the signal to the selected task */
        (void)OsTcbDispatch(tcb, sigInfo);  // 分发信号到选中的任务
    }
}

/**
 * @brief 向进程发送信号
 * @details 将信号发送给指定进程的适当任务
 * @param spcb 指向进程控制块的指针
 * @param sigInfo 指向信号信息的指针
 * @return 0表示成功，-EFAULT表示参数无效
 */
int OsSigProcessSend(LosProcessCB *spcb, siginfo_t *sigInfo)
{
    int ret;  // 返回值
    struct ProcessSignalInfo info = {  // 进程信号信息结构初始化
        .sigInfo = sigInfo,
        .defaultTcb = NULL,
        .unblockedTcb = NULL,
        .awakenedTcb = NULL,
        .receivedTcb = NULL
    };

    if (info.sigInfo == NULL) {  // 如果信号信息为空
        return -EFAULT;  // 返回参数错误
    }

    /* visit all taskcb and dispatch signal */
    if (info.sigInfo->si_signo == SIGKILL) {  // 如果是SIGKILL信号
        OsSigAddSet(&spcb->sigShare, info.sigInfo->si_signo);  // 添加到共享信号集
        (void)OsSigProcessForeachChild(spcb, SigProcessKillSigHandler, &info);  // 遍历子任务处理
        return 0;  // 返回成功
    } else {  // 其他信号
        ret = OsSigProcessForeachChild(spcb, SigProcessSignalHandler, &info);  // 遍历子任务处理
    }
    if (ret < 0) {  // 如果处理失败
        return ret;  // 返回错误
    }
    SigProcessLoadTcb(&info, sigInfo);  // 加载任务控制块处理信号
    return 0;  // 返回成功
}

/**
 * @brief 清空信号集
 * @details 将指定的信号集初始化为空集
 * @param set 指向信号集的指针
 * @return 0表示成功
 */
int OsSigEmptySet(sigset_t *set)
{
    *set = NULL_SIGNAL_SET;  // 将信号集设置为空
    return 0;  // 返回成功
}

/* Privilege process can't send to kernel and privilege process */
/**
 * @brief 检查信号发送权限
 * @details 验证是否允许向指定进程发送信号
 * @param spcb 指向进程控制块的指针
 * @return 0表示有权限，-EPERM表示无权限
 */
static int OsSignalPermissionToCheck(const LosProcessCB *spcb)
{
    UINTPTR gid = (UINTPTR)OS_GET_PGROUP_LEADER(spcb->pgroup);  // 获取进程组领导ID
    if (gid == OS_KERNEL_PROCESS_GROUP) {  // 如果是内核进程组
        return -EPERM;  // 返回无权限
    } else if (gid == OS_USER_PRIVILEGE_PROCESS_GROUP) {  // 如果是用户特权进程组
        return -EPERM;  // 返回无权限
    }

    return 0;  // 返回有权限
}

/**
 * @brief 发送信号权限检查
 * @details 综合检查发送信号的各种权限条件
 * @param spcb 指向进程控制块的指针
 * @param permission 权限类型
 * @return LOS_OK表示有权限，其他值表示无权限
 */
STATIC int SendSigPermissionCheck(LosProcessCB *spcb, int permission)
{
    if (spcb == NULL) {  // 如果进程控制块为空
        return -ESRCH;  // 返回进程不存在
    }

    if (OsProcessIsUnused(spcb)) {  // 如果进程未使用
        return -ESRCH;  // 返回进程不存在
    }

#ifdef LOSCFG_SECURITY_CAPABILITY
    LosProcessCB *current = OsCurrProcessGet();  // 获取当前进程
    /* Kernel process always has kill permission and user process should check permission */
    if (OsProcessIsUserMode(current) && !(current->processStatus & OS_PROCESS_FLAG_EXIT)) {  // 如果是用户模式且未退出
        if ((current != spcb) && (!IsCapPermit(CAP_KILL)) && (current->user->userID != spcb->user->userID)) {  // 权限检查
            return -EPERM;  // 返回无权限
        }
    }
#endif
    if ((permission == OS_USER_KILL_PERMISSION) && (OsSignalPermissionToCheck(spcb) < 0)) {  // 检查用户杀死权限
        return -EPERM;  // 返回无权限
    }
    return LOS_OK;  // 返回有权限
}
/**
 * @brief 向进程发送信号
 * @details 检查权限后向指定进程发送信号，支持处理非活动进程
 * @param spcb 进程控制块指针
 * @param sig 信号编号
 * @param permission 权限标志
 * @return 成功返回LOS_OK，失败返回错误码
 */
int OsSendSigToProcess(LosProcessCB *spcb, int sig, int permission)
{
    siginfo_t info;  // 信号信息结构体
    int ret = SendSigPermissionCheck(spcb, permission);  // 检查发送权限
    if (ret != LOS_OK) {  // 权限检查失败
        return ret;  // 返回错误码
    }

    /* If the process you want to kill had been inactive, but still exist. should return LOS_OK */
    if (OsProcessIsInactive(spcb)) {  // 进程已非活动但存在
        return LOS_OK;  // 返回成功
    }

    if (!GOOD_SIGNO(sig)) {  // 信号编号无效
        return -EINVAL;  // 返回参数错误
    }

    info.si_signo = sig;  // 设置信号编号
    info.si_code = SI_USER;  // 设置信号来源为用户
    info.si_value.sival_ptr = NULL;  // 信号附加数据为空

    return OsSigProcessSend(spcb, &info);  // 发送信号并返回结果
}

/**
 * @brief 向指定进程分发信号
 * @details 根据进程ID查找进程并发送信号，支持权限检查
 * @param pid 进程ID
 * @param info 信号信息指针
 * @param permission 权限标志
 * @return 成功返回LOS_OK，失败返回错误码
 */
int OsDispatch(pid_t pid, siginfo_t *info, int permission)
{
    if (OsProcessIDUserCheckInvalid(pid) || pid < 0) {  // 进程ID无效或为负
        return -ESRCH;  // 返回进程不存在
    }

    LosProcessCB *spcb = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    int ret = SendSigPermissionCheck(spcb, permission);  // 检查发送权限
    if (ret != LOS_OK) {  // 权限检查失败
        return ret;  // 返回错误码
    }

    /* If the process you want to kill had been inactive, but still exist. should return LOS_OK */
    if (OsProcessIsInactive(spcb)) {  // 进程已非活动但存在
        return LOS_OK;  // 返回成功
    }

    return OsSigProcessSend(spcb, info);  // 发送信号并返回结果
}

/**
 * @brief 向进程发送信号的入口函数
 * @details 支持向指定进程、所有进程或进程组发送信号
 * @param pid 进程ID，正数表示指定进程，-1表示所有进程，其他负数表示进程组
 * @param sig 信号编号
 * @param permission 权限标志
 * @return 成功返回LOS_OK，失败返回错误码
 */
int OsKill(pid_t pid, int sig, int permission)
{
    siginfo_t info;  // 信号信息结构体
    int ret;  // 返回值

    /* Make sure that the para is valid */
    if (!GOOD_SIGNO(sig)) {  // 信号编号无效
        return -EINVAL;  // 返回参数错误
    }

    /* Create the siginfo structure */
    info.si_signo = sig;  // 设置信号编号
    info.si_code = SI_USER;  // 设置信号来源为用户
    info.si_value.sival_ptr = NULL;  // 信号附加数据为空

    if (pid > 0) {  // 向指定进程发送
        /* Send the signal to the specify process */
        ret = OsDispatch(pid, &info, permission);  // 调用分发函数
    } else if (pid == -1) {  // 向所有进程发送
        /* Send SIG to all processes */
        ret = OsSendSignalToAllProcess(&info, permission);  // 发送给所有进程
    } else {  // 向进程组发送
        /* Send SIG to all processes in process group PGRP.
           If PGRP is zero, send SIG to all processes in
           the current process's process group. */
        ret = OsSendSignalToProcessGroup(pid, &info, permission);  // 发送给进程组
    }
    return ret;  // 返回结果
}

/**
 * @brief 加锁发送信号
 * @details 加锁保护下调用OsKill发送信号
 * @param pid 进程ID
 * @param sig 信号编号
 * @return 成功返回LOS_OK，失败返回错误码
 */
int OsKillLock(pid_t pid, int sig)
{
    int ret;  // 返回值
    unsigned int intSave;  // 中断状态保存变量

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    ret = OsKill(pid, sig, OS_USER_KILL_PERMISSION);  // 调用OsKill发送信号
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return ret;  // 返回结果
}

/**
 * @brief 不安全的任务杀死函数
 * @details 向指定任务发送杀死信号，不进行加锁保护
 * @param taskID 任务ID
 * @param signo 信号编号
 * @return 成功返回LOS_OK，失败返回错误码
 */
INT32 OsTaskKillUnsafe(UINT32 taskID, INT32 signo)
{
    siginfo_t info;  // 信号信息结构体
    LosTaskCB *taskCB = OsGetTaskCB(taskID);  // 获取任务控制块
    INT32 ret = OsUserTaskOperatePermissionsCheck(taskCB);  // 检查用户任务操作权限
    if (ret != LOS_OK) {  // 权限检查失败
        return -ret;  // 返回错误码
    }

    /* Create the siginfo structure */
    info.si_signo = signo;  // 设置信号编号
    info.si_code = SI_USER;  // 设置信号来源为用户
    info.si_value.sival_ptr = NULL;  // 信号附加数据为空

    /* Dispatch the signal to thread, bypassing normal task group thread
     * dispatch rules. */
    return OsTcbDispatch(taskCB, &info);  // 直接分发信号到任务控制块
}

/**
 * @brief 向线程发送信号
 * @details 向指定线程ID发送信号，包含加锁保护
 * @param tid 线程ID
 * @param signo 信号编号
 * @return 成功返回LOS_OK，失败返回错误码
 */
int OsPthreadKill(UINT32 tid, int signo)
{
    int ret;  // 返回值
    UINT32 intSave;  // 中断状态保存变量

    /* Make sure that the signal is valid */
    OS_RETURN_IF(!GOOD_SIGNO(signo), -EINVAL);  // 信号无效返回错误
    if (OS_TID_CHECK_INVALID(tid)) {  // 线程ID无效
        return -ESRCH;  // 返回线程不存在
    }

    /* Keep things stationary through the following */
    SCHEDULER_LOCK(intSave);  // 调度器加锁
    ret = OsTaskKillUnsafe(tid, signo);  // 调用不安全的任务杀死函数
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return ret;  // 返回结果
}

/**
 * @brief 添加信号到信号集
 * @details 将指定信号添加到信号集中，处理musl库的位表示差异
 * @param set 信号集指针
 * @param signo 信号编号
 * @return 成功返回LOS_OK，失败返回-EINVAL
 */
int OsSigAddSet(sigset_t *set, int signo)
{
    /* Verify the signal */
    if (!GOOD_SIGNO(signo)) {  // 信号编号无效
        return -EINVAL;  // 返回参数错误
    } else {  // 信号编号有效
        /* In musl, sig No bits 00000100 present sig No 3, but  1<< 3 = 00001000, so signo needs minus 1 */
        signo -= 1;  // 调整信号编号以匹配musl库的位表示
        /* Add the signal to the set */
        *set |= SIGNO2SET((unsigned int)signo);  // 将信号对应的位设置为1
        return LOS_OK;  // 返回成功
    }
}

/**
 * @brief 获取待处理信号集
 * @details 获取当前任务的待处理信号集
 * @param set 用于存储结果的信号集指针
 * @return 成功返回LOS_OK，失败返回-EFAULT
 */
int OsSigPending(sigset_t *set)
{
    LosTaskCB *tcb = NULL;  // 任务控制块指针
    unsigned int intSave;  // 中断状态保存变量

    if (set == NULL) {  // 参数为空指针
        return -EFAULT;  // 返回错误
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    tcb = OsCurrTaskGet();  // 获取当前任务
    *set = tcb->sig.sigPendFlag;  // 复制待处理信号集
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return LOS_OK;  // 返回成功
}

/**
 * @brief 查找64位整数中第一个置位的位
 * @details 从低位到高位查找第一个值为1的位位置
 * @param n 待检查的64位整数
 * @return 成功返回位位置，失败返回-1
 */
STATIC int FindFirstSetedBit(UINT64 n)
{
    int count;  // 位计数器

    if (n == 0) {  // 输入为0
        return -1;  // 返回-1表示无置位
    }
    for (count = 0; (count < UINT64_BIT_SIZE) && (n ^ 1ULL); n >>= 1, count++) {}  // 循环查找第一个置位
    return (count < UINT64_BIT_SIZE) ? count : (-1);  // 返回位位置或-1
}

/**
 * @brief 无锁的定时信号等待
 * @details 在无锁情况下等待指定信号，支持超时
 * @param set 等待的信号集
 * @param info 用于存储信号信息的指针
 * @param timeout 超时时间(毫秒)
 * @return 成功返回信号编号，失败返回错误码
 */
int OsSigTimedWaitNoLock(sigset_t *set, siginfo_t *info, unsigned int timeout)
{
    LosTaskCB *task = NULL;  // 任务控制块指针
    sig_cb *sigcb = NULL;  // 信号控制块指针
    int ret;  // 返回值

    task = OsCurrTaskGet();  // 获取当前任务
    sigcb = &task->sig;  // 获取信号控制块

    if (sigcb->waitList.pstNext == NULL) {  // 等待列表未初始化
        LOS_ListInit(&sigcb->waitList);  // 初始化等待列表
    }
    /* If pendingflag & set > 0, should clear pending flag */
    sigset_t clear = sigcb->sigPendFlag & *set;  // 计算待处理且在等待集中的信号
    if (clear) {  // 存在待处理信号
        sigcb->sigPendFlag ^= clear;  // 清除待处理标志
        ret = FindFirstSetedBit((UINT64)clear) + 1;  // 查找第一个信号编号
        OsMoveTmpInfoToUnbInfo(sigcb, ret);  // 移动信号信息
    } else {  // 无待处理信号
        OsSigAddSet(set, SIGKILL);  // 添加SIGKILL到等待集
        OsSigAddSet(set, SIGSTOP);  // 添加SIGSTOP到等待集

        sigcb->sigwaitmask |= *set;  // 设置等待掩码
        OsTaskWaitSetPendMask(OS_TASK_WAIT_SIGNAL, sigcb->sigwaitmask, timeout);  // 设置任务等待状态
        ret = task->ops->wait(task, &sigcb->waitList, timeout);  // 等待信号
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {  // 超时
            ret = -EAGAIN;  // 返回重试错误
        }
        sigcb->sigwaitmask = NULL_SIGNAL_SET;  // 清空等待掩码
    }
    if (info != NULL) {  // 需要返回信号信息
        (VOID)memcpy_s(info, sizeof(siginfo_t), &sigcb->sigunbinfo, sizeof(siginfo_t));  // 复制信号信息
    }
    return ret;  // 返回结果
}

/**
 * @brief 定时信号等待
 * @details 加锁保护的定时信号等待函数
 * @param set 等待的信号集
 * @param info 用于存储信号信息的指针
 * @param timeout 超时时间(毫秒)
 * @return 成功返回信号编号，失败返回错误码
 */
int OsSigTimedWait(sigset_t *set, siginfo_t *info, unsigned int timeout)
{
    int ret;  // 返回值
    unsigned int intSave;  // 中断状态保存变量

    SCHEDULER_LOCK(intSave);  // 调度器加锁

    ret = OsSigTimedWaitNoLock(set, info, timeout);  // 调用无锁版本的定时等待

    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return ret;  // 返回结果
}

/**
 * @brief 暂停进程直到信号到达
 * @details 暂停当前进程，保存旧信号掩码并调用挂起函数
 * @return 始终返回-EINTR
 */
int OsPause(void)
{
    LosTaskCB *spcb = NULL;  // 任务控制块指针
    sigset_t oldSigprocmask;  // 旧信号掩码

    spcb = OsCurrTaskGet();  // 获取当前任务
    oldSigprocmask = spcb->sig.sigprocmask;  // 保存旧信号掩码
    return OsSigSuspend(&oldSigprocmask);  // 挂起进程
}

/**
 * @brief 挂起进程直到信号到达
 * @details 根据信号集挂起进程，处理待处理信号
 * @param set 信号集指针
 * @return 成功返回0，失败返回错误码
 */
int OsSigSuspend(const sigset_t *set)
{
    unsigned int intSave;  // 中断状态保存变量
    LosTaskCB *rtcb = NULL;  // 任务控制块指针
    sigset_t setSuspend;  // 挂起信号集
    int ret;  // 返回值

    if (set == NULL) {  // 参数为空指针
        return -EINVAL;  // 返回参数错误
    }
    SCHEDULER_LOCK(intSave);  // 调度器加锁
    rtcb = OsCurrTaskGet();  // 获取当前任务

    /* Wait signal calc */
    setSuspend = FULL_SIGNAL_SET & (~(*set));  // 计算挂起信号集

    /* If pending mask not in sigmask, need set sigflag */
    OsSigMaskSwitch(rtcb, *set);  // 切换信号掩码

    if (rtcb->sig.sigFlag > 0) {  // 存在待处理信号
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁

        /*
         * If rtcb->sig.sigFlag > 0, it means that some signal have been
         * received, and we need to do schedule to handle the signal directly.
         */
        LOS_Schedule();  // 触发调度
        return -EINTR;  // 返回中断错误
    } else {  // 无待处理信号
        ret = OsSigTimedWaitNoLock(&setSuspend, NULL, LOS_WAIT_FOREVER);  // 无限期等待信号
        if (ret < 0) {  // 等待失败
            PRINT_ERR("FUNC %s LINE = %d, ret = %x\n", __FUNCTION__, __LINE__, ret);  // 打印错误信息
        }
    }

    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return -EINTR;  // 返回中断错误
}

/**
 * @brief 设置信号处理动作
 * @details 设置指定信号的处理函数，仅支持SIGSYS信号
 * @param sig 信号编号
 * @param act 新的信号处理动作
 * @param oact 用于存储旧的信号处理动作
 * @return 成功返回LOS_OK，失败返回错误码
 */
int OsSigAction(int sig, const sigaction_t *act, sigaction_t *oact)
{
    UINTPTR addr;  // 地址变量
    sigaction_t action;  // 信号处理动作结构体

    if (!GOOD_SIGNO(sig) || sig < 1 || act == NULL) {  // 信号无效或参数为空
        return -EINVAL;  // 返回参数错误
    }
    if (LOS_ArchCopyFromUser(&action, act, sizeof(sigaction_t)) != LOS_OK) {  // 从用户空间复制数据失败
        return -EFAULT;  // 返回错误
    }

    if (sig == SIGSYS) {  // 处理系统调用信号
        addr = OsGetSigHandler();  // 获取当前信号处理函数
        if (addr == 0) {  // 未设置处理函数
            OsSetSigHandler((unsigned long)(UINTPTR)action.sa_handler);  // 设置新的处理函数
            return LOS_OK;  // 返回成功
        }
        return -EINVAL;  // 已设置处理函数，返回错误
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 锁定信号中断
 * @details 增加信号中断锁定计数，防止信号处理中断系统调用
 */
VOID OsSigIntLock(VOID)
{
    LosTaskCB *task = OsCurrTaskGet();  // 获取当前任务
    sig_cb *sigcb = &task->sig;  // 获取信号控制块

    (VOID)LOS_AtomicAdd((Atomic *)&sigcb->sigIntLock, 1);  // 原子增加锁定计数
}

/**
 * @brief 解锁信号中断
 * @details 减少信号中断锁定计数
 */
VOID OsSigIntUnlock(VOID)
{
    LosTaskCB *task = OsCurrTaskGet();  // 获取当前任务
    sig_cb *sigcb = &task->sig;  // 获取信号控制块

    (VOID)LOS_AtomicSub((Atomic *)&sigcb->sigIntLock, 1);  // 原子减少锁定计数
}

/**
 * @brief 保存信号上下文
 * @details 在信号处理前保存当前上下文，准备信号处理环境
 * @param sp 当前栈指针
 * @param newSp 新的栈指针
 * @return 新的栈指针或原栈指针
 */
VOID *OsSaveSignalContext(VOID *sp, VOID *newSp)
{
    UINTPTR sigHandler;  // 信号处理函数地址
    UINT32 intSave;  // 中断状态保存变量

    LosTaskCB *task = OsCurrTaskGet();  // 获取当前任务
    LosProcessCB *process = OsCurrProcessGet();  // 获取当前进程
    sig_cb *sigcb = &task->sig;  // 获取信号控制块

    /* A thread is not allowed to interrupt the processing of its signals during a system call */
    if (sigcb->sigIntLock > 0) {  // 信号中断已锁定
        return sp;  // 返回原栈指针
    }

    if (OsTaskIsKilled(task)) {  // 任务已被杀死
        OsRunningTaskToExit(task, 0);  // 退出任务
        return sp;  // 返回原栈指针
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    if ((sigcb->count == 0) && ((sigcb->sigFlag != 0) || (process->sigShare != 0))) {  // 可以处理信号
        sigHandler = OsGetSigHandler();  // 获取信号处理函数
        if (sigHandler == 0) {  // 未设置信号处理函数
            sigcb->sigFlag = 0;  // 清除信号标志
            process->sigShare = 0;  // 清除共享信号
            SCHEDULER_UNLOCK(intSave);  // 调度器解锁
            PRINT_ERR("The signal processing function for the current process pid =%d is NULL!\n", process->processID);  // 打印错误
            return sp;  // 返回原栈指针
        }
        /* One pthread do the share signal */
        sigcb->sigFlag |= process->sigShare;  // 合并共享信号
        UINT32 signo = (UINT32)FindFirstSetedBit(sigcb->sigFlag) + 1;  // 获取第一个信号编号
        UINT32 sigVal = (UINT32)(UINTPTR)(sigcb->sigunbinfo.si_value.sival_ptr);  // 获取信号值
        OsMoveTmpInfoToUnbInfo(sigcb, signo);  // 移动信号信息
        OsProcessExitCodeSignalSet(process, signo);  // 设置进程退出码信号
        sigcb->sigContext = sp;  // 保存当前上下文

        OsInitSignalContext(sp, newSp, sigHandler, signo, sigVal);  // 初始化信号上下文

        /* sig No bits 00000100 present sig No 3, but  1<< 3 = 00001000, so signo needs minus 1 */
        sigcb->sigFlag ^= 1ULL << (signo - 1);  // 清除已处理信号
        sigcb->count++;  // 增加信号嵌套计数
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁
        return newSp;  // 返回新栈指针
    }

    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return sp;  // 返回原栈指针
}

/**
 * @brief 恢复信号上下文
 * @details 信号处理完成后恢复之前保存的上下文
 * @param sp 当前栈指针
 * @return 保存的上下文栈指针
 */
VOID *OsRestorSignalContext(VOID *sp)
{
    UINT32 intSave;  // 中断状态保存变量

    LosTaskCB *task = OsCurrTaskGet();  // 获取当前任务
    sig_cb *sigcb = &task->sig;  // 获取信号控制块

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    if (sigcb->count != 1) {  // 信号嵌套计数不正确
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁
        PRINT_ERR("sig error count : %d\n", sigcb->count);  // 打印错误
        return sp;  // 返回原栈指针
    }

    LosProcessCB *process = OsCurrProcessGet();  // 获取当前进程
    VOID *saveContext = sigcb->sigContext;  // 获取保存的上下文
    sigcb->sigContext = NULL;  // 清空上下文
    sigcb->count--;  // 减少信号嵌套计数
    process->sigShare = 0;  // 清除共享信号
    OsProcessExitCodeSignalClear(process);  // 清除进程退出码信号
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return saveContext;  // 返回保存的上下文
}
#endif
