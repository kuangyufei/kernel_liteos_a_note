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
/**
 * @file los_signal.h
 * @brief 
 * @verbatim
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
    #define SIGTERM   15	//终止信号, kill不带参数时默认发送这个信号
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

    https://www.cnblogs.com/hoys/archive/2012/08/19/2646377.html

    Musl官网 http://musl.libc.org/ 
    musl是构建在Linux系统调用API之上的C标准库的实现，包括在基本语言标准POSIX中定义的接口，
    以及广泛认可的扩展。musl是轻量级的，快速的，简单的，自由的.    
 * @endverbatim
 * @param pathname 
 * @return int 
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

/**
 * @brief 设置变量指定位为1
 * @details 将val参数的第bit位设置为1，支持64位操作
 * @param[in,out] val 待操作的变量
 * @param[in] bit 要设置的位位置（0-63）
 * @note 使用1ULL确保64位无符号运算，避免移位溢出
 */
#define LOS_BIT_SET(val, bit) ((val) = (val) | (1ULL << (UINT32)(bit)))

/**
 * @brief 清除变量指定位为0
 * @details 将val参数的第bit位清除为0，支持64位操作
 * @param[in,out] val 待操作的变量
 * @param[in] bit 要清除的位位置（0-63）
 * @note 使用1ULL确保64位无符号运算，避免移位溢出
 */
#define LOS_BIT_CLR(val, bit) ((val) = (val) & ~(1ULL << (UINT32)(bit)))

/**
 * @brief 检查变量指定位是否为1
 * @details 检查val参数的第bit位是否为1，返回布尔值
 * @param[in] val 待检查的变量
 * @param[in] bit 要检查的位位置（0-63）
 * @return bool - 位为1返回true，否则返回false
 * @note 使用1ULL确保64位无符号运算
 */
#define LOS_IS_BIT_SET(val, bit) (bool)((((val) >> (UINT32)(bit)) & 1ULL))

/**
 * @brief 信号遍历停止标志
 * @details 用于信号链表遍历时表示停止遍历的返回值
 * @note 该值在信号处理回调函数中使用
 */
#define SIG_STOP_VISIT 1

/**
 * @brief 内核态进程杀死权限
 * @details 表示内核态进程拥有杀死其他进程的权限等级
 * @note 值为0表示最高权限
 */
#define OS_KERNEL_KILL_PERMISSION 0U

/**
 * @brief 用户态进程杀死权限
 * @details 表示用户态进程拥有杀死其他进程的权限等级
 * @note 值为3表示普通用户权限
 */
#define OS_USER_KILL_PERMISSION   3U

/**
 * @brief 条件返回宏
 * @details 如果表达式expr为真，则返回错误码errcode
 * @param[in] expr 条件表达式
 * @param[in] errcode 错误码
 * @note 通常用于函数入口参数检查
 */
#define OS_RETURN_IF(expr, errcode) \
    if ((expr)) {                   \
        return errcode;             \
    }

/**
 * @brief 无返回值条件返回宏
 * @details 如果表达式expr为真，则执行return语句
 * @param[in] expr 条件表达式
 * @note 用于返回类型为void的函数
 */
#define OS_RETURN_IF_VOID(expr) \
    if ((expr)) {               \
        return;                 \
    }

/**
 * @brief 条件跳转宏
 * @details 如果表达式expr为真，则设置错误码并跳转到EXIT标签
 * @param[in] expr 条件表达式
 * @param[in] errcode 错误码
 * @note 假设函数内已定义ret变量和EXIT标签
 */
#define OS_GOTO_EXIT_IF(expr, errcode) \
    if (expr) {                        \
        ret = errcode;                 \
        goto EXIT;                     \
    }

/**
 * @brief 无条件跳转宏
 * @details 如果表达式expr为真，则直接跳转到EXIT标签
 * @param[in] expr 条件表达式
 * @note 假设函数内已定义EXIT标签
 */
#define OS_GOTO_EXIT_IF_ONLY(expr) \
    if (expr) {                    \
        goto EXIT;                 \
    }

/**
 * @brief 空指针检查返回宏（无返回值）
 * @details 如果指针pPara为NULL，则执行return语句
 * @param[in] pPara 待检查的指针
 * @note 用于返回类型为void的函数参数检查
 */
#define OS_RETURN_VOID_IF_NULL(pPara) \
    if (NULL == (pPara)) {            \
        return;                       \
    }

/**
 * @brief 空指针检查返回宏
 * @details 如果指针pPara为NULL，则返回-EINVAL错误码
 * @param[in] pPara 待检查的指针
 * @return -EINVAL - 表示无效参数
 * @note 用于标准错误码返回的函数
 */
