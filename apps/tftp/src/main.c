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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include "tftpc.h"

#ifdef LOSCFG_NET_LWIP_SACK_TFTP
static int tcpip_init_finish = 1;
static char *TftpError[] = {
    "TFTP transfer finish\n",
    "Error while creating UDP socket\n",
    "Error while binding to the UDP socket\n",
    "Error returned by select() system call\n",
    "Error while receiving data from the peer\n",
    "Error while sending data to the peer\n",
    "Requested file is not found\n",
    "This is the error sent by the server when hostname cannot be resolved\n",
    "Input paramters passed to TFTP interfaces are invalid\n",
    "Error detected in TFTP packet or the error received from the TFTP server\n",
    "Error during packet synhronization while sending or unexpected packet is received\n",
    "File size limit crossed, Max block can be 0xFFFF, each block containing 512 bytes\n",
    "File name lenght greater than 256\n",
    "Hostname IP is not valid\n",
    "TFTP server returned file access error\n",
    "TFTP server returned error signifying that the DISK is full to write\n",
    "TFTP server returned error signifying that the file exist\n",
    "The source file name do not exisits\n",
    "Memory allocaion failed in TFTP client\n",
    "File open failed\n",
    "File read error\n",
    "File create error\n",
    "File write error\n",
    "Max time expired while waiting for file to be recived\n",
    "Error when the received packet is less than 4bytes(error lenght) or greater than 512bytes\n",
    "Returned by TFTP server for protocol user error\n",
    "The destination file path length greater than 256\n",
    "Returned by TFTP server for undefined transfer ID\n",
    "IOCTL fucntion failed at TFTP client while setting the socket to non-block\n",
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

u32_t osShellTftp(int argc, const char **argv)
{
    u32_t ulRemoteAddr = IPADDR_NONE;
    const u16_t usTftpServPort = 69;
    u8_t ucTftpGet = 0;
    s8_t *szLocalFileName = NULL;
    s8_t *szRemoteFileName = NULL;
    u32_t ret;

    int i = 1;
    if (argc < 1 || argv == NULL) {
        goto usage;
    }

    if (!tcpip_init_finish) {
        PRINTK("%s: tcpip_init have not been called\n", __FUNCTION__);
        return LOS_NOK;
    }

    while (i < argc) {
        if (strcmp(argv[i], "-p") == 0) {
            ucTftpGet = 0;
            i++;
            continue;
        }

        if (strcmp(argv[i], "-g") == 0) {
            ucTftpGet = 1;
            i++;
            continue;
        }

        if (strcmp(argv[i], "-l") == 0 && ((i + 1) < argc)) {
            szLocalFileName = (s8_t *)argv[i + 1];
            i += 2;
            continue;
        }

        if (strcmp(argv[i], "-r") == 0 && ((i + 1) < argc)) {
            szRemoteFileName = (s8_t *)argv[i + 1];
            i += 2;
            continue;
        }

        if ((i + 1) == argc) {
            ulRemoteAddr = inet_addr(argv[i]);
            break;
        }

        goto usage;
    }

    if (ulRemoteAddr == IPADDR_NONE || szLocalFileName == NULL || szRemoteFileName == NULL) {
        goto usage;
    }

    if (ucTftpGet) {
        ret = lwip_tftp_get_file_by_filename(ntohl(ulRemoteAddr), usTftpServPort,
                                             TRANSFER_MODE_BINARY, szRemoteFileName, szLocalFileName);
    } else {
        ret = lwip_tftp_put_file_by_filename(ntohl(ulRemoteAddr), usTftpServPort,
                                             TRANSFER_MODE_BINARY, szLocalFileName, szRemoteFileName);
    }

    LWIP_ASSERT("TFTP UNKNOW ERROR!", ret < ARRAY_SIZE(TftpError));
    PRINTK("%s", TftpError[ret]);
    if (ret) {
        return LOS_NOK;
    } else {
        return LOS_OK;
    }
usage:
    PRINTK("usage:\nTransfer a file from/to tftp server\n");
    PRINTK("tftp <-g/-p> -l FullPathLocalFile -r RemoteFile Host\n");
    return LOS_NOK;
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(tftp_shellcmd, CMD_TYPE_EX, "tftp", XARGS, (CmdCallBackFunc)(uintptr_t)osShellTftp);
#endif /* LOSCFG_SHELL_CMD_DEBUG */
#endif /* LOSCFG_NET_LWIP_SACK_TFTP */
