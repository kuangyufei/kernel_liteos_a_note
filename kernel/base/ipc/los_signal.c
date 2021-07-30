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

int raise(int sig)
{
    (VOID)sig;
    PRINT_ERR("%s NOT SUPPORT\n", __FUNCTION__);
    errno = ENOSYS;
    return -1;
}

#define GETUNMASKSET(procmask, pendFlag) ((~(procmask)) & (sigset_t)(pendFlag))
#define UINT64_BIT_SIZE 64
/****************************************************
判定信号signo是否存在信号集中。
如果信号集里已有该信号则返回1，否则返回0。如果有错误则返回-1
****************************************************/
int OsSigIsMember(const sigset_t *set, int signo)
{
    int ret = LOS_NOK;
    /* In musl, sig No bits 00000100 present sig No 3, but  1<< 3 = 00001000, so signo needs minus 1 */ 
	//在musl中，sig No bits 00000100表示sig No 3，但是在SIGNO2SET中 1<<3 = 00001000,因此signo需要减1
    signo -= 1;
    /* Verify the signal */
    if (GOOD_SIGNO(signo)) {//有效信号判断
        /* Check if the signal is in the set */
        ret = ((*set & SIGNO2SET((unsigned int)signo)) != 0);//检查信号是否还在集合中
    }

    return ret;
}

STATIC INLINE VOID OsSigWaitTaskWake(LosTaskCB *taskCB, INT32 signo)
{
    sig_cb *sigcb = &taskCB->sig;

    if (!LOS_ListEmpty(&sigcb->waitList) && OsSigIsMember(&sigcb->sigwaitmask, signo)) {
        OsTaskWakeClearPendMask(taskCB);
        OsSchedTaskWake(taskCB);
        OsSigEmptySet(&sigcb->sigwaitmask);
    }
}

STATIC UINT32 OsPendingTaskWake(LosTaskCB *taskCB, INT32 signo)
{
    if (!OsTaskIsPending(taskCB) || !OsProcessIsUserMode(OS_PCB_FROM_PID(taskCB->processID))) {
        return 0;
    }

    if ((signo != SIGKILL) && (taskCB->waitFlag != OS_TASK_WAIT_SIGNAL)) {
        return 0;
    }

    switch (taskCB->waitFlag) {
        case OS_TASK_WAIT_PROCESS:
        case OS_TASK_WAIT_GID:
        case OS_TASK_WAIT_ANYPROCESS:
            OsWaitWakeTask(taskCB, OS_INVALID_VALUE);
            break;
        case OS_TASK_WAIT_JOIN:
            OsTaskWakeClearPendMask(taskCB);
            OsSchedTaskWake(taskCB);
            break;
        case OS_TASK_WAIT_SIGNAL:
            OsSigWaitTaskWake(taskCB, signo);
            break;
#ifdef LOSCFG_KERNEL_LITEIPC
        case OS_TASK_WAIT_LITEIPC:
            taskCB->ipcStatus &= ~IPC_THREAD_STATUS_PEND;
            OsTaskWakeClearPendMask(taskCB);
            OsSchedTaskWake(taskCB);
            break;
#endif
        case OS_TASK_WAIT_FUTEX:
            OsFutexNodeDeleteFromFutexHash(&taskCB->futex, TRUE, NULL, NULL);
            OsTaskWakeClearPendMask(taskCB);
            OsSchedTaskWake(taskCB);
            break;
        default:
            break;
    }

    return 0;
}
//给任务(线程)发送一个信号
int OsTcbDispatch(LosTaskCB *stcb, siginfo_t *info)
{
    bool masked = FALSE;
    sig_cb *sigcb = &stcb->sig;

    OS_RETURN_IF_NULL(sigcb);
    /* If signo is 0, not send signal, just check process or pthread exist */
    if (info->si_signo == 0) {//如果信号为0,则不发送信号,只是作为检查进程和线程是否还存在.
        return 0;
    }
    masked = (bool)OsSigIsMember(&sigcb->sigprocmask, info->si_signo);//@note_thinking 这里还有 masked= -1的情况要处理!!!
    if (masked) {//如果信号被屏蔽了,要看等待信号集,sigwaitmask
        /* If signal is in wait list and mask list, need unblock it */ //如果信号在等待列表和掩码列表中，需要解除阻止
        if (LOS_ListEmpty(&sigcb->waitList)  ||
           (!LOS_ListEmpty(&sigcb->waitList) && !OsSigIsMember(&sigcb->sigwaitmask, info->si_signo))) {
            OsSigAddSet(&sigcb->sigPendFlag, info->si_signo);//将信号加入阻塞集
        }
    } else {//信号没有被屏蔽的处理
        /* unmasked signal actions */
        OsSigAddSet(&sigcb->sigFlag, info->si_signo);//不屏蔽的信号集
    }
    (void) memcpy_s(&sigcb->sigunbinfo, sizeof(siginfo_t), info, sizeof(siginfo_t));

    return OsPendingTaskWake(stcb, info->si_signo);
}

