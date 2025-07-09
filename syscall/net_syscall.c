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
#include "stdlib.h"
#include "fs/file.h"
#include "los_process_pri.h"
#include "los_signal.h"
#include "los_syscall.h"
#include "los_vm_map.h"
#include "user_copy.h"

#ifdef LOSCFG_NET_LWIP_SACK
#include "lwip/sockets.h"
/**
 * @brief 用户空间到内核空间套接字描述符转换宏
 * @param s 用户空间套接字描述符
 * @note 若转换失败会设置EBADF错误码并返回
 */
#define SOCKET_U2K(s) \
    do { \
        s = GetAssociatedSystemFd(s);   \
        if (s == VFS_ERROR) {           \
            set_errno(EBADF);           \
            return -get_errno();        \
        }                               \
    } while (0)

/**
 * @brief 内核空间到用户空间套接字描述符转换宏
 * @param s 内核空间套接字描述符
 * @note 分配用户空间文件描述符并关联，失败时关闭内核套接字并返回错误
 */
#define SOCKET_K2U(s) \
    do { \
        int fd = AllocAndAssocProcessFd(s, MIN_START_FD); \
        if (fd == -1) { \
            closesocket(s); \
            set_errno(EMFILE); \
            s = -EMFILE; \
        } else { \
            s = fd; \
        } \
    } while (0)

/**
 * @brief 创建套接字系统调用
 * @param domain 协议域（如AF_INET表示IPv4）
 * @param type 套接字类型（如SOCK_STREAM表示TCP）
 * @param protocol 协议（通常为0，自动选择）
 * @return 成功返回用户空间套接字描述符，失败返回负数错误码
 */
int SysSocket(int domain, int type, int protocol)
{
    int ret;                            // 函数返回值

    ret = socket(domain, type, protocol); // 调用内核套接字创建函数
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    SOCKET_K2U(ret);                    // 转换为用户空间描述符
    return ret;
}

/**
 * @brief 绑定套接字到地址系统调用
 * @param s 用户空间套接字描述符
 * @param name 指向sockaddr结构体的指针
 * @param namelen 地址长度
 * @return 成功返回0，失败返回负数错误码
 */
