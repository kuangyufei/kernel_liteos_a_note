/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _FS_EPOLL_H_
#define _FS_EPOLL_H_

#include "los_typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FAR
#define FAR
#endif

/**
 * @brief epoll文件描述符标志，执行exec时关闭
 * @note 等同于O_CLOEXEC标志
 */
#define EPOLL_CLOEXEC O_CLOEXEC  // epoll文件描述符执行exec时关闭标志
/**
 * @brief epoll非阻塞模式标志
 * @note 等同于O_NONBLOCK标志
 */
#define EPOLL_NONBLOCK O_NONBLOCK  // epoll非阻塞模式标志

/** @name epoll事件类型定义 */
/** @{ */
#define EPOLLIN         0x001  // 可读事件（数据可读取）
#define EPOLLPRI        0x002  // 高优先级可读事件（紧急数据可读取）
#define EPOLLOUT        0x004  // 可写事件（数据可写入）
#define EPOLLRDNORM     0x040  // 普通数据可读
#define EPOLLNVAL       0x020  // 文件描述符无效
#define EPOLLRDBAND     0x080  // 带外数据可读
#define EPOLLWRNORM     0x100  // 普通数据可写
#define EPOLLWRBAND     0x200  // 带外数据可写
#define EPOLLMSG        0x400  // 消息可用（未实现）
#define EPOLLERR        0x008  // 错误事件
#define EPOLLHUP        0x010  // 挂起事件（连接关闭）
/** @} */

/** @name epoll控制操作类型 */
/** @{ */
#define EPOLL_CTL_ADD 1  // 添加文件描述符到epoll实例
#define EPOLL_CTL_DEL 2  // 从epoll实例中删除文件描述符
#define EPOLL_CTL_MOD 3  // 修改epoll实例中的文件描述符事件
/** @} */

/**
 * @brief epoll事件关联数据的联合体
 * @note 用于存储与epoll事件相关的用户数据
 */
typedef union epoll_data {
    void *ptr;    // 指向用户定义数据的指针
    int fd;       // 文件描述符
    UINT32 u32;   // 32位无符号整数
    UINT64 u64;   // 64位无符号整数
} epoll_data_t;  // epoll数据联合体类型定义

/**
 * @brief epoll事件结构体
 * @note 用于描述epoll监控的事件及其关联数据
 */
struct epoll_event {
    UINT32 events;      // 事件类型（EPOLLIN、EPOLLOUT等组合）
    epoll_data_t data;  // 与事件关联的数据
};  // epoll事件结构体定义

int epoll_create1(int flags);
int epoll_close(int epfd);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev);
int epoll_wait(int epfd, FAR struct epoll_event *evs, int maxevents, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* sys/epoll.h */
