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
系统中任何文件系统的支持必须满足两个条件：挂载点和文件系统。

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
#include "los_typedef.h"

#define ROOT_DIR_NAME           "/"
#define STORAGE_DIR_NAME        "/storage"
#ifdef LOSCFG_STORAGE_EMMC
#define USERDATA_DIR_NAME       "/userdata"
#endif
#define DEFAULT_MOUNT_DIR_MODE  0755
#define DEFAULT_MOUNT_DATA      NULL

#ifdef LOSCFG_STORAGE_SPINOR //外部开关定 使用哪种flash
#define FLASH_TYPE              "spinor" //flash类型
#define ROOT_DEV_NAME          "/dev/spinorblk0" //设备名称
#define USER_DEV_NAME           "/dev/spinorblk2"
#define ROOTFS_ADDR             0x600000
#define ROOTFS_SIZE             0x800000
#define USERFS_SIZE             0x80000
#elif defined(LOSCFG_STORAGE_SPINAND)
#define FLASH_TYPE              "nand"
#define ROOT_DEV_NAME          "/dev/nandblk0"	//设备名称
#define USER_DEV_NAME           "/dev/nandblk2"
#define ROOTFS_ADDR             0x600000
#define ROOTFS_SIZE             0x800000
#define USERFS_SIZE             0x80000
#elif defined (LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7)
#define ROOT_DEV_NAME           "/dev/cfiflash0"
#define USER_DEV_NAME           "/dev/cfiflash2"
#define ROOTFS_ADDR             CFIFLASH_ROOT_ADDR
#define ROOTFS_SIZE             0x1B00000
#define USERFS_SIZE             (CFIFLASH_CAPACITY - ROOTFS_ADDR - ROOTFS_SIZE)
#elif defined (LOSCFG_STORAGE_EMMC)
#define ROOT_DEV_NAME           "/dev/mmcblk0p0"
#define USER_DEV_NAME           "/dev/mmcblk0p1"
#define USERDATA_DEV_NAME       "/dev/mmcblk0p2"
#define ROOTFS_ADDR             0xA00000
#define ROOTFS_SIZE             0x1400000
#define USERFS_SIZE             0x3200000
#ifdef DEFAULT_MOUNT_DIR_MODE
#undef DEFAULT_MOUNT_DIR_MODE
#endif
#ifdef DEFAULT_MOUNT_DATA
#undef DEFAULT_MOUNT_DATA
#endif
#define DEFAULT_MOUNT_DIR_MODE  0777
#define DEFAULT_MOUNT_DATA      "umask=000"
#endif

INT32 OsMountRootfs(VOID);

#endif /* _LOS_ROOTFS_H */
