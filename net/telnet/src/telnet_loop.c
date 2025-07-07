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
/*!
 * @file   telnet_loop.c
 * @brief  telnet实现过程
 * @link RFC854 https://docs.huihoo.com/rfc/RFC854.txt @endlink
   @verbatim
	telnet命令通常用来远程登录。telnet程序是基于TELNET协议的远程登录客户端程序。Telnet协议是TCP/IP协议族中的一员，
	是Internet远程登陆服务的标准协议和主要方式。它为用户提供了在本地计算机上完成远程主机工作的 能力。
	在终端使用者的电脑上使用telnet程序，用它连接到服务器。终端使用者可以在telnet程序中输入命令，这些命令会在服务器上运行，
	就像直接在服务器的控制台上输入一样。可以在本地就能控制服务器。要开始一个 telnet会话，必须输入用户名和密码来登录服务器。
	Telnet是常用的远程控制Web服务器的方法。

	但是，telnet因为采用明文传送报文，安全性不好，很多Linux服务器都不开放telnet服务，而改用更安全的ssh方式了。
	但仍然有很多别的系统可能采用了telnet方式来提供远程登录，因此弄清楚telnet客户端的使用方式仍是很有必要的。

	telnet命令还可做别的用途，比如确定远程服务的状态，比如确定远程服务器的某个端口是否能访问。

	下面几个编码对NVT打印机有确定意义：
	
	名称 				 		编码 			意义
	
	NULL (NUL) 				0		没有操作
	BELL (BEL) 			 	7		产生一个可以看到或可以听到的信号（而不移动打印头。）
	Back Space (BS)		 	8		向左移动打印头一个字符位置。
	Horizontal Tab (HT)	 	9		把打印头移到下一个水平制表符停止的位置。它仍然没有指定每一方如何检测或者设定如何定位这样的制表符的停止位置。
	Line Feed (LF) 			10		打印头移到下一个打印行，但不改变打印头的水平位置。
	Vertical Tab (VT)		11 		把打印头移到下一个垂直制表符停止的位置。它仍然没有指定每一方如何检测或者设定如何定位这样的制表符的停止位置。
	Form Feed (FF) 		 	12 		把打印头移到下一页的顶部，保持打印头在相同的水平位置上。
	Carriage Return (CR)	13 		把打印头移到当前行的左边 。


	下面是所有已定义的TELNET命令。需要注意的是，这些代码和代码序列只有在前面跟一个IAC时才有意义。
	------------------------------------------------------------------------
	名称                     代码                     意义
	------------------------------------------------------------------------
	SE                      240                子谈判参数的结束
	NOP                     241                空操作
	Data Mark               242                一个同步信号的数据流部分。该命令的后面经常跟着一个TCP紧急通知
	Break                   243                NVT的BRK字符
	Interrupt Process       244                IP功能.
	Abort output            245                AO功能.
	Are You There           246                AYT功能.
	Erase character         247                EC功能.
	Erase Line              248                EL功能.
	Go ahead                249                GA信号.
	SB                      250                表示后面所跟的是对需要的选项的子谈判
	WILL (option code)      251                表示希望开始使用或者确认所使用的是指定的选项。
	WON'T (option code)     252                表示拒绝使用或者继续使用指定的选项。
	DO (option code)        253                表示一方要求另一方使用，或者确认你希望另一方使用指定的选项。
	DON'T (option code)     254                表示一方要求另一方停止使用，或者确认你不再希望另一方使用指定的选项。 
	IAC                     255            	   Data Byte 255.
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-22
 */

#include "telnet_loop.h"
#ifdef LOSCFG_NET_TELNET
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "pthread.h"
#include "netinet/tcp.h"
#include "sys/select.h"
#include "sys/types.h"
#include "sys/prctl.h"

#include "los_task.h"
#include "linux/atomic.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "console.h"
#ifdef LOSCFG_SHELL
#include "shell.h"
#include "shcmd.h"
#endif
#include "telnet_pri.h"
#include "telnet_dev.h"


