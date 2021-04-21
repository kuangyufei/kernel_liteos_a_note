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


/* Create and bind a UDP socket. */
u32_t lwip_tftp_create_bind_socket(s32_t *piSocketID)
{
    int retval;
    struct sockaddr_in stClientAddr;
    u32_t ulTempClientIp;
    u32_t set_non_block_socket = 1;

    /* create a socket */
    *piSocketID = lwip_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (*piSocketID == -1) {
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_create_bind_socket : lwip_socket create socket failed\n"));
        return TFTPC_SOCKET_FAILURE;
    }

    /* Make the socket as NON-BLOCKING. */
    retval = lwip_ioctl(*piSocketID, (long)FIONBIO, &set_non_block_socket);
    if (retval != 0) {
        (void)lwip_close(*piSocketID);
        *piSocketID = TFTP_NULL_INT32;
        return TFTPC_IOCTLSOCKET_FAILURE;
    }

    ulTempClientIp = INADDR_ANY;

    /* specify a local address for this socket */
    (void)memset_s(&stClientAddr, sizeof(stClientAddr), 0, sizeof(stClientAddr));
    stClientAddr.sin_family = AF_INET;
    stClientAddr.sin_port = 0;
    stClientAddr.sin_addr.s_addr = htonl(ulTempClientIp);

    retval = lwip_bind(*piSocketID, (struct sockaddr *)&stClientAddr, sizeof(stClientAddr));
    if (retval != 0) {
        (void)lwip_close(*piSocketID);
        *piSocketID = TFTP_NULL_INT32;

        return TFTPC_BIND_FAILURE;
    }

    return ERR_OK;
}


/* Function to create TFTP packet.
     usOpcode - indiacting the nature of the operation
     pFileName -filename on which the operation needs to done
     ulMode -mode in which the operation needs to done
     pstPacket - packet generated
     Returns packet address on success
*/
static s32_t lwip_tftp_make_tftp_packet(u16_t usOpcode, s8_t *szFileName, u32_t ulMode, TFTPC_PACKET_S *pstPacket)
{
    s8_t *pcCp = NULL;

    pstPacket->usOpcode = htons(usOpcode);
    pcCp = pstPacket->u.ucName_Mode;

    /* Request packet format is:
            | Opcode |  Filename  |   0  |    Mode    |   0  |
    */
    (void)strncpy_s((char *)pcCp, TFTP_MAX_PATH_LENGTH, (char *)szFileName, (TFTP_MAX_PATH_LENGTH - 1));
    pcCp[(TFTP_MAX_PATH_LENGTH - 1)] = '\0';

    pcCp += (strlen((char *)szFileName) + 1);
    if (ulMode == TRANSFER_MODE_BINARY) {
        (void)strncpy_s((char *)pcCp, TFTP_MAX_MODE_SIZE, "octet", (TFTP_MAX_MODE_SIZE - 1));
        pcCp[(TFTP_MAX_MODE_SIZE - 1)] = '\0';
    } else if (ulMode == TRANSFER_MODE_ASCII) {
        (void)strncpy_s((char *)pcCp, TFTP_MAX_MODE_SIZE, "netascii", (TFTP_MAX_MODE_SIZE - 1));
        pcCp[(TFTP_MAX_MODE_SIZE - 1)] = '\0';
    }

    pcCp += (strlen((char *)pcCp) + 1);

    return (pcCp - (s8_t *)pstPacket);
}

/* Function to recv a packet from server
  iSockNum - Socket Number
  pstServerAddr - Server address
  pulIgnorePkt - Ignore packet flag
  pstRecvBuf - received packet
  pulSize - Size of the packet
*/
u32_t lwip_tftp_recv_from_server(s32_t iSockNum, u32_t *pulSize, TFTPC_PACKET_S *pstRecvBuf, u32_t *pulIgnorePkt,
                                 struct sockaddr_in *pstServerAddr, TFTPC_PACKET_S *pstSendBuf)
{
    u32_t ulError;
    socklen_t slFromAddrLen;
    struct sockaddr_in stFromAddr;
    fd_set stReadfds;
    struct timeval stTimeout;
    u16_t usOpcode; /* Opcode value */
    s32_t iRet;

    slFromAddrLen = sizeof(stFromAddr);
    stTimeout.tv_sec = TFTPC_TIMEOUT_PERIOD;
    stTimeout.tv_usec = 0;

    /* wait for DATA packet */
    FD_ZERO(&stReadfds);
    FD_SET(iSockNum, &stReadfds);

    iRet = select((s32_t)(iSockNum + 1), &stReadfds, 0, 0, &stTimeout);
    if (iRet == -1) {
        return TFTPC_SELECT_ERROR;
    } else if (iRet == 0) {
        return TFTPC_TIMEOUT_ERROR;         /* Select timeout occured */
    }

    if (!FD_ISSET(iSockNum, &stReadfds)) {
        return TFTPC_TIMEOUT_ERROR;         /* FD not set*/
    }

    /* receive a packet from server */
    iRet = lwip_recvfrom(iSockNum, (s8_t *)pstRecvBuf, TFTP_PKTSIZE, 0,
                         (struct sockaddr *)&stFromAddr, &slFromAddrLen);
    if (iRet <= 0) {
        return TFTPC_RECVFROM_ERROR;
    }

    /* If received packet size < minimum packet size */
    if (iRet < TFTPC_FOUR) {
        /* Send Error packet to server */
        lwip_tftp_send_error(iSockNum,
                             TFTPC_PROTOCOL_PROTO_ERROR,
                             "Packet size < min size",
                             pstServerAddr, pstSendBuf);

        return TFTPC_PKT_SIZE_ERROR;
    }

    /* convert network opcode to host format after receive. */
    usOpcode = ntohs(pstRecvBuf->usOpcode);
    /* if this packet is ERROR packet */
    if (usOpcode == TFTPC_OP_ERROR) {
        ulError = ntohs (pstRecvBuf->u.stTFTP_Err.usErrNum);

        /*If the error is according to RFC,then convert to lwip error codes.
        Constant values are used in the cases as these error codes are as per
        the RFC, these are constant values returned by many standard TFTP
        serevrs.*/
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

        /* If length of error msg > 100 chars */
        pstRecvBuf->u.stTFTP_Err.ucErrMesg[TFTP_MAXERRSTRSIZE - 1] = '\0';

        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_recv_from_server : ERROR pkt received: %s\n",
            pstRecvBuf->u.stTFTP_Err.ucErrMesg));

        /* Now we get error block, so return. */
        return ulError;
    }

    /* Store the size of received block */
    *pulSize = (u32_t)iRet;

    /* If received packet is first block of data(for get operation) or if
       received packet is acknowledgement for write request (put operation)
       store the received port number */
    if (((usOpcode == TFTPC_OP_DATA) &&
         (ntohs(pstRecvBuf->u.stTFTP_Data.usBlknum) == 1)) ||
        ((usOpcode == TFTPC_OP_ACK) &&
         (ntohs(pstRecvBuf->u.usBlknum) == 0))) {
        /* If received packet from correct server */
        if (stFromAddr.sin_addr.s_addr == pstServerAddr->sin_addr.s_addr) {
            /* set the server port to received port */
            pstServerAddr->sin_port = stFromAddr.sin_port;
        } else {
            /* Received packet form wrong server. */
            LWIP_DEBUGF(TFTP_DEBUG,
                        ("lwip_tftp_recv_from_server : Received 1st packet from wrong Server or unknown server\n"));

            /* Set ignore packet flag */
            *pulIgnorePkt = 1;
        }
    } else {
        /* If not first packet, check if the received packet is from correct
           server and from correct port */
        if ((stFromAddr.sin_addr.s_addr != pstServerAddr->sin_addr.s_addr) ||
            (pstServerAddr->sin_port != stFromAddr.sin_port)) {
            /* Received packet form wrong server or wrong port.Ignore packet. */
            LWIP_DEBUGF(TFTP_DEBUG,
                        ("lwip_tftp_recv_from_server : Received a packet from wrong Server or unknown server\n"));

            /* Set ignore packet flag */
            *pulIgnorePkt = 1;
        }
    }

    return ERR_OK;
}