void OsSigMaskSwitch(LosTaskCB * const rtcb, sigset_t set)
{
    sigset_t unmaskset;

    rtcb->sig.sigprocmask = set;
    unmaskset = GETUNMASKSET(rtcb->sig.sigprocmask, rtcb->sig.sigPendFlag);//过滤出没有被屏蔽的信号集
    if (unmaskset != NULL_SIGNAL_SET) {
        /* pendlist do */
        rtcb->sig.sigFlag |= unmaskset;	//加入不屏蔽信号集
        rtcb->sig.sigPendFlag ^= unmaskset;//从阻塞集中去掉unmaskset
    }
}
/******************************************************************
向信号集设置信号屏蔽的方法
	SIG_BLOCK：将set指向信号集中的信号，添加到进程阻塞信号集；
	SIG_UNBLOCK：将set指向信号集中的信号，从进程阻塞信号集删除；
	SIG_SETMASK：将set指向信号集中的信号，设置成进程阻塞信号集；
*******************************************************************/
int OsSigprocMask(int how, const sigset_t_l *setl, sigset_t_l *oldset)
{
    LosTaskCB *spcb = NULL;
    sigset_t oldSigprocmask;
    int ret = LOS_OK;
    unsigned int intSave;
    sigset_t set;
    int retVal;

    if (setl != NULL) {
        retVal = LOS_ArchCopyFromUser(&set, &(setl->sig[0]), sizeof(sigset_t));
        if (retVal != 0) {
            return -EFAULT;
        }
    }
    SCHEDULER_LOCK(intSave);
    spcb = OsCurrTaskGet();
    /* If requested, copy the old mask to user. */ //如果需要，请将旧掩码复制给用户
    oldSigprocmask = spcb->sig.sigprocmask;

    /* If requested, modify the current signal mask. */ //如有要求，修改当前信号屏蔽
    if (setl != NULL) {
        /* Okay, determine what we are supposed to do */
        switch (how) {
            /* Set the union of the current set and the signal
             * set pointed to by set as the new sigprocmask.
             */
            case SIG_BLOCK:
                spcb->sig.sigprocmask |= set;//增加信号屏蔽位
                break;
            /* Set the intersection of the current set and the
             * signal set pointed to by set as the new sigprocmask.
             */
            case SIG_UNBLOCK:
                spcb->sig.sigprocmask &= ~(set);//解除信号屏蔽位
                break;
            /* Set the signal set pointed to by set as the new sigprocmask. */
            case SIG_SETMASK:
                spcb->sig.sigprocmask = set;//设置一个新的屏蔽掩码
                break;
            default:
                ret = -EINVAL;
                break;
        }
        /* If pending mask not in sigmask, need set sigflag. */
        OsSigMaskSwitch(spcb, spcb->sig.sigprocmask);//更新与屏蔽信号相关的变量
    }
    SCHEDULER_UNLOCK(intSave);

    if (oldset != NULL) {
        retVal = LOS_ArchCopyToUser(&(oldset->sig[0]), &oldSigprocmask, sizeof(sigset_t));
        if (retVal != 0) {
            return -EFAULT;
        }
    }
    return ret;
}
//让进程的每一个task执行参数函数
int OsSigProcessForeachChild(LosProcessCB *spcb, ForEachTaskCB handler, void *arg)
{
    int ret;

    /* Visit the main thread last (if present) */
    LosTaskCB *taskCB = NULL;//遍历进程的 threadList 链表,里面存放的都是task节点
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &(spcb->threadSiblingList), LosTaskCB, threadList) {//遍历进程的任务列表
        ret = handler(taskCB, arg);//回调参数函数
        OS_RETURN_IF(ret != 0, ret);//这个宏的意思就是只有ret = 0时,啥也不处理.其余就返回 ret
    }
    return LOS_OK;
}
//信号处理函数,这里就是上面的 handler =  SigProcessSignalHandler,见于 OsSigProcessSend
static int SigProcessSignalHandler(LosTaskCB *tcb, void *arg)
{
    struct ProcessSignalInfo *info = (struct ProcessSignalInfo *)arg;//先把参数解出来
    int ret;
    int isMember;

    if (tcb == NULL) {
        return 0;
    }

    /* If the default tcb is not setted, then set this one as default. */
    if (!info->defaultTcb) {//如果没有默认发送方的任务,即默认参数任务.
        info->defaultTcb = tcb;
    }

    isMember = OsSigIsMember(&tcb->sig.sigwaitmask, info->sigInfo->si_signo);//任务是否在等待这个信号
    if (isMember && (!info->awakenedTcb)) {//是在等待,并尚未向该任务时发送信号时
        /* This means the task is waiting for this signal. Stop looking for it and use this tcb.
         * The requirement is: if more than one task in this task group is waiting for the signal,
         * then only one indeterminate task in the group will receive the signal.
         */
        ret = OsTcbDispatch(tcb, info->sigInfo);//发送信号,注意这是给其他任务发送信号,tcb不是当前任务
        OS_RETURN_IF(ret < 0, ret);//这种写法很有意思

        /* set this tcb as awakenedTcb */
        info->awakenedTcb = tcb;
        OS_RETURN_IF(info->receivedTcb != NULL, SIG_STOP_VISIT); /* Stop search */
    }
    /* Is this signal unblocked on this thread? */
    isMember = OsSigIsMember(&tcb->sig.sigprocmask, info->sigInfo->si_signo);//任务是否屏蔽了这个信号
    if ((!isMember) && (!info->receivedTcb) && (tcb != info->awakenedTcb)) {//没有屏蔽,有唤醒任务没有接收任务.
        /* if unblockedTcb of this signal is not setted, then set it. */
        if (!info->unblockedTcb) {
            info->unblockedTcb = tcb;
        }

        ret = OsTcbDispatch(tcb, info->sigInfo);//向任务发送信号
        OS_RETURN_IF(ret < 0, ret);
        /* set this tcb as receivedTcb */
        info->receivedTcb = tcb;//设置这个任务为接收任务
        OS_RETURN_IF(info->awakenedTcb != NULL, SIG_STOP_VISIT); /* Stop search */
    }
    return 0; /* Keep searching */
}
//进程收到 SIGKILL 信号后,通知任务tcb处理.
static int SigProcessKillSigHandler(LosTaskCB *tcb, void *arg)
{
    struct ProcessSignalInfo *info = (struct ProcessSignalInfo *)arg;//转参

    return OsPendingTaskWake(tcb, info->sigInfo->si_signo);
}

