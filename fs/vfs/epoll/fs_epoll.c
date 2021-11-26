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

#include "epoll.h"
#include <stdint.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include "pthread.h"

/* 100, the number of fd one epollfd can control */
#define EPOLL_DEFAULT_SIZE 100

/* Internal data, used to manage each epoll fd */
struct epoll_head {
    int size;
    int nodeCount;
    struct epoll_event *evs;
};

STATIC pthread_mutex_t g_epollMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

#ifndef MAX_EPOLL_FD
#define MAX_EPOLL_FD CONFIG_EPOLL_DESCRIPTORS
#endif

/* Record the kernel fd of epoll  */
STATIC fd_set g_epollFdSet;

/* Record the private data of epoll  */
STATIC struct epoll_head *g_epPrivBuf[MAX_EPOLL_FD];

/**
 * Alloc sysFd, storage epoll private data, set using bit.
 *
 * @param maxfdp: Maximum allowed application of sysFd.
 * @param head: Private data.
 * @return the index of the new fd; -1 on error
 */
static int EpollAllocSysFd(int maxfdp, struct epoll_head *head)
{
    int i;

    fd_set *fdset = &g_epollFdSet;

    for (i = 0; i < maxfdp; i++) {
        if (fdset && !(FD_ISSET(i, fdset))) {
            FD_SET(i, fdset);
            if (!g_epPrivBuf[i]) {
                g_epPrivBuf[i] = head;
                return i + EPOLL_FD_OFFSET;
            }
        }
    }

    set_errno(EMFILE);
    return -1;
}

/**
 * free sysFd, delete epoll private data, clear using bit.
 *
 * @param fd: epoll fd.
 * @return 0 or -1
 */
static int EpollFreeSysFd(int fd)
{
    int efd = fd - EPOLL_FD_OFFSET;

    if ((efd < 0) || (efd >= MAX_EPOLL_FD)) {
        set_errno(EMFILE);
        return -1;
    }

    fd_set *fdset = &g_epollFdSet;
    if (fdset && FD_ISSET(efd, fdset)) {
        FD_CLR(efd, fdset);
        g_epPrivBuf[efd] = NULL;
    }

    return 0;
}

/**
 * get private data by epoll fd
 *
 * @param fd: epoll fd.
 * @return point to epoll_head
 */
static struct epoll_head *EpollGetDataBuff(int fd)
{
    int id = fd - EPOLL_FD_OFFSET;

    if ((id < 0) || (id >= MAX_EPOLL_FD)) {
        return NULL;
    }

    return g_epPrivBuf[id];
}

/**
 * when do EPOLL_CTL_ADD, need check if fd exist
 *
 * @param epHead: epoll control head, find by epoll id .
 * @param fd: ctl add fd.
 * @return 0 or -1
 */
static int CheckFdExist(struct epoll_head *epHead, int fd)
{
    int i;
    for (i = 0; i < epHead->nodeCount; i++) {
        if (epHead->evs[i].data.fd == fd) {
            return -1;
        }
    }

    return 0;
}

/**
 * close epoll
 *
 * @param epHead: epoll control head.
 * @return void
 */
static VOID DoEpollClose(struct epoll_head *epHead)
{
    if (epHead != NULL) {
        if (epHead->evs != NULL) {
            free(epHead->evs);
        }

        free(epHead);
    }

    return;
}

/**
 * epoll_create,
 * The simple version of epoll does not use red-black trees,
 * so when fd is normal value (greater than 0),
 * actually allocated epoll can manage num of EPOLL_DEFAULT_SIZE
 *
 * @param size: not actually used
 * @return epoll fd
 */
