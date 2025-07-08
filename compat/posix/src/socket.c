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

#include <los_config.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#ifdef LOSCFG_NET_LWIP_SACK
#include <lwip/sockets.h>

#if !LWIP_COMPAT_SOCKETS
/**
 * @brief 空指针检查宏
 * @param[in] ptr 需要检查的指针
 * @note 如果指针为NULL，设置errno为EFAULT并返回-1
 */
#define CHECK_NULL_PTR(ptr) do { if (ptr == NULL) { set_errno(EFAULT); return -1; } } while (0)

/**
 * @brief 接受客户端连接请求
 * @param[in] s 监听套接字描述符
 * @param[out] addr 客户端地址结构体指针
 * @param[in,out] addrlen 地址结构体长度指针
 * @return 成功返回新的连接套接字描述符，失败返回-1并设置errno
 */
int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return lwip_accept(s, addr, addrlen);  // 调用LWIP库接口接受连接
}

/**
 * @brief 将套接字绑定到指定地址和端口
 * @param[in] s 套接字描述符
 * @param[in] name 包含地址信息的结构体指针
 * @param[in] namelen 地址结构体长度
 * @return 成功返回0，失败返回-1并设置errno
 */
int bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    CHECK_NULL_PTR(name);  // 检查地址结构体指针有效性
    if (namelen < sizeof(*name)) {  // 检查地址结构体长度有效性
        set_errno(EINVAL);          // 设置错误码为参数无效
        return -1;                  // 返回错误
    }
    return lwip_bind(s, name, namelen);  // 调用LWIP库接口进行绑定
}

/**
 * @brief 关闭套接字的读或写功能
 * @param[in] s 套接字描述符
 * @param[in] how 关闭方式(SHUT_RD/SHUT_WR/SHUT_RDWR)
 * @return 成功返回0，失败返回-1并设置errno
 */
int shutdown(int s, int how)
{
    return lwip_shutdown(s, how);  // 调用LWIP库接口关闭连接
}

/**
 * @brief 获取套接字连接的对端地址
 * @param[in] s 套接字描述符
 * @param[out] name 存储对端地址的结构体指针
 * @param[in,out] namelen 地址结构体长度指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    CHECK_NULL_PTR(name);     // 检查地址结构体指针有效性
    CHECK_NULL_PTR(namelen);  // 检查长度指针有效性
    return lwip_getpeername(s, name, namelen);  // 调用LWIP库接口获取对端地址
}

/**
 * @brief 获取套接字自身的地址
 * @param[in] s 套接字描述符
 * @param[out] name 存储自身地址的结构体指针
 * @param[in,out] namelen 地址结构体长度指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    CHECK_NULL_PTR(name);     // 检查地址结构体指针有效性
    CHECK_NULL_PTR(namelen);  // 检查长度指针有效性
    return lwip_getsockname(s, name, namelen);  // 调用LWIP库接口获取自身地址
}

/**
 * @brief 获取套接字选项值
 * @param[in] s 套接字描述符
 * @param[in] level 选项级别(SOL_SOCKET/IPPROTO_TCP等)
 * @param[in] optname 选项名称
 * @param[out] optval 存储选项值的缓冲区指针
 * @param[in,out] optlen 选项值缓冲区长度指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    return lwip_getsockopt(s, level, optname, optval, optlen);  // 调用LWIP库接口获取选项值
}

/**
 * @brief 设置套接字选项值
 * @param[in] s 套接字描述符
 * @param[in] level 选项级别(SOL_SOCKET/IPPROTO_TCP等)
 * @param[in] optname 选项名称
 * @param[in] optval 包含选项值的缓冲区指针
 * @param[in] optlen 选项值缓冲区长度
 * @return 成功返回0，失败返回-1并设置errno
 */
int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    return lwip_setsockopt(s, level, optname, optval, optlen);  // 调用LWIP库接口设置选项值
}

/**
 * @brief 关闭套接字
 * @param[in] s 套接字描述符
 * @return 成功返回0，失败返回-1并设置errno
 */
int closesocket(int s)
{
    return lwip_close(s);  // 调用LWIP库接口关闭套接字
}