int SysBind(int s, const struct sockaddr *name, socklen_t namelen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符
    CHECK_ASPACE(name, namelen);        // 检查用户空间地址有效性

    DUP_FROM_USER(name, namelen);       // 从用户空间复制地址结构

    if (name == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = bind(s, name, namelen);   // 调用内核绑定函数
    }
    FREE_DUP(name);                     // 释放复制的内存
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 建立连接系统调用（客户端）
 * @param s 用户空间套接字描述符
 * @param name 目标服务器地址
 * @param namelen 地址长度
 * @return 成功返回0，失败返回负数错误码
 */
int SysConnect(int s, const struct sockaddr *name, socklen_t namelen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符
    CHECK_ASPACE(name, namelen);        // 检查用户空间地址有效性

    DUP_FROM_USER(name, namelen);       // 从用户空间复制地址结构

    if (name == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = connect(s, name, namelen); // 调用内核连接函数
    }
    FREE_DUP(name);                     // 释放复制的内存
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 监听连接请求系统调用（服务器）
 * @param sockfd 用户空间套接字描述符
 * @param backlog 连接请求队列最大长度
 * @return 成功返回0，失败返回负数错误码
 */
int SysListen(int sockfd, int backlog)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(sockfd);                 // 转换为内核空间描述符
    ret = listen(sockfd, backlog);      // 调用内核监听函数
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 接受连接请求系统调用（服务器）
 * @param socket 用户空间监听套接字描述符
 * @param address 用于存储客户端地址的缓冲区
 * @param addressLen 地址长度（输入/输出参数）
 * @return 成功返回新连接套接字描述符，失败返回负数错误码
 */
int SysAccept(int socket, struct sockaddr *address,
              socklen_t *addressLen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(socket);                 // 转换为内核空间描述符

    CHECK_ASPACE(addressLen, sizeof(socklen_t)); // 检查长度参数地址有效性
    CPY_FROM_USER(addressLen);          // 复制长度值

    CHECK_ASPACE(address, LEN(addressLen)); // 检查地址缓冲区有效性
    DUP_FROM_USER_NOCOPY(address, LEN(addressLen)); // 复制地址结构

    ret = accept(socket, address, addressLen); // 调用内核接受连接函数
    if (ret == -1) {
        FREE_DUP(address);              // 释放内存
        return -get_errno();            // 返回错误码
    }

    // 将结果复制回用户空间
    CPY_TO_USER(addressLen, close(ret); FREE_DUP(address));
    DUP_TO_USER(address, LEN(addressLen), close(ret); FREE_DUP(address));
    FREE_DUP(address);                  // 释放内存

    SOCKET_K2U(ret);                    // 转换新套接字为用户空间描述符
    return ret;
}

/**
 * @brief 获取套接字本地地址系统调用
 * @param s 用户空间套接字描述符
 * @param name 存储本地地址的缓冲区
 * @param namelen 地址长度（输入/输出参数）
 * @return 成功返回0，失败返回负数错误码
 */
int SysGetSockName(int s, struct sockaddr *name, socklen_t *namelen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符

    CHECK_ASPACE(namelen, sizeof(socklen_t)); // 检查长度参数有效性
    CPY_FROM_USER(namelen);             // 复制长度值

    CHECK_ASPACE(name, LEN(namelen));   // 检查地址缓冲区有效性
    DUP_FROM_USER_NOCOPY(name, LEN(namelen)); // 复制地址结构

    if (name == NULL || namelen == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = getsockname(s, name, namelen); // 调用内核获取本地地址函数
    }
    if (ret == -1) {
        FREE_DUP(name);                 // 释放内存
        return -get_errno();            // 返回错误码
    }

    // 将结果复制回用户空间
    CPY_TO_USER(namelen, FREE_DUP(name));
    DUP_TO_USER(name, LEN(namelen), FREE_DUP(name));
    FREE_DUP(name);                     // 释放内存
    return ret;
}

/**
 * @brief 获取对等方地址系统调用
 * @param s 用户空间套接字描述符
 * @param name 存储对等方地址的缓冲区
 * @param namelen 地址长度（输入/输出参数）
 * @return 成功返回0，失败返回负数错误码
 */
int SysGetPeerName(int s, struct sockaddr *name, socklen_t *namelen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符

    CHECK_ASPACE(namelen, sizeof(socklen_t)); // 检查长度参数有效性
    CPY_FROM_USER(namelen);             // 复制长度值

    CHECK_ASPACE(name, LEN(namelen));   // 检查地址缓冲区有效性
    DUP_FROM_USER_NOCOPY(name, LEN(namelen)); // 复制地址结构

    if (name == NULL || namelen == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = getpeername(s, name, namelen); // 调用内核获取对等方地址函数
    }
    if (ret == -1) {
        FREE_DUP(name);                 // 释放内存
        return -get_errno();            // 返回错误码
    }

    // 将结果复制回用户空间
    CPY_TO_USER(namelen, FREE_DUP(name));
    DUP_TO_USER(name, LEN(namelen), FREE_DUP(name));
    FREE_DUP(name);                     // 释放内存
    return ret;
}

/**
 * @brief 发送数据系统调用
 * @param s 用户空间套接字描述符
 * @param dataptr 待发送数据缓冲区
 * @param size 数据长度
 * @param flags 发送标志（如MSG_OOB表示带外数据）
 * @return 成功返回发送字节数，失败返回负数错误码
 */
ssize_t SysSend(int s, const void *dataptr, size_t size, int flags)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符
    CHECK_ASPACE(dataptr, size);        // 检查数据缓冲区有效性

    DUP_FROM_USER(dataptr, size);       // 从用户空间复制数据

    if (dataptr == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = send(s, dataptr, size, flags); // 调用内核发送函数
    }
    FREE_DUP(dataptr);                  // 释放复制的内存
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 发送数据到指定地址系统调用（UDP）
 * @param s 用户空间套接字描述符
 * @param dataptr 待发送数据缓冲区
 * @param size 数据长度
 * @param flags 发送标志
 * @param to 目标地址
 * @param tolen 地址长度
 * @return 成功返回发送字节数，失败返回负数错误码
 */
ssize_t SysSendTo(int s, const void *dataptr, size_t size, int flags,
                  const struct sockaddr *to, socklen_t tolen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符
    CHECK_ASPACE(dataptr, size);        // 检查数据缓冲区有效性
    CHECK_ASPACE(to, tolen);            // 检查目标地址有效性

    DUP_FROM_USER(dataptr, size);       // 复制数据
    DUP_FROM_USER(to, tolen, FREE_DUP(dataptr)); // 复制目标地址

    if (dataptr == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        // 调用内核发送函数
        ret = sendto(s, dataptr, size, flags, to, tolen);
    }
    FREE_DUP(dataptr);                  // 释放内存
    FREE_DUP(to);                       // 释放内存
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 接收数据系统调用
 * @param socket 用户空间套接字描述符
 * @param buffer 接收数据缓冲区
 * @param length 缓冲区长度
 * @param flags 接收标志（如MSG_PEEK表示预览数据）
 * @return 成功返回接收字节数，失败返回负数错误码
 */
ssize_t SysRecv(int socket, void *buffer, size_t length, int flags)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(socket);                 // 转换为内核空间描述符
    CHECK_ASPACE(buffer, length);       // 检查缓冲区有效性

    DUP_FROM_USER_NOCOPY(buffer, length); // 复制缓冲区结构

    if (buffer == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = recv(socket, buffer, length, flags); // 调用内核接收函数
    }
    if (ret == -1) {
        FREE_DUP(buffer);               // 释放内存
        return -get_errno();            // 返回错误码
    }

    DUP_TO_USER(buffer, ret, FREE_DUP(buffer)); // 将接收数据复制回用户空间
    FREE_DUP(buffer);                   // 释放内存
    return ret;
}

/**
 * @brief 从指定地址接收数据系统调用（UDP）
 * @param socket 用户空间套接字描述符
 * @param buffer 接收数据缓冲区
 * @param length 缓冲区长度
 * @param flags 接收标志
 * @param address 存储发送方地址的缓冲区
 * @param addressLen 地址长度（输入/输出参数）
 * @return 成功返回接收字节数，失败返回负数错误码
 */
ssize_t SysRecvFrom(int socket, void *buffer, size_t length,
                    int flags, struct sockaddr *address,
                    socklen_t *addressLen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(socket);                 // 转换为内核空间描述符
    CHECK_ASPACE(buffer, length);       // 检查数据缓冲区有效性

    CHECK_ASPACE(addressLen, sizeof(socklen_t)); // 检查长度参数有效性
    CPY_FROM_USER(addressLen);          // 复制长度值

    CHECK_ASPACE(address, LEN(addressLen)); // 检查地址缓冲区有效性
    DUP_FROM_USER_NOCOPY(address, LEN(addressLen)); // 复制地址结构

    DUP_FROM_USER_NOCOPY(buffer, length, FREE_DUP(address)); // 复制数据缓冲区

    if (buffer == NULL || (address != NULL && addressLen == NULL)) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        // 调用内核接收函数
        ret = recvfrom(socket, buffer, length, flags, address, addressLen);
    }
    if (ret == -1) {
        FREE_DUP(address);              // 释放内存
        FREE_DUP(buffer);               // 释放内存
        return -get_errno();            // 返回错误码
    }

    // 将结果复制回用户空间
    CPY_TO_USER(addressLen, FREE_DUP(address); FREE_DUP(buffer));
    DUP_TO_USER(address, LEN(addressLen), FREE_DUP(address); FREE_DUP(buffer));
    DUP_TO_USER(buffer, ret, FREE_DUP(address); FREE_DUP(buffer));
    FREE_DUP(address);                  // 释放内存
    FREE_DUP(buffer);                   // 释放内存
    return ret;
}

/**
 * @brief 关闭套接字连接系统调用
 * @param socket 用户空间套接字描述符
 * @param how 关闭方式（SHUT_RD/SHUT_WR/SHUT_RDWR）
 * @return 成功返回0，失败返回负数错误码
 */
int SysShutdown(int socket, int how)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(socket);                 // 转换为内核空间描述符
    ret = shutdown(socket, how);        // 调用内核关闭函数
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 设置套接字选项系统调用
 * @param socket 用户空间套接字描述符
 * @param level 选项级别（如SOL_SOCKET表示套接字级别）
 * @param optName 选项名称
 * @param optValue 选项值
 * @param optLen 选项长度
 * @return 成功返回0，失败返回负数错误码
 */
int SysSetSockOpt(int socket, int level, int optName,
                  const void *optValue, socklen_t optLen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(socket);                 // 转换为内核空间描述符
    CHECK_ASPACE(optValue, optLen);     // 检查选项值缓冲区有效性

    DUP_FROM_USER(optValue, optLen);    // 复制选项值
    ret = setsockopt(socket, level, optName, optValue, optLen); // 调用内核设置函数
    FREE_DUP(optValue);                 // 释放内存
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 获取套接字选项系统调用
 * @param sockfd 用户空间套接字描述符
 * @param level 选项级别
 * @param optName 选项名称
 * @param optValue 存储选项值的缓冲区
 * @param optLen 选项长度（输入/输出参数）
 * @return 成功返回0，失败返回负数错误码
 */
int SysGetSockOpt(int sockfd, int level, int optName,
                  void *optValue, socklen_t *optLen)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(sockfd);                 // 转换为内核空间描述符

    CHECK_ASPACE(optLen, sizeof(socklen_t)); // 检查长度参数有效性
    CPY_FROM_USER(optLen);              // 复制长度值

    CHECK_ASPACE(optValue, LEN(optLen)); // 检查选项值缓冲区有效性
    DUP_FROM_USER_NOCOPY(optValue, LEN(optLen)); // 复制缓冲区结构

    if (optLen == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = getsockopt(sockfd, level, optName, optValue, optLen); // 调用内核获取函数
    }
    if (ret == -1) {
        FREE_DUP(optValue);             // 释放内存
        return -get_errno();            // 返回错误码
    }

    // 将结果复制回用户空间
    CPY_TO_USER(optLen, FREE_DUP(optValue));
    DUP_TO_USER(optValue, LEN(optLen), FREE_DUP(optValue));
    FREE_DUP(optValue);                 // 释放内存
    return ret;
}

/**
 * @brief 发送消息系统调用（支持分散I/O和控制消息）
 * @param s 用户空间套接字描述符
 * @param message 指向msghdr结构体的指针
 * @param flags 发送标志
 * @return 成功返回发送字节数，失败返回负数错误码
 */
ssize_t SysSendMsg(int s, const struct msghdr *message, int flags)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符

    CHECK_ASPACE(message, sizeof(struct msghdr)); // 检查消息头有效性
    CPY_FROM_CONST_USER(struct msghdr, message); // 复制消息头

    // 检查iov数量是否超过系统限制
    if (message && (size_t)message->msg_iovlen > IOV_MAX) {
        set_errno(EMSGSIZE);            // 设置消息过大错误
        return -get_errno();
    }

    // 检查消息头各字段的用户空间地址有效性
    CHECK_FIELD_ASPACE(message, msg_name, message->msg_namelen);
    CHECK_FIELD_ASPACE(message, msg_iov, message->msg_iovlen * sizeof(struct iovec));
    CHECK_FIELD_ASPACE(message, msg_control, message->msg_controllen);

    // 从用户空间复制各字段数据
    DUP_FIELD_FROM_USER(message, msg_iov, message->msg_iovlen * sizeof(struct iovec));
    CHECK_ARRAY_FIELD_ASPACE(message, msg_iov, message->msg_iovlen, iov_base, iov_len,
        FREE_DUP_FIELD(message, msg_iov));
    DUP_FIELD_FROM_USER(message, msg_name, message->msg_namelen,
        FREE_DUP_FIELD(message, msg_iov));
    DUP_FIELD_FROM_USER(message, msg_control, message->msg_controllen,
        FREE_DUP_FIELD(message, msg_iov);
        FREE_DUP_FIELD(message, msg_name));
    DUP_ARRAY_FIELD_FROM_USER(message, msg_iov, message->msg_iovlen, iov_base, iov_len,
        FREE_DUP_FIELD(message, msg_control);
        FREE_DUP_FIELD(message, msg_iov);
        FREE_DUP_FIELD(message, msg_name));

    if (message == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = sendmsg(s, message, flags); // 调用内核发送消息函数
    }
    // 释放所有复制的内存
    FREE_DUP_ARRAY_FIELD(message, msg_iov, message->msg_iovlen, iov_base);
    FREE_DUP_FIELD(message, msg_control);
    FREE_DUP_FIELD(message, msg_iov);
    FREE_DUP_FIELD(message, msg_name);
    if (ret == -1) {
        return -get_errno();            // 返回错误码
    }

    return ret;
}

