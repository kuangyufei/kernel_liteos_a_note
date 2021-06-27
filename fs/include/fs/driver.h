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

/* This structure provides information about the state of a block driver */

struct geometry
{
  bool               geo_available;    /* true: The device is available */
  bool               geo_mediachanged; /* true: The media has changed since last query */
  bool               geo_writeenabled; /* true: It is okay to write to this device */
  unsigned long long geo_nsectors;     /* Number of sectors on the device */
  size_t             geo_sectorsize;   /* Size of one sector */
};

/* This structure is provided by block devices when they register with the
 * system.  It is used by file systems to perform filesystem transfers.  It
 * differs from the normal driver vtable in several ways -- most notably in
 * that it deals in struct Vnode vs. struct filep.
 */

struct block_operations
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

struct drv_data
{
  const void *ops;
  mode_t mode;
  void *priv;
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
