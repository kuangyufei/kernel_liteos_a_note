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

/**
 * @brief TFTP客户端功能实现
 * @details 当配置LOSCFG_NET_LWIP_SACK_TFTP启用时，提供TFTP文件传输的shell命令支持
 *          支持通过TFTP协议从服务器下载文件(-g)或向服务器上传文件(-p)
 */
#ifdef LOSCFG_NET_LWIP_SACK_TFTP
/**
 * @brief TCP/IP初始化完成标志
 * @note 1表示TCP/IP栈已初始化完成，0表示未完成
 */
static int tcpip_init_finish = 1;

/**
 * @brief TFTP错误信息数组
 * @details 索引对应错误码，内容为错误描述字符串
 *          0表示成功，1及以上表示不同类型错误
 */
static char *TftpError[] = {
    "TFTP transfer finish\n",                                  // 0: 传输成功
    "Error while creating UDP socket\n",                       // 1: 创建UDP socket失败
    "Error while binding to the UDP socket\n",                  // 2: 绑定UDP socket失败
    "Error returned by select() system call\n",                 // 3: select系统调用错误
    "Error while receiving data from the peer\n",               // 4: 从对等方接收数据错误
    "Error while sending data to the peer\n",                   // 5: 向对等方发送数据错误
    "Requested file is not found\n",                            // 6: 请求文件不存在
    "This is the error sent by the server when hostname cannot be resolved\n", // 7: 主机名无法解析
    "Input parameters passed to TFTP interfaces are invalid\n", // 8: TFTP接口输入参数无效
    "Error detected in TFTP packet or the error received from the TFTP server\n", // 9: TFTP数据包错误或服务器返回错误
    "Error during packet synhronization while sending or unexpected packet is received\n", // 10: 数据包同步错误或收到意外包
    "File size limit crossed, Max block can be 0xFFFF, each block containing 512 bytes\n", // 11: 文件大小超限(最大0xFFFF块×512字节)
    "File name length greater than 256\n",                      // 12: 文件名长度超过256字节
    "Hostname IP is not valid\n",                               // 13: 主机IP地址无效
    "TFTP server returned file access error\n",                 // 14: 服务器返回文件访问错误
    "TFTP server returned error signifying that the DISK is full to write\n", // 15: 服务器磁盘满
    "TFTP server returned error signifying that the file exist\n", // 16: 服务器文件已存在
    "The source file name do not exisits\n",                     // 17: 源文件不存在
    "Memory allocaion failed in TFTP client\n",                  // 18: TFTP客户端内存分配失败
    "File open failed\n",                                        // 19: 文件打开失败
    "File read error\n",                                         // 20: 文件读取错误
    "File create error\n",                                       // 21: 文件创建错误
    "File write error\n",                                        // 22: 文件写入错误
    "Max time expired while waiting for file to be recived\n",   // 23: 等待文件接收超时
    "Error when the received packet is less than 4bytes(error length) or greater than 512bytes\n", // 24: 数据包长度异常(小于4字节或大于512字节)
    "Returned by TFTP server for protocol user error\n",         // 25: 服务器返回协议用户错误
    "The destination file path length greater than 256\n",       // 26: 目标文件路径长度超过256字节
    "Returned by TFTP server for undefined transfer ID\n",       // 27: 服务器返回未定义的传输ID
    "IOCTL function failed at TFTP client while setting the socket to non-block\n", // 28: 设置socket为非阻塞模式失败
};

/**
 * @brief 计算数组元素个数宏
 * @param array 输入数组
 * @return 数组元素数量 = 数组总大小 / 单个元素大小
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

/**
 * @brief TFTP shell命令处理函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return LOS_OK表示成功，LOS_NOK表示失败
 * @details 解析tftp命令参数，执行文件上传或下载操作
 *          支持的命令格式: tftp <-g/-p> -l 本地文件路径 -r 远程文件路径 服务器IP
 */
