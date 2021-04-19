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

#include "mqueue.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "time_posix.h"
#include "user_copy.h"
#include "los_signal.h"
#include "los_strncpy_from_user.h"
/********************************************************
本文说明:包含消息队列和信号两部分内容,是实现IPC的其中两种方式 
IPC（Inter-Process Communication，进程间通信）
每个进程各自有不同的用户地址空间，进程之间地址保护,相互隔离,任何一个进程的信息在另一个进程中都看不到，
所以进程之间要交换数据必须通过内核，在内核中开辟一块缓冲区，进程A把数据从用户空间拷到内核缓冲区，
进程B再从内核缓冲区把数据读走，

IPC实现方式之消息队列:
消息队列特点总结：
（1）消息队列是消息的链表,具有特定的格式,存放在内存中并由消息队列标识符标识.
（2）消息队列允许一个或多个进程向它写入与读取消息.
（3）管道和消息队列的通信数据都是先进先出的原则。
（4）消息队列可以实现消息的随机查询,消息不一定要以先进先出的次序读取,也可以按消息的类型读取.比FIFO更有优势。
（5）消息队列克服了信号承载信息量少，管道只能承载无格式字节流以及缓冲区大小受限等缺点。
（6）目前主要有两种类型的消息队列：POSIX消息队列以及System V消息队列，System V消息队列是随内核持续的，
	 只有在内核重起或者人工删除时，该消息队列才会被删除。
	 
鸿蒙liteos 支持POSIX消息队列并加入了一种自研的消息队列 liteipc,此处重点讲 posix消息队列
********************************************************/
//打开一个消息队列,由posix接口封装
mqd_t SysMqOpen(const char *mqName, int openFlag, mode_t mode, struct mq_attr *attr)
{
    mqd_t ret;
    int retValue;
    char kMqName[PATH_MAX + 1] = { 0 };

    retValue = LOS_StrncpyFromUser(kMqName, mqName, PATH_MAX);
    if (retValue < 0) {
        return retValue;
    }
    ret = mq_open(kMqName, openFlag, mode, attr);//posix ,一个消息队列可以有多个进程向它读写消息
    if (ret == -1) {
        return (mqd_t)-get_errno();
    }
    return ret;
}
//关闭一个消息队列
int SysMqClose(mqd_t personal)
{
    int ret;

    ret = mq_close(personal);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}
/******************************************************
封装posix的标准接口，获取和设置消息队列的属性
new:来判断是否是获取还是设置功能
	new==null 获取
	否则为设置
******************************************************/
int SysMqGetSetAttr(mqd_t mqd, const struct mq_attr *new, struct mq_attr *old)
{
    int ret;
    struct mq_attr knew, kold;

    if (new != NULL) {
        ret = LOS_ArchCopyFromUser(&knew, new, sizeof(struct mq_attr));
        if (ret != 0) {
            return -EFAULT;
        }
    }
    ret = mq_getsetattr(mqd, new ? &knew : NULL, old ? &kold : NULL);
    if (ret < 0) {
        return -get_errno();
    }
    if (old != NULL) {
        ret = LOS_ArchCopyToUser(old, &kold, sizeof(struct mq_attr));
        if (ret != 0) {
            return -EFAULT;
        }
    }
    return ret;
}
/******************************************************
从内核中删除名为mqName的消息队列
如果该函数被调用了,但是仍然有进程已经打开了这个消息队列,那么这个消息队列
的销毁会被推迟到所有的引用都被关闭时执行.并且函数 mq_unlink() 不需要阻塞
到所有的引用都被关闭为止,它会立即返回.函数 mq_unlink()调用成功后, 如果在
随后调用 mq_open() 时重用这个消息队列名字,效果就像这个名字的消息队列不存在,
如果没有设置O_CREAT标志,函数mq_open() 会返回失败,否则会创建一个新的消息队列.
******************************************************/
int SysMqUnlink(const char *mqName)
{
    int ret;
    int retValue;
    char kMqName[PATH_MAX + 1] = { 0 };

    retValue = LOS_StrncpyFromUser(kMqName, mqName, PATH_MAX);
    if (retValue < 0) {
        return retValue;
    }

    ret = mq_unlink(kMqName);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}
/******************************************************
定时时间发送消息,任务将被阻塞,等待被唤醒写入消息
******************************************************/
int SysMqTimedSend(mqd_t personal, const char *msg, size_t msgLen, unsigned int msgPrio,
                   const struct timespec *absTimeout)
{
    int ret;
    struct timespec timeout;
    char *msgIntr = NULL;

    if (absTimeout != NULL) {
        ret = LOS_ArchCopyFromUser(&timeout, absTimeout, sizeof(struct timespec));
        if (ret != 0) {
            return -EFAULT;
        }
    }
    if (msgLen == 0) {
        return -EINVAL;
    }
    msgIntr = (char *)malloc(msgLen);
    if (msgIntr == NULL) {
        return -ENOMEM;
    }
    ret = LOS_ArchCopyFromUser(msgIntr, msg, msgLen);
    if (ret != 0) {
        free(msgIntr);
        return -EFAULT;
    }
    ret = mq_timedsend(personal, msgIntr, msgLen, msgPrio, absTimeout ? &timeout : NULL);//posix 接口的实现
    free(msgIntr);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}
