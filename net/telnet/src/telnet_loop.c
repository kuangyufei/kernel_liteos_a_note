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
#define TELNET_SB   250 /* Indicates that what follows is subnegotiation of the indicated option 
							 | 表示后面所跟的是对需要的选项的子谈判*/
#define TELNET_WILL 251 /* Indicates the desire to perform the indicated option 
							 | 表示希望执行指定的选项*/
#define TELNET_DO   253 /* Indicates the request for the other party to perform the indicated option 
							 |  表示一方要求另一方使用，或者确认你希望另一方使用指定的选项。*/
#define TELNET_IAC  255 /* Interpret as Command | 中断命令*/

/* telnet options in IANA */
#define TELNET_ECHO 1    /* Echo | 回显*/
#define TELNET_SGA  3    /* Suppress Go Ahead | 抑制继续进行*/
#define TELNET_NAWS 31   /* Negotiate About Window Size | 窗口大小*/
#define TELNET_NOP  0xf1 /* Unassigned in IANA, putty use this to keepalive */

#define LEN_IAC_CMD      2 /* Only 2 char: |IAC|cmd| */
#define LEN_IAC_CMD_OPT  3 /* Only 3 char: |IAC|cmd|option| */
#define LEN_IAC_CMD_NAWS 9 /* NAWS: |IAC|SB|NAWS|x1|x2|x3|x4|IAC|SE| */

/* server/client settings */
#define TELNET_TASK_STACK_SIZE  0x2000	///< 内核栈大小 8K
#define TELNET_TASK_PRIORITY    9	///< telnet 任务优先级

/* server settings | 服务端设置*/
#define TELNET_LISTEN_BACKLOG   128
#define TELNET_ACCEPT_INTERVAL  200

/* client settings  | 客户端设置*/
#define TELNET_CLIENT_POLL_TIMEOUT  2000	//一次客户端链接的 超时时间,
#define TELNET_CLIENT_READ_BUF_SIZE 256		//读buf大小
#define TELNET_CLIENT_READ_FILTER_BUF_SIZE (8 * 1024) ///< buf大小

/* limitation: only support 1 telnet client connection */
STATIC volatile INT32 g_telnetClientFd = -1;  /* client fd | 客户端文件句柄,仅支持一个客户端链接 */

/* limitation: only support 1 telnet server */
STATIC volatile INT32 g_telnetListenFd = -1;  /* listen fd of telnetd | 服务端文件句柄,仅支持一个服务端*/

/* each bit for a client connection, although only support 1 connection for now */
STATIC volatile UINT32 g_telnetMask = 0; //记录有任务打开了远程登录
/* taskID of telnetd */
STATIC atomic_t g_telnetTaskId = 0;	///< telnet 服务端任务ID
/* protect listenFd, clientFd etc. */
pthread_mutex_t g_telnetMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

VOID TelnetLock(VOID)
{
    (VOID)pthread_mutex_lock(&g_telnetMutex);
}

VOID TelnetUnlock(VOID)
{
    (VOID)pthread_mutex_unlock(&g_telnetMutex);
}