/**
 * @brief 接收消息系统调用（支持分散I/O和控制消息）
 * @param s 用户空间套接字描述符
 * @param message 指向msghdr结构体的指针
 * @param flags 接收标志
 * @return 成功返回接收字节数，失败返回负数错误码
 */
ssize_t SysRecvMsg(int s, struct msghdr *message, int flags)
{
    int ret;                            // 函数返回值

    SOCKET_U2K(s);                      // 转换为内核空间描述符

    CHECK_ASPACE(message, sizeof(struct msghdr)); // 检查消息头有效性
    CPY_FROM_NONCONST_USER(message);    // 复制消息头

    // 检查iov数量是否超过系统限制
    if (message && (size_t)message->msg_iovlen > IOV_MAX) {
        set_errno(EMSGSIZE);            // 设置消息过大错误
        return -get_errno();
    }

    // 检查消息头各字段的用户空间地址有效性
    CHECK_FIELD_ASPACE(message, msg_name, message->msg_namelen);
    CHECK_FIELD_ASPACE(message, msg_iov, message->msg_iovlen * sizeof(struct iovec));
    CHECK_FIELD_ASPACE(message, msg_control, message->msg_controllen);

    // 从用户空间复制各字段数据结构
    DUP_FIELD_FROM_USER(message, msg_iov, message->msg_iovlen * sizeof(struct iovec));
    CHECK_ARRAY_FIELD_ASPACE(message, msg_iov, message->msg_iovlen, iov_base, iov_len,
        FREE_DUP_FIELD(message, msg_iov));
    DUP_FIELD_FROM_USER_NOCOPY(message, msg_name, message->msg_namelen,
        FREE_DUP_FIELD(message, msg_iov));
    DUP_FIELD_FROM_USER_NOCOPY(message, msg_control, message->msg_controllen,
        FREE_DUP_FIELD(message, msg_iov);
        FREE_DUP_FIELD(message, msg_name));
    DUP_ARRAY_FIELD_FROM_USER_NOCOPY(message, msg_iov, message->msg_iovlen, iov_base, iov_len,
        FREE_DUP_FIELD(message, msg_control);
        FREE_DUP_FIELD(message, msg_iov);
        FREE_DUP_FIELD(message, msg_name));

    if (message == NULL) {
        set_errno(EFAULT);              // 设置内存访问错误
        ret = -1;
    } else {
        ret = recvmsg(s, message, flags); // 调用内核接收消息函数
    }
    if (ret == -1) {
        goto OUT;                       // 错误处理跳转
    }

    // 将结果复制回用户空间
    CPY_TO_USER(message, ret = -1; goto OUT);
    DUP_FIELD_TO_USER(message, msg_control, message->msg_controllen, ret = -1; goto OUT);
    DUP_FIELD_TO_USER(message, msg_iov, message->msg_iovlen * sizeof(struct iovec), ret = -1; goto OUT);
    DUP_FIELD_TO_USER(message, msg_name, message->msg_namelen, ret = -1; goto OUT);
    DUP_ARRAY_FIELD_TO_USER(message, msg_iov, message->msg_iovlen, iov_base, iov_len, ret = -1; goto OUT);
OUT:
    // 释放所有复制的内存
    FREE_DUP_ARRAY_FIELD(message, msg_iov, message->msg_iovlen, iov_base);
    FREE_DUP_FIELD(message, msg_control);
    FREE_DUP_FIELD(message, msg_iov);
    FREE_DUP_FIELD(message, msg_name);
    return (ret == -1) ? -get_errno() : ret;
}

#endif
