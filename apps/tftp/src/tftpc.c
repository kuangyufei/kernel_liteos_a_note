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

/* TFTP Client utility */

#include "tftpc.h"

#if LWIP_TFTP /* don't build if not configured for use in lwipopts.h */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <securec.h>

/* Function Declarations */
#ifdef LOSCFG_NET_LWIP_SACK_TFTP
static u32_t lwip_tftp_create_bind_socket(s32_t *piSocketID);

static s32_t lwip_tftp_make_tftp_packet(u16_t usOpcode, s8_t *szFileName,
                                        u32_t ulMode, TFTPC_PACKET_S *pstPacket);

static u32_t lwip_tftp_recv_from_server(s32_t iSockNum,
                                 u32_t *pulSize,
                                 TFTPC_PACKET_S *pstRecvBuf,
                                 u32_t *pulIgnorePkt,
                                 struct sockaddr_in *pstServerAddr,
                                 TFTPC_PACKET_S *pstSendBuf);

static u32_t lwip_tftp_send_to_server(s32_t iSockNum, u32_t ulSize,
                               TFTPC_PACKET_S *pstSendBuf,
                               struct sockaddr_in *pstServerAddr);

static u32_t lwip_tftp_validate_data_pkt(s32_t iSockNum,
                                  u32_t *pulSize,
                                  TFTPC_PACKET_S *pstRecvBuf,
                                  u16_t usCurrBlk, u32_t *pulResendPkt,
                                  struct sockaddr_in *pstServerAddr);

static u32_t lwip_tftp_inner_put_file(s32_t iSockNum, TFTPC_PACKET_S *pstSendBuf,
                               u32_t ulSendSize, u16_t usCurrBlk,
                               struct sockaddr_in *pstServerAddr);

static void lwip_tftp_send_error(s32_t iSockNum, u32_t ulError, const char *szErrMsg,
                                 struct sockaddr_in *pstServerAddr, TFTPC_PACKET_S *pstSendBuf);

/**
 * @brief 创建并绑定UDP socket
 * @param[out] piSocketID 输出参数，返回创建的socket描述符
 * @return 成功返回ERR_OK，失败返回错误码(TFTPC_SOCKET_FAILURE/TFTPC_IOCTLSOCKET_FAILURE/TFTPC_BIND_FAILURE)
 * @details 执行以下操作：
 *          1. 创建UDP类型socket
 *          2. 设置socket为非阻塞模式
 *          3. 绑定到本地任意地址(INADDR_ANY)和随机端口
 */
u32_t lwip_tftp_create_bind_socket(s32_t *piSocketID)
{
    int retval;                         // 系统调用返回值
    struct sockaddr_in stClientAddr;    // 客户端地址结构体
    u32_t ulTempClientIp;               // 临时客户端IP地址
    u32_t set_non_block_socket = 1;     // 非阻塞模式设置标志(1:非阻塞,0:阻塞)

    /* 创建UDP socket */
    *piSocketID = lwip_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (*piSocketID == -1) {            // socket创建失败检查
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_create_bind_socket : lwip_socket create socket failed\n"));
        return TFTPC_SOCKET_FAILURE;    // 返回socket创建失败错误码
    }

    /* 设置socket为非阻塞模式 */
    retval = lwip_ioctl(*piSocketID, (long)FIONBIO, &set_non_block_socket);
    if (retval != 0) {                  // ioctl调用失败处理
        (void)lwip_close(*piSocketID);  // 关闭已创建的socket
        *piSocketID = TFTP_NULL_INT32;  // 重置socket描述符
        return TFTPC_IOCTLSOCKET_FAILURE; // 返回ioctl失败错误码
    }

    ulTempClientIp = INADDR_ANY;        // 绑定到所有本地网络接口

    /* 初始化客户端地址结构体 */
    (void)memset_s(&stClientAddr, sizeof(stClientAddr), 0, sizeof(stClientAddr));
    stClientAddr.sin_family = AF_INET;  // IPv4地址族
    stClientAddr.sin_port = 0;          // 端口号设为0，表示由系统自动分配
    stClientAddr.sin_addr.s_addr = htonl(ulTempClientIp); // 转换为网络字节序

    /* 绑定socket到本地地址 */
    retval = lwip_bind(*piSocketID, (struct sockaddr *)&stClientAddr, sizeof(stClientAddr));
    if (retval != 0) {                  // 绑定失败处理
        (void)lwip_close(*piSocketID);  // 关闭socket
        *piSocketID = TFTP_NULL_INT32;  // 重置socket描述符
        return TFTPC_BIND_FAILURE;      // 返回绑定失败错误码
    }

    return ERR_OK;                      // 成功返回
}


/**
 * @brief 构建TFTP请求数据包
 * @param[in] usOpcode 操作码(TFTPC_OP_RRQ/TFTPC_OP_WRQ)
 * @param[in] szFileName 文件名
 * @param[in] ulMode 传输模式(TRANSFER_MODE_BINARY/TRANSFER_MODE_ASCII)
 * @param[out] pstPacket 输出参数，指向构建好的TFTP数据包
 * @return 成功返回数据包长度，失败返回错误值
 * @details TFTP请求包格式：| Opcode(2字节) | 文件名(字符串) | 0x00 | 模式(字符串) | 0x00 |
 */
static s32_t lwip_tftp_make_tftp_packet(u16_t usOpcode, s8_t *szFileName, u32_t ulMode, TFTPC_PACKET_S *pstPacket)
{
    s8_t *pcCp = NULL;  // 数据包指针，用于构建数据

    pstPacket->usOpcode = htons(usOpcode); // 设置操作码(网络字节序)
    pcCp = pstPacket->u.ucName_Mode;       // 指向文件名存储区域

    /* 拷贝文件名到数据包(带长度检查的安全拷贝) */
    (void)strncpy_s((char *)pcCp, TFTP_MAX_PATH_LENGTH, (char *)szFileName, (TFTP_MAX_PATH_LENGTH - 1));
    pcCp[(TFTP_MAX_PATH_LENGTH - 1)] = '\0'; // 确保字符串终止符

    /* 移动指针到模式字段(跳过文件名和终止符) */
    pcCp += (strlen((char *)szFileName) + 1);
    if (ulMode == TRANSFER_MODE_BINARY) {
        /* 二进制模式("octet") */
        (void)strncpy_s((char *)pcCp, TFTP_MAX_MODE_SIZE, "octet", (TFTP_MAX_MODE_SIZE - 1));
        pcCp[(TFTP_MAX_MODE_SIZE - 1)] = '\0';
    } else if (ulMode == TRANSFER_MODE_ASCII) {
        /* ASCII模式("netascii") */
        (void)strncpy_s((char *)pcCp, TFTP_MAX_MODE_SIZE, "netascii", (TFTP_MAX_MODE_SIZE - 1));
        pcCp[(TFTP_MAX_MODE_SIZE - 1)] = '\0';
    }

    /* 移动指针到数据包末尾(跳过模式和终止符) */
    pcCp += (strlen((char *)pcCp) + 1);

    /* 返回数据包总长度(指针差计算) */
    return (pcCp - (s8_t *)pstPacket);
}

/**
 * @brief 从TFTP服务器接收数据包
 * @param[in] iSockNum socket描述符
 * @param[out] pulSize 接收数据包大小
 * @param[out] pstRecvBuf 接收缓冲区
 * @param[out] pulIgnorePkt 是否忽略该数据包(1:忽略,0:处理)
 * @param[in,out] pstServerAddr 服务器地址(输入初始地址,输出实际通信端口)
 * @param[in] pstSendBuf 发送缓冲区(用于发送错误响应)
 * @return 成功返回ERR_OK，失败返回错误码
 * @details 主要功能：
 *          1. 使用select等待socket可读事件
 *          2. 接收数据包并验证长度
 *          3. 处理错误数据包并转换错误码
 *          4. 验证服务器身份和端口
 *          5. 处理首包端口协商(TFTP服务器使用随机端口响应)
 */
u32_t lwip_tftp_recv_from_server(s32_t iSockNum, u32_t *pulSize, TFTPC_PACKET_S *pstRecvBuf, u32_t *pulIgnorePkt,
                                 struct sockaddr_in *pstServerAddr, TFTPC_PACKET_S *pstSendBuf)
{
    u32_t ulError;                      // 错误码
    socklen_t slFromAddrLen;            // 源地址长度
    struct sockaddr_in stFromAddr;      // 源地址结构体
    fd_set stReadfds;                   // select读文件描述符集
    struct timeval stTimeout;           // 超时结构体
    u16_t usOpcode;                     // TFTP操作码
    s32_t iRet;                         // 系统调用返回值

    slFromAddrLen = sizeof(stFromAddr);
    stTimeout.tv_sec = TFTPC_TIMEOUT_PERIOD; // 设置超时时间(秒)
    stTimeout.tv_usec = 0;                   // 微秒部分设为0

    /* 初始化select读集合 */
    FD_ZERO(&stReadfds);
    FD_SET(iSockNum, &stReadfds);

    /* 等待socket可读事件 */
    iRet = select((s32_t)(iSockNum + 1), &stReadfds, 0, 0, &stTimeout);
    if (iRet == -1) {
        return TFTPC_SELECT_ERROR;      // select系统调用失败
    } else if (iRet == 0) {
        return TFTPC_TIMEOUT_ERROR;     // 等待超时
    }

    /* 检查socket是否可读 */
    if (!FD_ISSET(iSockNum, &stReadfds)) {
        return TFTPC_TIMEOUT_ERROR;     // socket不可读
    }

    /* 从服务器接收数据包 */
    iRet = lwip_recvfrom(iSockNum, (s8_t *)pstRecvBuf, TFTP_PKTSIZE, 0,
                         (struct sockaddr *)&stFromAddr, &slFromAddrLen);
    if (iRet <= 0) {
        return TFTPC_RECVFROM_ERROR;    // 接收失败
    }

    /* 验证数据包最小长度(至少4字节:2字节opcode+2字节其他数据) */
    if (iRet < TFTPC_FOUR) {
        /* 发送错误数据包给服务器 */
        lwip_tftp_send_error(iSockNum,
                             TFTPC_PROTOCOL_PROTO_ERROR,
                             "Packet size < min size",
                             pstServerAddr, pstSendBuf);

        return TFTPC_PKT_SIZE_ERROR;     // 数据包大小错误
    }