#define OS_RETURN_IF_NULL(pPara) \
    if (NULL == (pPara)) {       \
        return (-EINVAL);          \
    }

/**
 * @brief 空指针检查跳转宏
 * @details 如果指针pPara为NULL，则设置ret为-EINVAL并跳转到EXIT标签
 * @param[in] pPara 待检查的指针
 * @note 假设函数内已定义ret变量和EXIT标签
 */
#define OS_GOTO_EXIT_IF_NULL(pPara) \
    if (NULL == (pPara)) {          \
        ret = -EINVAL;              \
        goto EXIT;                  \
    }

/**
 * @brief 信号处理函数指针类型
 * @details 标准信号处理函数原型，仅接收信号编号
 * @param[in] signo 信号编号
 */
typedef void (*sa_sighandler_t)(int);

/**
 * @brief 带信号信息的处理函数指针类型
 * @details 扩展信号处理函数原型，可接收信号编号、信号信息和上下文
 * @param[in] signo 信号编号
 * @param[in] info 信号详细信息结构体指针
 * @param[in] context 信号处理上下文指针
 */
typedef void (*sa_siginfoaction_t)(int, siginfo_t *, void *);

/**
 * @brief 将信号编号转换为信号集
 * @details 将信号编号s转换为对应的信号集（仅该信号位为1）
 * @param[in] s 信号编号（1-64）
 * @return sigset_t - 包含指定信号的信号集
 * @note 信号编号从1开始，对应位0不使用
 */
#define SIGNO2SET(s) ((sigset_t)1ULL << (s))

/**
 * @brief 空信号集
 * @details 所有位均为0的信号集，表示无任何信号
 * @note 用于初始化信号集变量
 */
#define NULL_SIGNAL_SET ((sigset_t)0ULL)

/**
 * @brief 全信号集
 * @details 所有位均为1的信号集，表示包含所有信号
 * @note 值为~0ULL（十进制为18446744073709551615）
 */
#define FULL_SIGNAL_SET ((sigset_t)~0ULL)

/**
 * @brief 检查信号编号是否有效
 * @details 判断信号编号是否在有效范围内（小于_NSIG）
 * @param[in] sig 信号编号
 * @return int - 有效返回1，无效返回0
 * @note _NSIG通常定义为65，表示最大信号编号为64
 */
static inline int GOOD_SIGNO(unsigned int sig)
{
    return (sig < _NSIG) ? 1 : 0;
}

/**
 * @brief MUSL库信号数组最大长度
 * @details 定义MUSL标准库中信号集数组的最大长度
 * @note 值为128，与MUSL库实现保持兼容
 */
#define MAX_SIG_ARRAY_IN_MUSL 128

/**
 * @brief 信号集结构体（MUSL兼容）
 * @details 用于存储信号集的结构体，与MUSL库接口兼容
 * @core 通过unsigned long数组实现，每个元素表示64个信号
 * @note 数组大小为MAX_SIG_ARRAY_IN_MUSL / sizeof(unsigned long)，通常为2个元素
 */
typedef struct {
    unsigned long sig[MAX_SIG_ARRAY_IN_MUSL / sizeof(unsigned long)]; /* 信号集存储数组 */
} sigset_t_l;

/***************************************************
struct sigaction {
	union {
		void (*sa_handler)(int); //信号处理函数——普通版
		void (*sa_sigaction)(int, siginfo_t *, void *);//信号处理函数——高级版
	} __sa_handler;
	sigset_t sa_mask;//指定信号处理程序执行过程中需要阻塞的信号；
	int sa_flags;	 //标示位
					 //	SA_RESTART：使被信号打断的syscall重新发起。
					 //	SA_NOCLDSTOP：使父进程在它的子进程暂停或继续运行时不会收到 SIGCHLD 信号。
					 //	SA_NOCLDWAIT：使父进程在它的子进程退出时不会收到SIGCHLD信号，这时子进程如果退出也不会成为僵 尸进程。
					 //	SA_NODEFER：使对信号的屏蔽无效，即在信号处理函数执行期间仍能发出这个信号。
					 //	SA_RESETHAND：信号处理之后重新设置为默认的处理方式。
					 //	SA_SIGINFO：使用sa_sigaction成员而不是sa_handler作为信号处理函数。
	void (*sa_restorer)(void);
};
****************************************************/
typedef struct sigaction sigaction_t;

/**
 * @brief 信号操作队列节点结构体
 * @details 用于管理任务的信号处理动作，形成链表结构存储多个信号的处理方式
 * @attention 该结构体通过flink成员形成单向链表，不应直接修改链表指针
 */
