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

#ifndef _LOS_SIGNAL_H
#define _LOS_SIGNAL_H

#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include <signal.h>
#include "los_event.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/********************************************
https://www.cnblogs.com/hoys/archive/2012/08/19/2646377.html

信号是Linux系统中用于进程间互相通信或者操作的一种机制，信号可以在任何时候发给某一进程，而无需知道该进程的状态。
如果该进程当前并未处于执行状态，则该信号就由内核保存起来，直到该进程被调度执行并传递给它为止。
如果一个信号被进程设置为阻塞，则该信号的传递被延迟，直到其阻塞被取消时才被传递给进程。

软中断信号（signal，又简称为信号）用来通知进程发生了异步事件。在软件层次上是对中断机制的一种模拟，
在原理上，一个进程收到一个信号与处理器收到一个中断请求可以说是一样的。信号是进程间通信机制中异步通信机制，
一个进程不必通过任何操作来等待信号的到达，事实上，进程也不知道信号到底什么时候到达。
进程之间可以互相通过系统调用kill发送软中断信号。内核也可以因为内部事件而给进程发送信号，通知进程
发生了某个事件。信号机制除了基本通知功能外，还可以传递附加信息。

信号量定义如下: 见于..\third_party\musl\arch\aarch64\bits\signal.h
#define SIGHUP    1	//终端挂起或者控制进程终止
#define SIGINT    2	//键盘中断（如break键被按下）
#define SIGQUIT   3	//键盘的退出键被按下
#define SIGILL    4	//非法指令
#define SIGTRAP   5	//跟踪陷阱（trace trap），启动进程，跟踪代码的执行
#define SIGABRT   6	//由abort(3)发出的退出指令
#define SIGIOT    SIGABRT //abort发出的信号
#define SIGBUS    7	//总线错误 
#define SIGFPE    8	//浮点异常
#define SIGKILL   9		//常用的命令 kill 9 123 | 不能被忽略、处理和阻塞
#define SIGUSR1   10	//用户自定义信号1 
#define SIGSEGV   11	//无效的内存引用, 段违例（segmentation     violation），进程试图去访问其虚地址空间以外的位置 
#define SIGUSR2   12	//用户自定义信号2
#define SIGPIPE   13	//向某个非读管道中写入数据 
#define SIGALRM   14	//由alarm(2)发出的信号,默认行为为进程终止
#define SIGTERM   15	//终止信号
#define SIGSTKFLT 16	//栈溢出
#define SIGCHLD   17	//子进程结束信号
#define SIGCONT   18	//进程继续（曾被停止的进程）
#define SIGSTOP   19	//终止进程  	 | 不能被忽略、处理和阻塞
#define SIGTSTP   20	//控制终端（tty）上 按下停止键
#define SIGTTIN   21	//进程停止，后台进程企图从控制终端读
#define SIGTTOU   22	//进程停止，后台进程企图从控制终端写
#define SIGURG    23	//I/O有紧急数据到达当前进程
#define SIGXCPU   24	//进程的CPU时间片到期
#define SIGXFSZ   25	//文件大小的超出上限
#define SIGVTALRM 26	//虚拟时钟超时
#define SIGPROF   27	//profile时钟超时
#define SIGWINCH  28	//窗口大小改变
#define SIGIO     29	//I/O相关
#define SIGPOLL   29	//
#define SIGPWR    30	//电源故障,关机
#define SIGSYS    31	//系统调用中参数错，如系统调用号非法 
#define SIGUNUSED SIGSYS		//系统调用异常

#define _NSIG 65 //信号范围,不超过_NSIG
********************************************/

#define LOS_BIT_SET(val, bit) ((val) = (val) | (1ULL << (UINT32)(bit))) 	//按位设置
#define LOS_BIT_CLR(val, bit) ((val) = (val) & ~(1ULL << (UINT32)(bit)))	//按位清除
#define LOS_IS_BIT_SET(val, bit) (bool)((((val) >> (UINT32)(bit)) & 1ULL))	//位是否设置为1

#define OS_SYSCALL_SET_CPSR(regs, cpsr) (*((unsigned long *)((UINTPTR)(regs) - 4)) = (cpsr))
#define OS_SYSCALL_SET_SR(regs, cpsr) (*((unsigned long *)((UINTPTR)(regs))) = (cpsr))
#define OS_SYSCALL_GET_CPSR(regs) (*((unsigned long *)((UINTPTR)(regs) - 4)))
#define SIG_STOP_VISIT 1

#define OS_KERNEL_KILL_PERMISSION 0U	//内核态 kill 权限
#define OS_USER_KILL_PERMISSION   3U	//用户态 kill 权限

#define OS_RETURN_IF(expr, errcode) \
    if ((expr)) {                   \
        return errcode;             \
    }

#define OS_RETURN_IF_VOID(expr) \
    if ((expr)) {               \
        return;                 \
    }
#define OS_GOTO_EXIT_IF(expr, errcode) \
    if (expr) {                        \
        ret = errcode;                 \
        goto EXIT;                     \
    }
#define OS_GOTO_EXIT_IF_ONLY(expr) \
    if (expr) {                    \
        goto EXIT;                 \
    }

