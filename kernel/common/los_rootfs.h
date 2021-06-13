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

#ifndef _LOS_ROOTFS_H
#define _LOS_ROOTFS_H
/**********************************************
rootfs之所以存在，是因为需要在VFS机制下给系统提供最原始的挂载点
https://blog.csdn.net/tankai19880619/article/details/12093239

与linux一样,在鸿蒙内核一切皆文件:普通文件、目录、字符设备、块设备、套接字
等都以文件被对待；他们具体的类型及其操作不同，但需要向上层提供统一的操作接口。 
虚拟文件系统VFS就是内核中的一个软件层，向上给用户空间程序提供文件系统操作接口；
向下允许不同的文件系统共存。所以，所有实际文件系统都必须实现VFS的结构封装。
系统中任何文件系统的挂载必须满足两个条件：挂载点和文件系统。

rootfs是基于内存的文件系统，所有操作都在内存中完成；也没有实际的存储设备，所以不需要设备驱动程序的参与.
只在启动阶段使用rootfs文件系统，当磁盘驱动程序和磁盘文件系统成功加载后，系统会将系统根目录从rootfs切换
到磁盘上的具体文件系统。

rootfs的特点:
1.它是系统自己创建并加载的第一个文件系统；
2.该文件系统的挂载点就是它自己的根目录项对象；
3.该文件系统仅仅存在于内存中。
VFS是一种机制、是每一种文件系统都必须按照这个机制去实现的一种规范；
而rootfs仅仅是符合VFS规范的而且又具有如上3个特点的一个文件系统。
**********************************************/

#define BYTES_PER_MBYTE         0x100000	//1M = 1024 * 1024
#define BYTES_PER_KBYTE         0x400		//1K = 1024

#define COMMAND_LINE_ADDR       LOSCFG_BOOTENV_ADDR * BYTES_PER_KBYTE
#define COMMAND_LINE_SIZE       1024

#ifdef LOSCFG_STORAGE_SPINOR
#define ROOTFS_ROOT_TYPE        "flash"	//根文件系统存放在nor flash上
#define ROOTFS_FS_TYPE          "jffs2" //Journalling Flash File System（闪存设备日志型文件系统，JFFS）,一般用于nor flash的文件系统
#elif defined(LOSCFG_STORAGE_SPINAND)
#define ROOTFS_ROOT_TYPE        "nand"
#define ROOTFS_FS_TYPE          "yaffs2"
#endif

#ifdef LOSCFG_STORAGE_EMMC
#define ROOTFS_ROOT_TYPE        "emmc"	//eMMC=NAND闪存+闪存控制芯片+标准接口封装 https://blog.csdn.net/xjw1874/article/details/81505967
#define ROOTFS_FS_TYPE          "vfat" 	//即FAT32,采用32位的文件分配表，支持最大分区128GB，最大文件4GB 
#endif

#ifdef LOSCFG_TEE_ENABLE	//TEE(Trust Execution Environment)，可信执行环境,和REE(Rich Execution Environment)相对应
#define ROOTFS_FLASH_ADDR       0x600000
#define ROOTFS_FLASH_SIZE       0x800000
#else
#define ROOTFS_FLASH_ADDR       0x400000 //根文件系统地址
#define ROOTFS_FLASH_SIZE       0xa00000 //根文件系统大小 10M
#endif

#ifdef LOSCFG_STORAGE_SPINOR //外部开关定 使用哪种flash
#define FLASH_TYPE              "spinor" //flash类型
#define FLASH_DEV_NAME          "/dev/spinorblk0" //根文件系统路径
#elif defined(LOSCFG_STORAGE_SPINAND)
#define FLASH_TYPE              "nand"
#define FLASH_DEV_NAME          "/dev/nandblk0"
#endif
//扇区是对硬盘而言，而块是对文件系统而言
#define EMMC_SEC_SIZE           512	//扇区大小,按512个字节,按扇区对齐

#define DEC_NUMBER_STRING       "0123456789"
#define HEX_NUMBER_STRING       "0123456789abcdefABCDEF"

INT32 OsMountRootfs(VOID);
VOID OsSetCmdLineAddr(UINT64 addr);
UINT64 OsGetCmdLineAddr(VOID);

#ifdef LOSCFG_BOOTENV_RAM
CHAR *OsGetArgsAddr(VOID);
#endif
#endif /* _LOS_ROOTFS_H */
