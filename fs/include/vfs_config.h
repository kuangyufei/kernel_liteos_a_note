/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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


#ifndef _VFS_CONFIG_H_
#define _VFS_CONFIG_H_

#include "los_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define CONFIG_DISABLE_MQUEUE   // disable posix mqueue inode configure

/* file system configure */

#define CONFIG_FS_WRITABLE      // enable file system can be written
#define CONFIG_FS_READABLE      // enable file system can be read
#define CONFIG_DEBUG_FS         // enable vfs debug function


/* fatfs cache configur */
/* config block size for fat file system, only can be 0,32,64,128,256,512,1024 */
#define CONFIG_FS_FAT_SECTOR_PER_BLOCK  64

/* config block num for fat file system */
#define CONFIG_FS_FAT_READ_NUMS         7
#define CONFIG_FS_FAT_BLOCK_NUMS        28

#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD

/* config the priority of sync task */

#define CONFIG_FS_FAT_SYNC_THREAD_PRIO 10

/* config dirty ratio of bcache for fat file system */

#define CONFIG_FS_FAT_DIRTY_RATIO      80

/* config time interval of sync thread for fat file system, in milliseconds */

#define CONFIG_FS_FAT_SYNC_INTERVAL    5000
#endif

#define CONFIG_FS_FLASH_BLOCK_NUM 1

/* nfs configure */

#define CONFIG_NFS_MACHINE_NAME "IPC"   // nfs device name is IPC
#define CONFIG_NFS_MACHINE_NAME_SIZE 3  // size of nfs machine name


/* file descriptors configure */

#define CONFIG_NFILE_STREAMS        1   // enable file stream
#define CONFIG_STDIO_BUFFER_SIZE    0
#define CONFIG_NUNGET_CHARS         0

#define FD_SETSIZE                  (CONFIG_NFILE_DESCRIPTORS + CONFIG_NSOCKET_DESCRIPTORS)  // total fds

/* net configure */

#ifdef LOSCFG_NET_LWIP_SACK             // enable socket and net function //网络开关
#include "lwip/lwipopts.h"
#define CONFIG_NSOCKET_DESCRIPTORS  LWIP_CONFIG_NUM_SOCKETS  // max numbers of socket descriptor 套接字描述符的最大数目

/* max numbers of other descriptors except socket descriptors */

#define CONFIG_NFILE_DESCRIPTORS    512	//除套接字描述符外的其他描述符的最大数目
#define CONFIG_NET_SENDFILE         1   // enable sendfile function //因打开网络开关,所以同时也打开发送文件开关
#define CONFIG_NET_TCP              1   // enable sendfile and send function
#else
#define CONFIG_NSOCKET_DESCRIPTORS  0	//关闭网络开关,当然NFS的数量为0,鸿蒙和LINUX一样,一切皆为文件,而是文件就需要文件描述符(FD)
#define CONFIG_NFILE_DESCRIPTORS    512 //除套接字描述符外的其他描述符的最大数目
#define CONFIG_NET_SENDFILE         0   // disable sendfile function 
#define CONFIG_NET_TCP              0   // disable sendfile and send function //禁用sendfile和send函数功能
#endif

#define NR_OPEN_DEFAULT CONFIG_NFILE_DESCRIPTORS //

/* directory configure */

#define VFS_USING_WORKDIR               // enable current working directory 使能当前工作区

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif
