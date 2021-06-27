/****************************************************************************
 * include/fs/file.h
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

#ifndef __INCLUDE_FS_FILE_H
#define __INCLUDE_FS_FILE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "vfs_config.h"

#include "sys/types.h"
#include "sys/stat.h"
#include "semaphore.h"
#include "poll.h"
#include "los_vm_map.h"
#include "los_atomic.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifndef VFS_ERROR
#define VFS_ERROR -1
#endif

#ifndef OK
#define OK 0
#endif

/* This is the underlying representation of an open file.  A file
 * descriptor is an index into an array of such types. The type associates
 * the file descriptor to the file state and to a set of vnode operations.
 */

struct Vnode;

struct file
{
  unsigned int         f_magicnum;  /* file magic number */
  int                  f_oflags;    /* Open mode flags */
  struct Vnode         *f_vnode;    /* Driver interface */
  loff_t               f_pos;       /* File position */
  unsigned long        f_refcount;  /* reference count */
  char                 *f_path;     /* File fullpath */
  void                 *f_priv;     /* Per file driver private data */
  const char           *f_relpath;  /* realpath */
  struct page_mapping  *f_mapping;  /* mapping file to memory */
  void                 *f_dir;      /* DIR struct for iterate the directory if open a directory */
  const struct file_operations_vfs *ops;
  int fd;
};

/* This defines a list of files indexed by the file descriptor */

#if CONFIG_NFILE_DESCRIPTORS > 0
struct filelist
{
  sem_t   fl_sem;               /* Manage access to the file list */
  struct file fl_files[CONFIG_NFILE_DESCRIPTORS];
};

extern struct filelist tg_filelist;
#endif

/* This structure is provided by devices when they are registered with the
 * system.  It is used to call back to perform device specific operations.
 */

struct file_operations_vfs
{
  /* The device driver open method differs from the mountpoint open method */

  int     (*open)(struct file *filep);

  /* The following methods must be identical in signature and position because
   * the struct file_operations and struct mountp_operations are treated like
   * unions.
   */

  int     (*close)(struct file *filep);
  ssize_t (*read)(struct file *filep, char *buffer, size_t buflen);
  ssize_t (*write)(struct file *filep, const char *buffer, size_t buflen);
  off_t   (*seek)(struct file *filep, off_t offset, int whence);
  int     (*ioctl)(struct file *filep, int cmd, unsigned long arg);
  int     (*mmap)(struct file* filep, struct VmMapRegion *region);
  /* The two structures need not be common after this point */

#ifndef CONFIG_DISABLE_POLL
  int     (*poll)(struct file *filep, poll_table *fds);
#endif
  int     (*stat)(struct file *filep, struct stat* st);
  int     (*fallocate)(struct file* filep, int mode, off_t offset, off_t len);
  int     (*fallocate64)(struct file *filep, int mode, off64_t offset, off64_t len);
  int     (*fsync)(struct file *filep);
  ssize_t (*readpage)(struct file *filep, char *buffer, size_t buflen);
  int     (*unlink)(struct Vnode *vnode);
};

/* file mapped in VMM pages */
struct page_mapping {
  LOS_DL_LIST                           page_list;    /* all pages */
  SPIN_LOCK_S                           list_lock;    /* lock protecting it */
  LosMux                                mux_lock;     /* mutex lock */
  unsigned long                         nrpages;      /* number of total pages */
  unsigned long                         flags;
  Atomic                                ref;          /* reference counting */
  struct file                           *host;        /* owner of this mapping */
};

/* map: full_path(owner) <-> mapping */
struct file_map {
  LOS_DL_LIST                           head;
  LosMux                                lock;         /* lock to protect this mapping */
  struct page_mapping                   mapping;
  int                                   name_len;
  char                                  *rename;
  char                                  owner[0];     /* owner: full path of file */
};