/* filter out iacs from client stream | 从客户端流中过滤*/
STATIC UINT8 *ReadFilter(const UINT8 *src, UINT32 srcLen, UINT32 *dstLen)
{
    STATIC UINT8 buf[TELNET_CLIENT_READ_FILTER_BUF_SIZE];
    UINT8 *dst = buf;
    UINT32 left = srcLen;

    while (left > 0) {
        if (*src != TELNET_IAC) {
            *dst = *src;
            dst++;
            src++;
            left--;
            continue;
        }

        /*
         * if starting with IAC, filter out IAC as following
         * |IAC|          --> skip
         * |IAC|NOP|...   --> ... : skip for putty keepalive etc.
         * |IAC|x|        --> skip
         * |IAC|IAC|x|... --> |IAC|... : skip for literal cmds
         * |IAC|SB|NAWS|x1|x2|x3|x4|IAC|SE|... --> ... : skip NAWS(unsupported)
         * |IAC|x|x|...   --> ... : skip unsupported IAC
         */
        if (left == 1) {
            break;
        }
        /* left no less than 2 */
        if (*(src + 1) == TELNET_NOP) {
            src += LEN_IAC_CMD;
            left -= LEN_IAC_CMD;
            continue;
        }
        if (left == LEN_IAC_CMD) {
            break;
        }
        /* left no less than 3 */
        if (*(src + 1) == TELNET_IAC) {
            *dst = TELNET_IAC;
            dst++;
            src += LEN_IAC_CMD;
            left -= LEN_IAC_CMD;
            continue;
        }
        if ((*(src + 1) == TELNET_SB) && (*(src + LEN_IAC_CMD) == TELNET_NAWS)) {
            if (left > LEN_IAC_CMD_NAWS) {
                src += LEN_IAC_CMD_NAWS;
                left -= LEN_IAC_CMD_NAWS;
                continue;
            }
            break;
        }
        src += LEN_IAC_CMD_OPT;
        left -= LEN_IAC_CMD_OPT;
    }

    if (dstLen != NULL) {
        *dstLen = dst - buf;
    }
    return buf;
}

/*
 * Description : Write data to fd.
 * Input       : fd     --- the fd to write.
 *             : src    --- data pointer.
 *             : srcLen --- data length.
 * Return      : length of written data.
 */
STATIC ssize_t WriteToFd(INT32 fd, const CHAR *src, size_t srcLen)
{
    size_t sizeLeft;
    ssize_t sizeWritten;

    sizeLeft = srcLen;
    while (sizeLeft > 0) {
        sizeWritten = write(fd, src, sizeLeft);
        if (sizeWritten < 0) {
            /* last write failed */
            if (sizeLeft == srcLen) {
                /* nothing was written in any loop */
                return -1;
            } else {
                /* something was written in previous loop */
                break;
            }
        } else if (sizeWritten == 0) {
            break;
        }
        sizeLeft -= (size_t)sizeWritten;
        src += sizeWritten;
    }

    return (ssize_t)(srcLen - sizeLeft);
}

/* Try to remove the client device if there is any client connection | 如果有任务在远程登录，尝试删除它*/
STATIC VOID TelnetClientClose(VOID)
{
    /* check if there is any client connection */
    if (g_telnetMask == 0) {//没有任务在远程链接,
        return;
    }
    (VOID)TelnetDevDeinit();
    g_telnetMask = 0;
    printf("telnet client disconnected.\n");
}

/* Release the client and server fd */
STATIC VOID TelnetRelease(VOID)
{
    if (g_telnetClientFd >= 0) {
        (VOID)close(g_telnetClientFd);
        g_telnetClientFd = -1;
    }

    if (g_telnetListenFd >= 0) {
        (VOID)close(g_telnetListenFd);
        g_telnetListenFd = -1;
    }
}

/* Stop telnet server */
STATIC VOID TelnetdDeinit(VOID)
{
    if (atomic_read(&g_telnetTaskId) == 0) {
        PRINTK("telnet server is not running!\n");
        return;
    }

    TelnetRelease();
    (VOID)TelnetedUnregister();
    atomic_set(&g_telnetTaskId, 0);
    PRINTK("telnet server closed.\n");
}

/*
 * Description : Setup the listen fd for telnetd with specific port.
 * Input       : port --- the port at which telnet server listens.
 * Return      : -1           --- on error
 *             : non-negative --- listen fd if OK
 */
