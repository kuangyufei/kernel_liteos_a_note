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

/**
 * @brief 
 * @verbatim
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
 * @endverbatim
 */

#include "los_typedef.h"

#define ROOT_DIR_NAME           "/"  // 根目录路径名
#define STORAGE_DIR_NAME        "/storage"  // 存储目录路径名
#ifdef LOSCFG_STORAGE_EMMC  // 如果配置了EMMC存储
#define USERDATA_DIR_NAME       "/userdata"  // 用户数据目录路径名
#ifdef LOSCFG_PLATFORM_PATCHFS  // 如果配置了补丁文件系统
#define PATCH_DIR_NAME          "/patch"  // 补丁目录路径名
#endif
#endif
#define DEFAULT_MOUNT_DIR_MODE  0755  // 默认挂载目录权限模式
#define DEFAULT_MOUNT_DATA      NULL  // 默认挂载数据

#ifdef LOSCFG_STORAGE_SPINOR  // 如果配置了SPI NOR闪存
#define FLASH_TYPE              "spinor"  // 闪存类型：spinor
#define ROOT_DEV_NAME           "/dev/spinorblk0"  // 根设备名称
#define USER_DEV_NAME           "/dev/spinorblk2"  // 用户设备名称
#define ROOTFS_ADDR             0x600000  // 根文件系统起始地址
#define ROOTFS_SIZE             0x800000  // 根文件系统大小
#define USERFS_SIZE             0x80000  // 用户文件系统大小
#elif defined (LOSCFG_STORAGE_SPINAND)  // 否则如果配置了SPI NAND闪存
#define FLASH_TYPE              "nand"  // 闪存类型：nand
#define ROOT_DEV_NAME           "/dev/nandblk0"  // 根设备名称
#define USER_DEV_NAME           "/dev/nandblk2"  // 用户设备名称
#define ROOTFS_ADDR             0x600000  // 根文件系统起始地址
#define ROOTFS_SIZE             0x800000  // 根文件系统大小
#define USERFS_SIZE             0x80000  // 用户文件系统大小
#elif defined (LOSCFG_STORAGE_EMMC)  // 否则如果配置了EMMC存储
#define ROOT_DEV_NAME           "/dev/mmcblk0p0"  // 根设备名称
#ifdef LOSCFG_PLATFORM_PATCHFS  // 如果配置了补丁文件系统
#define PATCH_DEV_NAME          "/dev/mmcblk0p1"  // 补丁设备名称
#define USER_DEV_NAME           "/dev/mmcblk0p2"  // 用户设备名称
#define USERDATA_DEV_NAME       "/dev/mmcblk0p3"  // 用户数据设备名称
#else
#define USER_DEV_NAME           "/dev/mmcblk0p1"  // 用户设备名称
#define USERDATA_DEV_NAME       "/dev/mmcblk0p2"  // 用户数据设备名称
#endif
#define ROOTFS_ADDR             0xA00000  // 根文件系统起始地址
#define ROOTFS_SIZE             0x1400000  // 根文件系统大小
#define USERFS_SIZE             0x3200000  // 用户文件系统大小
#ifdef LOSCFG_PLATFORM_PATCHFS  // 如果配置了补丁文件系统
#define PATCH_SIZE              0x200000  // 补丁大小
#endif
#ifdef DEFAULT_MOUNT_DIR_MODE  // 如果已定义默认挂载目录权限模式
#undef DEFAULT_MOUNT_DIR_MODE  // 取消定义默认挂载目录权限模式
#endif
#ifdef DEFAULT_MOUNT_DATA  // 如果已定义默认挂载数据
#undef DEFAULT_MOUNT_DATA  // 取消定义默认挂载数据
#endif
#define DEFAULT_MOUNT_DIR_MODE  0777  // 重新定义默认挂载目录权限模式为0777
#define DEFAULT_MOUNT_DATA      "umask=000"  // 重新定义默认挂载数据为umask=000
#endif  // LOSCFG_STORAGE_EMMC条件编译结束


INT32 OsMountRootfs(VOID);

#endif /* _LOS_ROOTFS_H */
