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

/**

https://www.cnblogs.com/sparkdev/p/8341134.html
*/

#ifdef LOSCFG_NET_LWIP_SACK
#include <lwip/sockets.h>

#if !LWIP_COMPAT_SOCKETS

#define CHECK_NULL_PTR(ptr) do { if (ptr == NULL) { set_errno(EFAULT); return -1; } } while (0)

/**
 * @brief 
 * @verbatim
    TCP服务器端依次调用socket()、bind()、listen()之后，就会监听指定的socket地址了。
    TCP客户端依次调用socket()、connect()之后就向TCP服务器发送了一个连接请求。
    TCP服务器监听到这个请求之后，就会调用accept()函数取接收请求，这样连接就建立好了。
    之后就可以开始网络I/O操作了，即类同于普通文件的读写I/O操作。
    accept函数
        第一个参数为服务器的socket描述字，
        第二个参数为指向struct sockaddr *的指针，用于返回客户端的协议地址，
        第三个参数为客户端协议地址的长度。
    如果accpet成功，那么其返回值是由内核自动生成的一个全新的描述字，代表与返回客户的TCP连接。
    注意：accept的第一个参数为服务器的socket描述字，是服务器开始调用socket()函数生成的，称为监听socket描述字；
        而accept函数返回的是已连接的socket描述字。一个服务器通常通常仅仅只创建一个监听socket描述字，
        它在该服务器的生命周期内一直存在。内核为每个由服务器进程接受的客户连接创建了一个已连接socket描述字，
        当服务器完成了对某个客户的服务，相应的已连接socket描述字就被关闭。
 * @endverbatim
 * @param s 
 * @param addr 
 * @param addrlen 
 * @return int 
 */
int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return lwip_accept(s, addr, addrlen);
}

/**
 * @brief 
 * @verbatim
    bind()函数把一个地址族中的特定地址赋给socket。
    例如对应AF_INET、AF_INET6就是把一个ipv4或ipv6地址和端口号组合赋给socket。
    函数的三个参数分别为： 
        •sockfd：即socket描述字，它是通过socket()函数创建了，唯一标识一个socket。
            bind()函数就是将给这个描述字绑定一个名字。 
        •addr：一个const struct sockaddr *指针，指向要绑定给sockfd的协议地址。
        •addrlen：对应的是地址的长度。 
    通常服务器在启动的时候都会绑定一个众所周知的地址（如ip地址+端口号），用于提供服务，
    客户就可以通过它来接连服务器；而客户端就不用指定，有系统自动分配一个端口号和自身的ip地址组合。
    这就是为什么通常服务器端在listen之前会调用bind()，而客户端就不会调用，而是在connect()时由系统随机生成一个。
 * @endverbatim
 * @param s 
 * @param name 
 * @param namelen 
 * @return int 
 */
int bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    CHECK_NULL_PTR(name);
    if (namelen < sizeof(*name)) {
        set_errno(EINVAL);
        return -1;
    }
    return lwip_bind(s, name, namelen);
}

/**
 * @brief 
 * @verbatim
该函数的行为依赖于how的值 
	SHUT_RD：值为0，关闭连接的读这一半。 
	SHUT_WR：值为1，关闭连接的写这一半。 
	SHUT_RDWR：值为2，连接的读和写都关闭。 
终止网络连接的通用方法是调用close函数。但使用shutdown能更好的控制断连过程（使用第二个参数）。

closesocket 与 shutdown 的区别主要表现在： 
closesocket 函数会关闭套接字ID，如果有其他的进程共享着这个套接字，那么它仍然是打开的，
	这个连接仍然可以用来读和写，并且有时候这是非常重要的 ，特别是对于多进程并发服务器来说。 
而shutdown会切断进程共享的套接字的所有连接，不管这个套接字的引用计数是否为零，
	那些试图读得进程将会接收到EOF标识，那些试图写的进程将会检测到SIGPIPE信号，
	同时可利用shutdown的第二个参数选择断连的方式。 
 * @endverbatim
 * @param s 
 * @param how 
 * @return int 
 */