STATIC INT32 TelnetdInit(UINT16 port)
{
    INT32 listenFd;
    INT32 reuseAddr = 1;
    struct sockaddr_in inTelnetAddr;
	/**
		ai_family参数指定调用者期待返回的套接口地址结构的类型。它的值包括三种：AF_INET，AF_INET6和AF_UNSPEC
			AF_INET:	不能返回任何IPV6相关的地址信息
			AF_INET6:	不能返回任何IPV4地址信息
			AF_UNSPEC: 	返回的是适用于指定主机名和服务名且适合任何协议族的地址。如果某个主机既有AAAA记录(IPV6)地址，
					同时又有A记录(IPV4)地址，那么AAAA记录将作为sockaddr_in6结构返回，而A记录则作为sockaddr_in结构返回
	*/
	//SOCK_STREAM 表示TCP协议
    listenFd = socket(AF_INET, SOCK_STREAM, 0);//打开一个socker fd 
    if (listenFd == -1) {
        PRINT_ERR("TelnetdInit : socket error.\n");
        goto ERR_OUT;
    }

    /* reuse listen port */
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) != 0) {
        PRINT_ERR("TelnetdInit : setsockopt REUSEADDR error.\n");
        goto ERR_CLOSE_FD;
    }

    (VOID)memset_s(&inTelnetAddr, sizeof(struct sockaddr_in), 0, sizeof(struct sockaddr_in));
    inTelnetAddr.sin_family = AF_INET;
    inTelnetAddr.sin_addr.s_addr = INADDR_ANY;
    inTelnetAddr.sin_port = htons(port);
	//绑定端口
    if (bind(listenFd, (const struct sockaddr *)&inTelnetAddr, sizeof(struct sockaddr_in)) == -1) {
        PRINT_ERR("TelnetdInit : bind error.\n");
        goto ERR_CLOSE_FD;
    }
	//监听端口数据
    if (listen(listenFd, TELNET_LISTEN_BACKLOG) == -1) {
        PRINT_ERR("TelnetdInit : listen error.\n");
        goto ERR_CLOSE_FD;
    }

    return listenFd;

ERR_CLOSE_FD:
    (VOID)close(listenFd);
ERR_OUT:
    return -1;
}
/// 远程登录客户端准备阶段
STATIC INT32 TelnetClientPrepare(INT32 clientFd)
{
    INT32 keepAlive = TELNET_KEEPALIVE;
    INT32 keepIdle = TELNET_KEEPIDLE;
    INT32 keepInterval = TELNET_KEEPINTV;
    INT32 keepCnt = TELNET_KEEPCNT;
    const UINT8 doEcho[] = { TELNET_IAC, TELNET_DO, TELNET_ECHO };
    const UINT8 doNaws[] = { TELNET_IAC, TELNET_DO, TELNET_NAWS };
    const UINT8 willEcho[] = { TELNET_IAC, TELNET_WILL, TELNET_ECHO };
    const UINT8 willSga[] = { TELNET_IAC, TELNET_WILL, TELNET_SGA };

    if (g_telnetListenFd == -1) {
        return -1;
    }
    g_telnetClientFd = clientFd;
    if (TelnetDevInit(clientFd) != 0) {//远程登录设备初始化
        g_telnetClientFd = -1;
        return -1;
    }
    g_telnetMask = 1;//表示有任务在远程登录

    /* negotiate with client | 与客户协商*/
    (VOID)WriteToFd(clientFd, (CHAR *)doEcho, sizeof(doEcho));
    (VOID)WriteToFd(clientFd, (CHAR *)doNaws, sizeof(doNaws));
    (VOID)WriteToFd(clientFd, (CHAR *)willEcho, sizeof(willEcho));
    (VOID)WriteToFd(clientFd, (CHAR *)willSga, sizeof(willSga));

    /* enable TCP keepalive to check whether telnet link is alive | 设置保持连接的方式 */
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
    return 0;
}

/*!
 * @brief TelnetClientLoop	处理远程客户端的请求任务的入口函数
 *
 * @param arg	
 * @return	
 *
 * @see
 */