/* TELNET commands in RFC854 */
#define TELNET_SB   250 /* Indicates that what follows is subnegotiation of the indicated option */
#define TELNET_WILL 251 /* Indicates the desire to perform the indicated option */
#define TELNET_DO   253 /* Indicates the request for the other party to perform the indicated option */
#define TELNET_IAC  255 /* Interpret as Command */

/* telnet options in IANA */
#define TELNET_ECHO 1    /* Echo */
#define TELNET_SGA  3    /* Suppress Go Ahead */
#define TELNET_NAWS 31   /* Negotiate About Window Size */
#define TELNET_NOP  0xf1 /* Unassigned in IANA, putty use this to keepalive */

#define LEN_IAC_CMD      2 /* Only 2 char: |IAC|cmd| */
#define LEN_IAC_CMD_OPT  3 /* Only 3 char: |IAC|cmd|option| */
#define LEN_IAC_CMD_NAWS 9 /* NAWS: |IAC|SB|NAWS|x1|x2|x3|x4|IAC|SE| */

/* server/client settings */
#define TELNET_TASK_STACK_SIZE  0x2000
#define TELNET_TASK_PRIORITY    9

/* server settings */
#define TELNET_LISTEN_BACKLOG   128
#define TELNET_ACCEPT_INTERVAL  200

/* client settings */
#define TELNET_CLIENT_POLL_TIMEOUT  2000
#define TELNET_CLIENT_READ_BUF_SIZE 256
#define TELNET_CLIENT_READ_FILTER_BUF_SIZE (8 * 1024)

/* limitation: only support 1 telnet client connection */
STATIC volatile INT32 g_telnetClientFd = -1;  /* client fd */

/* limitation: only support 1 telnet server */
STATIC volatile INT32 g_telnetListenFd = -1;  /* listen fd of telnetd */

/* each bit for a client connection, although only support 1 connection for now */
STATIC volatile UINT32 g_telnetMask = 0;
/* taskID of telnetd */
STATIC atomic_t g_telnetTaskId = 0;
/* protect listenFd, clientFd etc. */
pthread_mutex_t g_telnetMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/**
 * @brief 获取Telnet互斥锁
 */
VOID TelnetLock(VOID)
{
    (VOID)pthread_mutex_lock(&g_telnetMutex);  // 加锁保护Telnet全局资源
}

/**
 * @brief 释放Telnet互斥锁
 */
VOID TelnetUnlock(VOID)
{
    (VOID)pthread_mutex_unlock(&g_telnetMutex);  // 解锁Telnet全局资源
}

/**
 * @brief 过滤客户端数据流中的IAC控制命令
 * @param src 源数据缓冲区
 * @param srcLen 源数据长度
 * @param dstLen 输出参数，过滤后的数据长度
 * @return 过滤后的数据缓冲区指针
 */