    /* 转换操作码为本地字节序 */
    usOpcode = ntohs(pstRecvBuf->usOpcode);
    /* 处理错误数据包 */
    if (usOpcode == TFTPC_OP_ERROR) {
        ulError = ntohs(pstRecvBuf->u.stTFTP_Err.usErrNum);

        /* 将RFC标准错误码转换为LWIP错误码 */
        /* RFC错误码定义: https://datatracker.ietf.org/doc/html/rfc1350#section-5 */
        switch (ulError) {
            case TFTPC_PROTOCOL_USER_DEFINED:
                ulError = TFTPC_ERROR_NOT_DEFINED;
                break;
            case TFTPC_PROTOCOL_FILE_NOT_FOUND:
                ulError = TFTPC_FILE_NOT_FOUND;
                break;
            case TFTPC_PROTOCOL_ACCESS_ERROR:
                ulError = TFTPC_ACCESS_ERROR;
                break;
            case TFTPC_PROTOCOL_DISK_FULL:
                ulError = TFTPC_DISK_FULL;
                break;
            case TFTPC_PROTOCOL_PROTO_ERROR:
                ulError = TFTPC_PROTO_ERROR;
                break;
            case TFTPC_PROTOCOL_UNKNOWN_TRANSFER_ID:
                ulError = TFTPC_UNKNOWN_TRANSFER_ID;
                break;
            case TFTPC_PROTOCOL_FILE_EXISTS:
                ulError = TFTPC_FILE_EXISTS;
                break;
            case TFTPC_PROTOCOL_CANNOT_RESOLVE_HOSTNAME:
                ulError = TFTPC_CANNOT_RESOLVE_HOSTNAME;
                break;
            default:
                ulError = TFTPC_ERROR_NOT_DEFINED;
                break;
        }

        /* 确保错误消息以终止符结束 */
        pstRecvBuf->u.stTFTP_Err.ucErrMesg[TFTP_MAXERRSTRSIZE - 1] = '\0';

        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_recv_from_server : ERROR pkt received: %s\n",
            pstRecvBuf->u.stTFTP_Err.ucErrMesg));

        return ulError;                  // 返回转换后的错误码
    }

    /* 保存接收数据包大小 */
    *pulSize = (u32_t)iRet;

    /*
     * 处理首包端口协商(TFTP协议特性):
     * 1. 服务器使用随机端口响应客户端初始请求(69端口)
     * 2. 客户端需使用该随机端口进行后续通信
     */
    // 场景1: 下载操作的第一个数据块(块号1)
    // 场景2: 上传操作的写请求确认(块号0)
    if (((usOpcode == TFTPC_OP_DATA) &&
         (ntohs(pstRecvBuf->u.stTFTP_Data.usBlknum) == 1)) ||
        ((usOpcode == TFTPC_OP_ACK) &&
         (ntohs(pstRecvBuf->u.usBlknum) == 0))) {
        /* 验证服务器IP地址是否匹配 */
        if (stFromAddr.sin_addr.s_addr == pstServerAddr->sin_addr.s_addr) {
            /* 更新服务器端口为实际通信端口 */
            pstServerAddr->sin_port = stFromAddr.sin_port;
        } else {
            /* 来自未知服务器的数据包 */
            LWIP_DEBUGF(TFTP_DEBUG,
                        ("lwip_tftp_recv_from_server : Received 1st packet from wrong Server or unknown server\n"));

            *pulIgnorePkt = 1;           // 设置忽略标志
        }
    } else {
        /* 验证后续数据包来源(IP和端口必须匹配) */
        if ((stFromAddr.sin_addr.s_addr != pstServerAddr->sin_addr.s_addr) ||
            (pstServerAddr->sin_port != stFromAddr.sin_port)) {
            /* 来自错误服务器或端口的数据包 */
            LWIP_DEBUGF(TFTP_DEBUG,
                        ("lwip_tftp_recv_from_server : Received a packet from wrong Server or unknown server\n"));

            *pulIgnorePkt = 1;           // 设置忽略标志
        }
    }

    return ERR_OK;                      // 成功接收并处理数据包
}

/**
 * @brief 向服务器发送TFTP数据包
 * @param iSockNum 套接字描述符
 * @param ulSize 要发送的数据包大小（字节）
 * @param pstSendBuf 指向要发送的TFTP数据包结构体的指针
 * @param pstServerAddr 指向服务器地址结构体的指针
 * @return 成功返回ERR_OK，失败返回TFTPC_SENDTO_ERROR
 */
u32_t lwip_tftp_send_to_server(s32_t iSockNum,
                               u32_t ulSize,
                               TFTPC_PACKET_S *pstSendBuf,
                               struct sockaddr_in *pstServerAddr)
{
    s32_t iRet;

    /* 调用lwip_sendto发送数据包到服务器 */
    iRet = lwip_sendto(iSockNum, (s8_t *)pstSendBuf,
                       (size_t)ulSize, 0,
                       (struct sockaddr *)pstServerAddr,
                       sizeof(struct sockaddr_in));
    /* 检查发送是否成功：发送字节数与请求发送字节数不匹配 */
    if ((iRet == TFTP_NULL_INT32) || ((u32_t)iRet != ulSize)) {
        return TFTPC_SENDTO_ERROR;  /* 返回发送错误码 */
    }

    return ERR_OK;  /* 发送成功 */
}

/**
 * @brief 验证接收到的TFTP数据数据包
 * @param[in] iSockNum 套接字描述符
 * @param[in,out] pulSize 指向接收数据包大小的指针，入参为预期大小，出参为实际接收大小
 * @param[in] pstRecvBuf 指向接收缓冲区的指针
 * @param[in] usCurrBlk 当前期望接收的块编号
 * @param[out] pulResendPkt 指向重发标志的指针，1表示需要重发上一个ACK包
 * @param[in] pstServerAddr 指向服务器地址结构体的指针
 * @return 成功返回ERR_OK，失败返回相应错误码
 */
u32_t lwip_tftp_validate_data_pkt(s32_t iSockNum,
                                  u32_t *pulSize,
                                  TFTPC_PACKET_S *pstRecvBuf,
                                  u16_t usCurrBlk,
                                  u32_t *pulResendPkt,
                                  struct sockaddr_in *pstServerAddr)
{
    fd_set stReadfds;           /* 文件描述符集合 */
    struct timeval stTimeout;   /* 超时结构体 */
    struct sockaddr_in stFromAddr; /* 发送方地址结构体 */
    socklen_t ulFromAddrLen;    /* 地址长度 */
    s32_t iRecvLen = (s32_t)*pulSize; /* 接收长度 */
    s32_t iError;               /* select返回值 */
    u16_t usBlknum;             /* 接收到的块编号 */
    u32_t ulLoopCnt = 0;        /* 循环计数器 */

    ulFromAddrLen = sizeof(stFromAddr);

    /* 初始化发送方地址为服务器地址 */
    if (memcpy_s((void *)&stFromAddr, sizeof(struct sockaddr_in), (void *)pstServerAddr, sizeof(stFromAddr)) != 0) {
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : memcpy_s error\n"));
        return TFTPC_MEMCPY_FAILURE;  /* 内存拷贝失败 */
    }

    /* 从接收缓冲区中获取网络字节序的块编号并转换为主机字节序 */
    usBlknum = ntohs(pstRecvBuf->u.stTFTP_Data.usBlknum);
    /* 检查接收到的块编号是否与当前期望块编号一致 */
    if (usBlknum != usCurrBlk) {
        /* 设置超时时间为1秒 */
        stTimeout.tv_sec = 1;
        stTimeout.tv_usec = 0;

        /* 初始化文件描述符集合 */
        FD_ZERO(&stReadfds);
        FD_SET(iSockNum, &stReadfds);

        /* 等待套接字可读 */
        iError = select((s32_t)(iSockNum + 1),
                        &stReadfds, 0, 0, &stTimeout);

        /* 循环读取接收缓冲区中的最后一个数据包 */
        while ((iError != TFTP_NULL_INT32) && (iError != 0)) {
            ulLoopCnt++;

            /*
             * TFTP协议最大文件大小为32MB
             * 设置75的原因：75 * 512字节 = 38400字节 = 37.5MB，确保能接收完整的最大文件
             */
            if (ulLoopCnt > TFTPC_MAX_WAIT_IN_LOOP) {
                LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : unexpected packets are received repeatedly\n"));
                *pulSize = TFTP_NULL_UINT32;
                return TFTPC_PKT_SIZE_ERROR;  /* 数据包大小错误 */
            }

            FD_ZERO(&stReadfds);
            FD_SET(iSockNum, &stReadfds);

            /* 从套接字接收数据 */
            iRecvLen = lwip_recvfrom(iSockNum,
                                     (s8_t *)pstRecvBuf,
                                     TFTP_PKTSIZE, 0,
                                     (struct sockaddr *)&stFromAddr,
                                     &ulFromAddrLen);
            if (iRecvLen == -1) {
                *pulSize = TFTP_NULL_UINT32;

                /* 接收操作失败 */
                return TFTPC_RECVFROM_ERROR;
            }

            stTimeout.tv_sec = 1;
            stTimeout.tv_usec = 0;
            iError = select((s32_t)(iSockNum + 1), &stReadfds, 0, 0, &stTimeout);
        }

        /* 检查接收数据包大小是否小于最小数据包大小（4字节：操作码2字节+块编号2字节） */
        if (iRecvLen < TFTPC_FOUR) {
            return TFTPC_PKT_SIZE_ERROR;  /* 数据包大小错误 */
        }

        /* 检查接收到的数据包是否来自正确的服务器和端口 */
        if ((stFromAddr.sin_addr.s_addr != pstServerAddr->sin_addr.s_addr) ||
            (pstServerAddr->sin_port != stFromAddr.sin_port)) {
            *pulResendPkt = 1;  /* 需要重发ACK包 */
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : Received pkt from unknown server\n"));

            return ERR_OK;
        }

        /* 检查接收到的是否为DATA数据包（操作码0x0003） */
        if (TFTPC_OP_DATA != ntohs(pstRecvBuf->usOpcode)) {
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : Received pkt not a DATA pkt\n"));

            /* 接收到非预期的数据包类型 */
            return TFTPC_PROTO_ERROR;  /* 协议错误 */
        }

        usBlknum = ntohs(pstRecvBuf->u.stTFTP_Data.usBlknum);
        /* 如果接收到的是上一个块编号（可能是服务器未收到ACK） */
        if (usBlknum == (usCurrBlk - 1)) {
            *pulResendPkt = 1;  /* 需要重发上一个ACK包 */
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : Received previous DATA pkt\n"));

            return ERR_OK;
        }

        /* If the block of data received is not current block or also
           previous block, then it is abnormal case. */
        if (usBlknum != usCurrBlk) {
            LWIP_DEBUGF(TFTP_DEBUG,
                        ("lwip_tftp_validate_data_pkt : Received DATA pkt no. %"S32_F" instead of pkt no.%"S32_F"\n",
                            usBlknum, usCurrBlk));

            return TFTPC_SYNC_FAILURE;
        }
    }