int shutdown(int s, int how)
{
    return lwip_shutdown(s, how);
}
///获取对等名称 = getsockname
int getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    CHECK_NULL_PTR(name);//参数判空检查,这种写法有点意思
    CHECK_NULL_PTR(namelen);
    return lwip_getpeername(s, name, namelen);
}
///获取socket名称和长度
int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    CHECK_NULL_PTR(name);
    CHECK_NULL_PTR(namelen);
    return lwip_getsockname(s, name, namelen);
}
///获取 socket 配置项
int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    return lwip_getsockopt(s, level, optname, optval, optlen);
}
///设置socket 配置项
int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    return lwip_setsockopt(s, level, optname, optval, optlen);
}

/**
 * @brief 
 * @verbatim
closesocket 一个套接字的默认行为是把套接字标记为已关闭，然后立即返回到调用进程，
该套接字描述符不能再由调用进程使用，也就是说它不能再作为read或write的第一个参数，
然而TCP将尝试发送已排队等待发送到对端，发送完毕后发生的是正常的TCP连接终止序列。 
在多进程并发服务器中，父子进程共享着套接字，套接字描述符引用计数记录着共享着的进程个数，
当父进程或某一子进程close掉套接字时，描述符引用计数会相应的减一，
当引用计数仍大于零时，这个close调用就不会引发TCP的四路握手断连过程。
 * @endverbatim
 * @param s 
 * @return int 
 */
int closesocket(int s)
{
    return lwip_close(s);
}

/**
 * @brief 
 * @verbatim
connect函数由客户端使用,发出连接请求，服务器端就会接收到这个请求
	第一个参数即为客户端的socket描述字，
	第二参数为服务器的socket地址，
	第三个参数为socket地址的长度。
客户端通过调用connect函数来建立与TCP服务器的连接。
 * @endverbatim
 * @param s 
 * @param name 
 * @param namelen 
 * @return int 
 */
int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    CHECK_NULL_PTR(name);
    if (namelen < sizeof(*name)) {
        set_errno(EINVAL);
        return -1;
    }
    return lwip_connect(s, name, namelen);
}

/**
 * @brief 
 * @verbatim
如果作为一个服务器，在调用socket()、bind()之后就会调用listen()来监听这个socket，
如果客户端这时调用connect()发出连接请求，服务器端就会接收到这个请求。
listen函数的
	第一个参数即为要监听的socket描述字，
	第二个参数为相应socket可以排队的最大连接个数。
	socket()函数创建的socket默认是一个主动类型的，listen函数将socket变为被动类型的，等待客户的连接请求。
 * @endverbatim
 * @param s 
 * @param backlog 
 * @return int 
 */
int listen(int s, int backlog)
{
    return lwip_listen(s, backlog);
}

/**
 * @brief 
 * @verbatim
相当于文件操作的 read 功能,区别是第四个参数

MSG_DONTROUTE：不查找表，是send函数使用的标志，这个标志告诉IP，目的主机在本地网络上，
	没有必要查找表，这个标志一般用在网络诊断和路由程序里面。 
MSG_OOB：表示可以接收和发送带外数据。 
MSG_PEEK：查看数据，并不从系统缓冲区移走数据。是recv函数使用的标志，
	表示只是从系统缓冲区中读取内容，而不清楚系统缓冲区的内容。这样在下次读取的时候，
	依然是一样的内容，一般在有个进程读写数据的时候使用这个标志。 
MSG_WAITALL：等待所有数据，是recv函数的使用标志，表示等到所有的信息到达时才返回，
	使用这个标志的时候，recv返回一直阻塞，直到指定的条件满足时，或者是发生了错误。
 * @endverbatim
 * @param s 
 * @param mem 
 * @param len 
 * @param flags 
 * @return ssize_t 
 */
ssize_t recv(int s, void *mem, size_t len, int flags)
{
    CHECK_NULL_PTR(mem);
    return lwip_recv(s, mem, len, flags);
}
///区别是返回源地址,意思是这些数据是从哪个地址过来的
ssize_t recvfrom(int s, void *mem, size_t len, int flags,
                 struct sockaddr *from, socklen_t *fromlen)
{
    CHECK_NULL_PTR(mem);
    return lwip_recvfrom(s, mem, len, flags, from, fromlen);
}
///只是数据的格式的不同
ssize_t recvmsg(int s, struct msghdr *message, int flags)
{
    CHECK_NULL_PTR(message);
    if (message->msg_iovlen) {
        CHECK_NULL_PTR(message->msg_iov);
    }
    return lwip_recvmsg(s, message, flags);
}