/* Function to send a packet to server
    iSockNum: Socket Number
    ulSize: Size of the packet
    pstSendBuf: Packet to send
    pstServerAddr: Server address
*/
u32_t lwip_tftp_send_to_server(s32_t iSockNum,
                               u32_t ulSize,
                               TFTPC_PACKET_S *pstSendBuf,
                               struct sockaddr_in *pstServerAddr)
{
    s32_t iRet;

    /* Send packet to server */
    iRet = lwip_sendto(iSockNum, (s8_t *)pstSendBuf,
                       (size_t)ulSize, 0,
                       (struct sockaddr *)pstServerAddr,
                       sizeof(struct sockaddr_in));
    /* Size of data sent not equal to size of packet */
    if ((iRet == TFTP_NULL_INT32) || ((u32_t)iRet != ulSize)) {
        return TFTPC_SENDTO_ERROR;
    }

    return ERR_OK;
}

/* lwip_tftp_validate_data_pkt
* Get the data block from the received packet
* @param Input iSockNum Socket Number
*                     pulSize: Size of received packet,
                       pstRecvBuf - received packet
                       usCurrBlk - Current block number
 * @param Output pulResendPkt - Resend packet flag
 * @return VOS_OK on success.else error code*/

u32_t lwip_tftp_validate_data_pkt(s32_t iSockNum,
                                  u32_t *pulSize,
                                  TFTPC_PACKET_S *pstRecvBuf,
                                  u16_t usCurrBlk,
                                  u32_t *pulResendPkt,
                                  struct sockaddr_in *pstServerAddr)
{
    fd_set stReadfds;
    struct timeval stTimeout;
    struct sockaddr_in stFromAddr;
    socklen_t ulFromAddrLen;
    s32_t iRecvLen = (s32_t)*pulSize;
    s32_t iError;
    u16_t usBlknum;
    u32_t ulLoopCnt = 0;

    ulFromAddrLen = sizeof(stFromAddr);

    /* Initialize from address to the server address at first */
    if (memcpy_s((void *)&stFromAddr, sizeof(struct sockaddr_in), (void *)pstServerAddr, sizeof(stFromAddr)) != 0) {
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : memcpy_s error\n"));
        return TFTPC_MEMCPY_FAILURE;
    }