    *pulSize = (u32_t)iRecvLen;
    return ERR_OK;
}
/**
 * @brief 向TFTP服务器发送错误数据包
 *
 * @param iSockNum      UDP套接字描述符
 * @param ulError       TFTP错误代码（符合RFC 1350定义）
 * @param szErrMsg      错误描述信息字符串
 * @param pstServerAddr 服务器网络地址结构体指针
 * @param pstSendBuf    发送缓冲区结构体指针
 *
 * @note 错误数据包格式：2字节操作码(TFTP_OP_ERROR) + 2字节错误码 + 错误信息(以null结尾)
 * @warning 缓冲区必须提前分配足够内存，最小大小为TFTP_MAXERRSTRSIZE + 4字节
 */
static void lwip_tftp_send_error(s32_t iSockNum, u32_t ulError, const char *szErrMsg,
                                 struct sockaddr_in *pstServerAddr, TFTPC_PACKET_S *pstSendBuf)
{
    u16_t usOpCode = TFTPC_OP_ERROR;  /* TFTP操作码：错误包标识 */

    /* 初始化发送缓冲区，将整个结构体空间清零 */
    if (memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S)) != 0) {
        /* 调试输出：缓冲区初始化失败 */
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_send_error : memset_s error\n"));
        return;  /* 初始化失败直接返回 */
    }

    /* 构建错误数据包头部信息 */
    pstSendBuf->usOpcode = htons(usOpCode);  /* 设置操作码（主机字节序转网络字节序） */
    pstSendBuf->u.stTFTP_Err.usErrNum = htons((u16_t)ulError);  /* 设置错误码（网络字节序） */

    /* 复制错误信息到数据包缓冲区（带长度检查的安全拷贝） */
    if (strncpy_s((char *)(pstSendBuf->u.stTFTP_Err.ucErrMesg), TFTP_MAXERRSTRSIZE,
                  (char *)szErrMsg, (TFTP_MAXERRSTRSIZE - 1)) != EOK) {
        /* 调试输出：错误信息复制失败 */
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_send_error : strncpy_s error\n"));
        return;  /* 字符串拷贝失败返回 */
    }
    pstSendBuf->u.stTFTP_Err.ucErrMesg[(TFTP_MAXERRSTRSIZE - 1)] = '\0';  /* 确保字符串终止符 */

    /* 发送错误数据包到服务器 */
    if (lwip_tftp_send_to_server(iSockNum,  /* 已绑定的UDP套接字 */
                                 sizeof(TFTPC_PACKET_S),  /* 数据包长度 */
                                 pstSendBuf,  /* 待发送数据缓冲区 */
                                 pstServerAddr) != ERR_OK) {  /* 服务器地址信息 */
        /* 调试输出：发送错误数据包失败 */
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_send_to_server error."));
        return;  /* 发送失败返回 */
    }
}
/**
 * @brief TFTP客户端文件下载核心函数
 *
 * 通过TFTP协议从指定服务器下载文件，并保存到本地目标路径
 * 实现完整的TFTP读请求(RRQ)流程，包括数据包验证、超时重传和错误处理
 *
 * @param ulHostAddr       服务器IP地址（网络字节序）
 * @param usTftpServPort   TFTP服务器端口号（主机字节序，0表示使用默认端口69）
 * @param ucTftpTransMode  传输模式（TRANSFER_MODE_BINARY或TRANSFER_MODE_ASCII）
 * @param szSrcFileName    源文件路径（服务器端）
 * @param szDestDirPath    目标目录路径（本地端）
 *
 * @return u32_t           错误码
 * @retval ERR_OK          下载成功
 * @retval TFTPC_xxx_ERROR 各类错误（具体含义见枚举定义）
 *
 * @note 遵循RFC 1350规范，支持最大文件大小为 (TFTP_MAX_BLK_NUM * TFTP_BLKSIZE) = 33553920字节 (32MB)
 * @warning 目标路径长度不得超过TFTP_MAX_PATH_LENGTH(256字节)，否则会返回路径长度错误
 */