u32_t osShellTftp(int argc, const char **argv)
{
    u32_t ulRemoteAddr = IPADDR_NONE;       // 服务器IP地址
    const u16_t usTftpServPort = 69;        // TFTP服务器默认端口号
    u8_t ucTftpGet = 0;                     // 传输方向标志(0:上传,1:下载)
    s8_t *szLocalFileName = NULL;           // 本地文件名
    s8_t *szRemoteFileName = NULL;          // 远程文件名
    u32_t ret;                              // 函数返回值

    int i = 1;                              // 参数索引(跳过命令名)
    if (argc < 1 || argv == NULL) {         // 参数有效性检查
        goto usage;                         // 跳转到用法提示
    }

    if (!tcpip_init_finish) {               // 检查TCP/IP栈初始化状态
        PRINTK("%s: tcpip_init have not been called\n", __FUNCTION__);
        return LOS_NOK;
    }

    // 解析命令行参数
    while (i < argc) {
        if (strcmp(argv[i], "-p") == 0) {   // 上传模式(-p: put)
            ucTftpGet = 0;
            i++;
            continue;
        }

        if (strcmp(argv[i], "-g") == 0) {   // 下载模式(-g: get)
            ucTftpGet = 1;
            i++;
            continue;
        }

        // 指定本地文件路径(-l: local)
        if (strcmp(argv[i], "-l") == 0 && ((i + 1) < argc)) {
            szLocalFileName = (s8_t *)argv[i + 1];
            i += 2;
            continue;
        }

        // 指定远程文件路径(-r: remote)
        if (strcmp(argv[i], "-r") == 0 && ((i + 1) < argc)) {
            szRemoteFileName = (s8_t *)argv[i + 1];
            i += 2;
            continue;
        }

        // 最后一个参数应为服务器IP地址
        if ((i + 1) == argc) {
            ulRemoteAddr = inet_addr(argv[i]);  // 将点分十进制IP转换为网络字节序
            break;
        }

        // 参数解析失败，跳转到用法提示
        goto usage;
    }

    // 检查必要参数是否齐全
    if (ulRemoteAddr == IPADDR_NONE || szLocalFileName == NULL || szRemoteFileName == NULL) {
        goto usage;
    }

    // 根据传输方向执行相应操作
    if (ucTftpGet) {
        // 从服务器下载文件
        ret = lwip_tftp_get_file_by_filename(ntohl(ulRemoteAddr), usTftpServPort,
                                             TRANSFER_MODE_BINARY, szRemoteFileName, szLocalFileName);
    } else {
        // 向服务器上传文件
        ret = lwip_tftp_put_file_by_filename(ntohl(ulRemoteAddr), usTftpServPort,
                                             TRANSFER_MODE_BINARY, szLocalFileName, szRemoteFileName);
    }

    // 验证返回值有效性并输出结果
    LWIP_ASSERT("TFTP UNKNOW ERROR!", ret < ARRAY_SIZE(TftpError));
    PRINTK("%s", TftpError[ret]);
    if (ret) {
        return LOS_NOK;  // 传输失败
    } else {
        return LOS_OK;   // 传输成功
    }

usage:  // 命令用法提示
    PRINTK("usage:\nTransfer a file from/to tftp server\n");
    PRINTK("tftp <-g/-p> -l FullPathLocalFile -r RemoteFile Host\n");
    return LOS_NOK;
}

/**
 * @brief 注册TFTP shell命令
 * @details 当LOSCFG_SHELL_CMD_DEBUG配置启用时，将tftp命令注册到shell中
 *          命令格式: tftp <-g/-p> -l 本地文件路径 -r 远程文件路径 服务器IP
 */
#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(tftp_shellcmd, CMD_TYPE_EX, "tftp", XARGS, (CmdCallBackFunc)(uintptr_t)osShellTftp);
#endif /* LOSCFG_SHELL_CMD_DEBUG */
#endif /* LOSCFG_NET_LWIP_SACK_TFTP */