    /* Get Block Number */
    usBlknum = ntohs(pstRecvBuf->u.stTFTP_Data.usBlknum);
    /* Now data blocks are not in sync. */
    if (usBlknum != usCurrBlk) {
        /* Set timeout value */
        stTimeout.tv_sec = 1;
        stTimeout.tv_usec = 0;

        /* Reset any stored packets. */
        FD_ZERO(&stReadfds);
        FD_SET(iSockNum, &stReadfds);

        iError = select((s32_t)(iSockNum + 1),
                        &stReadfds, 0, 0, &stTimeout);

        /* Loop to get the last data packet from the receive buffer */
        while ((iError != TFTP_NULL_INT32) && (iError != 0)) {
            ulLoopCnt++;

            /* MAX file size in TFTP is 32 MB.
                 Reason for keeping 75 here , is ((75*512=38400bytes)/1024) =  37MB. So the recv/Send
                 Loop can receive the complete MAX message from the network.
            */
            if (ulLoopCnt > TFTPC_MAX_WAIT_IN_LOOP) {
                LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : unexpected packets are received repeatedly\n"));
                *pulSize = TFTP_NULL_UINT32;
                return TFTPC_PKT_SIZE_ERROR;
            }

            FD_ZERO(&stReadfds);
            FD_SET(iSockNum, &stReadfds);

            iRecvLen = lwip_recvfrom(iSockNum,
                                     (s8_t *)pstRecvBuf,
                                     TFTP_PKTSIZE, 0,
                                     (struct sockaddr *)&stFromAddr,
                                     &ulFromAddrLen);
            if (iRecvLen == -1) {
                *pulSize = TFTP_NULL_UINT32;

                /* return from the function, recvfrom operation failed */
                return TFTPC_RECVFROM_ERROR;
            }

            stTimeout.tv_sec = 1;
            stTimeout.tv_usec = 0;
            iError = select((s32_t)(iSockNum + 1), &stReadfds, 0, 0, &stTimeout);
        }

        /* If received packet size < minimum packet size */
        if (iRecvLen < TFTPC_FOUR) {
            return TFTPC_PKT_SIZE_ERROR;
        }

        /* Check if the received packet is from correct server and from
           correct port
           */
        if ((stFromAddr.sin_addr.s_addr != pstServerAddr->sin_addr.s_addr) ||
            (pstServerAddr->sin_port != stFromAddr.sin_port)) {
            /* resend ack packet */
            *pulResendPkt = 1;
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : Received pkt from unknown server\n"));

            return ERR_OK;
        }

        /* if this packet is not DATA packet */
        if (TFTPC_OP_DATA != ntohs(pstRecvBuf->usOpcode)) {
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_validate_data_pkt : Received pkt not a DATA pkt\n"));

            /* return from the function, incorrect packet received,
               expected packet is data packet */
            return TFTPC_PROTO_ERROR;
        }

        usBlknum = ntohs(pstRecvBuf->u.stTFTP_Data.usBlknum);
        /* if we now have the earlier data packet, then the host probably
           never got our acknowledge packet, now we will send it again. */
        if (usBlknum == (usCurrBlk - 1)) {
            /* resend ack packet */
            *pulResendPkt = 1;
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

/* Send an error packet to the server
    iSockNum : Socket Number
    ulError: Error code
    szErrMsg - Error message
    pstServerAddr - Server address
*/
static void lwip_tftp_send_error(s32_t iSockNum, u32_t ulError, const char *szErrMsg,
                                 struct sockaddr_in *pstServerAddr, TFTPC_PACKET_S *pstSendBuf)
{
    u16_t usOpCode = TFTPC_OP_ERROR;

    if (memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S)) != 0) {
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_send_error : memset_s error\n"));
        return;
    }

    /* Set up the send buffer */
    pstSendBuf->usOpcode = htons(usOpCode);
    pstSendBuf->u.stTFTP_Err.usErrNum = htons((u16_t)ulError);

    if (strncpy_s((char *)(pstSendBuf->u.stTFTP_Err.ucErrMesg), TFTP_MAXERRSTRSIZE,
                  (char *)szErrMsg, (TFTP_MAXERRSTRSIZE - 1)) != EOK) {
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_send_error : strncpy_s error\n"));
        return;
    }
    pstSendBuf->u.stTFTP_Err.ucErrMesg[(TFTP_MAXERRSTRSIZE - 1)] = '\0';

    /* Send to server */
    if (lwip_tftp_send_to_server(iSockNum,
                                 sizeof(TFTPC_PACKET_S),
                                 pstSendBuf,
                                 pstServerAddr) != ERR_OK) {
        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_send_to_server error."));
        return;
    }
}

/* INTEFACE to get a file using filename
    ulHostAddr - IP address of Host
    szSrcFileName - Source file
    szDestDirPath - Destination file path
*/
u32_t lwip_tftp_get_file_by_filename(u32_t ulHostAddr,
                                     u16_t usTftpServPort,
                                     u8_t ucTftpTransMode,
                                     s8_t *szSrcFileName,
                                     s8_t *szDestDirPath)
{
    s32_t iSockNum = TFTP_NULL_INT32;
    u32_t ulSrcStrLen;
    u32_t ulDestStrLen;
    u32_t ulSize;
    u32_t ulRecvSize = TFTP_NULL_UINT32;
    s32_t iErrCode;
    u32_t ulErrCode;
    u16_t usReadReq;
    u16_t usTempServPort;
    s8_t *pszTempDestName = NULL;
    s8_t *szTempSrcName = NULL;
    u32_t ulCurrBlk = 1;
    u32_t ulResendPkt = 0; /*Resend the previous packet*/
    u32_t ulIgnorePkt = 0; /*Ignore received packet*/
    u32_t ulTotalTime = 0;
    u32_t isLocalFileOpened = false;

    TFTPC_PACKET_S *pstSendBuf = NULL;
    TFTPC_PACKET_S *pstRecvBuf = NULL;
    struct sockaddr_in stServerAddr;
    struct stat sb;
    u32_t IsDirExist = 0;
    s32_t fp = -1;

    /*Validate the parameters*/
    if ((szSrcFileName == NULL) || (szDestDirPath == NULL)) {
        return TFTPC_INVALID_PARAVALUE;
    }

    if ((ucTftpTransMode != TRANSFER_MODE_BINARY) && (ucTftpTransMode != TRANSFER_MODE_ASCII)) {
        return TFTPC_INVALID_PARAVALUE;
    }

    /*check IP address not within ( 1.0.0.0 - 126.255.255.255 )
    and ( 128.0.0.0 - 223.255.255.255 ) range.*/
    if (!(((ulHostAddr >= TFTPC_IP_ADDR_MIN) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_RESV)) ||
          ((ulHostAddr >= TFTPC_IP_ADDR_CLASS_B) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_CLASS_DE)))) {
        return TFTPC_IP_NOT_WITHIN_RANGE;
    }

    /*Check validity of source filename*/
    ulSrcStrLen = strlen((char *)szSrcFileName);
    if ((ulSrcStrLen == 0) || (ulSrcStrLen >= TFTP_MAX_PATH_LENGTH)) {
        return TFTPC_SRC_FILENAME_LENGTH_ERROR;
    }

    /*Check validity of destination path*/
    ulDestStrLen = strlen((char *)szDestDirPath);
    if ((ulDestStrLen >= TFTP_MAX_PATH_LENGTH) || (ulDestStrLen == 0)) {
        return TFTPC_DEST_PATH_LENGTH_ERROR;
    }

    pstSendBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstSendBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR;
    }

    pstRecvBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstRecvBuf == NULL) {
        mem_free(pstSendBuf);
        return TFTPC_MEMALLOC_ERROR;
    }

    pszTempDestName = (s8_t *)mem_malloc(TFTP_MAX_PATH_LENGTH);
    if (pszTempDestName == NULL) {
        mem_free(pstSendBuf);
        mem_free(pstRecvBuf);
        return TFTPC_MEMALLOC_ERROR;
    }

    /* First time initialize the buffers */
    (void)memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));
    (void)memset_s((void *)pstRecvBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    /*If given src filename is a relative path extract
    the file name from the path*/
    if ((0 != strchr((char *)szSrcFileName, '/')) || (0 != strchr((char *)szSrcFileName, '\\'))) {
        /*Move to the end of the src file path*/
        szTempSrcName = szSrcFileName + (ulSrcStrLen - 1);

        while (((*(szTempSrcName - 1) != '/') &&
                (*(szTempSrcName - 1) != '\\')) &&
               (szTempSrcName != szSrcFileName)) {
            szTempSrcName--;
        }

        /*Get length of the extracted src filename*/
        ulSrcStrLen = strlen((char *)szTempSrcName);
    } else {
        /*If not a relative src path use the given src filename*/
        szTempSrcName = szSrcFileName;
    }

    (void)memset_s(pszTempDestName, TFTP_MAX_PATH_LENGTH, 0, TFTP_MAX_PATH_LENGTH);
    if (strncpy_s((char *)pszTempDestName, TFTP_MAX_PATH_LENGTH, (char *)szDestDirPath, TFTP_MAX_PATH_LENGTH - 1) !=
        0) {
        ulErrCode = TFTPC_MEMCPY_FAILURE;
        goto err_handler;
    }
    pszTempDestName[TFTP_MAX_PATH_LENGTH - 1] = '\0';

    if (stat((char *)pszTempDestName, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        IsDirExist = 1;
    }

    if (IsDirExist == 1) {
        /*The filename is not present concat source filename and try*/
        if ((ulDestStrLen + ulSrcStrLen) >= TFTP_MAX_PATH_LENGTH) {
            /*If concatenating src filename exceeds 256 bytes*/
            ulErrCode = TFTPC_DEST_PATH_LENGTH_ERROR;
            goto err_handler;
        }

        /*Check if / present at end of string*/
        if ((pszTempDestName[ulDestStrLen - 1] != '/') &&
            (pszTempDestName[ulDestStrLen - 1] != '\\')) {
            if ((ulDestStrLen + ulSrcStrLen + 1) >= TFTP_MAX_PATH_LENGTH) {
                /*If concatenating src filename exceeds 256 bytes*/
                ulErrCode = TFTPC_DEST_PATH_LENGTH_ERROR;
                goto err_handler;
            }

            /*If not present concat / to the path*/
            if (strncat_s((char *)pszTempDestName, (TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName)),
                          "/", TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName) - 1) != 0) {
                ulErrCode = TFTPC_ERROR_NOT_DEFINED;
                goto err_handler;
            }
        }

        /*Concatenate src filename to destination path*/
        if (strncat_s((char *)pszTempDestName, (TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName)),
                      (char *)szTempSrcName, TFTP_MAX_PATH_LENGTH - strlen((char *)pszTempDestName) - 1) != 0) {
            ulErrCode = TFTPC_ERROR_NOT_DEFINED;
            goto err_handler;
        }
    }

    ulErrCode = lwip_tftp_create_bind_socket(&iSockNum);
    if (ulErrCode != ERR_OK) {
        goto err_handler;
    }

    if (usTftpServPort == 0) {
        usTftpServPort = TFTPC_SERVER_PORT;
    }

    usTempServPort = usTftpServPort;

    /* set server IP address */
    (void)memset_s(&stServerAddr, sizeof(stServerAddr), 0, sizeof(stServerAddr));
    stServerAddr.sin_family = AF_INET;
    stServerAddr.sin_port = htons(usTempServPort);
    stServerAddr.sin_addr.s_addr = htonl(ulHostAddr);

    /* Make a request packet - TFTPC_OP_RRQ */
    ulSize = (u32_t)lwip_tftp_make_tftp_packet(TFTPC_OP_RRQ, szSrcFileName,
                                               (u32_t)ucTftpTransMode,
                                               pstSendBuf);
    ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                         pstSendBuf, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        /* send to server failed */
        (void)lwip_close(iSockNum);
        goto err_handler;
    }

    for (;;) {
        if (ulIgnorePkt > 0) {
            ulIgnorePkt = 0;
        }

        ulErrCode = lwip_tftp_recv_from_server(iSockNum, &ulRecvSize, pstRecvBuf,
                                               &ulIgnorePkt, &stServerAddr, pstSendBuf);
        /* If select timeout occured */
        if (ulErrCode == TFTPC_TIMEOUT_ERROR) {
            ulTotalTime++;
            if (ulTotalTime < TFTPC_MAX_SEND_REQ_ATTEMPTS) {
                /* Max attempts not reached. Resend packet */
                ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                     pstSendBuf, &stServerAddr);
                if (ulErrCode != ERR_OK) {
                    (void)lwip_close(iSockNum);
                    if (isLocalFileOpened == true) {
                        close(fp);
                    }
                    goto err_handler;
                }

                continue;
            } else {
                /* return from the function, max attempts limit reached */
                (void)lwip_close(iSockNum);
                if (isLocalFileOpened == true) {
                    close(fp);
                }
                ulErrCode = TFTPC_TIMEOUT_ERROR;
                goto err_handler;
            }
        } else if (ulErrCode != ERR_OK) {
            (void)lwip_close(iSockNum);
            if (isLocalFileOpened == true) {
                close(fp);
            }
            goto err_handler;
        }

        /* Now we have receive block from different server. */
        if (ulIgnorePkt > 0) {
            /*Continue without processing this block. */
            continue;
        }

        /* if this packet is unkonwn or incorrect packet */
        if (ntohs (pstRecvBuf->usOpcode) != TFTPC_OP_DATA) {
            /* Send error packet to server */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Protocol error.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            if (isLocalFileOpened == true) {
                close(fp);
            }

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Received pkt not DATA pkt\n"));

            ulErrCode = TFTPC_PROTO_ERROR;
            goto err_handler;
        }

        /* Now the number of tries will be reset. */
        ulTotalTime = 0;

        /* Validate received  DATA packet. */
        ulErrCode = lwip_tftp_validate_data_pkt(iSockNum, &ulRecvSize,
                                                pstRecvBuf, (u16_t)ulCurrBlk,
                                                &ulResendPkt,
                                                &stServerAddr);
        if (ulErrCode != ERR_OK) {
            /* Send Error packet to server */
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

        /* Received previous data block again. Resend last packet */
        if (ulResendPkt > 0) {
            /* Now set ulResendPkt to 0 to send the last packet. */
            ulResendPkt = 0;
            ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                 pstSendBuf, &stServerAddr);
            if (ulErrCode != ERR_OK) {
                (void)lwip_close(iSockNum);
                if (isLocalFileOpened == true) {
                    close(fp);
                }

                goto err_handler;
            }

            /* Continue in loop to send last packet again. */
            continue;
        }

        /* Get the size of the data block received */
        ulRecvSize -= TFTP_HDRSIZE;

        /* Check if the size of the received data block > max size */
        if (ulRecvSize > TFTP_BLKSIZE) {
            /* Send Error packet to server */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Packet size > max size",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            if (isLocalFileOpened == true) {
                close(fp);
            }

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Packet size > max size\n"));

            ulErrCode = TFTPC_PKT_SIZE_ERROR;
            goto err_handler;
        }

        usReadReq = (u16_t)TFTPC_OP_ACK;
        pstSendBuf->usOpcode = htons(usReadReq);
        pstSendBuf->u.usBlknum = htons((u16_t)ulCurrBlk);
        ulSize = TFTP_HDRSIZE;

        if (isLocalFileOpened == false) {
            fp = open((char *)pszTempDestName, (O_WRONLY | O_CREAT | O_TRUNC), DEFFILEMODE);
            if (fp == TFTP_NULL_INT32) {
                ulErrCode = TFTPC_FILECREATE_ERROR;
                (void)lwip_close(iSockNum);
                goto err_handler;
            }
            isLocalFileOpened = true;
        }

        if (ulRecvSize != TFTP_BLKSIZE) {
            (void)lwip_tftp_send_to_server(iSockNum, ulSize, pstSendBuf, &stServerAddr);

            /* If the received packet has only header and do not have payload, the return failure */
            if (ulRecvSize != 0) {
                /* Write the last packet to the file */
                iErrCode = write(fp, (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize);
                if (ulRecvSize != (u32_t)iErrCode) {
                    /* Write to file failed. */
                    lwip_tftp_send_error(iSockNum,
                                         TFTPC_PROTOCOL_USER_DEFINED,
                                         "Write to file failed",
                                         &stServerAddr, pstSendBuf);

                    (void)lwip_close(iSockNum);
                    close(fp);

                    /* return from the function, file write failed */
                    ulErrCode = TFTPC_FILEWRITE_ERROR;
                    goto err_handler;
                }
            }

            /* Now free allocated resourdes and return,
              data block receiving is already completed */
            (void)lwip_close(iSockNum);
            close(fp);
            ulErrCode = ERR_OK;
            goto err_handler;
        }

        ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                             pstSendBuf, &stServerAddr);
        if (ulErrCode != ERR_OK) {
            (void)lwip_close(iSockNum);
            close(fp);
            goto err_handler;
        }

        iErrCode = write(fp, (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize);
        if (ulRecvSize != (u32_t)iErrCode) {
            /* Write to file failed. */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_USER_DEFINED,
                                 "Write to file failed",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            close(fp);

            /* return from the function, file write failed */
            ulErrCode = TFTPC_FILEWRITE_ERROR;
            goto err_handler;
        }

        /* form the ACK packet for the DATA packet received */
        /* Go to the next packet no. */
        ulCurrBlk++;

        /* if the file is too big, exit */
        if (ulCurrBlk > TFTP_MAX_BLK_NUM) {
            /* Send error packet to server */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_USER_DEFINED,
                                 "File is too big.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            close(fp);
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Data block number exceeded max value\n"));

            ulErrCode = TFTPC_FILE_TOO_BIG;
            goto err_handler;
        }
    }

