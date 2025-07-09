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

#include "syscall_pub.h"
#include "mqueue.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "time_posix.h"
#include "user_copy.h"
#include "los_signal.h"
#include "los_process_pri.h"
#include "los_strncpy_from_user.h"
#include "fs/file.h"

// 将用户空间的消息队列文件描述符转换为内核空间文件描述符
// 参数: id - 待转换的用户空间文件描述符
#define MQUEUE_FD_U2K(id) \
    do { \
        int sysFd = GetAssociatedSystemFd((INTPTR)(id)); \
        (id) = (mqd_t)sysFd; \
    } while (0)

// 将内核空间的消息队列文件描述符转换为用户空间文件描述符
// 参数: id - 待转换的内核空间文件描述符 
#define MQUEUE_FD_K2U(id) \
    do { \
        int procFd = AllocAndAssocProcessFd((INTPTR)(id), MIN_START_FD); \
        if (procFd == -1) { \
            mq_close(id); \
            set_errno(EMFILE); \
            (id) = (mqd_t)(-EMFILE); \
        } else { \
            (id) = (mqd_t)procFd; \
        } \
    } while (0)

/**
 * @brief 打开或创建消息队列
 * @param mqName 用户空间消息队列名称
 * @param openFlag 打开标志（如O_RDONLY, O_WRONLY, O_CREAT等）
 * @param mode 文件权限位
 * @param attr 消息队列属性（可为NULL）
 * @return 成功返回消息队列描述符，失败返回错误码负值
 */
mqd_t SysMqOpen(const char *mqName, int openFlag, mode_t mode, struct mq_attr *attr)
{
    mqd_t ret;                  // 返回值
    int retValue;               // 临时返回值
    char kMqName[PATH_MAX + 1] = { 0 };  // 内核空间消息队列名称缓冲区

    // 从用户空间拷贝消息队列名称到内核空间
    retValue = LOS_StrncpyFromUser(kMqName, mqName, PATH_MAX);
    if (retValue < 0) {         // 拷贝失败检查
        return retValue;
    }
    // 调用内核消息队列打开接口
    ret = mq_open(kMqName, openFlag, mode, attr);
    if (ret == -1) {            // 打开失败处理
        return (mqd_t)-get_errno();
    }
    /* 将内核文件描述符转换为用户进程文件描述符 */
    MQUEUE_FD_K2U(ret);
    return ret;
}

/**
 * @brief 关闭消息队列
 * @param personal 用户空间消息队列描述符
 * @return 成功返回0，失败返回错误码负值
 */
int SysMqClose(mqd_t personal)
{
    int ret;                    // 返回值
    int ufd = (INTPTR)personal; // 保存用户空间文件描述符

    MQUEUE_FD_U2K(personal);    // 将用户fd转换为内核fd
    ret = mq_close(personal);   // 调用内核关闭接口
    if (ret < 0) {              // 关闭失败处理
        return -get_errno();
    }
    FreeProcessFd(ufd);         // 释放进程文件描述符
    return ret;
}

/**
 * @brief 注册消息队列通知事件
 * @param personal 用户空间消息队列描述符
 * @param sigev 信号事件结构（包含通知方式和信号信息）
 * @return 成功返回0，失败返回错误码负值
 */
int SysMqNotify(mqd_t personal, const struct sigevent *sigev)
{
    int ret;                    // 返回值
    struct sigevent ksigev;     // 内核空间信号事件结构

    // 从用户空间拷贝信号事件结构
    ret = LOS_ArchCopyFromUser(&ksigev, sigev, sizeof(struct sigevent));
    if (ret != 0) {             // 拷贝失败返回EFAULT
        return -EFAULT;
    }

    MQUEUE_FD_U2K(personal);    // 将用户fd转换为内核fd
    ret = OsMqNotify(personal, &ksigev); // 调用内核通知注册接口
    if (ret < 0) {              // 注册失败处理
        return -get_errno();
    }
    return ret;
}

/**
 * @brief 获取/设置消息队列属性
 * @param mqd 用户空间消息队列描述符
 * @param new 新属性设置（可为NULL，表示仅获取）
 * @param old 用于存储旧属性的缓冲区（可为NULL，表示不获取）
 * @return 成功返回0，失败返回错误码负值
 */
