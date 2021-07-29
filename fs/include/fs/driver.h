/****************************************************************************
 * include/fs/fs.h
 *
 *   Copyright (C) 2007-2009, 2011-2013, 2015-2018 Gregory Nutt. All rights
 *     reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef _FS_DRIVER_H_
#define _FS_DRIVER_H_

#include <sys/stat.h>
#include "vnode.h"
#include "fs/file.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/****************************************************************
geometry应该翻译为几何数据，其实就是指的CHS(Cylinder、Head、Sector/Track)
C-Cylinder柱面数表示硬盘每面盘片上有几条磁道，编号从0开始，最大为1023，表示有1024个磁道(用10个二进制位存储)
H-Head磁头数表示硬盘总共有几个磁头，也就是几面盘片，编号从0开始，最大为255，表示有256个磁头(用8个二进制位存储)；
S-Sector/Track扇区数表示每条磁道上有几个扇区，编号从1开始，最大为63，表示63个扇区(用6个二进制位存储)，
	每个扇区512字节，它是硬盘的最小存储单位。
	
我们可以算一下：1024个柱面×63个扇区×256个磁头×512byte=8455716864byte。即通常的8.4GB(实际上应该是7.8GB左右)限制。
实际上磁头数通常只用到255个(由汇编语言的寻址寄存器决定),即使把这3个字节按线性寻址，依然力不从心。
当然现在的硬盘早就超过8.4GB了。
从大到小
H-Head（磁头）---》C-Cylinder（柱面数或者磁道数，即每个磁头的磁道数）------》S-Sector/Track（扇区，也就是每个磁道有多少扇区）--------》扇区大小（512bit）磁盘空间

Geometry:
Cylinder Number - 1020 (0-1024)
Head Number - 16 (0-256)
Sector Number - 63 (1-64)

*****************************************************************/

/* This structure provides information about the state of a block driver */

struct geometry
{
  bool               geo_available;    /* true: The device is available *///设备是否有效
  bool               geo_mediachanged; /* true: The media has changed since last query *///自上次查询以来媒体已更改
  bool               geo_writeenabled; /* true: It is okay to write to this device *///是否可写
  unsigned long long geo_nsectors;     /* Number of sectors on the device *///扇区的数量
  size_t             geo_sectorsize;   /* Size of one sector *///扇区的大小
};

/* This structure is provided by block devices when they register with the
 * system.  It is used by file systems to perform filesystem transfers.  It
 * differs from the normal driver vtable in several ways -- most notably in
 * that it deals in struct Vnode vs. struct filep.
 */

//块驱动操作方式,以 vnode方式操作
struct block_operations //块操作接口类,块是文件系统层面的概念,块（Block）是文件系统存取数据的最小单位，一般大小是4KB
{
  int     (*open)(struct Vnode *vnode);
  int     (*close)(struct Vnode *vnode);
  ssize_t (*read)(struct Vnode *vnode, unsigned char *buffer,
            unsigned long long start_sector, unsigned int nsectors);
  ssize_t (*write)(struct Vnode *vnode, const unsigned char *buffer,
            unsigned long long start_sector, unsigned int nsectors);
  int     (*geometry)(struct Vnode *vnode, struct geometry *geometry);
  int     (*ioctl)(struct Vnode *vnode, int cmd, unsigned long arg);
  int     (*unlink)(struct Vnode *vnode);
};

//该结构由文件系统提供，用于描述挂载点。请注意，此结构与 file_operations 的不同之处仅在于 open 方法的形式。
//打开文件后，它可以作为 struct file_operations 或 struct mountpt_operations 访问
struct drv_data
{
  const void *ops;	//驱动程序 ( block_operations | file_operations_vfs | ... )
  mode_t mode;		//RWE_RW_RW  0755
  void *priv;		//私有数据域 (struct file)
};

/****************************************************************************
 * Name: register_driver
 *
 * Description:
 *   Register a character driver vnode the pseudo file system.
 *
 * Input Parameters:
 *   path - The path to the vnode to create
 *   fops - The file operations structure
 *   mode - Access privileges (not used)
 *   priv - Private, user data that will be associated with the vnode.
 *
 * Returned Value:
 *   Zero on success (with the vnode point in 'vnode'); A negated errno
 *   value is returned on a failure (all error values returned by
 *   vnode_reserve):
 *
 *   EINVAL - 'path' is invalid for this operation
 *   EEXIST - An vnode already exists at 'path'
 *   ENOMEM - Failed to allocate in-memory resources for the operation
 *
 * Attention:
 *   This function should be called after los_vfs_init has been called.
 *   The parameter path must point a valid string, which end with the terminating null byte.
 *   The total length of parameter path must less than the value defined by PATH_MAX.
 *   The prefix of the parameter path must be /dev/.
 *   The fops must pointed the right functions, otherwise the system will crash when the device is being operated.
 *
 ****************************************************************************/

int register_driver(const char *path,
                    const struct file_operations_vfs *fops, mode_t mode,
                    void *priv);