err_handler:
    mem_free(pstSendBuf);
    mem_free(pstRecvBuf);
    mem_free(pszTempDestName);
    return ulErrCode;
}


/* INTERFACE Function to put a file using filename
  ulHostAddr: IP address of Host
  szSrcFileName: Source file
  szDestDirPath: Destination file path
*/
u32_t lwip_tftp_put_file_by_filename(u32_t ulHostAddr, u16_t usTftpServPort, u8_t ucTftpTransMode,
                                     s8_t *szSrcFileName, s8_t *szDestDirPath)
{
    u32_t ulSrcStrLen;
    u32_t ulDestStrLen;
    s32_t iSockNum = TFTP_NULL_INT32;
    s32_t iErrCode;
    u32_t ulErrCode;
    u16_t usTempServPort;
    TFTPC_PACKET_S *pstSendBuf = NULL;
    u16_t usReadReq;
    u32_t ulSize;
    s8_t *pucBuffer = 0;
    s8_t *szTempDestName = NULL;

    /*Initialize the block number*/
    u16_t usCurrBlk = 0;
    struct sockaddr_in stServerAddr;
    struct stat buffer;
    s32_t fp = -1;

    /* Validate parameters */
    if ((szSrcFileName == NULL) || (szDestDirPath == NULL)) {
        return TFTPC_INVALID_PARAVALUE;
    }

    if ((ucTftpTransMode != TRANSFER_MODE_BINARY) && (ucTftpTransMode != TRANSFER_MODE_ASCII)) {
        return TFTPC_INVALID_PARAVALUE;
    }

    /*check IP address not within ( 1.0.0.0 - 126.255.255.255 )
      and ( 128.0.0.0 - 223.255.255.255 ) range.*/
    if (!(((ulHostAddr >= TFTPC_IP_ADDR_MIN) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_RESV)) ||
          ((ulHostAddr >= TFTPC_IP_ADDR_CLASS_B) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_CLASS_DE)))) {
        return TFTPC_IP_NOT_WITHIN_RANGE;
    }

    /* If Src filename is empty or exceeded max length */
    ulSrcStrLen = strlen((char *)szSrcFileName);
    if ((ulSrcStrLen == 0) || (ulSrcStrLen >= TFTP_MAX_PATH_LENGTH)) {
        return TFTPC_SRC_FILENAME_LENGTH_ERROR;
    }

    /* Check if source file exists */
    if (stat((char *)szSrcFileName, &buffer) != 0) {
        return TFTPC_FILE_NOT_EXIST;
    }

    /* Check if the file is too big */
    if (buffer.st_size >= (off_t)(TFTP_MAX_BLK_NUM * TFTP_BLKSIZE)) {
        return TFTPC_FILE_TOO_BIG;
    }

    /* Check validity of destination path */
    ulDestStrLen = strlen((char *)szDestDirPath);
    /* If dest path length exceeded max value */
    if (ulDestStrLen >= TFTP_MAX_PATH_LENGTH) {
        return TFTPC_DEST_PATH_LENGTH_ERROR;
    }

    pstSendBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstSendBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR;
    }

    /* First time intialize the buffer */
    (void)memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    /* The destination path can only be one of the following:
    1. Only filename
    2. Relative path WITH filename
    3. Empty string
    */
    if (ulDestStrLen != 0) {
        /* If not empty string use the Destination path name */
        szTempDestName = szDestDirPath;
    } else {
        /* If destination directory is empty string use source filename
        If given src filename is a relative path extract the file name
        from the path */
        if ((strchr((char *)szSrcFileName, '/') != 0) ||
            (strchr((char *)szSrcFileName, '\\') != 0)) {
            /* Move to the end of the src file path */
            szTempDestName = szSrcFileName + (ulSrcStrLen - 1);

            while (((*(szTempDestName - 1) != '/') && (*(szTempDestName - 1) != '\\')) &&
                   (szTempDestName != szSrcFileName)) {
                szTempDestName--;
            }
        } else {
            /* If not a relative src path use the given src filename */
            szTempDestName = szSrcFileName;
        }
    }

    /* Create a socket and bind it to an available port number */
    ulErrCode = lwip_tftp_create_bind_socket(&iSockNum);
    if (ulErrCode != EOK) {
        /* Create and Bind socket failed */
        goto err_handler;
    }

    if (usTftpServPort == 0) {
        usTftpServPort = TFTPC_SERVER_PORT;
    }

    usTempServPort = usTftpServPort;

    /* set server internet address */
    (void)memset_s(&stServerAddr, sizeof(stServerAddr), 0, sizeof(stServerAddr));
    stServerAddr.sin_family = AF_INET;
    stServerAddr.sin_port = htons(usTempServPort);
    stServerAddr.sin_addr.s_addr = htonl(ulHostAddr);

    /* Make request packet - TFTPC_OP_WRQ */
    ulSize = (u32_t)lwip_tftp_make_tftp_packet(TFTPC_OP_WRQ,
                                               szTempDestName,
                                               ucTftpTransMode,
                                               pstSendBuf);

    ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                         pstSendBuf, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        /* Send to server error */
        (void)lwip_close(iSockNum);

        goto err_handler;
    }

    /* Send the request packet */
    ulErrCode = lwip_tftp_inner_put_file(iSockNum, pstSendBuf, ulSize,
                                         usCurrBlk, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        /* Send request packet failed */
        (void)lwip_close(iSockNum);

        LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_put_file_by_filename : Failed to send request packet\n"));

        goto err_handler;
    }

    /* Create buffer block size */
    pucBuffer = mem_malloc(TFTP_BLKSIZE);
    if (pucBuffer == NULL) {
        /* Memory allocation failed */
        lwip_tftp_send_error(iSockNum,
                             TFTPC_PROTOCOL_USER_DEFINED,
                             "Memory allocation failed.",
                             &stServerAddr, pstSendBuf);

        (void)lwip_close(iSockNum);
        ulErrCode = TFTPC_MEMALLOC_ERROR;
        goto err_handler;
    }

    (void)memset_s((void *)pucBuffer, TFTP_BLKSIZE, 0, TFTP_BLKSIZE);

    fp = open((char *)szSrcFileName, O_RDONLY);
    if (TFTP_NULL_INT32 == fp) {
        /* If file could not be opened send error to server */
        lwip_tftp_send_error(iSockNum,
                             TFTPC_PROTOCOL_USER_DEFINED,
                             "File open error.",
                             &stServerAddr, pstSendBuf);

        (void)lwip_close(iSockNum);
        mem_free(pucBuffer);

        ulErrCode = TFTPC_FILEOPEN_ERROR;
        goto err_handler;
    }

    iErrCode = read(fp, pucBuffer, TFTP_BLKSIZE);
    if (iErrCode < 0) {
        /* If failed to read from file */
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

    /* Read from source file and send to server */
    /* To send empty packet to server when file is a 0 byte file */
    do {
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

        /* Increment block number */
        usCurrBlk++;

        ulSize = (u32_t)iErrCode + TFTP_HDRSIZE;

        /* Form the DATA packet */
        usReadReq = (u16_t)TFTPC_OP_DATA;
        pstSendBuf->usOpcode = htons(usReadReq);
        pstSendBuf->u.stTFTP_Data.usBlknum = htons(usCurrBlk);
        if (memcpy_s((void *)pstSendBuf->u.stTFTP_Data.ucDataBuf, TFTP_BLKSIZE,
                     (void *)pucBuffer, (u32_t)iErrCode) != EOK) {
            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            goto err_handler;
        }

        ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                             pstSendBuf, &stServerAddr);
        if ((ulErrCode != ERR_OK) || (memset_s((void *)pucBuffer, TFTP_BLKSIZE, 0, TFTP_BLKSIZE) != 0)) {
            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            goto err_handler;
        }

        /* Read a block from the file to buffer */
        iErrCode = read(fp, pucBuffer, TFTP_BLKSIZE);
        if (iErrCode < 0) {
            /*If failed to read from file*/
            lwip_tftp_send_error(iSockNum, TFTPC_PROTOCOL_USER_DEFINED, "File read error.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            ulErrCode = TFTPC_FILEREAD_ERROR;
            goto err_handler;
        }

        /* Send the request packet */
        ulErrCode = lwip_tftp_inner_put_file(iSockNum, pstSendBuf, ulSize,
                                             usCurrBlk, &stServerAddr);
        if (ulErrCode != ERR_OK) {
            /* Sending buffer contents failed */
            (void)lwip_close(iSockNum);
            close(fp);
            mem_free(pucBuffer);
            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_put_file_by_filename : Sending file to server failed\n"));
            goto err_handler;
        }
    } while (ulSize == (TFTP_BLKSIZE + TFTP_HDRSIZE));

    /* Transfer of data is finished */
    (void)lwip_close(iSockNum);
    close(fp);
    mem_free(pucBuffer);

    ulErrCode = ERR_OK;
err_handler:
    mem_free(pstSendBuf);
    return ulErrCode;
}