u32_t lwip_tftp_get_file_by_filename(u32_t ulHostAddr,
                                     u16_t usTftpServPort,
                                     u8_t ucTftpTransMode,
                                     s8_t *szSrcFileName,
                                     s8_t *szDestDirPath)
{
    s32_t iSockNum = TFTP_NULL_INT32;          /* UDP套接字描述符，初始化为无效值 */
    u32_t ulSrcStrLen;                         /* 源文件名长度 */
    u32_t ulDestStrLen;                        /* 目标路径长度 */
    u32_t ulSize;                              /* 数据包大小 */
    u32_t ulRecvSize = TFTP_NULL_UINT32;       /* 接收数据大小 */
    s32_t iErrCode;                            /* 文件操作错误码 */
    u32_t ulErrCode;                           /* TFTP协议错误码 */
    u16_t usReadReq;                           /* TFTP读请求操作码 */
    u16_t usTempServPort;                      /* 临时服务器端口 */
    s8_t *pszTempDestName = NULL;              /* 临时目标文件路径缓冲区 */
    s8_t *szTempSrcName = NULL;                /* 临时源文件名指针 */
    u32_t ulCurrBlk = 1;                       /* 当前数据块编号（从1开始） */
    u32_t ulResendPkt = 0;                     /* 需要重发的数据包计数（0表示无需重发） */
    u32_t ulIgnorePkt = 0;                     /* 需要忽略的数据包计数（0表示正常处理） */
    u32_t ulTotalTime = 0;                     /* 总超时计数 */
    u32_t isLocalFileOpened = false;           /* 本地文件打开标志（false表示未打开） */

    TFTPC_PACKET_S *pstSendBuf = NULL;         /* 发送数据包结构体指针 */
    TFTPC_PACKET_S *pstRecvBuf = NULL;         /* 接收数据包结构体指针 */
    struct sockaddr_in stServerAddr;           /* 服务器地址结构体 */
    struct stat sb;                            /* 文件状态结构体（用于目录判断） */
    u32_t IsDirExist = 0;                      /* 目录存在标志（1表示存在） */
    s32_t fp = -1;                             /* 文件描述符，初始化为无效值 */

    /* 参数有效性校验 */
    if ((szSrcFileName == NULL) || (szDestDirPath == NULL)) {
        return TFTPC_INVALID_PARAVALUE;        /* 源文件名或目标路径为空 */
    }

    /* 校验传输模式合法性 */
    if ((ucTftpTransMode != TRANSFER_MODE_BINARY) && (ucTftpTransMode != TRANSFER_MODE_ASCII)) {
        return TFTPC_INVALID_PARAVALUE;        /* 仅支持二进制和ASCII模式 */
    }

    /* 校验IP地址有效性（排除组播和保留地址段） */
    /* 合法范围：A类(1.0.0.0-126.255.255.255)和B类(128.0.0.0-223.255.255.255) */
    if (!(((ulHostAddr >= TFTPC_IP_ADDR_MIN) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_RESV)) ||
          ((ulHostAddr >= TFTPC_IP_ADDR_CLASS_B) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_CLASS_DE)))) {
        return TFTPC_IP_NOT_WITHIN_RANGE;       /* IP地址不在有效范围内 */
    }

    /* 校验源文件名长度（非空且不超过最大路径长度） */
    ulSrcStrLen = strlen((char *)szSrcFileName);
    if ((ulSrcStrLen == 0) || (ulSrcStrLen >= TFTP_MAX_PATH_LENGTH)) {
        return TFTPC_SRC_FILENAME_LENGTH_ERROR; /* 源文件名长度错误（0或≥256字节） */
    }

    /* 校验目标路径长度（非空且不超过最大路径长度） */
    ulDestStrLen = strlen((char *)szDestDirPath);
    if ((ulDestStrLen >= TFTP_MAX_PATH_LENGTH) || (ulDestStrLen == 0)) {
        return TFTPC_DEST_PATH_LENGTH_ERROR;    /* 目标路径长度错误（0或≥256字节） */
    }

    /* 分配发送缓冲区内存（大小为TFTP_PACKET_S结构体大小） */
    pstSendBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstSendBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR;            /* 发送缓冲区分配失败 */
    }

    /* 分配接收缓冲区内存 */
    pstRecvBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstRecvBuf == NULL) {
        mem_free(pstSendBuf);                   /* 释放已分配的发送缓冲区 */
        return TFTPC_MEMALLOC_ERROR;            /* 接收缓冲区分配失败 */
    }

    /* 分配临时目标路径缓冲区（大小为最大路径长度256字节） */
    pszTempDestName = (s8_t *)mem_malloc(TFTP_MAX_PATH_LENGTH);
    if (pszTempDestName == NULL) {
        mem_free(pstSendBuf);                   /* 释放发送缓冲区 */
        mem_free(pstRecvBuf);                   /* 释放接收缓冲区 */
        return TFTPC_MEMALLOC_ERROR;            /* 目标路径缓冲区分配失败 */
    }

    /* 初始化发送和接收缓冲区（清零操作） */
    (void)memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));
    (void)memset_s((void *)pstRecvBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    /* 从源文件路径中提取文件名（处理相对路径） */
    /* 判断路径中是否包含目录分隔符（/或\） */
    if ((0 != strchr((char *)szSrcFileName, '/')) || (0 != strchr((char *)szSrcFileName, '\\'))) {
        /* 移动指针到路径末尾 */
        szTempSrcName = szSrcFileName + (ulSrcStrLen - 1);

        /* 向前查找最后一个目录分隔符 */
        while (((*(szTempSrcName - 1) != '/') &&
                (*(szTempSrcName - 1) != '\\')) &&
               (szTempSrcName != szSrcFileName)) {
            szTempSrcName--;                    /* 未找到分隔符则继续向前移动 */
        }

        /* 更新源文件名长度（仅包含文件名部分） */
        ulSrcStrLen = strlen((char *)szTempSrcName);
    } else {
        /* 若为简单文件名（无路径），直接使用原指针 */
        szTempSrcName = szSrcFileName;
    }

    /* 构建目标文件完整路径 */
    (void)memset_s(pszTempDestName, TFTP_MAX_PATH_LENGTH, 0, TFTP_MAX_PATH_LENGTH);
    /* 复制目标目录路径到临时缓冲区（带长度检查的安全拷贝） */
    if (strncpy_s((char *)pszTempDestName, TFTP_MAX_PATH_LENGTH, (char *)szDestDirPath, TFTP_MAX_PATH_LENGTH - 1) !=
        0) {
        ulErrCode = TFTPC_MEMCPY_FAILURE;       /* 路径复制失败 */
        goto err_handler;                       /* 跳转到错误处理 */
    }
    pszTempDestName[TFTP_MAX_PATH_LENGTH - 1] = '\0'; /* 确保字符串终止符 */

    /* 检查目标路径是否为目录 */
    if (stat((char *)pszTempDestName, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        IsDirExist = 1;                         /* 标记目标为目录 */
    }

    /* 若目标是目录，则拼接文件名 */
    if (IsDirExist == 1) {
        /* 检查路径长度是否超限（目标目录+文件名） */
        if ((ulDestStrLen + ulSrcStrLen) >= TFTP_MAX_PATH_LENGTH) {
            ulErrCode = TFTPC_DEST_PATH_LENGTH_ERROR; /* 拼接后路径过长 */
            goto err_handler;
        }

        /* 检查目录路径末尾是否有分隔符 */
        if ((pszTempDestName[ulDestStrLen - 1] != '/') &&
            (pszTempDestName[ulDestStrLen - 1] != '\\')) {
            /* 检查添加分隔符后的长度是否超限 */
            if ((ulDestStrLen + ulSrcStrLen + 1) >= TFTP_MAX_PATH_LENGTH) {
                ulErrCode = TFTPC_DEST_PATH_LENGTH_ERROR; /* 路径+分隔符+文件名过长 */
                goto err_handler;
            }

            /* 添加目录分隔符 '/' */
            if (strncat_s((char *)pszTempDestName, (TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName)),
                          "/", TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName) - 1) != 0) {
                ulErrCode = TFTPC_ERROR_NOT_DEFINED; /* 分隔符拼接失败 */
                goto err_handler;
            }
        }

        /* 拼接源文件名到目标路径 */
        if (strncat_s((char *)pszTempDestName, (TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName)),
                      (char *)szTempSrcName, TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName) - 1) != 0) {
            ulErrCode = TFTPC_ERROR_NOT_DEFINED;     /* 文件名拼接失败 */
            goto err_handler;
        }
    }

    /* 创建并绑定UDP套接字 */
    ulErrCode = lwip_tftp_create_bind_socket(&iSockNum);
    if (ulErrCode != ERR_OK) {
        goto err_handler;                           /* 套接字创建失败 */
    }

    /* 设置服务器端口（若未指定则使用默认端口69） */
    if (usTftpServPort == 0) {
        usTftpServPort = TFTPC_SERVER_PORT;         /* 默认TFTP端口 */
    }

    usTempServPort = usTftpServPort;

    /* 初始化服务器地址结构体 */
    (void)memset_s(&stServerAddr, sizeof(stServerAddr), 0, sizeof(stServerAddr));
    stServerAddr.sin_family = AF_INET;              /* IPv4地址族 */
    stServerAddr.sin_port = htons(usTempServPort);  /* 端口号转换为网络字节序 */
    stServerAddr.sin_addr.s_addr = htonl(ulHostAddr); /* IP地址转换为网络字节序 */

    /* 构建TFTP读请求(RRQ)数据包 */
    ulSize = (u32_t)lwip_tftp_make_tftp_packet(TFTPC_OP_RRQ, szSrcFileName,
                                               (u32_t)ucTftpTransMode,
                                               pstSendBuf);
    /* 发送RRQ请求到服务器 */
    ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                         pstSendBuf, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        (void)lwip_close(iSockNum);                 /* 关闭套接字 */
        goto err_handler;                           /* 发送失败 */
    }

    /* 主循环：接收数据并处理（TFTP数据传输核心逻辑） */
    for (;;) {
        if (ulIgnorePkt > 0) {
            ulIgnorePkt = 0;                        /* 重置忽略标志 */
        }

        /* 从服务器接收数据包（带超时处理） */
        ulErrCode = lwip_tftp_recv_from_server(iSockNum, &ulRecvSize, pstRecvBuf,
                                               &ulIgnorePkt, &stServerAddr, pstSendBuf);
        /* 处理接收超时 */
        if (ulErrCode == TFTPC_TIMEOUT_ERROR) {
            ulTotalTime++;                          /* 超时计数递增 */
            if (ulTotalTime < TFTPC_MAX_SEND_REQ_ATTEMPTS) {
                /* 未达到最大重试次数，重发上次数据包 */
                ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                     pstSendBuf, &stServerAddr);
                if (ulErrCode != ERR_OK) {
                    (void)lwip_close(iSockNum);     /* 关闭套接字 */
                    if (isLocalFileOpened == true) {
                        close(fp);                  /* 关闭已打开的文件 */
                    }
                    goto err_handler;               /* 重发失败 */
                }

                continue;                           /* 继续等待接收 */
            } else {
                /* 达到最大重试次数，返回超时错误 */
                (void)lwip_close(iSockNum);
                if (isLocalFileOpened == true) {
                    close(fp);
                }
                ulErrCode = TFTPC_TIMEOUT_ERROR;
                goto err_handler;
            }
        } else if (ulErrCode != ERR_OK) {
            /* 其他接收错误 */
            (void)lwip_close(iSockNum);
            if (isLocalFileOpened == true) {
                close(fp);
            }
            goto err_handler;
        }

        /* 忽略无效数据包 */
        if (ulIgnorePkt > 0) {
            continue;                               /* 不处理，继续接收下一个包 */
        }

        /* 验证数据包类型是否为DATA包 */
        if (ntohs(pstRecvBuf->usOpcode) != TFTPC_OP_DATA) {
            /* 发送协议错误包给服务器 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Protocol error.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            if (isLocalFileOpened == true) {
                close(fp);
            }

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Received pkt not DATA pkt\n"));

            ulErrCode = TFTPC_PROTO_ERROR;           /* 协议错误 */
            goto err_handler;
        }

        /* 重置超时计数（收到有效数据包） */
        ulTotalTime = 0;

        /* 验证DATA数据包合法性（块编号、来源等） */
        ulErrCode = lwip_tftp_validate_data_pkt(iSockNum, &ulRecvSize,
                                                pstRecvBuf, (u16_t)ulCurrBlk,
                                                &ulResendPkt,
                                                &stServerAddr);
        if (ulErrCode != ERR_OK) {
            /* 发送错误包（接收错误除外） */
            if (ulErrCode != TFTPC_RECVFROM_ERROR) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Received unexpected packet",
                                     &stServerAddr, pstSendBuf);
            }

            (void)lwip_close(iSockNum);
            if (isLocalFileOpened == true) {
                close(fp);
            }

            goto err_handler;
        }

        /* 处理需要重发的情况（收到重复的前一个数据包） */
        if (ulResendPkt > 0) {
            ulResendPkt = 0;                         /* 重置重发标志 */
            /* 重发上次的ACK包 */
            ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                 pstSendBuf, &stServerAddr);
            if (ulErrCode != ERR_OK) {
                (void)lwip_close(iSockNum);
                if (isLocalFileOpened == true) {
                    close(fp);
                }

                goto err_handler;
            }

            continue;                               /* 继续等待下一个数据包 */
        }

        /* 计算实际数据大小（减去2字节操作码+2字节块编号的头部） */
        ulRecvSize -= TFTP_HDRSIZE;                 /* TFTP_HDRSIZE = 4字节 */

        /* 检查数据包大小是否超过最大块大小（512字节） */
        if (ulRecvSize > TFTP_BLKSIZE) {
            /* 发送数据包大小错误 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Packet size > max size",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            if (isLocalFileOpened == true) {
                close(fp);
            }

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Packet size > max size\n"));

            ulErrCode = TFTPC_PKT_SIZE_ERROR;        /* 数据包大小错误 */
            goto err_handler;
        }

        /* 构建ACK应答包 */
        usReadReq = (u16_t)TFTPC_OP_ACK;            /* ACK操作码 */
        pstSendBuf->usOpcode = htons(usReadReq);     /* 操作码转网络字节序 */
        pstSendBuf->u.usBlknum = htons((u16_t)ulCurrBlk); /* 当前块编号转网络字节序 */
        ulSize = TFTP_HDRSIZE;                      /* ACK包大小固定为4字节 */

        /* 首次接收数据时打开本地文件 */
        if (isLocalFileOpened == false) {
            /* 以只写、创建、截断模式打开文件，权限为默认 */
            fp = open((char *)pszTempDestName, (O_WRONLY | O_CREAT | O_TRUNC), DEFFILEMODE);
            if (fp == TFTP_NULL_INT32) {
                ulErrCode = TFTPC_FILECREATE_ERROR;  /* 文件创建失败 */
                (void)lwip_close(iSockNum);
                goto err_handler;
            }
            isLocalFileOpened = true;               /* 标记文件已打开 */
        }

        /* 判断是否为最后一个数据包（数据大小小于512字节） */
        if (ulRecvSize != TFTP_BLKSIZE) {
            /* 发送ACK应答 */
            (void)lwip_tftp_send_to_server(iSockNum, ulSize, pstSendBuf, &stServerAddr);

            /* 处理非空的最后一个数据包 */
            if (ulRecvSize != 0) {
                /* 将数据写入本地文件 */
                iErrCode = write(fp, (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize);
                if (ulRecvSize != (u32_t)iErrCode) {
                    /* 写文件失败，发送错误包 */
                    lwip_tftp_send_error(iSockNum,
                                         TFTPC_PROTOCOL_USER_DEFINED,
                                         "Write to file failed",
                                         &stServerAddr, pstSendBuf);

                    (void)lwip_close(iSockNum);
                    close(fp);

                    ulErrCode = TFTPC_FILEWRITE_ERROR; /* 文件写入错误 */
                    goto err_handler;
                }
            }

            /* 传输完成，释放资源并返回成功 */
            (void)lwip_close(iSockNum);
            close(fp);
            ulErrCode = ERR_OK;
            goto err_handler;
        }

        /* 发送ACK应答包（非最后一个数据包） */
        ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                             pstSendBuf, &stServerAddr);
        if (ulErrCode != ERR_OK) {
            (void)lwip_close(iSockNum);
            close(fp);
            goto err_handler;
        }

        /* 将接收到的数据写入本地文件 */
        iErrCode = write(fp, (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize);
        if (ulRecvSize != (u32_t)iErrCode) {
            /* 写文件失败，发送错误包 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_USER_DEFINED,
                                 "Write to file failed",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            close(fp);

            ulErrCode = TFTPC_FILEWRITE_ERROR;       /* 文件写入错误 */
            goto err_handler;
        }

        /* 准备接收下一个数据块 */
        ulCurrBlk++;                                /* 块编号递增 */

        /* 检查是否超过最大块编号（防止文件过大） */
        if (ulCurrBlk > TFTP_MAX_BLK_NUM) {
            /* 发送文件过大错误包 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_USER_DEFINED,
                                 "File is too big.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            close(fp);
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Data block number exceeded max value\n"));

            ulErrCode = TFTPC_FILE_TOO_BIG;          /* 文件过大错误 */
            goto err_handler;
        }
    }