/// 相当于文件操作的 write 功能,区别是第四个参数 同 recv
ssize_t send(int s, const void *dataptr, size_t size, int flags)
{
    CHECK_NULL_PTR(dataptr);
    return  lwip_send(s, dataptr, size, flags);
}
///只是发送数据的格式的不同
ssize_t sendmsg(int s, const struct msghdr *message, int flags)
{
    return lwip_sendmsg(s, message, flags);
}
///区别是送达地址,意思是这些数据要发给哪个地址的
ssize_t sendto(int s, const void *dataptr, size_t size, int flags,
               const struct sockaddr *to, socklen_t tolen)
{
    CHECK_NULL_PTR(dataptr);
    if (to && tolen < sizeof(*to)) {
        set_errno(EINVAL);
        return -1;
    }
    return lwip_sendto(s, dataptr, size, flags, to, tolen);
}

/**
 * @brief
 * @verbatim
用于创建一个socket描述符（socket descriptor），它唯一标识一个socket。
这个socket描述符跟文件描述符一样，后续的操作都有用到它，把它作为参数，通过它来进行一些读写操作。
正如可以给fopen的传入不同参数值，以打开不同的文件。创建socket的时候，也可以指定不同的参数
创建不同的socket描述符，socket函数的三个参数分别为： 

•domain：即协议域，又称为协议族（family）。
	常用的协议族有，
	AF_INET（IPv4)、
	AF_INET6(IPv6)、
	AF_LOCAL（或称AF_UNIX，Unix域socket）、
	AF_ROUTE等等。
	协议族决定了socket的地址类型，在通信中必须采用对应的地址，
	如AF_INET决定了要用ipv4地址（32位的）与端口号（16位的）的组合、
	AF_UNIX决定了要用一个绝对路径名作为地址。 
•type：指定socket类型。
	常用的socket类型有，
	SOCK_STREAM（流式套接字）、
	SOCK_DGRAM（数据报式套接字）、
	SOCK_RAW、SOCK_PACKET、
	SOCK_SEQPACKET等等 
•protocol：就是指定协议。
	常用的协议有，
	IPPROTO_TCP、PPTOTO_UDP、IPPROTO_SCTP、IPPROTO_TIPC等，
	它们分别对应TCP传输协议、UDP传输协议、STCP传输协议、TIPC传输协议。
注意：并不是上面的type和protocol可以随意组合的，如SOCK_STREAM不可以跟IPPROTO_UDP组合。
当protocol为0时，会自动选择type类型对应的默认协议。	
 * @endverbatim
 * @param domain 
 * @param type 
 * @param protocol 
 * @return int 
 */
int socket(int domain, int type, int protocol)
{
    return lwip_socket(domain, type, protocol);
}
/*!
inet_ntop 函数转换网络二进制结构到ASCII类型的地址，参数的作用和上面相同，
只是多了一个参数socklen_t cnt,他是所指向缓存区dst的大小，避免溢出，
如果缓存区太小无法存储地址的值，则返回一个空指针，并将errno置为ENOSPC
*/
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    return lwip_inet_ntop(af, src, dst, size);
}
///inet_pton 函数转换字符串到网络地址，第一个参数af是地址族，转换后存在dst中
int inet_pton(int af, const char *src, void *dst)
{
    return lwip_inet_pton(af, src, dst);
}

#ifndef LWIP_INET_ADDR_FUNC
in_addr_t inet_addr(const char* cp)
{
    return ipaddr_addr(cp);
}
#endif

#ifndef LWIP_INET_ATON_FUNC
int inet_aton(const char* cp, struct in_addr* inp)
{
    return ip4addr_aton(cp, (ip4_addr_t*)inp);
}
#endif

#ifndef LWIP_INET_NTOA_FUNC
char* inet_ntoa(struct in_addr in)
{
    return ip4addr_ntoa((const ip4_addr_t*)&(in));
}
#endif

#endif /* !LWIP_COMPAT_SOCKETS */
#endif /* LOSCFG_NET_LWIP_SACK */