/* Put file function
    iSockNum: Socket ID
    pstSendBuf: Packet to send to server
    ulSendSize: Packet length
    usCurrBlk: Current block number
    pstServerAddr: Server address
*/
u32_t lwip_tftp_inner_put_file(s32_t iSockNum,
                               TFTPC_PACKET_S *pstSendBuf,
                               u32_t ulSendSize,
                               u16_t usCurrBlk,
                               struct sockaddr_in *pstServerAddr)
{
    u32_t ulPktSize;
    u32_t ulError;
    s32_t iError;
    int iRecvLen = 0;
    socklen_t iFromAddrLen;
    u32_t ulTotalTime = 0;
    fd_set stReadfds;
    struct sockaddr_in stFromAddr;
    struct timeval stTimeout;
    TFTPC_PACKET_S *pstRecvBuf = NULL;
    u32_t ulIgnorePkt = 0;
    u16_t usBlknum;
    u32_t ulLoopCnt = 0;

    iFromAddrLen = sizeof(stFromAddr);

    pstRecvBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstRecvBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR;
    }

    /* First time intialize the buffer */
    (void)memset_s((void *)pstRecvBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    /* Initialize from address to the server address at first */
    if (memcpy_s((void *)&stFromAddr, sizeof(struct sockaddr_in),
                 (void *)pstServerAddr, sizeof(stFromAddr)) != EOK) {
        ulError = TFTPC_MEMCPY_FAILURE;
        goto err_handler;
    }

    for (;;) {
        ulError = lwip_tftp_recv_from_server(iSockNum, &ulPktSize,
                                             pstRecvBuf, &ulIgnorePkt,
                                             pstServerAddr, pstSendBuf);
        /* If select timeout occured */
        if (ulError == TFTPC_TIMEOUT_ERROR) {
            ulTotalTime++;
            if (ulTotalTime < TFTPC_MAX_SEND_REQ_ATTEMPTS) {
                /*Max attempts not reached. Resend packet*/
                ulError = lwip_tftp_send_to_server(iSockNum, ulSendSize,
                                                   pstSendBuf, pstServerAddr);
                if (ulError != ERR_OK) {
                    goto err_handler;
                }

                continue;
            } else {
                /* return from the function, max attempts limit reached */
                ulError = TFTPC_TIMEOUT_ERROR;
                goto err_handler;
            }
        } else if (ulError != ERR_OK) {
            /* return from the function, RecvFromServer failed */
            goto err_handler;
        }

        /* If Received packet from another server */
        if (ulIgnorePkt > 0) {
            /* The packet that is received is to be ignored.
               So continue without processing it. */
            ulIgnorePkt = 0;
            continue;
        }

        /* if this packet is unknown or incorrect packet */
        if (TFTPC_OP_ACK != ntohs(pstRecvBuf->usOpcode)) {
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Protocol error.",
                                 pstServerAddr, pstSendBuf);

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_inner_put_file : Received pkt not Ack pkt\n"));

            ulError = TFTPC_PROTO_ERROR;
            goto err_handler;
        }

        ulTotalTime = 0;

        /* if the packet is acknowledge packet */
        usBlknum = ntohs(pstRecvBuf->u.usBlknum);
        iRecvLen = (int)ulPktSize;

        /* If not correct block no. */
        if (usBlknum != usCurrBlk) {
            /* we are not in sync now */
            /* reset any collected packets. */
            stTimeout.tv_sec = 1;
            stTimeout.tv_usec = 0;

            FD_ZERO(&stReadfds);
            FD_SET(iSockNum, &stReadfds);

            /*
            Need to take care of timeout scenario in Select call.
            Since the socket used is blocking, if select timeout occurs,
            the following recvfrom will block indefinitely.
            */
            iError = select((s32_t)(iSockNum + 1), &stReadfds, 0, 0, &stTimeout);

            /* Loop to get the last data packet from the receive buffer */
            while ((iError != -1) && (iError != 0)) {
                ulLoopCnt++;

                /* MAX file size in TFTP is 32 MB.
                     Reason for keeping 75 here , is ((75*512=38400bytes)/1024) =  37MB. So the recv/Snd
                     Loop can receive the complete MAX message from the network.
                */
                if (ulLoopCnt > TFTPC_MAX_WAIT_IN_LOOP) {
                    LWIP_DEBUGF(TFTP_DEBUG,
                                ("lwip_tftp_inner_put_file : unexpected packets are received repeatedly\n"));
                    ulError = TFTPC_PKT_SIZE_ERROR;
                    goto err_handler;
                }

                FD_ZERO(&stReadfds);
                FD_SET(iSockNum, &stReadfds);
                iRecvLen = lwip_recvfrom(iSockNum,
                                         (s8_t *)pstRecvBuf,
                                         TFTP_PKTSIZE, 0,
                                         (struct sockaddr *)&stFromAddr,
                                         &iFromAddrLen);
                if (TFTP_NULL_INT32 == iRecvLen) {
                    ulError = TFTPC_RECVFROM_ERROR;
                    goto err_handler;
                }

                stTimeout.tv_sec = 1;
                stTimeout.tv_usec = 0;
                iError = select((s32_t)(iSockNum + 1),
                                &stReadfds, 0, 0, &stTimeout);
            }

            /* If a new packet is not received then donot change the byte order
             * as it has already been done
             */
            /* If received packet size < minimum packet size */
            if (iRecvLen < TFTPC_FOUR) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Packet size < min packet size",
                                     pstServerAddr, pstSendBuf);

                LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_inner_put_file : Received pkt not Ack pkt\n"));

                ulError = TFTPC_PKT_SIZE_ERROR;
                goto err_handler;
            }

            /* Check if the received packet is from correct server and from
               correct port
               */
            if ((stFromAddr.sin_addr.s_addr != pstServerAddr->sin_addr.s_addr) ||
                (pstServerAddr->sin_port != stFromAddr.sin_port)) {
                /* This ACK packet is invalid. Just ignore it. */
                LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_inner_put_file : Received pkt from unknown server\n"));
                continue;
            }

            /* if this packet is not ACK packet */
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
            * In this case we have received a duplicate ACK for data block.
            * (ACK for this data block was aready received earlier)
            * In this case we have usRecvBlkNum == (usNextBlkNum - 1).
            * This could mean that:
            * (i) last data packet that was sent was not received at server side
            * (ii) Acknowledgement of peer side is delayed.
            *
            * In this case, this duplicate ACK will be ignored and return to the
            * state machine to initiate a receive of this data packet.
            */
            if ((usCurrBlk - 1) == usBlknum) {
                /* This ACK packet is invalid. Just ignore it. */
                continue;
            }

            /* Now check the block number with current block.
            * If it is not the previous block and the current block,
            * then it is an unexpected packet.
            */
            if (usBlknum != usCurrBlk) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Received unexpected packet",
                                     pstServerAddr, pstSendBuf);

                LWIP_DEBUGF(TFTP_DEBUG,
                            ("lwip_tftp_inner_put_file : Received DATA pkt no. %"S32_F"instead of pkt no. %"S32_F"\n",
                                usBlknum, usCurrBlk));

                ulError = TFTPC_SYNC_FAILURE;
                goto err_handler;
            }
        }

        ulError = ERR_OK;
        goto err_handler;
    }

