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
 * @defgroup mqueue Message queue
 * @ingroup posix
 */

#ifndef _HWLITEOS_POSIX_MQUEUE_H
#define _HWLITEOS_POSIX_MQUEUE_H

/* INCLUDES */
#include "stdarg.h"
#include "stdlib.h"
#include "limits.h"
#include "los_typedef.h"
#include "time.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "los_queue_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @file mqueue.h
 * @brief POSIX消息队列兼容层头文件
 * @details 定义消息队列相关的宏、数据结构和接口，实现POSIX标准的消息队列功能
 *          提供进程间通信的消息传递机制，支持消息优先级和异步通知
 */

/**
 * @defgroup MQUEUE_MACROS 消息队列宏定义
 * @{ 
 */

/**
 * @ingroup mqueue
 * @brief 消息队列中最大消息数量
 * @note 定义一个消息队列可容纳的最大消息条数，固定值为16
 */
#define MQ_MAX_MSG_NUM    16

/**
 * @ingroup mqueue
 * @brief 单条消息的最大长度
 * @note 定义消息队列中每条消息的最大字节数，固定值为64字节
 */
#define MQ_MAX_MSG_LEN    64


/* 常量定义 */

/**
 * @brief 消息队列魔术数
 * @note 用于消息队列验证的魔术值，十六进制0x89abcdef
 */
#define MQ_USE_MAGIC  0x89abcdef

/**
 * @brief 最大优先级值
 * @note 当前不支持消息优先级功能，固定值为1
 */
#define MQ_PRIO_MAX 1

/**
 * @brief 最大消息队列文件描述符数量
 * @note 如果未定义，则使用配置项CONFIG_NQUEUE_DESCRIPTORS的值
 */
#ifndef MAX_MQ_FD
#define MAX_MQ_FD CONFIG_NQUEUE_DESCRIPTORS
#endif

/** @} */ // end of MQUEUE_MACROS

/**
 * @defgroup MQUEUE_TYPES 数据类型定义
 * @{ 
 */

/**
 * @brief 权限模式联合体
 * @details 用于存储消息队列的访问权限位，支持按位操作
 */
typedef union send_receive_t {
    unsigned oth : 3;  /**< 其他用户权限位（3位） */
    unsigned grp : 6;  /**< 组用户权限位（6位） */
    unsigned usr : 9;  /**< 用户权限位（9位） */
    short data;        /**< 整体权限数据（16位） */
} mode_s;

/**
 * @brief 消息队列通知结构体
 * @details 用于设置消息到达时的通知方式
 */
struct mqnotify {
    pid_t pid;                 /**< 接收通知的进程ID */
    struct sigevent notify;    /**< 信号事件结构，定义通知方式 */
};

/* 类型定义 */

/**
 * @brief 消息队列数组结构
 * @details 描述系统级消息队列的核心属性和状态
 */
struct mqarray {
    UINT32 mq_id : 31;         /**< 消息队列ID（31位） */
    UINT32 unlinkflag : 1;     /**< 解除链接标志（1位）：0-未解除，1-已解除 */
    char *mq_name;             /**< 消息队列名称（以'/'开头的字符串） */
    UINT32 unlink_ref;         /**< 解除链接引用计数 */
    mode_s mode_data;          /**< 消息队列权限数据 */
    uid_t euid;                /**< 有效用户ID */
    gid_t egid;                /**< 有效组ID */
    struct mqnotify mq_notify; /**< 通知结构，用于异步消息通知 */
    LosQueueCB *mqcb;          /**< 指向LiteOS内核队列控制块的指针 */
    struct mqpersonal *mq_personal; /**< 指向个人消息队列结构的指针 */
};

/**
 * @brief 个人消息队列结构
 * @details 描述进程打开的消息队列实例属性
 */
struct mqpersonal {
    struct mqarray *mq_posixdes; /**< 指向系统级消息队列结构的指针 */
    struct mqpersonal *mq_next;  /**< 指向下一个个人消息队列结构的指针（链表） */
    int mq_flags;                /**< 打开标志（如O_NONBLOCK等） */
    int mq_mode;                 /**< 访问模式（如O_RDONLY, O_WRONLY, O_RDWR） */
    UINT32 mq_status;            /**< 状态标志 */
    UINT32 mq_refcount;          /**< 引用计数 */
};