int SysMqGetSetAttr(mqd_t mqd, const struct mq_attr *new, struct mq_attr *old)
{
    int ret;                    // 返回值
    struct mq_attr knew;        // 内核空间新属性结构
    struct mq_attr kold = { 0 }; // 内核空间旧属性结构

    if (new != NULL) {          // 如果需要设置新属性
        // 从用户空间拷贝新属性
        ret = LOS_ArchCopyFromUser(&knew, new, sizeof(struct mq_attr));
        if (ret != 0) {         // 拷贝失败返回EFAULT
            return -EFAULT;
        }
    }
    MQUEUE_FD_U2K(mqd);         // 将用户fd转换为内核fd
    // 调用内核获取/设置属性接口
    ret = mq_getsetattr(mqd, new ? &knew : NULL, old ? &kold : NULL);
    if (ret < 0) {              // 操作失败处理
        return -get_errno();
    }
    if (old != NULL) {          // 如果需要获取旧属性
        // 将内核空间旧属性拷贝到用户空间
        ret = LOS_ArchCopyToUser(old, &kold, sizeof(struct mq_attr));
        if (ret != 0) {         // 拷贝失败返回EFAULT
            return -EFAULT;
        }
    }
    return ret;
}

/**
 * @brief 删除消息队列
 * @param mqName 用户空间消息队列名称
 * @return 成功返回0，失败返回错误码负值
 */
int SysMqUnlink(const char *mqName)
{
    int ret;                    // 返回值
    int retValue;               // 临时返回值
    char kMqName[PATH_MAX + 1] = { 0 };  // 内核空间消息队列名称缓冲区

    // 从用户空间拷贝消息队列名称
    retValue = LOS_StrncpyFromUser(kMqName, mqName, PATH_MAX);
    if (retValue < 0) {         // 拷贝失败处理
        return retValue;
    }

    ret = mq_unlink(kMqName);   // 调用内核删除接口
    if (ret < 0) {              // 删除失败处理
        return -get_errno();
    }
    return ret;
}

/**
 * @brief 带超时的消息发送
 * @param personal 用户空间消息队列描述符
 * @param msg 用户空间消息缓冲区
 * @param msgLen 消息长度
 * @param msgPrio 消息优先级
 * @param absTimeout 绝对超时时间（可为NULL，表示阻塞等待）
 * @return 成功返回0，失败返回错误码负值
 */
int SysMqTimedSend(mqd_t personal, const char *msg, size_t msgLen, unsigned int msgPrio,
                   const struct timespec *absTimeout)
{
    int ret;                    // 返回值
    struct timespec timeout;    // 内核空间超时结构
    char *msgIntr = NULL;       // 内核空间消息缓冲区

    if (absTimeout != NULL) {   // 如果指定了超时时间
        // 从用户空间拷贝超时结构
        ret = LOS_ArchCopyFromUser(&timeout, absTimeout, sizeof(struct timespec));
        if (ret != 0) {         // 拷贝失败返回EFAULT
            return -EFAULT;
        }
    }
    if (msgLen == 0) {          // 消息长度为0检查
        return -EINVAL;         // 返回参数无效错误
    }
    msgIntr = (char *)malloc(msgLen); // 分配内核消息缓冲区
    if (msgIntr == NULL) {      // 内存分配失败检查
        return -ENOMEM;         // 返回内存不足错误
    }
    // 从用户空间拷贝消息内容
    ret = LOS_ArchCopyFromUser(msgIntr, msg, msgLen);
    if (ret != 0) {             // 拷贝失败处理
        free(msgIntr);          // 释放已分配内存
        return -EFAULT;
    }
    MQUEUE_FD_U2K(personal);    // 将用户fd转换为内核fd
    // 调用内核带超时发送接口
    ret = mq_timedsend(personal, msgIntr, msgLen, msgPrio, absTimeout ? &timeout : NULL);
    free(msgIntr);              // 释放内核消息缓冲区
    if (ret < 0) {              // 发送失败处理
        return -get_errno();
    }
    return ret;
}

/**
 * @brief 带超时的消息接收
 * @param personal 用户空间消息队列描述符
 * @param msg 用户空间消息缓冲区
 * @param msgLen 缓冲区长度
 * @param msgPrio 用于存储消息优先级的指针（可为NULL）
 * @param absTimeout 绝对超时时间（可为NULL，表示阻塞等待）
 * @return 成功返回接收的消息长度，失败返回错误码负值
 */