err_handler:
    mem_free(pstRecvBuf);
    return ulError;
}

#ifdef TFTP_TO_RAWMEM
/* INTEFACE to get a file using filename
    ulHostAddr - IP address of Host
    szSrcFileName - Source file
    szDestMemAddr - The target memory address in the client

    Example :
    ulHostAddr = ntohl(inet_addr ("192.168.1.3"));
    lwip_tftp_get_file_by_filename_to_rawmem(ulHostAddr, "/ramfs/vs_server.bin", memaddr, &filelen);
*/
u32_t lwip_tftp_get_file_by_filename_to_rawmem(u32_t ulHostAddr,
                                               u16_t usTftpServPort,
                                               u8_t ucTftpTransMode,
                                               s8_t *szSrcFileName,
                                               s8_t *szDestMemAddr,
                                               u32_t *ulFileLength)
{
    s32_t iSockNum = TFTP_NULL_INT32;
    u32_t ulSrcStrLen;
    u32_t lDestStrLen;
    u32_t ulSize;
    u32_t ulRecvSize = TFTP_NULL_UINT32;
    s32_t iErrCode;
    u32_t ulErrCode;
    u16_t usReadReq;
    u16_t usTempServPort;
    u32_t ulCurrBlk = 1;
    u32_t ulResendPkt = 0; /* Resend the previous packet */
    u32_t ulIgnorePkt = 0; /* Ignore received packet */
    u32_t ulTotalTime = 0;

    TFTPC_PACKET_S *pstSendBuf = NULL;
    TFTPC_PACKET_S *pstRecvBuf = NULL;
    struct sockaddr_in stServerAddr;
    u32_t ulMemOffset = 0;

    /* Validate the parameters */
    if ((szSrcFileName == NULL) || (szDestMemAddr == NULL) || (*ulFileLength == 0)) {
        return TFTPC_INVALID_PARAVALUE;
    }

    if ((ucTftpTransMode != TRANSFER_MODE_BINARY) && (ucTftpTransMode != TRANSFER_MODE_ASCII)) {
        return TFTPC_INVALID_PARAVALUE;
    }

    /* check IP address not within ( 1.0.0.0 - 126.255.255.255 )
     and ( 128.0.0.0 - 223.255.255.255 ) range. */
    if (!(((ulHostAddr >= TFTPC_IP_ADDR_MIN) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_RESV)) ||
          ((ulHostAddr >= TFTPC_IP_ADDR_CLASS_B) &&
           (ulHostAddr <= TFTPC_IP_ADDR_EX_CLASS_DE)))) {
        return TFTPC_IP_NOT_WITHIN_RANGE;
    }

    /*Check validity of source filename*/
    ulSrcStrLen = strlen(szSrcFileName);
    if ((ulSrcStrLen == 0) || (ulSrcStrLen >= TFTP_MAX_PATH_LENGTH)) {
        return TFTPC_SRC_FILENAME_LENGTH_ERROR;
    }

    pstSendBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstSendBuf == NULL) {
        return TFTPC_MEMALLOC_ERROR;
    }

    pstRecvBuf = (TFTPC_PACKET_S *)mem_malloc(sizeof(TFTPC_PACKET_S));
    if (pstRecvBuf == NULL) {
        mem_free(pstSendBuf);
        return TFTPC_MEMALLOC_ERROR;
    }

    /* First time initialize the buffers */
    (void)memset_s((void *)pstSendBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));
    (void)memset_s((void *)pstRecvBuf, sizeof(TFTPC_PACKET_S), 0, sizeof(TFTPC_PACKET_S));

    ulErrCode = lwip_tftp_create_bind_socket(&iSockNum);
    if (ulErrCode != EOK) {
        goto err_handler;
    }

    if (usTftpServPort == 0) {
        usTftpServPort = TFTPC_SERVER_PORT;
    }

    usTempServPort = usTftpServPort;

    /* set server IP address */
    (void)memset_s(&stServerAddr, sizeof(stServerAddr), 0, sizeof(stServerAddr));
    stServerAddr.sin_family = AF_INET;
    stServerAddr.sin_port = htons(usTempServPort);
    stServerAddr.sin_addr.s_addr = htonl(ulHostAddr);

    /* Make a request packet - TFTPC_OP_RRQ */
    ulSize = (u32_t)lwip_tftp_make_tftp_packet(TFTPC_OP_RRQ, szSrcFileName, (u32_t)ucTftpTransMode, pstSendBuf);
    ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize, pstSendBuf, &stServerAddr);
    if (ulErrCode != ERR_OK) {
        /* send to server failed */
        (void)lwip_close(iSockNum);
        goto err_handler;
    }

    for (;;) {
        if (ulIgnorePkt > 0) {
            ulIgnorePkt = 0;
        }

        ulErrCode = lwip_tftp_recv_from_server(iSockNum, &ulRecvSize, pstRecvBuf, &ulIgnorePkt,
                                               &stServerAddr, pstSendBuf);
        /* If select timeout occured */
        if (ulErrCode == TFTPC_TIMEOUT_ERROR) {
            ulTotalTime++;
            if (ulTotalTime < TFTPC_MAX_SEND_REQ_ATTEMPTS) {
                /* Max attempts not reached. Resend packet */
                ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                     pstSendBuf, &stServerAddr);
                if (ulErrCode != ERR_OK) {
                    (void)lwip_close(iSockNum);
                    goto err_handler;
                }

                continue;
            } else {
                /* return from the function, max attempts limit reached */
                (void)lwip_close(iSockNum);
                ulErrCode = TFTPC_TIMEOUT_ERROR;
                goto err_handler;
            }
        } else if (ulErrCode != ERR_OK) {
            (void)lwip_close(iSockNum);
            goto err_handler;
        }

        /* Now we have receive block from different server. */
        if (ulIgnorePkt > 0) {
            /*Continue without processing this block. */
            continue;
        }

        /* if this packet is unkonwn or incorrect packet */
        if (ntohs (pstRecvBuf->usOpcode) != TFTPC_OP_DATA) {
            /* Send error packet to server */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Protocol error.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Received pkt not DATA pkt\n"));

            ulErrCode = TFTPC_PROTO_ERROR;
            goto err_handler;
        }

        /* Now the number of tries will be reset. */
        ulTotalTime = 0;

        /* Validate received  DATA packet. */
        ulErrCode = lwip_tftp_validate_data_pkt(iSockNum, &ulRecvSize,
                                                pstRecvBuf, (u16_t)ulCurrBlk,
                                                &ulResendPkt,
                                                &stServerAddr);
        if (ulErrCode != ERR_OK) {
            /* Send Error packet to server */
            if (ulErrCode != TFTPC_RECVFROM_ERROR) {
                lwip_tftp_send_error(iSockNum,
                                     TFTPC_PROTOCOL_PROTO_ERROR,
                                     "Received unexpected packet",
                                     &stServerAddr, pstSendBuf);
            }

            (void)lwip_close(iSockNum);

            goto err_handler;
        }

        /* Received previous data block again. Resend last packet */
        if (ulResendPkt > 0) {
            /* Now set ulResendPkt to 0 to send the last packet. */
            ulResendPkt = 0;
            ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                                 pstSendBuf, &stServerAddr);
            if (ulErrCode != ERR_OK) {
                (void)lwip_close(iSockNum);

                goto err_handler;
            }

            /* Continue in loop to send last packet again. */
            continue;
        }

        /* Get the size of the data block received */
        ulRecvSize -= TFTP_HDRSIZE;

        /* Check if the size of the received data block > max size */
        if (ulRecvSize > TFTP_BLKSIZE) {
            /* Send Error packet to server */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_PROTO_ERROR,
                                 "Packet size > max size",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Packet size > max size\n"));

            ulErrCode = TFTPC_PKT_SIZE_ERROR;
            goto err_handler;
        }

        usReadReq = (u16_t)TFTPC_OP_ACK;
        pstSendBuf->usOpcode = htons(usReadReq);
        pstSendBuf->u.usBlknum = htons((u16_t)ulCurrBlk);
        ulSize = TFTP_HDRSIZE;

        if (ulRecvSize != TFTP_BLKSIZE) {
            (void)lwip_tftp_send_to_server(iSockNum, ulSize,
                                           pstSendBuf, &stServerAddr);

            /* If the received packet has only header and do not have payload, the return failure */
            if (ulRecvSize != 0) {
                /* memcopy filed */
                if (*ulFileLength < (ulMemOffset + ulRecvSize)) {
                    ulErrCode = TFTPC_MEMCPY_FAILURE;
                    (void)lwip_close(iSockNum);
                    *ulFileLength = ulMemOffset;
                    goto err_handler;
                }
                /* copy the last packet to the memory */
                if (memcpy_s(szDestMemAddr + ulMemOffset, TFTP_MAX_BLK_NUM * TFTP_BLKSIZE,
                             (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize) != EOK) {
                    ulErrCode = TFTPC_MEMCPY_FAILURE;
                    (void)lwip_close(iSockNum);
                    *ulFileLength = ulMemOffset;
                    goto err_handler;
                }
                ulMemOffset += ulRecvSize;
            }

            /* Now free allocated resourdes and return,
              data block receiving is already completed */
            (void)lwip_close(iSockNum);
            ulErrCode = ERR_OK;
            *ulFileLength = ulMemOffset;
            goto err_handler;
        }

        ulErrCode = lwip_tftp_send_to_server(iSockNum, ulSize,
                                             pstSendBuf, &stServerAddr);
        if (ulErrCode != ERR_OK) {
            (void)lwip_close(iSockNum);
            goto err_handler;
        }

        /* memcopy filed */
        if (*ulFileLength < ulRecvSize * ulCurrBlk) {
            ulErrCode = TFTPC_MEMCPY_FAILURE;
            (void)lwip_close(iSockNum);
            *ulFileLength = ulMemOffset;
            goto err_handler;
        }
        if (memcpy_s(szDestMemAddr + ulMemOffset, TFTP_MAX_BLK_NUM * TFTP_BLKSIZE,
                     (void *)pstRecvBuf->u.stTFTP_Data.ucDataBuf, (size_t)ulRecvSize) != EOK) {
            ulErrCode = TFTPC_MEMCPY_FAILURE;
            (void)lwip_close(iSockNum);
            *ulFileLength = ulMemOffset;
            goto err_handler;
        }

        ulMemOffset += ulRecvSize;
        /* form the ACK packet for the DATA packet received */
        /* Go to the next packet no. */
        ulCurrBlk++;
        /* if the file is too big, exit */
        if (ulCurrBlk > TFTP_MAX_BLK_NUM) {
            /* Send error packet to server */
            lwip_tftp_send_error(iSockNum,
                                 TFTPC_PROTOCOL_USER_DEFINED,
                                 "File is too big.",
                                 &stServerAddr, pstSendBuf);

            (void)lwip_close(iSockNum);

            LWIP_DEBUGF(TFTP_DEBUG, ("lwip_tftp_get_file_by_filename : Data block number exceeded max value\n"));

            ulErrCode = TFTPC_FILE_TOO_BIG;
            goto err_handler;
        }
    }

err_handler:
    mem_free(pstSendBuf);
    mem_free(pstRecvBuf);
    return ulErrCode;
}
#endif

#endif /* LOSCFG_NET_LWIP_SACK_TFTP */
#endif /* LWIP_TFTP */