STATIC UINT8 *ReadFilter(const UINT8 *src, UINT32 srcLen, UINT32 *dstLen)
{
    STATIC UINT8 buf[TELNET_CLIENT_READ_FILTER_BUF_SIZE];  // 静态缓冲区存储过滤后的数据
    UINT8 *dst = buf;  // 目标缓冲区指针
    UINT32 left = srcLen;  // 剩余未处理数据长度

    while (left > 0) {  // 循环处理所有输入数据
        if (*src != TELNET_IAC) {  // 如果不是IAC控制字符，直接复制
            *dst = *src;
            dst++;
            src++;
            left--;
            continue;
        }

        /*
         * IAC控制字符处理逻辑:
         * |IAC|          --> 跳过
         * |IAC|NOP|...   --> 跳过NOP(用于putty保活等)
         * |IAC|x|        --> 跳过单个选项
         * |IAC|IAC|x|... --> 保留单个IAC后继续处理
         * |IAC|SB|NAWS|x1|x2|x3|x4|IAC|SE|... --> 跳过不支持的NAWS窗口大小协商
         * |IAC|x|x|...   --> 跳过不支持的IAC命令
         */
        if (left == 1) {  // 单个IAC字符，无法解析，跳出循环
            break;
        }
        // 至少还有2个字节数据
        if (*(src + 1) == TELNET_NOP) {  // NOP命令，跳过2字节
            src += LEN_IAC_CMD;
            left -= LEN_IAC_CMD;
            continue;
        }
        if (left == LEN_IAC_CMD) {  // 仅IAC+1字节，无法解析，跳出循环
            break;
        }
        // 至少还有3个字节数据
        if (*(src + 1) == TELNET_IAC) {  // IAC转义序列(IAC IAC)，保留一个IAC
            *dst = TELNET_IAC;
            dst++;
            src += LEN_IAC_CMD;
            left -= LEN_IAC_CMD;
            continue;
        }
        // 处理SB NAWS子选项(不支持，跳过整个子选项)
        if ((*(src + 1) == TELNET_SB) && (*(src + LEN_IAC_CMD) == TELNET_NAWS)) {
            if (left > LEN_IAC_CMD_NAWS) {  // 数据长度足够，跳过整个NAWS选项
                src += LEN_IAC_CMD_NAWS;
                left -= LEN_IAC_CMD_NAWS;
                continue;
            }
            break;  // 数据长度不足，跳出循环
        }
        // 其他不支持的IAC命令，跳过3字节
        src += LEN_IAC_CMD_OPT;
        left -= LEN_IAC_CMD_OPT;
    }

    if (dstLen != NULL) {  // 设置输出长度
        *dstLen = dst - buf;
    }
    return buf;  // 返回过滤后的数据
}

/**
 * @brief 向文件描述符写入数据
 * @param fd 文件描述符
 * @param src 数据缓冲区指针
 * @param srcLen 数据长度
 * @return 成功写入的字节数，失败返回-1
 */
STATIC ssize_t WriteToFd(INT32 fd, const CHAR *src, size_t srcLen)
{
    size_t sizeLeft;  // 剩余未写入数据长度
    ssize_t sizeWritten;  // 单次写入字节数

    sizeLeft = srcLen;
    while (sizeLeft > 0) {  // 循环直到所有数据写入或出错
        sizeWritten = write(fd, src, sizeLeft);
        if (sizeWritten < 0) {  // 写入出错
            /* 最后一次写入失败 */
            if (sizeLeft == srcLen) {  // 首次写入即失败
                return -1;  // 返回错误
            } else {  // 部分数据已写入
                break;  // 退出循环
            }
        } else if (sizeWritten == 0) {  // 写入0字节，退出循环
            break;
        }
        sizeLeft -= (size_t)sizeWritten;  // 更新剩余数据长度
        src += sizeWritten;  // 移动数据指针
    }

    return (ssize_t)(srcLen - sizeLeft);  // 返回已写入字节数
}

/**
 * @brief 关闭Telnet客户端连接
 */
STATIC VOID TelnetClientClose(VOID)
{
    /* 检查是否存在客户端连接 */
    if (g_telnetMask == 0) {
        return;  // 无连接，直接返回
    }
    (VOID)TelnetDevDeinit();  // 反初始化Telnet设备
    g_telnetMask = 0;  // 清除连接标志
    printf("telnet client disconnected.\n");  // 打印断开连接信息
}

/**
 * @brief 释放客户端和服务器文件描述符
 */
STATIC VOID TelnetRelease(VOID)
{
    if (g_telnetClientFd >= 0) {  // 关闭客户端FD
        (VOID)close(g_telnetClientFd);
        g_telnetClientFd = -1;  // 重置为无效值
    }

    if (g_telnetListenFd >= 0) {  // 关闭监听FD
        (VOID)close(g_telnetListenFd);
        g_telnetListenFd = -1;  // 重置为无效值
    }
}