ssize_t SysMqTimedReceive(mqd_t personal, char *msg, size_t msgLen, unsigned int *msgPrio,
                          const struct timespec *absTimeout)
{
    int ret, receiveLen;        // 返回值和接收长度
    struct timespec timeout;    // 内核空间超时结构
    char *msgIntr = NULL;       // 内核空间消息缓冲区
    unsigned int kMsgPrio;      // 内核空间消息优先级

    if (absTimeout != NULL) {   // 如果指定了超时时间
        // 从用户空间拷贝超时结构
        ret = LOS_ArchCopyFromUser(&timeout, absTimeout, sizeof(struct timespec));
        if (ret != 0) {         // 拷贝失败返回EFAULT
            return -EFAULT;
        }
    }
    if (msgLen == 0) {          // 缓冲区长度为0检查
        return -EINVAL;         // 返回参数无效错误
    }
    msgIntr = (char *)malloc(msgLen); // 分配内核消息缓冲区
    if (msgIntr == NULL) {      // 内存分配失败检查
        return -ENOMEM;         // 返回内存不足错误
    }
    MQUEUE_FD_U2K(personal);    // 将用户fd转换为内核fd
    // 调用内核带超时接收接口
    receiveLen = mq_timedreceive(personal, msgIntr, msgLen, &kMsgPrio, absTimeout ? &timeout : NULL);
    if (receiveLen < 0) {       // 接收失败处理
        free(msgIntr);          // 释放已分配内存
        return -get_errno();
    }

    if (msgPrio != NULL) {      // 如果需要获取消息优先级
        // 将内核空间优先级拷贝到用户空间
        ret = LOS_ArchCopyToUser(msgPrio, &kMsgPrio, sizeof(unsigned int));
        if (ret != 0) {         // 拷贝失败处理
            free(msgIntr);      // 释放已分配内存
            return -EFAULT;
        }
    }

    // 将接收的消息从内核空间拷贝到用户空间
    ret = LOS_ArchCopyToUser(msg, msgIntr, receiveLen);
    free(msgIntr);              // 释放内核消息缓冲区
    if (ret != 0) {             // 拷贝失败处理
        return -EFAULT;
    }
    return receiveLen;          // 返回接收的消息长度
}

/**
 * @brief 设置信号处理动作
 * @param sig 信号编号
 * @param sa 新的信号处理结构（可为NULL）
 * @param old 用于存储旧处理结构的缓冲区（可为NULL）
 * @param sigsetsize 信号集大小（未使用）
 * @return 成功返回0，失败返回错误码
 */
int SysSigAction(int sig, const sigaction_t *restrict sa, sigaction_t *restrict old, size_t sigsetsize)
{
    return OsSigAction(sig, sa, old);  // 调用内核信号处理接口
}

/**
 * @brief 修改进程信号屏蔽字
 * @param how 修改方式（SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK）
 * @param setl 新的信号集（可为NULL）
 * @param oldl 用于存储旧信号集的缓冲区（可为NULL）
 * @param sigsetsize 信号集大小
 * @return 成功返回0，失败返回错误码
 */
int SysSigprocMask(int how, const sigset_t_l *restrict setl, sigset_t_l *restrict oldl, size_t sigsetsize)
{
    CHECK_ASPACE(setl, sizeof(sigset_t_l));  // 检查用户空间地址有效性
    CHECK_ASPACE(oldl, sizeof(sigset_t_l));  // 检查用户空间地址有效性
    CPY_FROM_USER(setl);                     // 从用户空间拷贝信号集
    CPY_FROM_USER(oldl);                     // 从用户空间拷贝信号集
    /* 调用内核信号屏蔽字修改接口 */
    int ret = OsSigprocMask(how, setl, oldl);
    CPY_TO_USER(oldl);                       // 将旧信号集拷贝回用户空间
    return ret;
}

/**
 * @brief 向进程发送信号
 * @param pid 目标进程ID
 * @param sig 信号编号
 * @return 成功返回0，失败返回错误码
 */
int SysKill(pid_t pid, int sig)
{
    return OsKillLock(pid, sig);  // 调用内核带锁的kill接口
}

/**
 * @brief 向线程发送信号
 * @param pid 目标线程ID
 * @param sig 信号编号
 * @return 成功返回0，失败返回错误码
 */
int SysPthreadKill(pid_t pid, int sig)
{
    return OsPthreadKill(pid, sig);  // 调用内核线程kill接口
}

/**
 * @brief 带超时的信号等待
 * @param setl 用户空间信号集
 * @param info 用于存储信号信息的结构（可为NULL）
 * @param timeout 超时时间（可为NULL，表示永久等待）
 * @param sigsetsize 信号集大小
 * @return 成功返回接收到的信号编号，失败返回错误码
 */