/**
 * @ingroup mqueue
 * @brief 消息队列属性结构
 * @details 用于获取或设置消息队列的属性信息
 */
struct mq_attr {
    long mq_flags;    /**< 消息队列标志（如O_NONBLOCK） */
    long mq_maxmsg;   /**< 最大消息数量（受MQ_MAX_MSG_NUM限制） */
    long mq_msgsize;  /**< 最大消息长度（受MQ_MAX_MSG_LEN限制） */
    long mq_curmsgs;  /**< 当前消息队列中的消息数 */
};

/**
 * @ingroup mqueue
 * Handle type of a message queue
 */
typedef UINTPTR   mqd_t;

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to open an existed message queue that has a specified name or create a new message queue.
 * @attention
 * <ul>
 * <li>A message queue does not restrict the read and write permissions.</li>
 * <li>The length of mqueue name must less than 256.</li>
 * <li>This operation and closed mqueue scheduling must be used in coordination to release the resource.</li>
 * <li>The parameter "mode" is not supported.</li>
 * <li>The "mq_curmsgs" member of the mq_attr structure is not supported.</li>
 * </ul>
 *
 * @param mqName     [IN] Message queue name.
 * @param openFlag   [IN] Permission attributes of the message queue. The value range is
 *                        [O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_EXCL, O_NONBLOCK].
 * @param mode       [IN] Message queue mode (variadic argument). When oflag is O_CREAT, it requires
 *                        two additional arguments: mode, which shall be of type mode_t, and attr,
 *                        which shall be a pointer to an mq_attr structure.
 * @param attr       [IN] Message queue attribute (variadic argument).
 *
 * @retval  mqd_t  The message queue is successfully opened or created.
 * @retval  (mqd_t)-1 The message queue fails to be opened or created, with any of the following error codes in errno.
 *
 *
 * @par Errors
 * <ul>
 * <li><b>ENOENT</b>: O_CREAT flag is not set for oflag, and the message queue specified by name does not exist.</li>
 * <li><b>EEXIST</b>: Both O_CREAT and O_EXCL are set for oflag, but the message queue
 *                    specified by name already exists.</li>
 * <li><b>EINVAL</b>: invalid parameter.</li>
 * <li><b>ENFILE</b>: The number of opened message queues exceeds the maximum limit.</li>
 * <li><b>ENOSPC</b>: insufficient memory.</li>
 * <li><b>ENAMETOOLONG</b>: The message queue name specified by name is too long.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see mq_close
 */
extern mqd_t mq_open(const char *mqName, int openFlag, ...);

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to close a message queue that has a specified descriptor.
 * @attention
 * <ul>
 * <li> If the message queue is empty, it will be reclaimed, which is similar to when mq_unlink is called.</li>
 * </ul>
 *
 * @param personal   [IN] Message queue descriptor.
 *
 * @retval  0    The message queue is successfully closed.
 * @retval -1    The message queue fails to be closed, with either of the following error codes in errno.
 *
 * @par Errors
 * <ul>
 * <li><b>EBADF</b>: Invalid message queue descriptor.</li>
 * <li><b>EAGAIN</b>: Failed to delete the message queue.</li>
 * <li><b>EFAULT</b>: Failed to free the message queue.</li>
 * <li><b>EINVAL</b>: Invalid parameter.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see mq_open
 */
extern int mq_close(mqd_t personal);

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to remove a message queue that has a specified name.
 * @attention
 * <ul>
 * <li> If the message queue is empty, it will be reclaimed, which is similar to when mq_close is called.</li>
 * <li> The length of mqueue name must less than 256.</li>
 * </ul>
 *
 * @param mqName   [IN] Message queue name.
 *
 * @retval  0    The message queue is successfully removed.
 * @retval -1    The message queue fails to be removed, with any of the following error codes in errno.
 *
 * @par Errors
 * <ul>
 * <li><b>ENOENT</b>: The message queue specified by name does not exist.</li>
 * <li><b>EAGAIN</b>: Failed to delete the message queue.</li>
 * <li><b>EBUSY</b>: The message queue to be removed is being used.</li>
 * <li><b>EINVAL</b>: Invalid parameter.</li>
 * <li><b>ENAMETOOLONG</b>: The name of mqueue is too long.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see mq_close
 */