/**
 * @brief 停止Telnet服务器
 */
STATIC VOID TelnetdDeinit(VOID)
{
    if (atomic_read(&g_telnetTaskId) == 0) {  // 检查服务器是否运行
        PRINTK("telnet server is not running!\n");
        return;
    }

    TelnetRelease();  // 释放文件描述符
    (VOID)TelnetedUnregister();  // 注销Telnet服务
    atomic_set(&g_telnetTaskId, 0);  // 重置任务ID
    PRINTK("telnet server closed.\n");  // 打印服务器关闭信息
}

/**
 * @brief 初始化Telnet服务器监听套接字
 * @param port 监听端口号
 * @return 成功返回监听文件描述符，失败返回-1
 */
STATIC INT32 TelnetdInit(UINT16 port)
{
    INT32 listenFd;  // 监听文件描述符
    INT32 reuseAddr = 1;  // 地址重用标志
    struct sockaddr_in inTelnetAddr;  // 地址结构

    listenFd = socket(AF_INET, SOCK_STREAM, 0);  // 创建TCP套接字
    if (listenFd == -1) {
        PRINT_ERR("TelnetdInit : socket error.\n");
        goto ERR_OUT;  // 跳转至错误处理
    }

    /* 设置地址重用 */
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) != 0) {
        PRINT_ERR("TelnetdInit : setsockopt REUSEADDR error.\n");
        goto ERR_CLOSE_FD;  // 跳转至关闭FD
    }

    (VOID)memset_s(&inTelnetAddr, sizeof(struct sockaddr_in), 0, sizeof(struct sockaddr_in));
    inTelnetAddr.sin_family = AF_INET;  // IPv4协议族
    inTelnetAddr.sin_addr.s_addr = INADDR_ANY;  // 绑定所有接口
    inTelnetAddr.sin_port = htons(port);  // 端口号转换为网络字节序

    if (bind(listenFd, (const struct sockaddr *)&inTelnetAddr, sizeof(struct sockaddr_in)) == -1) {  // 绑定地址
        PRINT_ERR("TelnetdInit : bind error.\n");
        goto ERR_CLOSE_FD;
    }

    if (listen(listenFd, TELNET_LISTEN_BACKLOG) == -1) {  // 开始监听
        PRINT_ERR("TelnetdInit : listen error.\n");
        goto ERR_CLOSE_FD;
    }

    return listenFd;  // 返回监听FD

ERR_CLOSE_FD:
    (VOID)close(listenFd);  // 关闭监听FD
ERR_OUT:
    return -1;  // 返回错误
}