STATIC VOID *TelnetClientLoop(VOID *arg)
{
    struct pollfd pollFd;
    INT32 ret;
    INT32 nRead;
    UINT32 len;
    UINT8 buf[TELNET_CLIENT_READ_BUF_SIZE];
    UINT8 *cmdBuf = NULL;
    INT32 clientFd = (INT32)(UINTPTR)arg;

    (VOID)prctl(PR_SET_NAME, "TelnetClientLoop", 0, 0, 0);
    TelnetLock();
    if (TelnetClientPrepare(clientFd) != 0) {//做好准备工作
        TelnetUnlock();
        (VOID)close(clientFd);
        return NULL;
    }
    TelnetUnlock();

    while (1) {//死循环接受远程输入的数据
        pollFd.fd = clientFd;
        pollFd.events = POLLIN | POLLRDHUP;//监听读数据和挂起事件
        pollFd.revents = 0;
		/*
		POLLIN 普通或优先级带数据可读
		POLLRDNORM 普通数据可读
		POLLRDBAND 优先级带数据可读
		POLLPRI 高优先级数据可读
		POLLOUT 普通数据可写
		POLLWRNORM 普通数据可写
		POLLWRBAND 优先级带数据可写
		POLLERR 发生错误
		POLLHUP 发生挂起
		POLLNVAL 描述字不是一个打开的文件
		poll本质上和select没有区别，它将用户传入的数组拷贝到内核空间，然后查询每个fd对应的设备状态，
		如果设备就绪则在设备等待队列中加入一项并继续遍历，如果遍历完所有fd后没有发现就绪设备，则挂起当前进程，
		直到设备就绪或者主动超时，被唤醒后它又要再次遍历fd。
	　　这个过程经历了多次无谓的遍历。
	　　poll还有一个特点是“水平触发”，如果报告了fd后，没有被处理，那么下次poll时会再次报告该fd。
	　　poll与select的不同，通过一个pollfd数组向内核传递需要关注的事件，故没有描述符个数的限制，
			pollfd中的events字段和revents分别用于标示关注的事件和发生的事件，故pollfd数组只需要被初始化一次
	　　poll的实现机制与select类似，其对应内核中的sys_poll，只不过poll向内核传递pollfd数组，
			然后对pollfd中的每个描述符进行poll，相比处理fdset来说，poll效率更高。poll返回后，
			需要对pollfd中的每个元素检查其revents值，来得指事件是否发生。
			优点
			1）poll() 不要求开发者计算最大文件描述符加一的大小。
			2）poll() 在应付大数目的文件描述符的时候速度更快，相比于select。
			3）它没有最大连接数的限制，原因是它是基于链表来存储的。
			缺点
			1）大量的fd的数组被整体复制于用户态和内核地址空间之间，而不管这样的复制是不是有意义。
			2）与select一样，poll返回后，需要轮询pollfd来获取就绪的描述符
		*/
        ret = poll(&pollFd, 1, TELNET_CLIENT_POLL_TIMEOUT);//等2秒钟返回
        if (ret < 0) {//失败时，poll()返回-1
            break;
			/*	ret < 0 各值
			　　EBADF　　		一个或多个结构体中指定的文件描述符无效。
			　　EFAULTfds　　 指针指向的地址超出进程的地址空间。
			　　EINTR　　　　  请求的事件之前产生一个信号，调用可以重新发起。
			　　EINVALnfds　　参数超出PLIMIT_NOFILE值。
			　　ENOMEM　　	   可用内存不足，无法完成请求

			*/
        }
        if (ret == 0) {//如果在超时前没有任何事件发生，poll()返回0
            continue;
        }
        /* connection reset, maybe keepalive failed or reset by peer | 连接重置，可能keepalive失败或被peer重置*/
        if ((UINT16)pollFd.revents & (POLLERR | POLLHUP | POLLRDHUP)) {
            break;
        }

        if ((UINT16)pollFd.revents & POLLIN) {//数据事件
            nRead = read(clientFd, buf, sizeof(buf));//读远程终端过来的数据
            if (nRead <= 0) {
                /* telnet client shutdown */
                break;
            }
            cmdBuf = ReadFilter(buf, (UINT32)nRead, &len);//对数据过滤
            if (len > 0) {
                (VOID)TelnetTx((CHAR *)cmdBuf, len);//对数据加工处理
            }
        }
    }
    TelnetLock();
    TelnetClientClose();
    (VOID)close(clientFd);
    clientFd = -1;
    g_telnetClientFd = -1;
    TelnetUnlock();
    return NULL;
}