int epoll_create(int size)
{
    int fd = -1;

    if (size <= 0) {
        set_errno(EINVAL);
        return fd;
    }

    struct epoll_head *epHead = (struct epoll_head *)malloc(sizeof(struct epoll_head));
    if (epHead == NULL) {
        set_errno(ENOMEM);
        return fd;
    }

    /* actually allocated epoll can manage num is EPOLL_DEFAULT_SIZE */
    epHead->size = EPOLL_DEFAULT_SIZE;
    epHead->nodeCount = 0;
    epHead->evs = malloc(sizeof(struct epoll_event) * EPOLL_DEFAULT_SIZE);
    if (epHead->evs == NULL) {
        free(epHead);
        set_errno(ENOMEM);
        return fd;
    }

    /* fd set, get sysfd, for close */
    (VOID)pthread_mutex_lock(&g_epollMutex);
    fd = EpollAllocSysFd(MAX_EPOLL_FD, epHead);
    if (fd == -1) {
        (VOID)pthread_mutex_unlock(&g_epollMutex);
        DoEpollClose(epHead);
        set_errno(EMFILE);
        return fd;
    }
    (VOID)pthread_mutex_unlock(&g_epollMutex);
    return fd;
}

/**
 * epoll_close,
 * called by close
 * @param epfd: epoll fd
 * @return 0 or -1
 */
int epoll_close(int epfd)
{
    struct epoll_head *epHead = NULL;

    epHead = EpollGetDataBuff(epfd);
    if (epHead == NULL) {
        set_errno(EBADF);
        return -1;
    }

    DoEpollClose(epHead);
    return EpollFreeSysFd(epfd);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev)
{
    struct epoll_head *epHead = NULL;
    int i;
    int ret = -1;

    epHead = EpollGetDataBuff(epfd);
    if (epHead == NULL) {
        set_errno(EBADF);
        return ret;
    }

    switch (op) {
        case EPOLL_CTL_ADD:
            ret = CheckFdExist(epHead, fd);
            if (ret == -1) {
                set_errno(EEXIST);
                return -1;
            }

            if (epHead->nodeCount == EPOLL_DEFAULT_SIZE) {
                set_errno(ENOMEM);
                return -1;
            }

            epHead->evs[epHead->nodeCount].events = ev->events | POLLERR | POLLHUP;
            epHead->evs[epHead->nodeCount].data.fd = fd;
            epHead->nodeCount++;
            return 0;
        case EPOLL_CTL_DEL:
            for (i = 0; i < epHead->nodeCount; i++) {
                if (epHead->evs[i].data.fd != fd) {
                    continue;
                }

                if (i != epHead->nodeCount - 1) {
                    memmove_s(&epHead->evs[i], epHead->nodeCount - i, &epHead->evs[i + 1],
                              epHead->nodeCount - i);
                }
                epHead->nodeCount--;
                return 0;
            }
            set_errno(ENOENT);
            return -1;
        case EPOLL_CTL_MOD:
            for (i = 0; i < epHead->nodeCount; i++) {
                if (epHead->evs[i].data.fd == fd) {
                    epHead->evs[i].events = ev->events | POLLERR | POLLHUP;
                    return 0;
                }
            }
            set_errno(ENOENT);
            return -1;
        default:
            set_errno(EINVAL);
            return -1;
    }
}

int epoll_wait(int epfd, FAR struct epoll_event *evs, int maxevents, int timeout)
{
    struct epoll_head *epHead = NULL;
    int ret;
    int counter;
    int i;
    struct pollfd *pFd = NULL;
    int pollSize;

    epHead = EpollGetDataBuff(epfd);
    if (epHead== NULL) {
        set_errno(EBADF);
        return -1;
    }

    if (maxevents <= 0) {
        set_errno(EINVAL);
        return -1;
    }

    if (maxevents > epHead->nodeCount) {
        pollSize = epHead->nodeCount;
    } else {
        pollSize = maxevents;
    }

    pFd = malloc(sizeof(struct pollfd) * pollSize);
    if (pFd == NULL) {
        set_errno(EINVAL);
        return -1;
    }

    for (i = 0; i < epHead->nodeCount; i++) {
        pFd[i].fd = epHead->evs[i].data.fd;
        pFd[i].events = (short)epHead->evs[i].events;
    }


    ret = poll(pFd, pollSize, timeout);
    if (ret <= 0) {
        free(pFd);
        return 0;
    }

    for (i = 0, counter = 0; i < ret && counter < pollSize; counter++) {
        if (pFd[counter].revents != 0) {
            evs[i].data.fd = pFd[counter].fd;
            evs[i].events  = pFd[counter].revents;
            i++;
        }
    }

    free(pFd);
    return i;
}