/**
 * @brief 准备Telnet客户端连接
 * @param clientFd 客户端文件描述符
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 TelnetClientPrepare(INT32 clientFd)
{
    INT32 keepAlive = TELNET_KEEPALIVE;  // 开启TCP保活
    INT32 keepIdle = TELNET_KEEPIDLE;  // 空闲时间(秒)
    INT32 keepInterval = TELNET_KEEPINTV;  // 探测间隔(秒)
    INT32 keepCnt = TELNET_KEEPCNT;  // 探测次数
    // Telnet协议协商命令
    const UINT8 doEcho[] = { TELNET_IAC, TELNET_DO, TELNET_ECHO };  // 要求客户端回显
    const UINT8 doNaws[] = { TELNET_IAC, TELNET_DO, TELNET_NAWS };  // 要求窗口大小协商
    const UINT8 willEcho[] = { TELNET_IAC, TELNET_WILL, TELNET_ECHO };  // 服务器将回显
    const UINT8 willSga[] = { TELNET_IAC, TELNET_WILL, TELNET_SGA };  // 服务器将使用行模式

    if (g_telnetListenFd == -1) {  // 检查服务器是否已关闭
        return -1;
    }
    g_telnetClientFd = clientFd;  // 保存客户端FD
    if (TelnetDevInit(clientFd) != 0) {  // 初始化Telnet设备
        g_telnetClientFd = -1;  // 初始化失败，重置FD
        return -1;
    }
    g_telnetMask = 1;  // 设置连接标志

    /* 与客户端进行协议协商 */
    (VOID)WriteToFd(clientFd, (CHAR *)doEcho, sizeof(doEcho));
    (VOID)WriteToFd(clientFd, (CHAR *)doNaws, sizeof(doNaws));
    (VOID)WriteToFd(clientFd, (CHAR *)willEcho, sizeof(willEcho));
    (VOID)WriteToFd(clientFd, (CHAR *)willSga, sizeof(willSga));

    /* 启用TCP保活机制检测连接状态 */
    if (setsockopt(clientFd, SOL_SOCKET, SO_KEEPALIVE, (VOID *)&keepAlive, sizeof(keepAlive)) < 0) {
        PRINT_ERR("telnet setsockopt SO_KEEPALIVE error.\n");
    }
    if (setsockopt(clientFd, IPPROTO_TCP, TCP_KEEPIDLE, (VOID *)&keepIdle, sizeof(keepIdle)) < 0) {
        PRINT_ERR("telnet setsockopt TCP_KEEPIDLE error.\n");
    }
    if (setsockopt(clientFd, IPPROTO_TCP, TCP_KEEPINTVL, (VOID *)&keepInterval, sizeof(keepInterval)) < 0) {
        PRINT_ERR("telnet setsockopt TCP_KEEPINTVL error.\n");
    }
    if (setsockopt(clientFd, IPPROTO_TCP, TCP_KEEPCNT, (VOID *)&keepCnt, sizeof(keepCnt)) < 0) {
        PRINT_ERR("telnet setsockopt TCP_KEEPCNT error.\n");
    }
    return 0;  // 成功返回
}

/**
 * @brief Telnet客户端处理循环
 * @param arg 客户端文件描述符
 * @return NULL
 */
STATIC VOID *TelnetClientLoop(VOID *arg)
{
    struct pollfd pollFd;  // poll文件描述符结构
    INT32 ret;  // 返回值
    INT32 nRead;  // 读取字节数
    UINT32 len;  // 过滤后数据长度
    UINT8 buf[TELNET_CLIENT_READ_BUF_SIZE];  // 接收缓冲区
    UINT8 *cmdBuf = NULL;  // 过滤后命令缓冲区
    INT32 clientFd = (INT32)(UINTPTR)arg;  // 客户端FD

    (VOID)prctl(PR_SET_NAME, "TelnetClientLoop", 0, 0, 0);  // 设置线程名
    TelnetLock();
    if (TelnetClientPrepare(clientFd) != 0) {  // 准备客户端连接
        TelnetUnlock();
        (VOID)close(clientFd);  // 关闭客户端FD
        return NULL;
    }
    TelnetUnlock();

    while (1) {  // 客户端处理主循环
        pollFd.fd = clientFd;  // 客户端FD
        pollFd.events = POLLIN | POLLRDHUP;  // 监听读事件和挂断事件
        pollFd.revents = 0;  // 重置事件

        ret = poll(&pollFd, 1, TELNET_CLIENT_POLL_TIMEOUT);  // 等待事件
        if (ret < 0) {  // poll出错
            break;
        }
        if (ret == 0) {  // 超时，继续等待
            continue;
        }
        /* 连接重置，可能是保活失败或对方重置连接 */
        if ((UINT16)pollFd.revents & (POLLERR | POLLHUP | POLLRDHUP)) {
            break;  // 退出循环
        }

        if ((UINT16)pollFd.revents & POLLIN) {  // 有数据可读
            nRead = read(clientFd, buf, sizeof(buf));  // 读取数据
            if (nRead <= 0) {  // 读取失败或连接关闭
                break;  // 退出循环
            }
            cmdBuf = ReadFilter(buf, (UINT32)nRead, &len);  // 过滤IAC命令
            if (len > 0) {  // 有有效数据
                (VOID)TelnetTx((CHAR *)cmdBuf, len);  // 发送数据
            }
        }
    }
    TelnetLock();
    TelnetClientClose();  // 关闭客户端连接
    (VOID)close(clientFd);  // 关闭FD
    clientFd = -1;
    g_telnetClientFd = -1;  // 重置客户端FD
    TelnetUnlock();
    return NULL;
}