//处理信号发送
static void SigProcessLoadTcb(struct ProcessSignalInfo *info, siginfo_t *sigInfo)
{
    LosTaskCB *tcb = NULL;

    if (info->awakenedTcb == NULL && info->receivedTcb == NULL) {//信号即没有指定接收task 也没有指定被唤醒task
        if (info->unblockedTcb) {//如果进程信号信息体中有阻塞task
            tcb = info->unblockedTcb;//
        } else if (info->defaultTcb) {//如果有默认的发送方task
            tcb = info->defaultTcb;
        } else {
            return;
        }
        /* Deliver the signal to the selected task */
        (void)OsTcbDispatch(tcb, sigInfo);//向所选任务发送信号
    }
}
//给参数进程发送参数信号
int OsSigProcessSend(LosProcessCB *spcb, siginfo_t *sigInfo)
{
    int ret;
    struct ProcessSignalInfo info = {
        .sigInfo = sigInfo, //信号内容
        .defaultTcb = NULL, //以下四个值将在OsSigProcessForeachChild中根据条件完善
        .unblockedTcb = NULL,
        .awakenedTcb = NULL,
        .receivedTcb = NULL
    };
	//总之是要从进程中找个至少一个任务来接受这个信号,优先级
	//awakenedTcb > receivedTcb > unblockedTcb > defaultTcb
    if (info.sigInfo == NULL){
        return -EFAULT;
    }
    /* visit all taskcb and dispatch signal */ //访问所有任务和分发信号
    if (info.sigInfo->si_signo == SIGKILL) {//需要干掉进程时 SIGKILL = 9， #linux kill 9 14
        OsSigAddSet(&spcb->sigShare, info.sigInfo->si_signo);//信号集中增加信号
        (void)OsSigProcessForeachChild(spcb, SigProcessKillSigHandler, &info);
        return 0;
    } else {
        ret = OsSigProcessForeachChild(spcb, SigProcessSignalHandler, &info);//进程通知所有task处理信号
    }
    if (ret < 0) {
        return ret;
    }
    SigProcessLoadTcb(&info, sigInfo);//确保能给一个任务发送信号
    return 0;
}
//信号集全部清0
int OsSigEmptySet(sigset_t *set)
{
    *set = NULL_SIGNAL_SET;
    return 0;
}