STATIC VOID TelnetClientTaskAttr(pthread_attr_t *threadAttr)
{
    (VOID)pthread_attr_init(threadAttr);//初始化线程属性
    threadAttr->inheritsched = PTHREAD_EXPLICIT_SCHED;//
    threadAttr->schedparam.sched_priority = TELNET_TASK_PRIORITY;//线程优先级
    threadAttr->detachstate = PTHREAD_CREATE_DETACHED;//任务分离模式
    (VOID)pthread_attr_setstacksize(threadAttr, TELNET_TASK_STACK_SIZE);//设置任务内核栈大小
}

/*
 * Description : Handle the client connection request. | 处理客户端连接请求
 *               Create a client connection if permitted. | 如果允许，创建客户端连接
 * Return      : 0  --- please continue the server accept loop
 *             : -1 --- please stop the server accept loop.
 */
STATIC INT32 TelnetdAcceptClient(INT32 clientFd, const struct sockaddr_in *inTelnetAddr)
{
    INT32 ret = 0;
    pthread_t tmp;
    pthread_attr_t useAttr;

    TelnetClientTaskAttr(&useAttr);

    if (clientFd < 0) {
        ret = -1;
        goto ERROUT;
    }

    TelnetLock();

    if (g_telnetListenFd == -1) {
        /* server already has been closed, so quit this task now */
        ret = -1;
        goto ERROUT_UNLOCK;
    }

    if (g_telnetClientFd >= 0) { //只接收一个客户端
        /* alreay connected and support only one */
        goto ERROUT_UNLOCK;
    }

    g_telnetClientFd = clientFd;
	//创建一个线程处理客户端的请求
    if (pthread_create(&tmp, &useAttr, TelnetClientLoop, (VOID *)(UINTPTR)clientFd) != 0) {
        PRINT_ERR("Failed to create client handle task\n");
        g_telnetClientFd = -1;
        goto ERROUT_UNLOCK;
    }
    TelnetUnlock();
    return ret;

ERROUT_UNLOCK:
    (VOID)close(clientFd);
    clientFd = -1;
    TelnetUnlock();
ERROUT:
    return ret;
}

/*
 * Waiting for client's connection. Only allow 1 connection, and others will be discarded.
 * | 等待客户端的连接。 只允许1个连接，其他将被丢弃
 */
STATIC VOID TelnetdAcceptLoop(INT32 listenFd)
{
    INT32 clientFd = -1;
    struct sockaddr_in inTelnetAddr;
    INT32 len = sizeof(inTelnetAddr);

    TelnetLock();
    g_telnetListenFd = listenFd;

    while (g_telnetListenFd >= 0) {//必须启动监听
        TelnetUnlock();

        (VOID)memset_s(&inTelnetAddr, sizeof(inTelnetAddr), 0, sizeof(inTelnetAddr));
        clientFd = accept(listenFd, (struct sockaddr *)&inTelnetAddr, (socklen_t *)&len);//接收数据
        if (TelnetdAcceptClient(clientFd, &inTelnetAddr) == 0) {//
            /*
             * Sleep sometime before next loop: mostly we already have one connection here,
             * and the next connection will be declined. So don't waste our cpu.
             | 在下一个循环来临之前休息片刻,因为鸿蒙只支持一个远程登录,此时已经有一个链接,
             在TelnetdAcceptClient中创建线程不会立即调度, 休息下任务会挂起,重新调度
             */
            LOS_Msleep(TELNET_ACCEPT_INTERVAL);//以休息的方式发起调度. 直接申请调度也未尝不可吧 @note_thinking 
        } else {
            return;
        }
        TelnetLock();
    }
    TelnetUnlock();
}

/*!
 * @brief TelnetdMain
 @verbatim
 	telnet启动要确保网络驱动及网络协议栈已经初始化完成，且板子的网卡是link up状态。
	暂时无法支持多个客户端（telnet + IP）同时连接开发板。
 	输入 telnet on
		OHOS # telnet on
		OHOS # start telnet server successfully, waiting for connection.
 @endverbatim
 * @return	
 *
 * @see
 */