/**
 * @brief 设置Telnet客户端线程属性
 * @param threadAttr 线程属性指针
 */
STATIC VOID TelnetClientTaskAttr(pthread_attr_t *threadAttr)
{
    (VOID)pthread_attr_init(threadAttr);  // 初始化线程属性
    threadAttr->inheritsched = PTHREAD_EXPLICIT_SCHED;  // 使用显式调度策略
    threadAttr->schedparam.sched_priority = TELNET_TASK_PRIORITY;  // 设置优先级
    threadAttr->detachstate = PTHREAD_CREATE_DETACHED;  // 设置为分离线程
    (VOID)pthread_attr_setstacksize(threadAttr, TELNET_TASK_STACK_SIZE);  // 设置栈大小
}

/**
 * @brief 处理客户端连接请求
 * @param clientFd 客户端文件描述符
 * @param inTelnetAddr 客户端地址结构
 * @return 0表示继续接受连接，-1表示停止接受连接
 */
STATIC INT32 TelnetdAcceptClient(INT32 clientFd, const struct sockaddr_in *inTelnetAddr)
{
    INT32 ret = 0;  // 返回值
    pthread_t tmp;  // 线程ID
    pthread_attr_t useAttr;  // 线程属性

    TelnetClientTaskAttr(&useAttr);  // 设置线程属性

    if (clientFd < 0) {  // 客户端FD无效
        ret = -1;
        goto ERROUT;  // 跳转至错误处理
    }

    TelnetLock();

    if (g_telnetListenFd == -1) {  // 服务器已关闭
        ret = -1;
        goto ERROUT_UNLOCK;  // 跳转至解锁
    }

    if (g_telnetClientFd >= 0) {  // 已有客户端连接(仅支持单连接)
        goto ERROUT_UNLOCK;  // 跳转至解锁
    }

    g_telnetClientFd = clientFd;  // 保存客户端FD

    // 创建客户端处理线程
    if (pthread_create(&tmp, &useAttr, TelnetClientLoop, (VOID *)(UINTPTR)clientFd) != 0) {
        PRINT_ERR("Failed to create client handle task\n");
        g_telnetClientFd = -1;  // 重置客户端FD
        goto ERROUT_UNLOCK;
    }
    TelnetUnlock();
    return ret;

ERROUT_UNLOCK:
    (VOID)close(clientFd);  // 关闭客户端FD
    clientFd = -1;
    TelnetUnlock();
ERROUT:
    return ret;
}

/**
 * @brief 等待客户端连接(仅允许一个连接，其他连接将被拒绝)
 * @param listenFd 监听文件描述符
 */
STATIC VOID TelnetdAcceptLoop(INT32 listenFd)
{
    INT32 clientFd = -1;  // 客户端FD
    struct sockaddr_in inTelnetAddr;  // 客户端地址
    INT32 len = sizeof(inTelnetAddr);  // 地址长度

    TelnetLock();
    g_telnetListenFd = listenFd;  // 保存监听FD

    while (g_telnetListenFd >= 0) {  // 循环接受连接
        TelnetUnlock();

        (VOID)memset_s(&inTelnetAddr, sizeof(inTelnetAddr), 0, sizeof(inTelnetAddr));
        // 接受客户端连接
        clientFd = accept(listenFd, (struct sockaddr *)&inTelnetAddr, (socklen_t *)&len);
        if (TelnetdAcceptClient(clientFd, &inTelnetAddr) == 0) {  // 处理客户端连接
            /*
             * 循环前休眠一段时间：通常已有一个连接，后续连接将被拒绝
             * 避免CPU空转
             */
            LOS_Msleep(TELNET_ACCEPT_INTERVAL);
        } else {
            return;  // 停止接受连接
        }
        TelnetLock();
    }
    TelnetUnlock();
}