/****************************************************************************
 * Name: register_blockdriver
 *
 * Description:
 *   Register a block driver vnode the pseudo file system.
 *
 * Attention:
 *   This function should be called after los_vfs_init has been called.
 *   The parameter path must point a valid string, which end with the terminating null byte.
 *   The length of parameter path must be less than the value defined by PATH_MAX.
 *   The prefix of the parameter path must be '/dev/'.
 *   The bops must pointed the right functions, otherwise the system will crash when the device is being operated.
 *
 * Input Parameters:
 *   path - The path to the vnode to create
 *   bops - The block driver operations structure
 *   mode - Access privileges (not used)
 *   priv - Private, user data that will be associated with the vnode.
 *
 * Returned Value:
 *   Zero on success (with the vnode point in 'vnode'); A negated errno
 *   value is returned on a failure (all error values returned by
 *   vnode_reserve):
 *
 *   EINVAL - 'path' is invalid for this operation
 *   EEXIST - An vnode already exists at 'path'
 *   ENOMEM - Failed to allocate in-memory resources for the operation
 *
 ****************************************************************************/

int register_blockdriver(const char *path,
                         const struct block_operations *bops,
                         mode_t mode, void *priv);

/****************************************************************************
 * Name: unregister_driver
 *
 * Description:
 *   Remove the character driver vnode at 'path' from the pseudo-file system
 *
 * Returned Value:
 *   Zero on success (with the vnode point in 'vnode'); A negated errno
 *   value is returned on a failure (all error values returned by
 *   vnode_reserve):
 *
 *   EBUSY - Resource is busy ,not permit for this operation.
 *   ENOENT - 'path' is invalid for this operation.
 *
 * Attention:
 *   This function should be called after register_blockdriver has been called.
 *   The parameter path must point a valid string, which end with the terminating null byte.
 *   The total length of parameter path must less than the value defined by PATH_MAX.
 *   The block device node referred by parameter path must be really exist.
 ****************************************************************************/

int unregister_driver(const char *path);

/****************************************************************************
 * Name: unregister_blockdriver
 *
 * Description:
 *   Remove the block driver vnode at 'path' from the pseudo-file system
 *
 * Input Parameters:
 *   path - The path that the vnode to be destroyed.
 *
 * Returned Value:
 *   Zero on success (with the vnode point in 'vnode'); A negated errno
 *   value is returned on a failure (all error values returned by
 *   vnode_reserve):
 *
 *   EBUSY - Resource is busy ,not permit for this operation.
 *   ENOENT - 'path' is invalid for this operation.
 *
 * Attention:
 *   This function should be called after register_blockdriver has been called.
 *   The parameter path must point a valid string, which end with the terminating null byte.
 *   The total length of parameter path must less than the value defined by PATH_MAX.
 *   The block device node referred by parameter path must be really exist.
 *
 ****************************************************************************/

int unregister_blockdriver(const char *path);


/****************************************************************************
 * Name: open_blockdriver
 *
 * Description:
 *   Return the vnode of the block driver specified by 'pathname'
 *
 * Input Parameters:
 *   pathname - the full path to the block driver to be opened
 *   mountflags - if MS_RDONLY is not set, then driver must support write
 *     operations (see include/sys/mount.h)
 *   ppvnode - address of the location to return the vnode reference
 *
 * Returned Value:
 *   Returns zero on success or a negated errno on failure:
 *
 *   EINVAL  - pathname or pvnode is NULL
 *   ENOENT  - No block driver of this name is registered
 *   ENOTBLK - The vnode associated with the pathname is not a block driver
 *   EACCESS - The MS_RDONLY option was not set but this driver does not
 *     support write access
 *
 * Aattention:
 *   The parameter path must point a valid string, which end with the terminating null byte.
 *   The total length of parameter path must less than the value defined by PATH_MAX.
 *   The parameter ppvnode must point a valid memory, which size must be enough for storing struct Vnode.

 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int open_blockdriver(const char *pathname, int mountflags,
                     struct Vnode **ppvnode);
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: find_blockdriver
 *
 * Description:
 *   Return the inode of the block driver specified by 'pathname'
 *
 * Input Parameters:
 *   pathname   - The full path to the block driver to be located
 *   mountflags - If MS_RDONLY is not set, then driver must support write
 *                operations (see include/sys/mount.h)
 *   ppinode    - Address of the location to return the inode reference
 *
 * Returned Value:
 *   Returns zero on success or a negated errno on failure:
 *
 *   ENOENT  - No block driver of this name is registered
 *   ENOTBLK - The inode associated with the pathname is not a block driver
 *   EACCESS - The MS_RDONLY option was not set but this driver does not
 *             support write access
 *
 ****************************************************************************/

int find_blockdriver(const char *pathname, int mountflags,
                     struct Vnode **vpp);


/****************************************************************************
 * Name: close_blockdriver
 *
 * Description:
 *   Call the close method and release the vnode
 *
 * Input Parameters:
 *   vnode - reference to the vnode of a block driver opened by open_blockdriver
 *
 * Returned Value:
 *   Returns zero on success or a negated errno on failure:
 *
 *   EINVAL  - vnode is NULL
 *   ENOTBLK - The vnode is not a block driver
 *
 * Attention:
 *   This function should be called after open_blockdriver has been called.
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int close_blockdriver(struct Vnode *vnode);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* _FS_DRIVER_H_ */