struct sigactq {
    struct sigactq *flink; /* 前向链接指针，指向下一个信号操作队列节点 */
    sigaction_t act;       /* 信号操作结构体，包含信号处理函数、掩码等信息 */
    uint8_t signo;         /* 关联的信号编号，取值范围1-64（标准信号） */
}; 
typedef struct sigactq sigactq_t; /* 信号操作队列节点类型定义 */

/**
 * @brief 信号队列入口结构体
 * @details 作为信号队列的基础节点结构，用于构建各种信号相关队列
 * @note 所有基于信号的队列结构都应使用此入口结构
 */
struct sq_entry_s {
    struct sq_entry_s *flink; /* 前向链接指针，指向下一个队列节点 */
}; 
typedef struct sq_entry_s sq_entry_t; /* 信号队列入口类型定义 */

/**
 * @brief 待处理信号队列结构体
 * @details 存储等待被处理的信号信息，形成链表结构管理多个待处理信号
 * @attention 内核会定期扫描该队列并分发信号，用户不应直接操作
 */
struct sigpendq {
    struct sigpendq *flink; /* 前向链接指针，指向下一个待处理信号节点 */
    siginfo_t info;         /* 信号信息结构体，包含信号类型、发送者PID等详细信息 */
    uint8_t type;           /* 信号分配管理类型，0表示动态分配，1表示静态分配 */
}; 
typedef struct sigpendq sigpendq_t; /* 待处理信号队列类型定义 */

/**
 * @brief 信号队列结构
 * @details 用于管理信号相关节点的队列，采用头尾指针实现高效的入队出队操作
 * @note 此结构适用于FIFO（先进先出）类型的信号处理场景
 */
struct sq_queue_s {
    sq_entry_t *head; /* 队列头指针，指向队列中的第一个节点 */
    sq_entry_t *tail; /* 队列尾指针，指向队列中的最后一个节点 */
}; 
typedef struct sq_queue_s sq_queue_t; /* 信号队列类型定义 */

/**
 * @brief 信号信息链表节点
 * @details 用于临时存储信号信息，形成链表结构处理批量信号
 * @attention 该链表在信号处理完成后需及时释放，避免内存泄漏
 */
typedef struct SigInfoListNode {
    struct SigInfoListNode *next; /* 指向下一个信号信息节点的指针 */
    siginfo_t info;               /* 信号信息结构体，包含信号的详细信息 */
} SigInfoListNode; 

/**
 * @brief 信号控制块结构体
 * @details 每个任务对应一个信号控制块，管理任务的信号掩码、待处理信号和信号处理队列
 * @core 负责任务信号的整体管理，包括信号阻塞、挂起、等待和分发机制
 * @constraint 每个任务只能有一个信号控制块，由内核自动创建和释放
 */
typedef struct {
    sigset_t sigFlag;              /* 信号标志集，表示当前已处理的信号 */
    sigset_t sigPendFlag;          /* 待处理信号标志集，bit位置1表示对应信号待处理 */
    sigset_t sigprocmask;          /* 信号阻塞掩码，bit位置1表示对应信号被阻塞 */
    sq_queue_t sigactionq;         /* 信号操作队列，存储该任务注册的所有信号处理动作 */
    LOS_DL_LIST waitList;          /* 等待信号的任务链表，使用双向循环链表实现 */
    sigset_t sigwaitmask;          /* 等待的信号掩码，表示任务正在等待的信号集合 */
    siginfo_t sigunbinfo;          /* 任务解除阻塞时的信号信息，记录唤醒任务的信号 */
    SigInfoListNode *tmpInfoListHead; /* 临时信号信息链表头指针，用于批量处理信号 */
    unsigned int sigIntLock;       /* 信号中断锁，0表示未锁定，非0表示锁定计数 */
    void *sigContext;              /* 信号上下文指针，指向信号处理时的CPU上下文 */
    unsigned int count;            /* 信号处理计数，记录当前正在处理的信号数量 */
} sig_cb; 
typedef struct ProcessCB LosProcessCB;

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
int OsKill(pid_t pid, int sig, int permission);
int OsSendSigToProcess(LosProcessCB *spcb, int sig, int permission);
int OsDispatch(pid_t pid, siginfo_t *info, int permission);
int OsSigTimedWait(sigset_t *set, siginfo_t *info, unsigned int timeout);
int OsPause(void);
int OsSigPending(sigset_t *set);
int OsSigSuspend(const sigset_t *set);
VOID OsSigIntLock(VOID);
VOID OsSigIntUnlock(VOID);
INT32 OsTaskKillUnsafe(UINT32 taskID, INT32 signo);
VOID OsClearSigInfoTmpList(sig_cb *sigcb);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SIGNAL_H */
