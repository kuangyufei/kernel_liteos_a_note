/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd. All rights reserved.
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
/* 100，表示一个epoll文件描述符可控制的文件描述符数量 */
#define EPOLL_DEFAULT_SIZE 100

/* 内部数据结构，用于管理每个epoll文件描述符 */
struct epoll_head {
    int size;               // epoll实例可管理的最大文件描述符数量
    int nodeCount;          // 当前已注册的文件描述符数量
    struct epoll_event *evs; // 存储已注册事件的数组
};

/* 静态互斥锁，用于保护epoll相关全局数据的线程安全访问 */
STATIC pthread_mutex_t g_epollMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

#ifndef MAX_EPOLL_FD
#define MAX_EPOLL_FD CONFIG_EPOLL_DESCRIPTORS  // 定义epoll文件描述符的最大数量，由配置项决定
#endif

/* 记录epoll内核文件描述符的集合 */
STATIC fd_set g_epollFdSet;

/* 记录epoll私有数据的数组，索引对应epoll文件描述符 */
STATIC struct epoll_head *g_epPrivBuf[MAX_EPOLL_FD];

/**
 * @brief 分配系统文件描述符，存储epoll私有数据并设置使用标记
 *
 * @param maxfdp 允许申请的最大系统文件描述符数量
 * @param head 指向epoll私有数据结构的指针
 * @return 新分配的文件描述符索引（偏移EPOLL_FD_OFFSET后）；失败时返回-1
 */
static int EpollAllocSysFd(int maxfdp, struct epoll_head *head)
{
    int i;

    fd_set *fdset = &g_epollFdSet;  // 获取epoll文件描述符集合指针

    for (i = 0; i < maxfdp; i++) {  // 遍历查找可用的文件描述符
        if (fdset && !(FD_ISSET(i, fdset))) {  // 检查当前文件描述符是否未被使用
            FD_SET(i, fdset);  // 标记该文件描述符为已使用
            if (!g_epPrivBuf[i]) {  // 检查私有数据缓冲区对应位置是否为空
                g_epPrivBuf[i] = head;  // 存储私有数据指针
                return i + EPOLL_FD_OFFSET;  // 返回偏移后的文件描述符
            }
        }
    }

    set_errno(EMFILE);  // 设置错误号为"打开文件过多"
    return -1;  // 分配失败返回-1
}

/**
 * @brief 释放系统文件描述符，删除epoll私有数据并清除使用标记
 *
 * @param fd 需要释放的epoll文件描述符
 * @return 成功返回0；失败返回-1
 */
static int EpollFreeSysFd(int fd)
{
    int efd = fd - EPOLL_FD_OFFSET;  // 计算实际索引（减去偏移量）

    if ((efd < 0) || (efd >= MAX_EPOLL_FD)) {  // 检查索引是否越界
        set_errno(EMFILE);  // 设置错误号为"打开文件过多"
        return -1;  // 失败返回-1
    }

    fd_set *fdset = &g_epollFdSet;  // 获取epoll文件描述符集合指针
    if (fdset && FD_ISSET(efd, fdset)) {  // 检查文件描述符是否已被设置
        FD_CLR(efd, fdset);  // 清除使用标记
        g_epPrivBuf[efd] = NULL;  // 清空私有数据指针
    }

    return 0;  // 成功返回0
}

/**
 * @brief 通过epoll文件描述符获取对应的私有数据
 *
 * @param fd epoll文件描述符
 * @return 指向epoll_head结构的指针；失败返回NULL
 */
static struct epoll_head *EpollGetDataBuff(int fd)
{
    int id = fd - EPOLL_FD_OFFSET;  // 计算实际索引（减去偏移量）

    if ((id < 0) || (id >= MAX_EPOLL_FD)) {  // 检查索引是否越界
        return NULL;  // 越界返回NULL
    }

    return g_epPrivBuf[id];  // 返回对应私有数据指针
}