/****************************************************************************
 * Name: files_initlist
 *
 * Description:
 *   Initializes the list of files for a new task
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
void files_initlist(struct filelist *list);
#endif

/****************************************************************************
 * Name: files_releaselist
 *
 * Description:
 *   Release a reference to the file list
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
void files_releaselist(struct filelist *list);
#endif

/****************************************************************************
 * Name: file_dup2
 *
 * Description:
 *   Assign an vnode to a specific files structure.  This is the heart of
 *   dup2.
 *
 *   Equivalent to the non-standard fs_dupfd2() function except that it
 *   accepts struct file instances instead of file descriptors and it does
 *   not set the errno variable.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is return on
 *   any failure.
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int file_dup2(struct file *filep1, struct file *filep2);
#endif

/****************************************************************************
 * Name: fs_dupfd OR dup
 *
 * Description:
 *   Clone a file descriptor 'fd' to an arbitrary descriptor number (any value
 *   greater than or equal to 'minfd'). If socket descriptors are
 *   implemented, then this is called by dup() for the case of file
 *   descriptors.  If socket descriptors are not implemented, then this
 *   function IS dup().
 *
 *   This alternative naming is used when dup could operate on both file and
 *   socket descriptors to avoid drawing unused socket support into the link.
 *
 * Returned Value:
 *   fs_dupfd is sometimes an OS internal function and sometimes is a direct
 *   substitute for dup().  So it must return an errno value as though it
 *   were dup().
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int fs_dupfd(int fd, int minfd);
#endif

/****************************************************************************
 * Name: file_dup
 *
 * Description:
 *   Equivalent to the non-standard fs_dupfd() function except that it
 *   accepts a struct file instance instead of a file descriptor and does
 *   not set the errno variable.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int file_dup(struct file *filep, int minfd);

/****************************************************************************
 * Name: fs_dupfd2 OR dup2
 *
 * Description:
 *   Clone a file descriptor to a specific descriptor number. If socket
 *   descriptors are implemented, then this is called by dup2() for the
 *   case of file descriptors.  If socket descriptors are not implemented,
 *   then this function IS dup2().
 *
 *   This alternative naming is used when dup2 could operate on both file and
 *   socket descriptors to avoid drawing unused socket support into the link.
 *
 * Returned Value:
 *   fs_dupfd2 is sometimes an OS internal function and sometimes is a direct
 *   substitute for dup2().  So it must return an errno value as though it
 *   were dup2().
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int fs_dupfd2(int fd1, int fd2);
#endif

/****************************************************************************
 * Name: fs_ioctl
 *
 * Description:
 *   Perform device specific operations.
 *
 * Input Parameters:
 *   fd       File/socket descriptor of device
 *   req      The ioctl command
 *   arg      The argument of the ioctl cmd
 *
 * Returned Value:
 *   >=0 on success (positive non-zero values are cmd-specific)
 *   -1 on failure with errno set properly:
 *
 *   EBADF
 *     'fd' is not a valid descriptor.
 *   EFAULT
 *     'arg' references an inaccessible memory area.
 *   EINVAL
 *     'cmd' or 'arg' is not valid.
 *   ENOTTY
 *     'fd' is not associated with a character special device.
 *   ENOTTY
 *      The specified request does not apply to the kind of object that the
 *      descriptor 'fd' references.
 *
 ****************************************************************************/
#ifdef CONFIG_LIBC_IOCTL_VARIADIC
int fs_ioctl(int fd, int req, unsigned long arg);
#endif

/****************************************************************************
 * Name: lib_sendfile
 *
 * Description:
 *   Transfer a file
 *
 ****************************************************************************/

#ifdef CONFIG_NET_SENDFILE
ssize_t lib_sendfile(int outfd, int infd, off_t *offset, size_t count);
#endif

/****************************************************************************
 * Name: fs_getfilep
 *
 * Description:
 *   Given a file descriptor, return the corresponding instance of struct
 *   file.  NOTE that this function will currently fail if it is provided
 *   with a socket descriptor.
 *
 * Input Parameters:
 *   fd    - The file descriptor
 *   filep - The location to return the struct file instance
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int fs_getfilep(int fd, struct file **filep);
#endif

/****************************************************************************
 * Name: file_read
 *
 * Description:
 *   file_read() is an internal OS interface.  It is functionally similar to
 *   the standard read() interface except:
 *
 *    - It does not modify the errno variable,
 *    - It is not a cancellation point,
 *    - It does not handle socket descriptors, and
 *    - It accepts a file structure instance instead of file descriptor.
 *
 * Input Parameters:
 *   filep  - File structure instance
 *   buf    - User-provided to save the data
 *   nbytes - The maximum size of the user-provided buffer
 *
 * Returned Value:
 *   The positive non-zero number of bytes read on success, 0 on if an
 *   end-of-file condition, or a negated errno value on any failure.
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
ssize_t file_read(struct file *filep, void *buf, size_t nbytes);
#endif

/****************************************************************************
 * Name: file_write
 *
 * Description:
 *   Equivalent to the standard write() function except that is accepts a
 *   struct file instance instead of a file descriptor.  Currently used
 *   only by aio_write();
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
ssize_t file_write(struct file *filep, const void *buf, size_t nbytes);
#endif

/****************************************************************************
 * Name: file_pread
 *
 * Description:
 *   Equivalent to the standard pread function except that is accepts a
 *   struct file instance instead of a file descriptor.  Currently used
 *   only by aio_read();
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
ssize_t file_pread(struct file *filep, void *buf, size_t nbytes,
                   off_t offset);
#endif

/****************************************************************************
 * Name: file_pwrite
 *
 * Description:
 *   Equivalent to the standard pwrite function except that is accepts a
 *   struct file instance instead of a file descriptor.  Currently used
 *   only by aio_write();
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
ssize_t file_pwrite(struct file *filep, const void *buf,
                    size_t nbytes, off_t offset);
#endif

/****************************************************************************
 * Name: file_seek
 *
 * Description:
 *   Equivalent to the standard lseek() function except that is accepts a
 *   struct file instance instead of a file descriptor.  Currently used
 *   only by net_sendfile()
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
off_t file_seek(struct file *filep, off_t offset, int whence);
#endif

/****************************************************************************
 * Name: file_fsync
 *
 * Description:
 *   Equivalent to the standard fsync() function except that is accepts a
 *   struct file instance instead of a file descriptor and it does not set
 *   the errno variable.
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int file_fsync(struct file *filep);
#endif

/****************************************************************************
 * Name: file_vfcntl
 *
 * Description:
 *   Similar to the standard vfcntl function except that is accepts a struct
 *   struct file instance instead of a file descriptor.
 *
 * Input Parameters:
 *   filep - Instance for struct file for the opened file.
 *   cmd   - Indentifies the operation to be performed.
 *   ap    - Variable argument following the command.
 *
 * Returned Value:
 *   The nature of the return value depends on the command.  Non-negative
 *   values indicate success.  Failures are reported as negated errno
 *   values.
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
int file_vfcntl(struct file *filep, int cmd, va_list ap);
#endif

/****************************************************************************
 * Name: file_seek64
 *
 * Description:
 *   Equivalent to the standard lseek64() function except that is accepts a
 *   struct file instance instead of a file descriptor.  Currently used
 *   only by net_sendfile()
 *
 ****************************************************************************/

