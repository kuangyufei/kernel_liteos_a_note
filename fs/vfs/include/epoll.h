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

#define EPOLL_CLOEXEC O_CLOEXEC
#define EPOLL_NONBLOCK O_NONBLOCK

#define EPOLLIN         0x001
#define EPOLLPRI        0x002
#define EPOLLOUT        0x004
#define EPOLLRDNORM     0x040
#define EPOLLNVAL       0x020
#define EPOLLRDBAND     0x080
#define EPOLLWRNORM     0x100
#define EPOLLWRBAND     0x200
#define EPOLLMSG        0x400
#define EPOLLERR        0x008
#define EPOLLHUP        0x010

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

typedef union epoll_data {
    void *ptr;
    int fd;
    UINT32 u32;
    UINT64 u64;
} epoll_data_t;

struct epoll_event {
    UINT32 events;
    epoll_data_t data;
};

int epoll_create1(int flags);
int epoll_close(int epfd);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev);
int epoll_wait(int epfd, FAR struct epoll_event *evs, int maxevents, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* sys/epoll.h */