int SysSigTimedWait(const sigset_t_l *setl, siginfo_t *info, const struct timespec *timeout, size_t sigsetsize)
{
    sigset_t set;               // 内核空间信号集
    unsigned int tick;          // 转换后的超时tick数
    int retVal, ret;            // 临时返回值和函数返回值
    siginfo_t infoIntr = { 0 }; // 内核空间信号信息结构
    struct timespec timeoutIntr;// 内核空间超时结构

    // 从用户空间拷贝信号集
    retVal = LOS_ArchCopyFromUser(&set, &(setl->sig[0]), sizeof(sigset_t));
    if (retVal != 0) {          // 拷贝失败返回EFAULT
        return -EFAULT;
    }

    if (timeout == NULL) {      // 如果未指定超时时间
        tick = LOS_WAIT_FOREVER; // 设置为永久等待
    } else {
        // 从用户空间拷贝超时结构
        retVal = LOS_ArchCopyFromUser(&timeoutIntr, timeout, sizeof(struct timespec));
        if (retVal != 0) {      // 拷贝失败返回EFAULT
            return -EFAULT;
        }
        if (!ValidTimeSpec(&timeoutIntr)) { // 检查超时时间有效性
            return -EINVAL;     // 无效时间返回EINVAL
        }
        tick = OsTimeSpec2Tick(&timeoutIntr); // 将timespec转换为tick数
    }
    // 调用内核带超时信号等待接口
    ret = OsSigTimedWait(&set, &infoIntr, tick);
    if (ret < 0) {              // 等待失败处理
        return ret;
    }
    if (info != NULL) {         // 如果需要返回信号信息
        // 将内核空间信号信息拷贝到用户空间
        retVal = LOS_ArchCopyToUser(info, &infoIntr, sizeof(siginfo_t));
        if (retVal != 0) {      // 拷贝失败返回EFAULT
            return -EFAULT;
        }
    }
    return (ret == 0 ? infoIntr.si_signo : ret); // 返回接收到的信号编号
}

/**
 * @brief 挂起进程直到收到信号
 * @return 始终返回-1并设置errno为EINTR
 */
int SysPause(void)
{
    return OsPause();  // 调用内核pause接口
}

/**
 * @brief 获取当前挂起的信号集
 * @param setl 用于存储信号集的用户空间缓冲区
 * @return 成功返回0，失败返回错误码
 */
int SysSigPending(sigset_t_l *setl)
{
    sigset_t set;               // 内核空间信号集
    int ret;                    // 返回值

    // 从用户空间拷贝信号集
    ret = LOS_ArchCopyFromUser(&set, &(setl->sig[0]), sizeof(sigset_t));
    if (ret != 0) {             // 拷贝失败返回EFAULT
        return -EFAULT;
    }
    ret = OsSigPending(&set);   // 调用内核获取挂起信号接口
    if (ret != LOS_OK) {        // 操作失败处理
        return ret;
    }
    // 将内核空间信号集拷贝回用户空间
    ret = LOS_ArchCopyToUser(&(setl->sig[0]), &set, sizeof(sigset_t));
    if (ret != LOS_OK) {        // 拷贝失败返回EFAULT
        return -EFAULT;
    }
    return ret;
}

/**
 * @brief 挂起进程并替换信号屏蔽字
 * @param setl 新的信号屏蔽字
 * @return 始终返回-1并设置errno为EINTR
 */
int SysSigSuspend(sigset_t_l *setl)
{
    sigset_t set;               // 内核空间信号集
    int retVal;                 // 临时返回值

    // 从用户空间拷贝信号集
    retVal = LOS_ArchCopyFromUser(&set, &(setl->sig[0]), sizeof(sigset_t));
    if (retVal != 0) {          // 拷贝失败返回EFAULT
        return -EFAULT;
    }

    return OsSigSuspend(&set);  // 调用内核suspend接口
}

#ifdef LOSCFG_KERNEL_PIPE
/**
 * @brief 创建命名管道
 * @param pathName 管道路径名
 * @param mode 文件权限位
 * @return 成功返回0，失败返回错误码
 */
int SysMkFifo(const char *pathName, mode_t mode)
{
    int retValue;               // 临时返回值
    char kPathName[PATH_MAX + 1] = { 0 };  // 内核空间路径名缓冲区

    // 从用户空间拷贝路径名
    retValue = LOS_StrncpyFromUser(kPathName, pathName, PATH_MAX);
    if (retValue < 0) {         // 拷贝失败处理
        return retValue;
    }
    return mkfifo(kPathName, mode);  // 调用内核mkfifo接口
}
#endif