STATIC INT32 TelnetdMain(VOID)
{
    INT32 sock;
    INT32 ret;

    sock = TelnetdInit(TELNETD_PORT);//1.初始化 创建 socket ,socket的本质就是打开了一个虚拟文件
    if (sock == -1) {
        PRINT_ERR("telnet init error.\n");
        return -1;
    }

    TelnetLock();
    ret = TelnetedRegister();//2.注册驱动程序 /dev/telnet ,g_telnetOps g_telnetDev
    TelnetUnlock();

    if (ret != 0) {
        PRINT_ERR("telnet register error.\n");
        (VOID)close(sock);
        return -1;
    }
	
    PRINTK("start telnet server successfully, waiting for connection.\n");
    TelnetdAcceptLoop(sock);//3.等待连接,处理远程终端过来的命令 例如#task 命令
    return 0;
}

/*
 * Try to create telnetd task.
 */
/*!
 * @brief TelnetdTaskInit 创建 telnet 服务端任务	
 * \n 1. telnet启动要确保网络驱动及网络协议栈已经初始化完成，且板子的网卡是link up状态。
 * \n 2. 暂时无法支持多个客户端（telnet + IP）同时连接开发板。
 		\n 须知： telnet属于调测功能，默认配置为关闭，正式产品中禁止使用该功能。
 * @return	
 *
 * @see
 */
STATIC VOID TelnetdTaskInit(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S initParam = {0};

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)TelnetdMain; // telnet任务入口函数
    initParam.uwStackSize = TELNET_TASK_STACK_SIZE;	// 8K
    initParam.pcName = "TelnetServer"; //任务名称
    initParam.usTaskPrio = TELNET_TASK_PRIORITY; //优先级 9,和 shell 的优先级一样
    initParam.uwResved = LOS_TASK_STATUS_DETACHED; //独立模式

    if (atomic_read(&g_telnetTaskId) != 0) {//只支持一个 telnet 服务任务
        PRINT_ERR("telnet server is already running!\n");
        return;
    }

    ret = LOS_TaskCreate((UINT32 *)&g_telnetTaskId, &initParam);//创建远程登录服务端任务并发起调度
    if (ret != LOS_OK) {
        PRINT_ERR("failed to create telnet server task!\n");
    }
}

/*
 * Try to destroy telnetd task. | 销毁,删除远程登录任务
 */
STATIC VOID TelnetdTaskDeinit(VOID)
{
    if (atomic_read(&g_telnetTaskId) == 0) {
        PRINTK("telnet server is not running, please start up telnet server first.\n");
        return;
    }

    TelnetLock();
    TelnetClientClose();
    TelnetdDeinit();
    TelnetUnlock();
}
/// 远程登录用法 telnet [on | off]
STATIC VOID TelnetUsage(VOID)
{
    PRINTK("Usage: telnet [OPTION]...\n");
    PRINTK("Start or close telnet server.\n\n");
    PRINTK("  on       Init the telnet server\n");
    PRINTK("  off      Deinit the telnet server\n");
}
/// 本命令用于启动或关闭telnet server服务
INT32 TelnetCmd(UINT32 argc, const CHAR **argv)
{
    if (argc != 1) {
        TelnetUsage();
        return 0;
    }

    if (strcmp(argv[0], "on") == 0) {// 输入 telnet on
        /* telnet on: try to start telnet server task */
        TelnetdTaskInit(); //启动远程登录 服务端任务
        return 0;
    }
    if (strcmp(argv[0], "off") == 0) {// 输入 telnet off
        /* telnet off: try to stop clients, then stop server task */
        TelnetdTaskDeinit();//关闭所有的客户端,并关闭服务端任务
        return 0;
    }

    TelnetUsage();
    return 0;
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(telnet_shellcmd, CMD_TYPE_EX, "telnet", 1, (CmdCallBackFunc)TelnetCmd);/// 以静态方式注册shell 命令
#endif /* LOSCFG_SHELL_CMD_DEBUG */
#endif