extern int mq_unlink(const char *mqName);

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to put a message with specified message content and length into
 * a message queue that has a specified descriptor.
 * @attention
 * <ul>
 * <li> Priority-based message processing is not supported.</li>
 * <li> The msg_len should be same to the length of string which msg_ptr point to.</li>
 * </ul>
 *
 * @param personal   [IN] Message queue descriptor.
 * @param msg        [IN] Pointer to the message content to be sent.
 * @param msgLen     [IN] Length of the message to be sent.
 * @param msgPrio    [IN] Priority of the message to be sent (the value of this parameter must
 *                        be 0 because priority-based message sending is not supported. If the
 *                        value is not 0, this API will cease to work.)
 *
 * @retval  0    The message is successfully sent.
 * @retval -1    The message fails to be sent, with any of the following error codes in errno.
 *
 * @par Errors
 * <ul>
 * <li><b>EINTR</b>: An interrupt is in progress while the message is being sent.</li>
 * <li><b>EBADF</b>: The message queue is invalid or not writable.</li>
 * <li><b>EAGAIN</b>: The message queue is full.</li>
 * <li><b>EINVAL</b>: Invalid parameter.</li>
 * <li><b>ENOSPC</b>: Insufficient memory.</li>
 * <li><b>EMSGSIZE</b>: The message to be sent is too long.</li>
 * <li><b>EOPNOTSUPP</b>: The operation is not supported.</li>
 * <li><b>ETIMEDOUT</b>: The operation times out.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see mq_receive
 */
extern int mq_send(mqd_t personal, const char *msg, size_t msgLen, unsigned int msgPrio);

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to remove the oldest message from the message queue that has a specified descriptor,
 * and puts it in the buffer pointed to by msg_ptr.
 * @attention
 * <ul>
 * <li> Priority-based message processing is not supported.</li>
 * <li> The msg_len should be same to the length of string which msg_ptr point to.</li>
 * </ul>
 *
 * @param personal   [IN] Message queue descriptor.
 * @param msg        [IN] Pointer to the message content to be received.
 * @param msgLen     [IN] Length of the message to be received.
 * @param msgPrio    [OUT] Priority of the message to be received
 *                         because priority-based message processing is not supported, this parameter is useless).
 *
 * @retval  0    The message is successfully received.
 * @retval -1    The message fails to be received, with any of the following error codes in the errno.
 *
 * @par Errors
 * <ul>
 * <li><b>EINTR</b>: An interrupt is in progress while the message is being received.</li>
 * <li><b>EBADF</b>: The message queue is invalid or not readable.</li>
 * <li><b>EAGAIN</b>: The message queue is empty.</li>
 * <li><b>EINVAL</b>: invalid parameter.</li>
 * <li><b>EMSGSIZE</b>: The message to be received is too long.</li>
 * <li><b>ETIMEDOUT</b>: The operation times out.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see mq_send
 */
extern ssize_t mq_receive(mqd_t personal, char *msg, size_t msgLen, unsigned int *msgPrio);

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to obtain or modify attributes of the message queue that has a specified descriptor.
 * @attention
 * <ul>
 * <li> The mq_maxmsg, mq_msgsize, and mq_curmsgs attributes are not modified
 *      in the message queue attribute setting.</li>
 * </ul>
 *
 * @param personal    [IN] Message queue descriptor.
 * @param mqSetAttr   [IN] New attribute of the message queue.
 * @param MqOldAttr   [OUT] Old attribute of the message queue.
 *
 * @retval   0    The message queue attributes are successfully set or get.
 * @retval   -1   The message queue attributes fail to be set or get,
 *                with either of the following error codes in the errno.
 *
 * @par Errors
 * <ul>
 * <li><b>EBADF</b>: Invalid message queue.</li>
 * <li><b>EINVAL</b>: Invalid parameter.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see sys_mq_getsetattr
 */