err_handler:                                      /* 统一错误处理出口 */
    mem_free(pstSendBuf);                          /* 释放发送缓冲区 */
    mem_free(pstRecvBuf);                          /* 释放接收缓冲区 */
    mem_free(pszTempDestName);                     /* 释放目标路径缓冲区 */
    return ulErrCode;                              /* 返回错误码 */
}

/**
 * @brief TFTP客户端文件上传核心函数
 * @details 通过TFTP协议将本地文件上传至指定服务器，实现WRQ(写请求)操作及后续数据传输流程
 *          包括参数校验、内存分配、路径处理、套接字操作、数据分包发送和错误处理
 * @param[in]  ulHostAddr      服务器IP地址(网络字节序)
 * @param[in]  usTftpServPort  服务器TFTP端口号，0则使用默认端口(69)
 * @param[in]  ucTftpTransMode 传输模式，支持TRANSFER_MODE_BINARY(二进制)和TRANSFER_MODE_ASCII(ASCII)
 * @param[in]  szSrcFileName   本地源文件路径
 * @param[in]  szDestDirPath   服务器目标路径，可为文件名、包含文件名的相对路径或空字符串
 * @retval     ERR_OK          上传成功
 * @retval     TFTPC_xxx_ERROR 各类错误码，包括参数无效、文件不存在、内存分配失败等
 * @note       最大支持文件大小为 TFTP_MAX_BLK_NUM * TFTP_BLKSIZE 字节
 *             TFTP_MAX_BLK_NUM通常为65535，TFTP_BLKSIZE通常为512字节，最大支持约32MB文件
 */
u32_t lwip_tftp_put_file_by_filename(u32_t ulHostAddr, u16_t usTftpServPort, u8_t ucTftpTransMode,
                                     s8_t *szSrcFileName, s8_t *szDestDirPath)
{
    u32_t ulSrcStrLen;               /* 源文件名字符长度 */
    u32_t ulDestStrLen;              /* 目标路径字符长度 */
    s32_t iSockNum = TFTP_NULL_INT32;/* UDP套接字句柄，初始化为无效值 */
    s32_t iErrCode;                  /* 文件读写操作返回码 */
    u32_t ulErrCode;                 /* 函数调用错误码 */
    u16_t usTempServPort;            /* 临时服务器端口号 */
    TFTPC_PACKET_S *pstSendBuf = NULL;/* TFTP发送缓冲区指针 */
    u16_t usReadReq;                 /* TFTP操作码 */
    u32_t ulSize;                    /* 数据包大小 */
    s8_t *pucBuffer = 0;             /* 文件数据读取缓冲区 */
    s8_t *szTempDestName = NULL;     /* 处理后的目标文件名 */

    /* 初始化数据块编号，从0开始计数 */
    u16_t usCurrBlk = 0;
    struct sockaddr_in stServerAddr; /* 服务器地址结构体 */
    struct stat buffer;              /* 文件状态结构体，用于获取文件信息 */
    s32_t fp = -1;                   /* 文件描述符，初始化为无效值 */

    /* 参数有效性校验 */
    if ((szSrcFileName == NULL) || (szDestDirPath == NULL)) {
        return TFTPC_INVALID_PARAVALUE; /* 源文件名或目标路径为空 */
    }

    /* 校验传输模式合法性 */
    if ((ucTftpTransMode != TRANSFER_MODE_BINARY) && (ucTftpTransMode != TRANSFER_MODE_ASCII)) {
        return TFTPC_INVALID_PARAVALUE; /* 不支持的传输模式 */
    }

    /*
     * 校验IP地址有效性
     * 合法范围：1.0.0.0-126.255.255.255 (A类) 或 128.0.0.0-223.255.255.255 (B/C类)
     * 排除127.0.0.0-127.255.255.255 (环回地址)和224.0.0.0以上(组播/保留地址)
     */
    if (!(((ulHostAddr >= TFTPC_IP_ADDR_MIN) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_RESV)) ||
          ((ulHostAddr >= TFTPC_IP_ADDR_CLASS_B) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_CLASS_DE)))) {
        return TFTPC_IP_NOT_WITHIN_RANGE; /* IP地址不在有效范围内 */
    }

    /* 校验源文件名长度 */
    ulSrcStrLen = strlen((char *)szSrcFileName);
    if ((ulSrcStrLen == 0) || (ulSrcStrLen >= TFTP_MAX_PATH_LENGTH)) {
        return TFTPC_SRC_FILENAME_LENGTH_ERROR; /* 文件名空或超过最大长度限制 */
    }

    /* 检查源文件是否存在 */
    if (stat((char *)szSrcFileName, &buffer) != 0) {
        return TFTPC_FILE_NOT_EXIST; /* 文件不存在 */
    }

    /* 检查文件大小是否超过TFTP协议支持的最大值 */
    /* TFTP_MAX_BLK_NUM通常为65535，TFTP_BLKSIZE通常为512字节，最大支持约32MB */
    if (buffer.st_size >= (off_t)(TFTP_MAX_BLK_NUM * TFTP_BLKSIZE)) {
        return TFTPC_FILE_TOO_BIG; /* 文件过大 */
    }

    /* 校验目标路径长度 */
    ulDestStrLen = strlen((char *)szDestDirPath);
    /* 目标路径长度超过最大限制 */
    if (ulDestStrLen >= TFTP_MAX_PATH_LENGTH) {
        return TFTPC_DEST_PATH_LENGTH_ERROR;
    }

    /* 分配TFTP数据包缓冲区内存 */
    pstSendBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstSendBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR; /* 内存分配失败 */
    }

    /* 初始化发送缓冲区，全部置0 */
    (void)memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    /*
     * 目标路径处理逻辑：
     * 1. 若目标路径非空，则直接使用提供的路径名
     * 2. 若目标路径为空，则从源文件名提取文件名：
     *    a. 若源文件名为绝对路径或相对路径，提取最后一个/或\后的文件名
     *    b. 若源文件名为单纯文件名，则直接使用源文件名
     */
    if (ulDestStrLen != 0) {
        /* 使用指定的目标路径名 */
        szTempDestName = szDestDirPath;
    } else {
        /* 目标路径为空，从源文件名提取 */
        if ((strchr((char *)szSrcFileName, '/') != 0) ||
            (strchr((char *)szSrcFileName, '\\') != 0)) {
            /* 源文件包含路径分隔符，移动到路径末尾 */
            szTempDestName = szSrcFileName + (ulSrcStrLen - 1);

            /* 向前查找路径分隔符，定位文件名起始位置 */
            while (((*(szTempDestName - 1) != '/') && (*(szTempDestName - 1) != '\\')) &&
                   (szTempDestName != szSrcFileName)) {
                szTempDestName--;
            }
        } else {
            /* 源文件名不包含路径，直接使用 */
            szTempDestName = szSrcFileName;
        }
    }

    /* 创建并绑定UDP套接字到可用端口 */
    ulErrCode = lwip_tftp_create_bind_socket(&iSockNum);
    if (ulErrCode != EOK) {
        /* 创建或绑定套接字失败，跳转到错误处理 */
        goto err_handler;
    }

    /* 若未指定服务器端口，使用TFTP默认端口69 */
    if (usTftpServPort == 0) {
        usTftpServPort = TFTPC_SERVER_PORT; /* TFTPC_SERVER_PORT定义为69 */
    }

    usTempServPort = usTftpServPort;

    /* 初始化服务器地址结构体 */
    (void)memset_s(&stServerAddr, sizeof(stServerAddr), 0, sizeof(stServerAddr));
    stServerAddr.sin_family = AF_INET;         /* IPv4地址族 */
    stServerAddr.sin_port = htons(usTempServPort); /* 端口号转换为网络字节序 */
    stServerAddr.sin_addr.s_addr = htonl(ulHostAddr); /* IP地址转换为网络字节序 */

    /* 构建TFTP写请求(WRQ)数据包 */
    ulSize = (u32_t)lwip_tftp_make_tftp_packet(TFTPC_OP_WRQ,
                                               szTempDestName,
                                               ucTftpTransMode,
                                               pstSendBuf);

    /* 发送WRQ请求到服务器 */
    ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                         pstSendBuf, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        /* 发送请求失败，关闭套接字并跳转错误处理 */
        (void)lwip_close(iSockNum);

        goto err_handler;
    }

    /* 处理WRQ请求的响应，等待服务器确认 */
    ulErrCode = lwip_tftp_inner_put_file(iSockNum, pstSendBuf, ulSize,
                                         usCurrBlk, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        /* 发送请求包失败 */
        (void)lwip_close(iSockNum);

        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_put_file_by_filename : Failed to send request packet\n"));

        goto err_handler;
    }

    /* 分配文件数据读取缓冲区，大小为TFTP数据块大小(TFTP_BLKSIZE通常为512字节) */
    pucBuffer = mem_malloc(TFTP_BLKSIZE);
    if (pucBuffer == NULL) {
        /* 内存分配失败，向服务器发送错误包 */
        lwip_tftp_send_error(iSockNum,
                             TFTPC_PROTOCOL_USER_DEFINED,
                             "Memory allocation failed.",
                             &stServerAddr, pstSendBuf);

        (void)lwip_close(iSockNum);
        ulErrCode = TFTPC_MEMALLOC_ERROR;
        goto err_handler;
    }

    /* 初始化数据缓冲区 */
    (void)memset_s((void *)pucBuffer, TFTP_BLKSIZE, 0, TFTP_BLKSIZE);

    /* 以只读方式打开源文件 */
    fp = open((char *)szSrcFileName, O_RDONLY);
    if (TFTP_NULL_INT32 == fp) {
        /* 文件打开失败，向服务器发送错误信息 */
        lwip_tftp_send_error(iSockNum,
                             TFTPC_PROTOCOL_USER_DEFINED,
                             "File open error.",
                             &stServerAddr, pstSendBuf);

        (void)lwip_close(iSockNum);
        mem_free(pucBuffer);

        ulErrCode = TFTPC_FILEOPEN_ERROR;
        goto err_handler;
    }

    /* 从文件读取第一块数据，最多读取TFTP_BLKSIZE字节 */
    iErrCode = read(fp, pucBuffer, TFTP_BLKSIZE);
    if (iErrCode < 0) {
        /* 文件读取失败，向服务器发送错误信息 */
        lwip_tftp_send_error(iSockNum,
                             TFTPC_PROTOCOL_USER_DEFINED,
                             "File read error.",
                             &stServerAddr, pstSendBuf);

        (void)lwip_close(iSockNum);
        close(fp);
        mem_free(pucBuffer);

        ulErrCode = TFTPC_FILEREAD_ERROR;
        goto err_handler;
    }

    /*
     * 数据发送主循环：
     * 1. 读取文件数据到缓冲区
     * 2. 构建TFTP DATA数据包
     * 3. 发送数据包到服务器
     * 4. 等待服务器ACK确认
     * 5. 重复直到文件发送完成
     * 注：对于0字节文件，也会发送一个空数据包
     */
    do {
        /* 检查块编号是否超过最大值(TFTP_MAX_BLK_NUM通常为65535) */
        if (((u32_t)usCurrBlk + 1) > TFTP_MAX_BLK_NUM) {
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_USER_DEFINED,
                                 "File is too big.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_put_file_by_filename : Data block number exceeded max value\n"));

            ulErrCode = TFTPC_FILE_TOO_BIG;
            goto err_handler;
        }

        /* 增加块编号(从1开始计数) */
        usCurrBlk++;

        /* 计算数据包大小 = 数据长度 + TFTP头部大小(TFTP_HDRSIZE为4字节) */
        ulSize = (u32_t)iErrCode + TFTP_HDRSIZE;

        /* 构建DATA数据包 */
        usReadReq = (u16_t)TFTPC_OP_DATA;          /* 设置操作码为DATA(0x0003) */
        pstSendBuf->usOpcode = htons(usReadReq);   /* 操作码转换为网络字节序 */
        pstSendBuf->u.stTFTP_Data.usBlknum = htons(usCurrBlk); /* 块编号转换为网络字节序 */
        /* 将文件数据复制到发送缓冲区 */
        if (memcpy_s((void *)pstSendBuf->u.stTFTP_Data.ucDataBuf, TFTP_BLKSIZE,
                     (void *)pucBuffer, (u32_t)iErrCode) != EOK) {
            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            goto err_handler;
        }

        /* 发送DATA数据包到服务器 */
        ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                             pstSendBuf, &stServerAddr);
        /* 发送失败或缓冲区初始化失败 */
        if ((ulErrCode != ERR_OK) || (memset_s((void *)pucBuffer, TFTP_BLKSIZE, 0, TFTP_BLKSIZE) != 0)) {
            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            goto err_handler;
        }

        /* 从文件读取下一块数据 */
        iErrCode = read(fp, pucBuffer, TFTP_BLKSIZE);
        if (iErrCode < 0) {
            /* 文件读取失败 */
            lwip_tftp_send_error(iSockNum, TFTPC_PROTOCOL_USER_DEFINED, "File read error.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            ulErrCode = TFTPC_FILEREAD_ERROR;
            goto err_handler;
        }

        /* 等待服务器ACK确认当前数据块 */
        ulErrCode = lwip_tftp_inner_put_file(iSockNum, pstSendBuf, ulSize,
                                             usCurrBlk, &stServerAddr);
        if (ulErrCode != ERR_OK) {
            /* 发送缓冲区内容失败 */
            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_put_file_by_filename : Sending file to server failed\n"));
            goto err_handler;
        }
    /*
     * 循环条件：当前发送的数据包大小等于最大块大小(TFTP_BLKSIZE + TFTP_HDRSIZE)
     * 当发送的数据包小于最大块大小时，表示已到达文件末尾
     */
    } while (ulSize == (TFTP_BLKSIZE + TFTP_HDRSIZE));

    /* 数据传输完成，清理资源 */
    (void)lwip_close(iSockNum);  /* 关闭套接字 */
    close(fp);                   /* 关闭文件 */
    mem_free(pucBuffer);         /* 释放数据缓冲区 */

    ulErrCode = ERR_OK;          /* 上传成功 */