/**
 * @brief 建立与服务器的连接
 * @param[in] s 套接字描述符
 * @param[in] name 包含服务器地址信息的结构体指针
 * @param[in] namelen 地址结构体长度
 * @return 成功返回0，失败返回-1并设置errno
 */
int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    CHECK_NULL_PTR(name);  // 检查地址结构体指针有效性
    if (namelen < sizeof(*name)) {  // 检查地址结构体长度有效性
        set_errno(EINVAL);          // 设置错误码为参数无效
        return -1;                  // 返回错误
    }
    return lwip_connect(s, name, namelen);  // 调用LWIP库接口建立连接
}

/**
 * @brief 将套接字设置为监听状态
 * @param[in] s 套接字描述符
 * @param[in] backlog 连接请求队列的最大长度
 * @return 成功返回0，失败返回-1并设置errno
 */
int listen(int s, int backlog)
{
    return lwip_listen(s, backlog);  // 调用LWIP库接口设置监听
}

/**
 * @brief 从套接字接收数据
 * @param[in] s 套接字描述符
 * @param[out] mem 存储接收数据的缓冲区指针
 * @param[in] len 缓冲区长度
 * @param[in] flags 接收标志(MSG_OOB/MSG_PEEK等)
 * @return 成功返回接收字节数，失败返回-1并设置errno
 */
ssize_t recv(int s, void *mem, size_t len, int flags)
{
    CHECK_NULL_PTR(mem);  // 检查接收缓冲区指针有效性
    return lwip_recv(s, mem, len, flags);  // 调用LWIP库接口接收数据
}

/**
 * @brief 从套接字接收数据并获取发送方地址
 * @param[in] s 套接字描述符
 * @param[out] mem 存储接收数据的缓冲区指针
 * @param[in] len 缓冲区长度
 * @param[in] flags 接收标志
 * @param[out] from 存储发送方地址的结构体指针
 * @param[in,out] fromlen 地址结构体长度指针
 * @return 成功返回接收字节数，失败返回-1并设置errno
 */
ssize_t recvfrom(int s, void *mem, size_t len, int flags,
                 struct sockaddr *from, socklen_t *fromlen)
{
    CHECK_NULL_PTR(mem);  // 检查接收缓冲区指针有效性
    return lwip_recvfrom(s, mem, len, flags, from, fromlen);  // 调用LWIP库接口接收数据
}

/**
 * @brief 通过消息结构体接收数据
 * @param[in] s 套接字描述符
 * @param[out] message 包含接收信息的msghdr结构体指针
 * @param[in] flags 接收标志
 * @return 成功返回接收字节数，失败返回-1并设置errno
 */
ssize_t recvmsg(int s, struct msghdr *message, int flags)
{
    CHECK_NULL_PTR(message);  // 检查消息结构体指针有效性
    if (message->msg_iovlen) {  // 如果设置了分散接收缓冲区
        CHECK_NULL_PTR(message->msg_iov);  // 检查缓冲区数组指针有效性
    }
    return lwip_recvmsg(s, message, flags);  // 调用LWIP库接口接收消息
}

/**
 * @brief 向套接字发送数据
 * @param[in] s 套接字描述符
 * @param[in] dataptr 包含发送数据的缓冲区指针
 * @param[in] size 发送数据长度
 * @param[in] flags 发送标志
 * @return 成功返回发送字节数，失败返回-1并设置errno
 */
ssize_t send(int s, const void *dataptr, size_t size, int flags)
{
    CHECK_NULL_PTR(dataptr);  // 检查发送数据指针有效性
    return  lwip_send(s, dataptr, size, flags);  // 调用LWIP库接口发送数据
}

/**
 * @brief 通过消息结构体发送数据
 * @param[in] s 套接字描述符
 * @param[in] message 包含发送信息的msghdr结构体指针
 * @param[in] flags 发送标志
 * @return 成功返回发送字节数，失败返回-1并设置errno
 */
ssize_t sendmsg(int s, const struct msghdr *message, int flags)
{
    return lwip_sendmsg(s, message, flags);  // 调用LWIP库接口发送消息
}