extern int mq_getsetattr(mqd_t personal, const struct mq_attr *mqSetAttr, struct mq_attr *MqOldAttr);

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to put a message with specified message content and length into
 * a message queue that has a descriptor at a scheduled time.
 * @attention
 * <ul>
 * <li> Priority-based message processing is not supported.</li>
 * <li> The expiry time must be later than the current time.</li>
 * <li> The wait time is a relative time.</li>
 * <li> The msg_len should be same to the length of string which msg_ptr point to.</li>
 * </ul>
 *
 * @param mqdes           [IN] Message queue descriptor.
 * @param msg             [IN] Pointer to the message content to be sent.
 * @param msgLen          [IN] Length of the message to be sent.
 * @param msgPrio         [IN] Priority of the message to be sent (the value of this parameter must be 0
 *                             because priority-based message processing is not supported).
 * @param absTimeout      [IN] Scheduled time at which the message will be sent. If the value is 0,
 *                             the message is an instant message.
 *
 * @retval   0   The message is successfully sent.
 * @retval  -1   The message fails to be sent, with any of the following error codes in errno.
 *
 * @par Errors
 * <ul>
 * <li><b>EINTR</b>: An interrupt is in progress while the message is being sent.</li>
 * <li><b>EBADF</b>: The message queue is invalid or not writable.</li>
 * <li><b>EAGAIN</b>: The message queue is full.</li>
 * <li><b>EINVAL</b>: Invalid parameter.</li>
 * <li><b>EMSGSIZE</b>: The message to be sent is too long.</li>
 * <li><b>EOPNOTSUPP</b>: The operation is not supported.</li>
 * <li><b>ETIMEDOUT</b>: The operation times out.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see mq_receive
 */
extern int mq_timedsend(mqd_t personal, const char *msg, size_t msgLen,
                        unsigned int msgPrio, const struct timespec *absTimeout);

/**
 * @ingroup mqueue
 *
 * @par Description:
 * This API is used to obtain a message with specified message content and length from
 * a message queue message that has a specified descriptor.
 * @attention
 * <ul>
 * <li> Priority-based message processing is not supported.</li>
 * <li> The expiry time must be later than the current time.</li>
 * <li> The wait time is a relative time.</li>
 * <li> The msg_len should be same to the length of string which msg_ptr point to.</li>
 * </ul>
 *
 * @param personal        [IN] Message queue descriptor.
 * @param msg             [IN] Pointer to the message content to be received.
 * @param msgLen          [IN] Length of the message to be received.
 * @param msgPrio         [OUT] Priority of the message to be received (because priority-based message
 *                              processing is not supported, this parameter is useless ).
 * @param absTimeout      [IN] Scheduled time at which the messagewill be received. If the value is 0,
 *                             the message is an instant message.
 *
 * @retval  0    The message is successfully received.
 * @retval -1    The message fails to be received, with any of the following error codes in errno.
 *
 * @par Errors
 * <ul>
 * <li><b>EINTR</b>: An interrupt is in progress while the message is being received.</li>
 * <li><b>EBADF</b>: The message queue is invalid or not readable.</li>
 * <li><b>EAGAIN</b>: The message queue is empty.</li>
 * <li><b>EINVAL</b>: invalid parameter.</li>
 * <li><b>EMSGSIZE</b>: The message to be received is too long.</li>
 * <li><b>ETIMEDOUT</b>: The operation times out.</li>
 * </ul>
 *
 * @par Dependency:
 * <ul><li>mqueue.h</li></ul>
 * @see mq_send
 */
extern ssize_t mq_timedreceive(mqd_t personal, char *msg, size_t msgLen,
                               unsigned int *msgPrio, const struct timespec *absTimeout);

extern void MqueueRefer(int sysFd);
extern int OsMqNotify(mqd_t personal, const struct sigevent *sigev);
extern VOID OsMqueueCBDestroy(struct mqarray *queueTable);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