/**
 * @brief Telnet服务器主函数
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 TelnetdMain(VOID)
{
    INT32 sock;  // 监听套接字
    INT32 ret;  // 返回值

    sock = TelnetdInit(TELNETD_PORT);  // 初始化服务器
    if (sock == -1) {
        PRINT_ERR("telnet init error.\n");
        return -1;
    }

    TelnetLock();
    ret = TelnetedRegister();  // 注册Telnet服务
    TelnetUnlock();

    if (ret != 0) {  // 注册失败
        PRINT_ERR("telnet register error.\n");
        (VOID)close(sock);  // 关闭套接字
        return -1;
    }

    PRINTK("start telnet server successfully, waiting for connection.\n");  // 打印启动信息
    TelnetdAcceptLoop(sock);  // 进入接受连接循环
    return 0;
}

/**
 * @brief 创建Telnet服务器任务
 */
STATIC VOID TelnetdTaskInit(VOID)
{
    UINT32 ret;  // 返回值
    TSK_INIT_PARAM_S initParam = {0};  // 任务初始化参数

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)TelnetdMain;  // 任务入口函数
    initParam.uwStackSize = TELNET_TASK_STACK_SIZE;  // 栈大小
    initParam.pcName = "TelnetServer";  // 任务名
    initParam.usTaskPrio = TELNET_TASK_PRIORITY;  // 优先级
    initParam.uwResved = LOS_TASK_STATUS_DETACHED;  // 分离状态

    if (atomic_read(&g_telnetTaskId) != 0) {  // 检查服务器是否已运行
        PRINT_ERR("telnet server is already running!\n");
        return;
    }

    ret = LOS_TaskCreate((UINT32 *)&g_telnetTaskId, &initParam);  // 创建任务
    if (ret != LOS_OK) {
        PRINT_ERR("failed to create telnet server task!\n");
    }
}

/**
 * @brief 销毁Telnet服务器任务
 */
STATIC VOID TelnetdTaskDeinit(VOID)
{
    if (atomic_read(&g_telnetTaskId) == 0) {  // 检查服务器是否未运行
        PRINTK("telnet server is not running, please start up telnet server first.\n");
        return;
    }

    TelnetLock();
    TelnetClientClose();  // 关闭客户端
    TelnetdDeinit();  // 停止服务器
    TelnetUnlock();
}

/**
 * @brief 打印Telnet命令用法
 */
STATIC VOID TelnetUsage(VOID)
{
    PRINTK("Usage: telnet [OPTION]...\n");
    PRINTK("Start or close telnet server.\n\n");
    PRINTK("  on       Init the telnet server\n");
    PRINTK("  off      Deinit the telnet server\n");
}

/**
 * @brief Telnet命令处理函数
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 成功返回0
 */
INT32 TelnetCmd(UINT32 argc, const CHAR **argv)
{
    if (argc != 1) {  // 参数个数错误
        TelnetUsage();  // 打印用法
        return 0;
    }

    if (strcmp(argv[0], "on") == 0) {  // 启动服务器
        /* telnet on: 尝试启动telnet服务器任务 */
        TelnetdTaskInit();
        return 0;
    }
    if (strcmp(argv[0], "off") == 0) {  // 停止服务器
        /* telnet off: 尝试停止客户端，然后停止服务器任务 */
        TelnetdTaskDeinit();
        return 0;
    }

    TelnetUsage();  // 参数无效，打印用法
    return 0;
}
#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(telnet_shellcmd, CMD_TYPE_EX, "telnet", 1, (CmdCallBackFunc)TelnetCmd);
#endif /* LOSCFG_SHELL_CMD_DEBUG */
#endif