/**
 * @brief 向指定地址发送数据
 * @param[in] s 套接字描述符
 * @param[in] dataptr 包含发送数据的缓冲区指针
 * @param[in] size 发送数据长度
 * @param[in] flags 发送标志
 * @param[in] to 目标地址结构体指针
 * @param[in] tolen 地址结构体长度
 * @return 成功返回发送字节数，失败返回-1并设置errno
 */
ssize_t sendto(int s, const void *dataptr, size_t size, int flags,
               const struct sockaddr *to, socklen_t tolen)
{
    CHECK_NULL_PTR(dataptr);  // 检查发送数据指针有效性
    if (to && tolen < sizeof(*to)) {  // 如果指定了目标地址且长度无效
        set_errno(EINVAL);            // 设置错误码为参数无效
        return -1;                    // 返回错误
    }
    return lwip_sendto(s, dataptr, size, flags, to, tolen);  // 调用LWIP库接口发送数据
}

/**
 * @brief 创建套接字
 * @param[in] domain 地址族(AF_INET/AF_INET6等)
 * @param[in] type 套接字类型(SOCK_STREAM/SOCK_DGRAM等)
 * @param[in] protocol 传输协议(IPPROTO_TCP/IPPROTO_UDP等)
 * @return 成功返回套接字描述符，失败返回-1并设置errno
 */
int socket(int domain, int type, int protocol)
{
    return lwip_socket(domain, type, protocol);  // 调用LWIP库接口创建套接字
}

/**
 * @brief 将网络地址转换为字符串表示
 * @param[in] af 地址族
 * @param[in] src 网络地址结构体指针
 * @param[out] dst 存储字符串的缓冲区
 * @param[in] size 缓冲区大小
 * @return 成功返回字符串指针，失败返回NULL并设置errno
 */
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    return lwip_inet_ntop(af, src, dst, size);  // 调用LWIP库接口进行地址转换
}

/**
 * @brief 将字符串表示的地址转换为网络地址
 * @param[in] af 地址族
 * @param[in] src 字符串地址
 * @param[out] dst 存储网络地址的结构体指针
 * @return 成功返回1，失败返回0并设置errno
 */
int inet_pton(int af, const char *src, void *dst)
{
    return lwip_inet_pton(af, src, dst);  // 调用LWIP库接口进行地址转换
}

#ifndef LWIP_INET_ADDR_FUNC
/**
 * @brief 将点分十进制IP字符串转换为32位整数(仅当LWIP_INET_ADDR_FUNC未定义时生效)
 * @param[in] cp 点分十进制IP字符串
 * @return 成功返回32位网络字节序IP地址，失败返回INADDR_NONE
 */
in_addr_t inet_addr(const char* cp)
{
    return ipaddr_addr(cp);  // 调用LWIP库接口进行地址转换
}
#endif

#ifndef LWIP_INET_ATON_FUNC
/**
 * @brief 将点分十进制IP字符串转换为in_addr结构体(仅当LWIP_INET_ATON_FUNC未定义时生效)
 * @param[in] cp 点分十进制IP字符串
 * @param[out] inp 存储IP地址的in_addr结构体指针
 * @return 成功返回1，失败返回0
 */
int inet_aton(const char* cp, struct in_addr* inp)
{
    return ip4addr_aton(cp, (ip4_addr_t*)inp);  // 调用LWIP库接口进行地址转换
}
#endif

#ifndef LWIP_INET_NTOA_FUNC
/**
 * @brief 将32位整数IP地址转换为点分十进制字符串(仅当LWIP_INET_NTOA_FUNC未定义时生效)
 * @param[in] in 32位网络字节序IP地址
 * @return 成功返回字符串指针，失败返回NULL
 */
char* inet_ntoa(struct in_addr in)
{
    return ip4addr_ntoa((const ip4_addr_t*)&(in));  // 调用LWIP库接口进行地址转换
}
#endif

#endif /* !LWIP_COMPAT_SOCKETS */
#endif /* LOSCFG_NET_LWIP_SACK */