/**
 * @brief 执行EPOLL_CTL_ADD操作时，检查文件描述符是否已存在
 *
 * @param epHead epoll控制头结构指针，通过epoll ID查找得到
 * @param fd 需要添加的文件描述符
 * @return 成功返回0（不存在）；失败返回-1（已存在）
 */
static int CheckFdExist(struct epoll_head *epHead, int fd)
{
    int i;
    for (i = 0; i < epHead->nodeCount; i++) {  // 遍历已注册的文件描述符
        if (epHead->evs[i].data.fd == fd) {  // 检查是否已存在相同文件描述符
            return -1;  // 已存在返回-1
        }
    }

    return 0;  // 不存在返回0
}

/**
 * @brief 关闭epoll实例并释放相关资源
 *
 * @param epHead 需要关闭的epoll控制头结构指针
 * @return 无
 */
static VOID DoEpollClose(struct epoll_head *epHead)
{
    if (epHead != NULL) {  // 检查epoll头指针是否有效
        if (epHead->evs != NULL) {  // 检查事件数组指针是否有效
            free(epHead->evs);  // 释放事件数组内存
        }

        free(epHead);  // 释放epoll头结构内存
    }

    return;
}

/**
 * @brief 创建epoll实例（不支持的API）
 *
 * 注：epoll_create通过调用epoll_create1实现，其参数'size'无用
 * epoll_create1的简单版本不使用红黑树，因此当fd为正常值（大于0）时，
 * 实际分配的epoll可管理EPOLL_DEFAULT_SIZE数量的文件描述符
 *
 * @param flags 未实际使用的标志位
 * @return 创建成功返回epoll文件描述符；失败返回-1
 */
int epoll_create1(int flags)
{
    (void)flags;  // 未使用的参数，避免编译警告
    int fd = -1;  // 初始化文件描述符为-1（表示失败）

    struct epoll_head *epHead = (struct epoll_head *)malloc(sizeof(struct epoll_head));  // 分配epoll头结构内存
    if (epHead == NULL) {  // 检查内存分配是否成功
        set_errno(ENOMEM);  // 设置错误号为"内存不足"
        return fd;  // 返回-1表示失败
    }

    /* 实际分配的epoll可管理数量为EPOLL_DEFAULT_SIZE */
    epHead->size = EPOLL_DEFAULT_SIZE;  // 设置最大可管理文件描述符数量
    epHead->nodeCount = 0;  // 初始化已注册文件描述符数量为0
    epHead->evs = malloc(sizeof(struct epoll_event) * EPOLL_DEFAULT_SIZE);  // 分配事件数组内存
    if (epHead->evs == NULL) {  // 检查事件数组内存分配是否成功
        free(epHead);  // 释放已分配的epoll头结构
        set_errno(ENOMEM);  // 设置错误号为"内存不足"
        return fd;  // 返回-1表示失败
    }

    /* 文件描述符设置，获取系统文件描述符，用于关闭操作 */
    (VOID)pthread_mutex_lock(&g_epollMutex);  // 加锁保护全局数据访问
    fd = EpollAllocSysFd(MAX_EPOLL_FD, epHead);  // 分配系统文件描述符
    if (fd == -1) {  // 检查文件描述符分配是否成功
        (VOID)pthread_mutex_unlock(&g_epollMutex);  // 解锁
        DoEpollClose(epHead);  // 关闭epoll实例并释放资源
        set_errno(EMFILE);  // 设置错误号为"打开文件过多"
        return fd;  // 返回-1表示失败
    }
    (VOID)pthread_mutex_unlock(&g_epollMutex);  // 解锁
    return fd;  // 返回创建成功的epoll文件描述符
}

/**
 * @brief 关闭epoll文件描述符
 *
 * 由close系统调用触发
 * @param epfd 需要关闭的epoll文件描述符
 * @return 成功返回0；失败返回-1
 */