/* Privilege process can't send to kernel and privilege process */ //内核进程组和用户特权进程组无法发送
static int OsSignalPermissionToCheck(const LosProcessCB *spcb)
{
    UINT32 gid = spcb->group->groupID;

    if (gid == OS_KERNEL_PROCESS_GROUP) {//内核进程组
        return -EPERM;
    } else if (gid == OS_USER_PRIVILEGE_PROCESS_GROUP) {//用户特权进程组
        return -EPERM;
    }

    return 0;
}
//信号分发,发送信号权限/进程组过滤.
int OsDispatch(pid_t pid, siginfo_t *info, int permission)
{
    LosProcessCB *spcb = OS_PCB_FROM_PID(pid);//找到这个进程
    if (OsProcessIsUnused(spcb)) {//进程是否还在使用,不一定是当前进程但必须是个有效进程
        return -ESRCH;
    }

    /* If the process you want to kill had been inactive, but still exist. should return LOS_OK */
    if (OsProcessIsInactive(spcb)) {//不向非活动进程发送信息,但返回OK
        return LOS_OK;
    }

#ifdef LOSCFG_SECURITY_CAPABILITY	//启用能力安全模式
    LosProcessCB *current = OsCurrProcessGet();//获取当前进程,检查当前进程是否有发送信号的权限.
    /* Kernel process always has kill permission and user process should check permission *///内核进程总是有kill权限，用户进程需要检查权限
    if (OsProcessIsUserMode(current) && !(current->processStatus & OS_PROCESS_FLAG_EXIT)) {//用户进程检查能力范围
        if ((current != spcb) && (!IsCapPermit(CAP_KILL)) && (current->user->userID != spcb->user->userID)) {
            return -EPERM;
        }
    }
#endif
    if ((permission == OS_USER_KILL_PERMISSION) && (OsSignalPermissionToCheck(spcb) < 0)) {
        return -EPERM;
    }
    return OsSigProcessSend(spcb, info);//给参数进程发送信号
}
/************************************************
用于向进程或进程组发送信号
shell命令 kill 14 7（kill -14 7效果相同）
发送信号14（SIGALRM默认行为为进程终止）给7号进程
************************************************/
int OsKill(pid_t pid, int sig, int permission)
{
    siginfo_t info;
    int ret;

    /* Make sure that the para is valid */
    if (!GOOD_SIGNO(sig) || pid < 0) {//有效信号 [0,64]
        return -EINVAL;
    }
    if (OsProcessIDUserCheckInvalid(pid)) {//检查参数进程 
        return -ESRCH;
    }

    /* Create the siginfo structure */ //创建信号结构体
    info.si_signo = sig;	//信号编号
    info.si_code = SI_USER;	//来自用户进程信号
    info.si_value.sival_ptr = NULL;

    /* Send the signal */
    ret = OsDispatch(pid, &info, permission);//发送信号
    return ret;
}
//给发送信号过程加锁
int OsKillLock(pid_t pid, int sig)
{
    int ret;
    unsigned int intSave;

    SCHEDULER_LOCK(intSave);
    ret = OsKill(pid, sig, OS_USER_KILL_PERMISSION);//用户权限向进程发送信号
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
INT32 OsTaskKillUnsafe(UINT32 taskID, INT32 signo)
{
    siginfo_t info;
    LosTaskCB *taskCB = OsGetTaskCB(taskID);
    INT32 ret = OsUserTaskOperatePermissionsCheck(taskCB);
    if (ret != LOS_OK) {
        return -ret;
    }

    /* Create the siginfo structure */
    info.si_signo = signo;
    info.si_code = SI_USER;
    info.si_value.sival_ptr = NULL;

    /* Dispatch the signal to thread, bypassing normal task group thread
     * dispatch rules. */
    return OsTcbDispatch(taskCB, &info);
}
//发送信号
int OsPthreadKill(UINT32 tid, int signo)
{
    int ret;
    UINT32 intSave;

    /* Make sure that the signal is valid */
    OS_RETURN_IF(!GOOD_SIGNO(signo), -EINVAL);
    if (OS_TID_CHECK_INVALID(tid)) {
        return -ESRCH;
    }

    /* Keep things stationary through the following */
    SCHEDULER_LOCK(intSave);
    ret = OsTaskKillUnsafe(tid, signo);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
//向信号集中加入signo信号
int OsSigAddSet(sigset_t *set, int signo)
{
    /* Verify the signal */
    if (!GOOD_SIGNO(signo)) {
        return -EINVAL;
    } else {
        /* In musl, sig No bits 00000100 present sig No 3, but  1<< 3 = 00001000, so signo needs minus 1 */
        signo -= 1;// 信号范围是 [1 ~ 64 ],而保存变量位的范围是[0 ~ 63]
        /* Add the signal to the set */
        *set |= SIGNO2SET((unsigned int)signo);//填充信号集
        return LOS_OK;
    }
}
//获取阻塞当前任务的信号集
int OsSigPending(sigset_t *set)
{
    LosTaskCB *tcb = NULL;
    unsigned int intSave;

    if (set == NULL) {
        return -EFAULT;
    }

    SCHEDULER_LOCK(intSave);
    tcb = OsCurrTaskGet();
    *set = tcb->sig.sigPendFlag;//被阻塞的信号集
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

STATIC int FindFirstSetedBit(UINT64 n)
{
    int count;

    if (n == 0) {
        return -1;
    }
    for (count = 0; (count < UINT64_BIT_SIZE) && (n ^ 1ULL); n >>= 1, count++) {}
    return (count < UINT64_BIT_SIZE) ? count : (-1);
}
//等待信号时间
int OsSigTimedWaitNoLock(sigset_t *set, siginfo_t *info, unsigned int timeout)
{
    LosTaskCB *task = NULL;
    sig_cb *sigcb = NULL;
    int ret;

    task = OsCurrTaskGet();
    sigcb = &task->sig;

    if (sigcb->waitList.pstNext == NULL) {
        LOS_ListInit(&sigcb->waitList);//初始化信号等待链表
    }
    /* If pendingflag & set > 0, shound clear pending flag */
    sigset_t clear = sigcb->sigPendFlag & *set;
    if (clear) {
        sigcb->sigPendFlag ^= clear;
        ret = FindFirstSetedBit((UINT64)clear) + 1;
    } else {
        OsSigAddSet(set, SIGKILL);//kill 9 14 必须要处理
        OsSigAddSet(set, SIGSTOP);//终止进程的信号也必须处理

        sigcb->sigwaitmask |= *set;//按位加到等待集上,也就是说sigwaitmask的信号来了都是要处理的.
        OsTaskWaitSetPendMask(OS_TASK_WAIT_SIGNAL, sigcb->sigwaitmask, timeout);
        ret = OsSchedTaskWait(&sigcb->waitList, timeout, TRUE);
        if (ret == LOS_ERRNO_TSK_TIMEOUT) {
            ret = -EAGAIN;
        }
        sigcb->sigwaitmask = NULL_SIGNAL_SET;
    }
    if (info != NULL) {
        (void) memcpy_s(info, sizeof(siginfo_t), &sigcb->sigunbinfo, sizeof(siginfo_t));
    }
    return ret;
}
//让当前任务等待的信号
int OsSigTimedWait(sigset_t *set, siginfo_t *info, unsigned int timeout)
{
    int ret;
    unsigned int intSave;

    SCHEDULER_LOCK(intSave);

    ret = OsSigTimedWaitNoLock(set, info, timeout);//以不加锁的方式等待

    SCHEDULER_UNLOCK(intSave);
    return ret;
}
//通过信号挂起当前任务
int OsPause(void)
{
    LosTaskCB *spcb = NULL;
    sigset_t oldSigprocmask;

    spcb = OsCurrTaskGet();
    oldSigprocmask = spcb->sig.sigprocmask;
    return OsSigSuspend(&oldSigprocmask);
}
//用参数set代替进程的原有掩码，并暂停进程执行，直到收到信号再恢复原有掩码并继续执行进程。
int OsSigSuspend(const sigset_t *set)
{
    unsigned int intSave;
    LosTaskCB *rtcb = NULL;
    sigset_t setSuspend;
    int ret;

    if (set == NULL) {
        return -EINVAL;
    }
    SCHEDULER_LOCK(intSave);
    rtcb = OsCurrTaskGet();

    /* Wait signal calc */
    setSuspend = FULL_SIGNAL_SET & (~(*set));

    /* If pending mask not in sigmask, need set sigflag */
    OsSigMaskSwitch(rtcb, *set);
    ret = OsSigTimedWaitNoLock(&setSuspend, NULL, LOS_WAIT_FOREVER);
    if (ret < 0) {
        PRINT_ERR("FUNC %s LINE = %d, ret = %x\n", __FUNCTION__, __LINE__, ret);
    }

    SCHEDULER_UNLOCK(intSave);
    return -EINTR;
}
/**************************************************
信号安装,函数用于改变进程接收到特定信号后的行为。
sig:信号的值，可以为除SIGKILL及SIGSTOP外的任何一个特定有效的信号（为这两个信号定义自己的处理函数，将导致信号安装错误）。
act:设置对signal信号的新处理方式。
oldact:原来对信号的处理方式。
如果把第二、第三个参数都设为NULL，那么该函数可用于检查信号的有效性。
返回值：0 表示成功，-1 表示有错误发生。
**************************************************/
int OsSigAction(int sig, const sigaction_t *act, sigaction_t *oact)
{
    UINTPTR addr;
    sigaction_t action;

    if (!GOOD_SIGNO(sig) || sig < 1 || act == NULL) {
        return -EINVAL;
    }
	//将数据从用户空间拷贝到内核空间
	if (LOS_ArchCopyFromUser(&action, act, sizeof(sigaction_t)) != LOS_OK) {
		return -EFAULT;
	}
	
	if (sig == SIGSYS) {//鸿蒙此处通过错误的系统调用 来安装信号处理函数,有点巧妙. 
		addr = OsGetSigHandler();//是否已存在信号处理函数
		if (addr == 0) {//进程没有设置信号处理函数时
			OsSetSigHandler((unsigned long)(UINTPTR)action.sa_handler);//设置进程信号处理函数
			//void (*sa_handler)(int); //信号处理函数——普通版
			//void (*sa_sigaction)(int, siginfo_t *, void *);//信号处理函数——高级版
            return LOS_OK;
        }
        return -EINVAL;
    }

    return LOS_OK;
}

VOID OsSigIntLock(VOID)
{
    LosTaskCB *task = OsCurrTaskGet();
    sig_cb *sigcb = &task->sig;

    (VOID)LOS_AtomicAdd((Atomic *)&sigcb->sigIntLock, 1);
}

VOID OsSigIntUnlock(VOID)
{
    LosTaskCB *task = OsCurrTaskGet();
    sig_cb *sigcb = &task->sig;

    (VOID)LOS_AtomicSub((Atomic *)&sigcb->sigIntLock, 1);
}
/**********************************************
产生系统调用时,也就是软中断时,保存用户栈寄存器现场信息
改写PC寄存器的值
**********************************************/
VOID *OsSaveSignalContext(VOID *sp, VOID *newSp)
{
    UINTPTR sigHandler;
    UINT32 intSave;
    LosTaskCB *task = OsCurrTaskGet();
    LosProcessCB *process = OsCurrProcessGet();
    sig_cb *sigcb = &task->sig;

    /* A thread is not allowed to interrupt the processing of its signals during a system call */
    if (sigcb->sigIntLock > 0) {
        return sp;
    }

    if (task->taskStatus & OS_TASK_FLAG_EXIT_KILL) {
        OsTaskToExit(task, 0);
        return sp;
    }

    SCHEDULER_LOCK(intSave);
    if ((sigcb->count == 0) && ((sigcb->sigFlag != 0) || (process->sigShare != 0))) {
        sigHandler = OsGetSigHandler();
        if (sigHandler == 0) {
            sigcb->sigFlag = 0;
            process->sigShare = 0;
            SCHEDULER_UNLOCK(intSave);
            PRINT_ERR("The signal processing function for the current process pid =%d is NULL!\n", task->processID);
            return sp;
        }
        /* One pthread do the share signal */
        sigcb->sigFlag |= process->sigShare;
        UINT32 signo = (UINT32)FindFirstSetedBit(sigcb->sigFlag) + 1;
        UINT32 sigVal = (UINT32)(UINTPTR)(sigcb->sigunbinfo.si_value.sival_ptr);
        OsProcessExitCodeSignalSet(process, signo);
        sigcb->sigContext = sp;

        OsInitSignalContext(sp, newSp, sigHandler, signo, sigVal);

        /* sig No bits 00000100 present sig No 3, but  1<< 3 = 00001000, so signo needs minus 1 */
        sigcb->sigFlag ^= 1ULL << (signo - 1);
        sigcb->count++;
        SCHEDULER_UNLOCK(intSave);
        return newSp;
    }

    SCHEDULER_UNLOCK(intSave);
    return sp;
}

/****************************************************
恢复信号上下文,由系统调用之__NR_sigreturn产生,这是一个内部产生的系统调用.
为什么要恢复呢?
因为系统调用的执行由任务内核态完成,使用的栈也是内核栈,CPU相关寄存器记录的都是内核栈的内容,
而系统调用完成后,需返回任务的用户栈执行,这时需将CPU各寄存器回到用户态现场
所以函数的功能就变成了还原寄存器的值
****************************************************/
VOID *OsRestorSignalContext(VOID *sp)
{
    UINT32 intSave;

    LosTaskCB *task = OsCurrTaskGet();
    sig_cb *sigcb = &task->sig;

    SCHEDULER_LOCK(intSave);
    if (sigcb->count != 1) {
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("sig error count : %d\n", sigcb->count);
        return sp;
    }

    LosProcessCB *process = OsCurrProcessGet();
    VOID *saveContext = sigcb->sigContext;
    sigcb->sigContext = NULL;
    sigcb->count--;
    process->sigShare = 0;	//回到用户态,信号共享清0
    OsProcessExitCodeSignalClear(process);//清空进程退出码
    SCHEDULER_UNLOCK(intSave);
    return saveContext;
}

#endif