err_handler:                   /* 统一错误处理标签 */
    mem_free(pstSendBuf);        /* 释放发送缓冲区 */
    return ulErrCode;            /* 返回结果 */
}
/**
 * @brief TFTP文件上传内部处理函数
 * @details 处理TFTP上传过程中的ACK包接收、超时重传和错误处理逻辑，确保数据块正确传输
 *          实现了TFTP协议的超时重传机制和数据包同步校验
 * @param[in]  iSockNum        UDP套接字句柄
 * @param[in]  pstSendBuf      待发送的TFTP数据包缓冲区
 * @param[in]  ulSendSize      发送数据包大小(字节)
 * @param[in]  usCurrBlk       当前数据块编号(从0开始)
 * @param[in]  pstServerAddr   服务器地址结构体指针
 * @retval     ERR_OK          处理成功(收到正确ACK)
 * @retval     TFTPC_xxx_ERROR 各类错误码，包括内存分配失败、超时、协议错误等
 * @note       最大重传次数由TFTPC_MAX_SEND_REQ_ATTEMPTS定义，通常为5次
 *             超时时间由lwip_tftp_recv_from_server函数内部定义
 */
u32_t lwip_tftp_inner_put_file(s32_t iSockNum,
                               TFTPC_PACKET_S *pstSendBuf,
                               u32_t ulSendSize,
                               u16_t usCurrBlk,
                               struct sockaddr_in *pstServerAddr)
{
    u32_t ulPktSize;               /* 接收数据包大小 */
    u32_t ulError;                 /* 错误码 */
    s32_t iError;                  /* select/recvfrom系统调用返回值 */
    int iRecvLen = 0;              /* 接收数据长度 */
    socklen_t iFromAddrLen;        /* 源地址结构体长度 */
    u32_t ulTotalTime = 0;         /* 超时重传计数 */
    fd_set stReadfds;              /* select读文件描述符集 */
    struct sockaddr_in stFromAddr; /* 源地址结构体 */
    struct timeval stTimeout;      /* 超时结构体 */
    TFTPC_PACKET_S *pstRecvBuf = NULL; /* 接收缓冲区指针 */
    u32_t ulIgnorePkt = 0;         /* 忽略数据包标志(0:不忽略,>0:忽略) */
    u16_t usBlknum;                /* 接收到的块编号 */
    u32_t ulLoopCnt = 0;           /* 循环计数器，防止无限循环 */

    iFromAddrLen = sizeof(stFromAddr);