#if CONFIG_NFILE_DESCRIPTORS > 0
off64_t file_seek64(struct file *filep, off64_t offset, int whence);
#endif

/****************************************************************************
 * Name: files_allocate
 *
 * Description:
 *   Allocate a struct files instance and associate it with an vnode instance.
 *   Returns the file descriptor == index into the files array.
 *
 ****************************************************************************/

int files_allocate(struct Vnode *vnode, int oflags, off_t pos,void *priv, int minfd);

/****************************************************************************
 * Name: files_close
 *
 * Description:
 *   Close an vnode (if open)
 *
 * Assumuptions:
 *   Caller holds the list semaphore because the file descriptor will be freed.
 *
 ****************************************************************************/

int files_close(int fd);

/****************************************************************************
 * Name: files_release
 *
 * Assumuptions:
 *   Similar to files_close().  Called only from open() logic on error
 *   conditions.
 *
 ****************************************************************************/

void files_release(int fd);

/****************************************************************************
 * Name: files_initialize
 *
 * Description:
 *   This is called from the FS initialization logic to configure the files.
 *
 ****************************************************************************/

void WEAK files_initialize(void);

int vfs_normalize_path(const char *directory, const char *filename, char **pathname);
int vfs_normalize_pathat(int fd, const char *filename, char **pathname);

struct filelist *sched_getfiles(void);

/* fs/fs_sendfile.c *************************************************/
/****************************************************************************
 * Name: sendfile
 *
 * Description:
 *   Copy data between one file descriptor and another.
 *
 ****************************************************************************/
ssize_t sendfile(int outfd, int infd, off_t *offset, size_t count);

/**
 * @ingroup  fs
 * @brief get the path by a given file fd.
 *
 * @par Description:
 * The function is used for getting the path by a given file fd.
 *
 * @attention
 * <ul>
 * <li>Only support file fd, not any dir fd.</li>
 * </ul>
 *
 * @param  fd               [IN] Type #int     file fd.
 * @param  path             [IN] Type #char ** address of the location to return the path reference.
 *
 * @retval #0      get path success
 * @retval #~0     get path failed
 *
 * @par Dependency:
 * <ul><li>fs.h: the header file that contains the API declaration.</li></ul>
 * @see
 *
 * @since 2020-1-8
 */

int get_path_from_fd(int fd, char **path);

bool get_bit(int i);

int AllocProcessFd(void);

int AllocLowestProcessFd(int minFd);

int AllocSpecifiedProcessFd(int procFd);

int AllocAndAssocProcessFd(int sysFd, int minFd);

int AllocAndAssocSystemFd(int procFd, int minFd);

void AssociateSystemFd(int procFd, int sysFd);

int DisassociateProcessFd(int procFd);

int GetAssociatedSystemFd(int procFd);

int CheckProcessFd(int procFd);

void FreeProcessFd(int procFd);

int CopyFdToProc(int fd, unsigned int targetPid);

int CloseProcFd(int fd, unsigned int targetPid);

void lsfd(void);

void set_sd_sync_fn(int (*sync_fn)(int));

struct Vnode *files_get_openfile(int fd);

void poll_wait(struct file *filp, wait_queue_head_t *wait_address, poll_table *p);

int follow_symlink(int dirfd, const char *path, struct Vnode **vnode, char **fullpath);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* __INCLUDE_FS_FILE_H */