int epoll_close(int epfd)
{
    struct epoll_head *epHead = NULL;  // 声明epoll头结构指针

    (VOID)pthread_mutex_lock(&g_epollMutex);  // 加锁保护全局数据访问
    epHead = EpollGetDataBuff(epfd);  // 通过文件描述符获取私有数据
    if (epHead == NULL) {  // 检查私有数据是否存在
        (VOID)pthread_mutex_unlock(&g_epollMutex);  // 解锁
        set_errno(EBADF);  // 设置错误号为"错误的文件描述符"
        return -1;  // 返回-1表示失败
    }

    DoEpollClose(epHead);  // 关闭epoll实例并释放资源
    int ret = EpollFreeSysFd(epfd);  // 释放系统文件描述符
    (VOID)pthread_mutex_unlock(&g_epollMutex);  // 解锁
    return ret;  // 返回操作结果
}

/**
 * @brief 控制epoll实例，执行添加、删除或修改文件描述符的事件监听
 *
 * @param epfd epoll实例的文件描述符
 * @param op 操作类型：EPOLL_CTL_ADD(添加)、EPOLL_CTL_DEL(删除)、EPOLL_CTL_MOD(修改)
 * @param fd 需要操作的文件描述符
 * @param ev 指向epoll_event结构的指针，包含事件类型和用户数据
 * @return 成功返回0；失败返回-1
 */
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev)
{
    struct epoll_head *epHead = NULL;  // 声明epoll头结构指针
    int i;  // 循环索引变量
    int ret = -1;  // 初始化返回值为-1（表示失败）

    (VOID)pthread_mutex_lock(&g_epollMutex);  // 加锁保护全局数据访问
    epHead = EpollGetDataBuff(epfd);  // 通过文件描述符获取私有数据
    if (epHead == NULL) {  // 检查私有数据是否存在
        set_errno(EBADF);  // 设置错误号为"错误的文件描述符"
        goto OUT_RELEASE;  // 跳转到解锁并返回
    }

    if (ev == NULL) {  // 检查事件结构指针是否为空
        set_errno(EINVAL);  // 设置错误号为"无效的参数"
        goto OUT_RELEASE;  // 跳转到解锁并返回
    }

    switch (op) {  // 根据操作类型执行相应处理
        case EPOLL_CTL_ADD:  // 添加文件描述符事件监听
            ret = CheckFdExist(epHead, fd);  // 检查文件描述符是否已存在
            if (ret == -1) {  // 已存在
                set_errno(EEXIST);  // 设置错误号为"文件已存在"
                goto OUT_RELEASE;  // 跳转到解锁并返回
            }

            if (epHead->nodeCount == EPOLL_DEFAULT_SIZE) {  // 检查是否达到最大容量
                set_errno(ENOMEM);  // 设置错误号为"内存不足"
                goto OUT_RELEASE;  // 跳转到解锁并返回
            }

            // 设置事件（添加POLLERR和POLLHUP事件）
            epHead->evs[epHead->nodeCount].events = ev->events | POLLERR | POLLHUP;
            epHead->evs[epHead->nodeCount].data.fd = fd;  // 存储文件描述符
            epHead->nodeCount++;  // 增加已注册文件描述符计数
            ret = 0;  // 设置返回值为成功
            break;
        case EPOLL_CTL_DEL:  // 删除文件描述符事件监听
            for (i = 0; i < epHead->nodeCount; i++) {  // 遍历已注册的文件描述符
                if (epHead->evs[i].data.fd != fd) {  // 找到匹配的文件描述符
                    continue;
                }

                if (i != epHead->nodeCount - 1) {  // 如果不是最后一个元素
                    // 移动数组元素覆盖当前位置
                    memmove_s(&epHead->evs[i], epHead->nodeCount - i, &epHead->evs[i + 1],
                              epHead->nodeCount - i);
                }
                epHead->nodeCount--;  // 减少已注册文件描述符计数
                ret = 0;  // 设置返回值为成功
                goto OUT_RELEASE;  // 跳转到解锁并返回
            }
            set_errno(ENOENT);  // 设置错误号为"文件不存在"
            break;
        case EPOLL_CTL_MOD:  // 修改文件描述符事件监听
            for (i = 0; i < epHead->nodeCount; i++) {  // 遍历已注册的文件描述符
                if (epHead->evs[i].data.fd == fd) {  // 找到匹配的文件描述符
                    // 更新事件（添加POLLERR和POLLHUP事件）
                    epHead->evs[i].events = ev->events | POLLERR | POLLHUP;
                    ret = 0;  // 设置返回值为成功
                    goto OUT_RELEASE;  // 跳转到解锁并返回
                }
            }
            set_errno(ENOENT);  // 设置错误号为"文件不存在"
            break;
        default:  // 无效的操作类型
            set_errno(EINVAL);  // 设置错误号为"无效的参数"
            break;
    }

OUT_RELEASE:
    (VOID)pthread_mutex_unlock(&g_epollMutex);  // 解锁
    return ret;  // 返回操作结果
}