    /* 分配接收缓冲区内存 */
    pstRecvBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstRecvBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR; /* 内存分配失败 */
    }

    /* 初始化接收缓冲区，全部置0 */
    (void)memset_s((void *)pstRecvBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    /* 初始化源地址为服务器地址 */
    if (memcpy_s((void *)&stFromAddr, sizeof(struct sockaddr_in),
                 (void *)pstServerAddr, sizeof(stFromAddr)) != EOK) {
        ulError = TFTPC_MEMCPY_FAILURE; /* 内存拷贝失败 */
        goto err_handler;
    }

    /*
     * 主循环：等待服务器ACK确认
     * 功能：接收ACK包，验证其有效性，处理超时重传
     */
    for (;;) {
        /* 从服务器接收数据包 */
        ulError = lwip_tftp_recv_from_server(iSockNum, &ulPktSize,
                                             pstRecvBuf, &ulIgnorePkt,
                                             pstServerAddr, pstSendBuf);
        /* 处理接收超时 */
        if (ulError == TFTPC_TIMEOUT_ERROR) {
            ulTotalTime++; /* 超时计数加1 */
            if (ulTotalTime < TFTPC_MAX_SEND_REQ_ATTEMPTS) {
                /* 未达到最大重传次数，重发当前数据包 */
                ulError = lwip_tftp_send_to_server(iSockNum, ulSendSize,
                                                   pstSendBuf, pstServerAddr);
                if (ulError != ERR_OK) {
                    goto err_handler; /* 发送失败，跳转错误处理 */
                }

                continue; /* 继续等待ACK */
            } else {
                /* 达到最大重传次数，返回超时错误 */
                ulError = TFTPC_TIMEOUT_ERROR;
                goto err_handler;
            }
        } else if (ulError != ERR_OK) {
            /* 其他接收错误，直接跳转错误处理 */
            goto err_handler;
        }

        /* 如果需要忽略当前数据包(来自未知服务器) */
        if (ulIgnorePkt > 0) {
            /* 忽略该数据包，继续等待下一个包 */
            ulIgnorePkt = 0;
            continue;
        }

        /* 验证接收到的是否为ACK包 */
        if (TFTPC_OP_ACK != ntohs(pstRecvBuf->usOpcode)) {
            /* 不是ACK包，向服务器发送协议错误 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Protocol error.",
                                 pstServerAddr, pstSendBuf);

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_inner_put_file : Received pkt not Ack pkt\n"));

            ulError = TFTPC_PROTO_ERROR;
            goto err_handler;
        }

        ulTotalTime = 0; /* 收到有效包，重置超时计数 */

        /* 解析ACK包中的块编号 */
        usBlknum = ntohs(pstRecvBuf->u.usBlknum);
        iRecvLen = (int)ulPktSize;

        /* 验证块编号是否匹配当前块编号 */
        if (usBlknum != usCurrBlk) {
            /* 块编号不匹配，进入同步处理流程 */
            /* 重置接收缓冲区，准备接收新数据包 */
            stTimeout.tv_sec = 1;       /* 1秒超时 */
            stTimeout.tv_usec = 0;

            FD_ZERO(&stReadfds);
            FD_SET(iSockNum, &stReadfds);

            /*
             * 处理select超时场景：
             * 由于使用阻塞套接字，如果select超时，后续recvfrom将无限阻塞
             */
            iError = select((s32_t)(iSockNum + 1), &stReadfds, 0, 0, &stTimeout);

            /* 循环读取接收缓冲区中的所有数据包，直到获取最新的包 */
            while ((iError != -1) && (iError != 0)) {
                ulLoopCnt++; /* 循环计数加1 */

                /*
                 * 限制最大循环次数：TFTPC_MAX_WAIT_IN_LOOP通常为75
                 * 75 * 512字节 = 38400字节 = 37.5MB，大于TFTP最大支持的32MB
                 * 确保能接收完所有可能的乱序数据包
                 */
                if (ulLoopCnt > TFTPC_MAX_WAIT_IN_LOOP) {
                    LWIP_DEBUGF(TFTP_DEBUG,
                                ("lwip_tftp_inner_put_file : unexpected packets are received repeatedly\n"));
                    ulError = TFTPC_PKT_SIZE_ERROR;
                    goto err_handler;
                }

                FD_ZERO(&stReadfds);
                FD_SET(iSockNum, &stReadfds);
                /* 从套接字接收数据 */
                iRecvLen = lwip_recvfrom(iSockNum,
                                         (s8_t *)pstRecvBuf,
                                         TFTP_PKTSIZE, 0,
                                         (struct sockaddr *)&stFromAddr,
                                         &iFromAddrLen);
                if (TFTP_NULL_INT32 == iRecvLen) {
                    ulError = TFTPC_RECVFROM_ERROR; /* 接收失败 */
                    goto err_handler;
                }

                /* 重置超时，继续等待下一个数据包 */
                stTimeout.tv_sec = 1;
                stTimeout.tv_usec = 0;
                iError = select((s32_t)(iSockNum + 1),
                                &stReadfds, 0, 0, &stTimeout);
            }

            /* 如果未收到新数据包，不改变已有的字节序转换 */
            /* 检查数据包大小是否小于最小要求(4字节:2字节操作码+2字节块编号) */
            if (iRecvLen < TFTPC_FOUR) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Packet size < min packet size",
                                     pstServerAddr, pstSendBuf);

                LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_inner_put_file : Received pkt not Ack pkt\n"));

                ulError = TFTPC_PKT_SIZE_ERROR;
                goto err_handler;
            }

            /* 验证数据包是否来自正确的服务器和端口 */
            if ((stFromAddr.sin_addr.s_addr != pstServerAddr->sin_addr.s_addr) ||
                (pstServerAddr->sin_port != stFromAddr.sin_port)) {
                /* 来自未知服务器的ACK包，忽略 */
                LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_inner_put_file : Received pkt from unknown server\n"));
                continue;
            }

            /* 再次验证是否为ACK包 */
            if (TFTPC_OP_ACK != ntohs(pstRecvBuf->usOpcode)) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Protocol error.",
                                     pstServerAddr, pstSendBuf);

                LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_inner_put_file : Received pkt not Ack pkt\n"));

                ulError = TFTPC_PROTO_ERROR;
                goto err_handler;
            }

            usBlknum = ntohs(pstRecvBuf->u.usBlknum);
            /*
             * 处理重复ACK：
             * 如果收到的块编号是当前块编号-1，表示服务器未收到最后一个数据包
             * 或ACK确认延迟，此时忽略该重复ACK，继续等待正确的ACK
             */
            if ((usCurrBlk - 1) == usBlknum) {
                /* 忽略重复ACK */
                continue;
            }

            /* 验证块编号是否为当前块编号 */
            /* 如果既不是前一个块也不是当前块，则为意外数据包 */
            if (usBlknum != usCurrBlk) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Received unexpected packet",
                                     pstServerAddr, pstSendBuf);

                LWIP_DEBUGF(TFTP_DEBUG,
                            ("lwip_tftp_inner_put_file : Received DATA pkt no. %"S32_F"instead of pkt no. %"S32_F"\n",
                                usBlknum, usCurrBlk));

                ulError = TFTPC_SYNC_FAILURE; /* 同步失败 */
                goto err_handler;
            }
        }

        /* 所有校验通过，返回成功 */
        ulError = ERR_OK;
        goto err_handler;
    }

err_handler:                   /* 统一错误处理标签 */
    mem_free(pstRecvBuf);        /* 释放接收缓冲区 */
    return ulError;              /* 返回结果 */
}
#ifdef TFTP_TO_RAWMEM
/**
 * @brief TFTP客户端从服务器获取文件并写入指定内存地址
 * @details 实现TFTP协议的文件下载功能，将远程文件通过UDP协议传输并直接写入客户端指定的内存地址
 *          支持二进制和ASCII两种传输模式，包含完整的错误处理和超时重传机制
 * @param[in]  ulHostAddr      服务器IP地址（网络字节序）
 * @param[in]  usTftpServPort  TFTP服务器端口号，若为0则使用默认端口（69）
 * @param[in]  ucTftpTransMode 传输模式，支持TRANSFER_MODE_BINARY(二进制)和TRANSFER_MODE_ASCII(ASCII)
 * @param[in]  szSrcFileName   源文件路径（服务器端）
 * @param[out] szDestMemAddr   目标内存地址（客户端）
 * @param[in,out] ulFileLength 输入时为内存缓冲区大小，输出时为实际接收的文件长度
 * @return u32_t 错误码
 *         - ERR_OK: 成功
 *         - TFTPC_INVALID_PARAVALUE: 参数无效
 *         - TFTPC_IP_NOT_WITHIN_RANGE: IP地址不在有效范围内
 *         - TFTPC_SRC_FILENAME_LENGTH_ERROR: 源文件名长度错误
 *         - TFTPC_MEMALLOC_ERROR: 内存分配失败
 *         - TFTPC_TIMEOUT_ERROR: 超时错误
 *         - TFTPC_PROTO_ERROR: 协议错误
 *         - TFTPC_PKT_SIZE_ERROR: 数据包大小错误
 *         - TFTPC_MEMCPY_FAILURE: 内存复制失败
 *         - TFTPC_FILE_TOO_BIG: 文件过大
 * @note 示例用法:
 *       ulHostAddr = ntohl(inet_addr("192.168.1.3"));
 *       lwip_tftp_get_file_by_filename_to_rawmem(ulHostAddr, "/ramfs/vs_server.bin", memaddr, &filelen);
 */