/******************************************************
定时接收消息,任务将被阻塞,等待被唤醒读取
******************************************************/
ssize_t SysMqTimedReceive(mqd_t personal, char *msg, size_t msgLen, unsigned int *msgPrio,
                          const struct timespec *absTimeout)
{
    int ret, receiveLen;
    struct timespec timeout;
    char *msgIntr = NULL;
    unsigned int kMsgPrio;

    if (absTimeout != NULL) {
        ret = LOS_ArchCopyFromUser(&timeout, absTimeout, sizeof(struct timespec));
        if (ret != 0) {
            return -EFAULT;
        }
    }
    if (msgLen == 0) {
        return -EINVAL;
    }
    msgIntr = (char *)malloc(msgLen);
    if (msgIntr == NULL) {
        return -ENOMEM;
    }
    receiveLen = mq_timedreceive(personal, msgIntr, msgLen, &kMsgPrio, absTimeout ? &timeout : NULL);//posix 接口的实现
    if (receiveLen < 0) {
        free(msgIntr);
        return -get_errno();
    }

    if (msgPrio != NULL) {
        ret = LOS_ArchCopyToUser(msgPrio, &kMsgPrio, sizeof(unsigned int));
        if (ret != 0) {
            free(msgIntr);
            return -EFAULT;
        }
    }

    ret = LOS_ArchCopyToUser(msg, msgIntr, receiveLen);
    free(msgIntr);
    if (ret != 0) {
        return -EFAULT;
    }
    return receiveLen;
}
//系统调用之捕捉信号,鸿蒙内核只捕捉了SIGSYS 信号
int SysSigAction(int sig, const sigaction_t *restrict sa, sigaction_t *restrict old, size_t sigsetsize)
{
    return OsSigAction(sig, sa, old);
}

/*****************************************************
系统调用之进程信号屏蔽, 
什么意思?简单说就是 一个信号来了进程要不要处理,屏蔽就是不处理,注意不能屏蔽SIGKILL和SIGSTOP信号,必须要处理.

how
SIG_BLOCK	  加入信号到进程屏蔽。set包含了希望阻塞的附加信号
SIG_UNBLOCK	  从进程屏蔽里将信号删除。set包含了希望解除阻塞的信号
SIG_SETMASK	  将set的值设定为新的进程屏蔽
*****************************************************/
int SysSigprocMask(int how, const sigset_t_l *restrict setl, sigset_t_l *restrict oldl, size_t sigsetsize)
{
    /* Let nxsig_procmask do all of the work */
    return OsSigprocMask(how, setl, oldl);
}
//系统调用之向进程发送信号
int SysKill(pid_t pid, int sig)
{
    return OsKillLock(pid, sig);
}
//系统调用之之向进程发送信号
int SysPthreadKill(pid_t pid, int sig)
{
    return OsPthreadKill(pid, sig);
}

int SysSigTimedWait(const sigset_t_l *setl, siginfo_t *info, const struct timespec *timeout, size_t sigsetsize)
{
    sigset_t set;
    unsigned int tick;
    int retVal, ret;
    siginfo_t infoIntr;
    struct timespec timeoutIntr;

    retVal = LOS_ArchCopyFromUser(&set, &(setl->sig[0]), sizeof(sigset_t));
    if (retVal != 0) {
        return -EFAULT;
    }

    if (timeout == NULL) {
        tick = LOS_WAIT_FOREVER;
    } else {
        retVal = LOS_ArchCopyFromUser(&timeoutIntr, timeout, sizeof(struct timespec));
        if (retVal != 0) {
            return -EFAULT;
        }
        if (!ValidTimeSpec(&timeoutIntr)) {
            return -EINVAL;
        }
        tick = OsTimeSpec2Tick(&timeoutIntr);
    }
    ret = OsSigTimedWait(&set, &infoIntr, tick);
    if (ret < 0) {
        return ret;
    }
    if (info != NULL) {
        retVal = LOS_ArchCopyToUser(info, &infoIntr, sizeof(siginfo_t));
        if (retVal != 0) {
            return -EFAULT;
        }
    }
    return (ret == 0 ? infoIntr.si_signo : ret);
}
//系统调用之暂停任务
int SysPause(void)
{
    return OsPause();
}
//获取阻塞当前任务的信号集
int SysSigPending(sigset_t_l *setl)
{
    sigset_t set;
    int ret;

    ret = LOS_ArchCopyFromUser(&set, &(setl->sig[0]), sizeof(sigset_t));
    if (ret != 0) {
        return -EFAULT;
    }
    ret = OsSigPending(&set);
    if (ret != LOS_OK) {
        return ret;
    }
    ret = LOS_ArchCopyToUser(&(setl->sig[0]), &set, sizeof(sigset_t));
    if (ret != LOS_OK) {
        return -EFAULT;
    }
    return ret;
}
//
int SysSigSuspend(sigset_t_l *setl)
{
    sigset_t set;
    int retVal;

    retVal = LOS_ArchCopyFromUser(&set, &(setl->sig[0]), sizeof(sigset_t));
    if (retVal != 0) {
        return -EFAULT;
    }

    return OsSigSuspend(&set);
}

int SysMkFifo(const char *pathName, mode_t mode)
{
    int retValue;
    char kPathName[PATH_MAX + 1] = { 0 };

    retValue = LOS_StrncpyFromUser(kPathName, pathName, PATH_MAX);
    if (retValue < 0) {
        return retValue;
    }
    return mkfifo(kPathName, mode);
}