/**
 * @brief 等待epoll实例中的文件描述符上的事件
 *
 * @param epfd epoll实例的文件描述符
 * @param evs 指向epoll_event数组的指针，用于存储就绪事件
 * @param maxevents 最多可返回的事件数量
 * @param timeout 超时时间（毫秒），-1表示无限等待
 * @return 成功返回就绪事件数量；超时返回0；失败返回-1
 */
int epoll_wait(int epfd, FAR struct epoll_event *evs, int maxevents, int timeout)
{
    struct epoll_head *epHead = NULL;  // 声明epoll头结构指针
    int ret = -1;  // 初始化返回值为-1（表示失败）
    int counter;  // 循环计数变量
    int i;  // 循环索引变量
    struct pollfd *pFd = NULL;  // 声明pollfd结构指针，用于调用poll
    int pollSize;  // poll操作的文件描述符数量

    (VOID)pthread_mutex_lock(&g_epollMutex);  // 加锁保护全局数据访问
    epHead = EpollGetDataBuff(epfd);  // 通过文件描述符获取私有数据
    if (epHead == NULL) {  // 检查私有数据是否存在
        set_errno(EBADF);  // 设置错误号为"错误的文件描述符"
        goto OUT_RELEASE;  // 跳转到解锁并返回
    }

    if ((maxevents <= 0) || (evs == NULL)) {  // 检查参数有效性
        set_errno(EINVAL);  // 设置错误号为"无效的参数"
        goto OUT_RELEASE;  // 跳转到解锁并返回
    }

    // 确定poll操作的文件描述符数量（不超过已注册数量和maxevents）
    if (maxevents > epHead->nodeCount) {
        pollSize = epHead->nodeCount;
    } else {
        pollSize = maxevents;
    }

    pFd = malloc(sizeof(struct pollfd) * pollSize);  // 分配pollfd数组内存
    if (pFd == NULL) {  // 检查内存分配是否成功
        set_errno(EINVAL);  // 设置错误号为"无效的参数"
        goto OUT_RELEASE;  // 跳转到解锁并返回
    }

    // 初始化pollfd数组
    for (i = 0; i < pollSize; i++) {
        pFd[i].fd = epHead->evs[i].data.fd;  // 设置文件描述符
        pFd[i].events = (short)epHead->evs[i].events;  // 设置关注的事件
    }


    ret = poll(pFd, pollSize, timeout);  // 调用poll等待事件
    if (ret <= 0) {  // poll返回0（超时）或-1（错误）
        free(pFd);  // 释放pollfd数组内存
        ret = 0;  // 设置返回值为0
        goto OUT_RELEASE;  // 跳转到解锁并返回
    }

    // 收集就绪事件
    for (i = 0, counter = 0; i < ret && counter < pollSize; counter++) {
        if (pFd[counter].revents != 0) {  // 检查是否有就绪事件
            evs[i].data.fd = pFd[counter].fd;  // 存储就绪文件描述符
            evs[i].events  = pFd[counter].revents;  // 存储就绪事件类型
            i++;  // 增加就绪事件计数
        }
    }

    free(pFd);  // 释放pollfd数组内存
    ret = i;  // 设置返回值为就绪事件数量

OUT_RELEASE:
    (VOID)pthread_mutex_unlock(&g_epollMutex);  // 解锁
    return ret;  // 返回就绪事件数量
}