#define OS_RETURN_VOID_IF_NULL(pPara) \
    if (NULL == (pPara)) {            \
        return;                       \
    }
#define OS_RETURN_IF_NULL(pPara) \
    if (NULL == (pPara)) {       \
        return (-EINVAL);          \
    }

#define OS_GOTO_EXIT_IF_NULL(pPara) \
    if (NULL == (pPara)) {          \
        ret = -EINVAL;              \
        goto EXIT;                  \
    }

typedef void (*sa_sighandler_t)(int);
typedef void (*sa_siginfoaction_t)(int, siginfo_t *, void *);

#define SIGNO2SET(s) ((sigset_t)1ULL << (s))
#define NULL_SIGNAL_SET ((sigset_t)0ULL)	//信号集全部清0
#define FULL_SIGNAL_SET ((sigset_t)~0ULL)	//信号集全部置1
//信号ID是否有效
static inline int GOOD_SIGNO(unsigned int sig)
{
    return (sig < _NSIG) ? 1 : 0;// 
}
/********************************************************************
Musl官网 http://musl.libc.org/ 
musl是构建在Linux系统调用API之上的C标准库的实现，包括在基本语言标准POSIX中定义的接口，
以及广泛认可的扩展。musl是轻量级的，快速的，简单的，自由的.
********************************************************************/

#define MAX_SIG_ARRAY_IN_MUSL 128 //128个信号

typedef struct {
    unsigned long sig[MAX_SIG_ARRAY_IN_MUSL / sizeof(unsigned long)];
} sigset_t_l;

typedef struct sigaction sigaction_t;

struct sigactq {
    struct sigactq *flink; /* Forward link */
    sigaction_t act;       /* Sigaction data */
    uint8_t signo;         /* Signal associated with action */
};
typedef struct sigactq sigactq_t;

struct sq_entry_s {
    struct sq_entry_s *flink;
};
typedef struct sq_entry_s sq_entry_t;

struct sigpendq {
    struct sigpendq *flink; /* Forward link */
    siginfo_t info;         /* Signal information */
    uint8_t type;           /* (Used to manage allocations) */
};
typedef struct sigpendq sigpendq_t;

struct sq_queue_s {//信号队列
    sq_entry_t *head;
    sq_entry_t *tail;
};
typedef struct sq_queue_s sq_queue_t;

#define TASK_IRQ_CONTEXT \
        unsigned int R0;     \
        unsigned int R1;     \
        unsigned int R2;     \
        unsigned int R3;     \
        unsigned int R12;    \
        unsigned int USP;    \
        unsigned int ULR;    \
        unsigned int CPSR;   \
        unsigned int PC;

typedef struct {//中断上下文
    TASK_IRQ_CONTEXT
} TaskIrqDataSize;

typedef struct {//信号切换上下文
    TASK_IRQ_CONTEXT
    unsigned int R7;	//存放系统调用的ID
    unsigned int count;	//记录是否保存了任务上下文
} sig_switch_context;

typedef struct {//信号控制块(描述符)
    sigset_t sigFlag;		//信号标签集
    sigset_t sigPendFlag;	//信号阻塞标签集,记录因哪些信号被阻塞
    sigset_t sigprocmask; /* Signals that are blocked            */	//进程屏蔽了哪些信号
    sq_queue_t sigactionq;	//信号捕捉队列					
    LOS_DL_LIST waitList;	//等待链表,上面挂的是等待信号到来的任务, 请查找 OsTaskWait(&sigcb->waitList, timeout, TRUE)	理解						
    sigset_t sigwaitmask; /* Waiting for pending signals         */	//任务在等待某某信号的掩码
    siginfo_t sigunbinfo; /* Signal info when task unblocked     */	//任务解除阻止时的信号信息
    sig_switch_context context;	//信号切换上下文, 用于保存切换现场, 比如发生系统调用时的返回,涉及同一个任务的两个栈进行切换							
} sig_cb;

#define SIGEV_THREAD_ID 4

int sys_sigqueue(pid_t, int, const union sigval);
int sys_sigpending(sigset_t *);
int sys_rt_sigtimedwait(const sigset_t *mask, siginfo_t *si, const struct timespec *ts, size_t sigsetsize);
int sys_sigsuspend(const sigset_t *);
int OsKillLock(pid_t pid, int sig);
int OsSigAction(int sig, const sigaction_t *act, sigaction_t *oact);
int OsSigprocMask(int how, const sigset_t_l *set, sigset_t_l *oldset);
int OsPthreadKill(UINT32 tid, int signo);
int OsSigEmptySet(sigset_t *);
int OsSigAddSet(sigset_t *, int);
int OsSigIsMember(const sigset_t *, int);
void OsSaveSignalContext(unsigned int *sp);
void OsRestorSignalContext(unsigned int *sp);
int OsKill(pid_t pid, int sig, int permission);
int OsDispatch(pid_t pid, siginfo_t *info, int permission);
int OsSigTimedWait(sigset_t *set, siginfo_t *info, unsigned int timeout);
int OsPause(void);
int OsSigPending(sigset_t *set);
int OsSigSuspend(const sigset_t *set);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SIGNAL_H */