u32_t lwip_tftp_get_file_by_filename_to_rawmem(u32_t ulHostAddr,
                                               u16_t usTftpServPort,
                                               u8_t ucTftpTransMode,
                                               s8_t *szSrcFileName,
                                               s8_t *szDestMemAddr,
                                               u32_t *ulFileLength)
{
    s32_t iSockNum = TFTP_NULL_INT32;          /* 套接字句柄，初始化为无效值(-1) */
    u32_t ulSrcStrLen;                         /* 源文件名长度 */
    u32_t ulSize;                              /* 数据包大小 */
    u32_t ulRecvSize = TFTP_NULL_UINT32;       /* 接收数据大小，初始化为0 */
    u32_t ulErrCode;                           /* 错误码 */
    u16_t usReadReq;                           /* 读请求操作码 */
    u16_t usTempServPort;                      /* 临时服务器端口 */
    u32_t ulCurrBlk = 1;                       /* 当前数据块编号，从1开始 */
    u32_t ulResendPkt = 0;                     /* 重传标志：0-不需要重传，1-需要重传上一个数据包 */
    u32_t ulIgnorePkt = 0;                     /* 忽略标志：0-处理数据包，1-忽略当前数据包 */
    u32_t ulTotalTime = 0;                     /* 总超时计数 */

    TFTPC_PACKET_S *pstSendBuf = NULL;         /* 发送缓冲区指针 */
    TFTPC_PACKET_S *pstRecvBuf = NULL;         /* 接收缓冲区指针 */
    struct sockaddr_in stServerAddr;           /* 服务器地址结构体 */
    u32_t ulMemOffset = 0;                     /* 内存写入偏移量 */

    /* 参数有效性校验 */
    if ((szSrcFileName == NULL) || (szDestMemAddr == NULL) || (*ulFileLength == 0)) {
        return TFTPC_INVALID_PARAVALUE;        /* 参数为空或文件长度为0 */
    }

    /* 校验传输模式合法性 */
    if ((ucTftpTransMode != TRANSFER_MODE_BINARY) && (ucTftpTransMode != TRANSFER_MODE_ASCII)) {
        return TFTPC_INVALID_PARAVALUE;        /* 仅支持二进制和ASCII模式 */
    }

    /* 校验IP地址有效性
     * 有效范围：1.0.0.0-126.255.255.255（A类）和128.0.0.0-223.255.255.255（B/C类）
     * 排除127.0.0.0-127.255.255.255（环回地址）和224.0.0.0以上（组播/保留地址）
     */
    if (!(((ulHostAddr >= TFTPC_IP_ADDR_MIN) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_RESV)) ||
          ((ulHostAddr >= TFTPC_IP_ADDR_CLASS_B) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_CLASS_DE)))) {
        return TFTPC_IP_NOT_WITHIN_RANGE;
    }

    /* 校验源文件名长度
     * 文件名不能为空且长度不能超过最大路径长度(TFTP_MAX_PATH_LENGTH)
     */
    ulSrcStrLen = strlen(szSrcFileName);
    if ((ulSrcStrLen == 0) || (ulSrcStrLen >= TFTP_MAX_PATH_LENGTH)) {
        return TFTPC_SRC_FILENAME_LENGTH_ERROR;
    }

    /* 分配发送缓冲区内存
     * TFTPC_PACKET_S结构体包含TFTP协议头和数据缓冲区
     */
    pstSendBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstSendBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR;            /* 内存分配失败 */
    }

    /* 分配接收缓冲区内存 */
    pstRecvBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstRecvBuf == NULL) {
        mem_free(pstSendBuf);                   /* 释放已分配的发送缓冲区 */
        return TFTPC_MEMALLOC_ERROR;            /* 内存分配失败 */
    }

    /* 初始化缓冲区（安全清零）
     * 使用memset_s而非memset，确保清零操作不会被编译器优化省略
     */
    (void)memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));
    (void)memset_s((void *)pstRecvBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    /* 创建并绑定UDP套接字
     * 调用lwip_tftp_create_bind_socket函数创建非阻塞UDP套接字
     */
    ulErrCode = lwip_tftp_create_bind_socket(&iSockNum);
    if (ulErrCode != EOK) {
        goto err_handler;                       /* 创建套接字失败，跳转至错误处理 */
    }

    /* 设置服务器端口
     * 若未指定端口(usTftpServPort=0)，使用默认TFTP端口69
     */
    if (usTftpServPort == 0) {
        usTftpServPort = TFTPC_SERVER_PORT;     /* TFTPC_SERVER_PORT宏定义为69 */
    }

    usTempServPort = usTftpServPort;

    /* 初始化服务器地址结构体 */
    (void)memset_s(&stServerAddr, sizeof(stServerAddr), 0, sizeof(stServerAddr));
    stServerAddr.sin_family = AF_INET;          /* IPv4地址族 */
    stServerAddr.sin_port = htons(usTempServPort); /* 端口号转换为网络字节序 */
    stServerAddr.sin_addr.s_addr = htonl(ulHostAddr); /* IP地址转换为网络字节序 */

    /* 构建TFTP读请求(RRQ)数据包
     * 调用lwip_tftp_make_tftp_packet函数，操作码为TFTPC_OP_RRQ(1)
     */
    ulSize = (u32_t)lwip_tftp_make_tftp_packet(TFTPC_OP_RRQ, szSrcFileName, (u32_t)ucTftpTransMode, pstSendBuf);
    ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize, pstSendBuf, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        /* 发送RRQ请求失败 */
        (void)lwip_close(iSockNum);             /* 关闭套接字 */
        goto err_handler;                       /* 跳转至错误处理 */
    }

    /* TFTP文件传输主循环
     * 持续接收数据块、发送ACK、处理超时和错误
     */
    for (;;) {
        if (ulIgnorePkt > 0) {
            ulIgnorePkt = 0;                    /* 重置忽略标志 */
        }

        /* 从服务器接收数据包
         * 调用lwip_tftp_recv_from_server处理接收超时和数据包验证
         */
        ulErrCode = lwip_tftp_recv_from_server(iSockNum, &ulRecvSize, pstRecvBuf, &ulIgnorePkt,
                                               &stServerAddr, pstSendBuf);
        /* 处理接收超时 */
        if (ulErrCode == TFTPC_TIMEOUT_ERROR) {
            ulTotalTime++;                      /* 超时计数递增 */
            if (ulTotalTime < TFTPC_MAX_SEND_REQ_ATTEMPTS) {/* 未达到最大重传次数(TFTPC_MAX_SEND_REQ_ATTEMPTS=5) */
                /* 重传上一个数据包 */
                ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                     pstSendBuf, &stServerAddr);
                if (ulErrCode != ERR_OK) {
                    (void)lwip_close(iSockNum); /* 发送失败，关闭套接字 */
                    goto err_handler;
                }

                continue;                       /* 继续等待接收 */
            } else {
                /* 达到最大重传次数，超时错误 */
                (void)lwip_close(iSockNum);     /* 关闭套接字 */
                ulErrCode = TFTPC_TIMEOUT_ERROR;
                goto err_handler;
            }
        } else if (ulErrCode != ERR_OK) {
            (void)lwip_close(iSockNum);         /* 其他接收错误，关闭套接字 */
            goto err_handler;
        }

        /* 忽略无效数据包 */
        if (ulIgnorePkt > 0) {
            continue;                           /* 不处理，继续接收下一个数据包 */
        }

        /* 验证数据包类型是否为DATA包
         * TFTP协议规定：服务器应回应DATA包(操作码3)
         */
        if (ntohs(pstRecvBuf->usOpcode) != TFTPC_OP_DATA) {
            /* 发送协议错误包给服务器 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Protocol error.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);         /* 关闭套接字 */

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Received pkt not DATA pkt\n"));

            ulErrCode = TFTPC_PROTO_ERROR;       /* 协议错误 */
            goto err_handler;
        }

        /* 重置超时计数（成功接收有效数据包） */
        ulTotalTime = 0;

        /* 验证DATA数据包有效性
         * 检查块编号、来源地址等是否匹配
         */
        ulErrCode = lwip_tftp_validate_data_pkt(iSockNum, &ulRecvSize,
                                                pstRecvBuf, (u16_t)ulCurrBlk,
                                                &ulResendPkt,
                                                &stServerAddr);
        if (ulErrCode != ERR_OK) {
            /* 发送错误包给服务器（接收错误除外） */
            if (ulErrCode != TFTPC_RECVFROM_ERROR) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Received unexpected packet",
                                     &stServerAddr, pstSendBuf);
            }

            (void)lwip_close(iSockNum);         /* 关闭套接字 */

            goto err_handler;
        }

        /* 重传上一个ACK包
         * 当收到重复的上一个数据块时，需要重传对应的ACK
         */
        if (ulResendPkt > 0) {
            ulResendPkt = 0;                     /* 重置重传标志 */
            ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                 pstSendBuf, &stServerAddr);
            if (ulErrCode != ERR_OK) {
                (void)lwip_close(iSockNum);     /* 发送失败，关闭套接字 */
                goto err_handler;
            }

            continue;                           /* 继续等待下一个数据包 */
        }

        /* 计算实际数据大小（减去TFTP协议头大小）
         * TFTP_HDRSIZE宏定义为4字节（2字节操作码+2字节块编号）
         */
        ulRecvSize -= TFTP_HDRSIZE;

        /* 检查数据块大小是否超过最大允许值
         * TFTP_BLKSIZE宏定义为512字节（标准TFTP数据块大小）
         */
        if (ulRecvSize > TFTP_BLKSIZE) {
            /* 发送数据包大小错误 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Packet size > max size",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);         /* 关闭套接字 */

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Packet size > max size\n"));

            ulErrCode = TFTPC_PKT_SIZE_ERROR;    /* 数据包大小错误 */
            goto err_handler;
        }

        /* 构建ACK包
         * ACK包包含操作码(TFTPC_OP_ACK=4)和当前块编号
         */
        usReadReq = (u16_t)TFTPC_OP_ACK;
        pstSendBuf->usOpcode = htons(usReadReq);  /* 操作码转换为网络字节序 */
        pstSendBuf->u.usBlknum = htons((u16_t)ulCurrBlk); /* 块编号转换为网络字节序 */
        ulSize = TFTP_HDRSIZE;                   /* ACK包大小固定为4字节 */

        /* 判断是否为最后一个数据块
         * TFTP协议规定：数据块大小小于512字节表示传输结束
         */
        if (ulRecvSize != TFTP_BLKSIZE) {
            /* 发送ACK包确认最后一个数据块 */
            (void)lwip_tftp_send_to_server(iSockNum, ulSize,
                                           pstSendBuf, &stServerAddr);

            /* 处理非空的最后一个数据块 */
            if (ulRecvSize != 0) {
                /* 检查内存空间是否足够 */
                if (*ulFileLength < (ulMemOffset + ulRecvSize)) {
                    ulErrCode = TFTPC_MEMCPY_FAILURE; /* 内存空间不足 */
                    (void)lwip_close(iSockNum);
                    *ulFileLength = ulMemOffset;     /* 返回已接收长度 */
                    goto err_handler;
                }
                /* 将数据复制到目标内存（安全复制） */
                if (memcpy_s(szDestMemAddr + ulMemOffset, TFTP_MAX_BLK_NUM * TFTP_BLKSIZE,
                             (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize) != EOK) {
                    ulErrCode = TFTPC_MEMCPY_FAILURE; /* 内存复制失败 */
                    (void)lwip_close(iSockNum);
                    *ulFileLength = ulMemOffset;     /* 返回已接收长度 */
                    goto err_handler;
                }
                ulMemOffset += ulRecvSize;          /* 更新内存偏移量 */
            }

            /* 传输完成，释放资源并返回 */
            (void)lwip_close(iSockNum);           /* 关闭套接字 */
            ulErrCode = ERR_OK;                   /* 成功 */
            *ulFileLength = ulMemOffset;          /* 设置实际文件长度 */
            goto err_handler;
        }

        /* 发送ACK包确认当前数据块 */
        ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                             pstSendBuf, &stServerAddr);
        if (ulErrCode != ERR_OK) {
            (void)lwip_close(iSockNum);           /* 发送失败，关闭套接字 */
            goto err_handler;
        }

        /* 检查内存空间是否足够
         * 确保有足够空间存储当前块数据（ulRecvSize * ulCurrBlk）
         */
        if (*ulFileLength < ulRecvSize * ulCurrBlk) {
            ulErrCode = TFTPC_MEMCPY_FAILURE;     /* 内存空间不足 */
            (void)lwip_close(iSockNum);
            *ulFileLength = ulMemOffset;          /* 返回已接收长度 */
            goto err_handler;
        }
        /* 将数据复制到目标内存（安全复制）
         * TFTP_MAX_BLK_NUM宏定义为最大块数，防止缓冲区溢出
         */
        if (memcpy_s(szDestMemAddr + ulMemOffset, TFTP_MAX_BLK_NUM * TFTP_BLKSIZE,
                     (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize) != EOK) {
            ulErrCode = TFTPC_MEMCPY_FAILURE;     /* 内存复制失败 */
            (void)lwip_close(iSockNum);
            *ulFileLength = ulMemOffset;          /* 返回已接收长度 */
            goto err_handler;
        }

        ulMemOffset += ulRecvSize;                /* 更新内存偏移量 */
        ulCurrBlk++;                              /* 块编号递增 */
        /* 检查是否超过最大块数
         * TFTP_MAX_BLK_NUM宏定义为最大允许块数（防止文件过大）
         */
        if (ulCurrBlk > TFTP_MAX_BLK_NUM) {
            /* 发送文件过大错误 */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_USER_DEFINED,
                                 "File is too big.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);           /* 关闭套接字 */

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Data block number exceeded max value\n"));

            ulErrCode = TFTPC_FILE_TOO_BIG;       /* 文件过大错误 */
            goto err_handler;
        }
    }

err_handler:                                   /* 统一错误处理标签 */
    mem_free(pstSendBuf);                       /* 释放发送缓冲区 */
    mem_free(pstRecvBuf);                       /* 释放接收缓冲区 */
    return ulErrCode;                           /* 返回错误码 */
}
#endif

#endif /* LOSCFG_NET_LWIP_SACK_TFTP */
#endif /* LWIP_TFTP */
