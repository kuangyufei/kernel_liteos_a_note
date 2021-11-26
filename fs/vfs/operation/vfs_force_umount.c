/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

#include "fs/mount.h"
#include "fs/dirent_fs.h"
#include "fs/file.h"
#include "vnode.h"
#include "path_cache.h"

/* vnode operations returns EIO */
static int ErrorVopCreate (struct Vnode *parent, const char *name, int mode, struct Vnode **vnode)
{
    return -EIO;
}

static int ErrorVopLookup (struct Vnode *parent, const char *name, int len, struct Vnode **vnode)
{
    return -EIO;
}

static int ErrorVopOpen (struct Vnode *vnode, int fd, int mode, int flags)
{
    return -EIO;
}

static int ErrorVopClose (struct Vnode *vnode)
{
    /* already closed at force umount, do nothing here */
    return OK;
}

static int ErrorVopReclaim (struct Vnode *vnode)
{
    return -EIO;
}

static int ErrorVopUnlink (struct Vnode *parent, struct Vnode *vnode, const char *fileName)
{
    return -EIO;
}

static int ErrorVopRmdir (struct Vnode *parent, struct Vnode *vnode, const char *dirName)
{
    return -EIO;
}

static int ErrorVopMkdir (struct Vnode *parent, const char *dirName, mode_t mode, struct Vnode **vnode)
{
    return -EIO;
}

static int ErrorVopReaddir (struct Vnode *vnode, struct fs_dirent_s *dir)
{
    return -EIO;
}

static int ErrorVopOpendir (struct Vnode *vnode, struct fs_dirent_s *dir)
{
    return -EIO;
}

static int ErrorVopRewinddir (struct Vnode *vnode, struct fs_dirent_s *dir)
{
    return -EIO;
}

static int ErrorVopClosedir (struct Vnode *vnode, struct fs_dirent_s *dir)
{
    /* already closed at force umount, do nothing here */
    return OK;
}

static int ErrorVopGetattr (struct Vnode *vnode, struct stat *st)
{
    return -EIO;
}

static int ErrorVopSetattr (struct Vnode *vnode, struct stat *st)
{
    return -EIO;
}

static int ErrorVopChattr (struct Vnode *vnode, struct IATTR *attr)
{
    return -EIO;
}

static int ErrorVopRename (struct Vnode *src, struct Vnode *dstParent, const char *srcName, const char *dstName)
{
    return -EIO;
}

static int ErrorVopTruncate (struct Vnode *vnode, off_t len)
{
    return -EIO;
}

static int ErrorVopTruncate64 (struct Vnode *vnode, off64_t len)
{
    return -EIO;
}

static int ErrorVopFscheck (struct Vnode *vnode, struct fs_dirent_s *dir)
{
    return -EIO;
}

static int ErrorVopLink (struct Vnode *src, struct Vnode *dstParent, struct Vnode **dst, const char *dstName)
{
    return -EIO;
}

static int ErrorVopSymlink (struct Vnode *parentVnode, struct Vnode **newVnode, const char *path, const char *target)
{
    return -EIO;
}

static ssize_t ErrorVopReadlink (struct Vnode *vnode, char *buffer, size_t bufLen)
{
    return -EIO;
}

static struct VnodeOps g_errorVnodeOps = {
    .Create = ErrorVopCreate,
    .Lookup = ErrorVopLookup,
    .Open = ErrorVopOpen,
    .Close = ErrorVopClose,
    .Reclaim = ErrorVopReclaim,
    .Unlink = ErrorVopUnlink,
    .Rmdir = ErrorVopRmdir,
    .Mkdir = ErrorVopMkdir,
    .Readdir = ErrorVopReaddir,
    .Opendir = ErrorVopOpendir,
    .Rewinddir = ErrorVopRewinddir,
    .Closedir = ErrorVopClosedir,
    .Getattr = ErrorVopGetattr,
    .Setattr = ErrorVopSetattr,
    .Chattr = ErrorVopChattr,
    .Rename = ErrorVopRename,
    .Truncate = ErrorVopTruncate,
    .Truncate64 = ErrorVopTruncate64,
    .Fscheck = ErrorVopFscheck,
    .Link = ErrorVopLink,
    .Symlink = ErrorVopSymlink,
    .Readlink = ErrorVopReadlink,
};

/* file operations returns EIO */
static int ErrorFopOpen (struct file *filep)
{
    return -EIO;
}

static int ErrorFopClose (struct file *filep)
{
    /* already closed at force umount, do nothing here */
    return OK;
}

static ssize_t ErrorFopRead (struct file *filep, char *buffer, size_t buflen)
{
    return -EIO;
}

static ssize_t ErrorFopWrite (struct file *filep, const char *buffer, size_t buflen)
{
    return -EIO;
}

static off_t ErrorFopSeek (struct file *filep, off_t offset, int whence)
{
    return -EIO;
}

static int ErrorFopIoctl (struct file *filep, int cmd, unsigned long arg)
{
    return -EIO;
}

static int ErrorFopMmap (struct file* filep, struct VmMapRegion *region)
{
    return -EIO;
}

static int ErrorFopPoll (struct file *filep, poll_table *fds)
{
    return -EIO;
}

static int ErrorFopStat (struct file *filep, struct stat* st)
{
    return -EIO;
}

static int ErrorFopFallocate (struct file* filep, int mode, off_t offset, off_t len)
{
    return -EIO;
}

static int ErrorFopFallocate64 (struct file *filep, int mode, off64_t offset, off64_t len)
{
    return -EIO;
}

static int ErrorFopFsync (struct file *filep)
{
    return -EIO;
}

static ssize_t ErrorFopReadpage (struct file *filep, char *buffer, size_t buflen)
{
    return -EIO;
}

static int ErrorFopUnlink (struct Vnode *vnode)
{
    return -EIO;
}

static struct file_operations_vfs g_errorFileOps = {
    .open = ErrorFopOpen,
    .close = ErrorFopClose,
    .read = ErrorFopRead,
    .write = ErrorFopWrite,
    .seek = ErrorFopSeek,
    .ioctl = ErrorFopIoctl,
    .mmap = ErrorFopMmap,
    .poll = ErrorFopPoll,
    .stat = ErrorFopStat,
    .fallocate = ErrorFopFallocate,
    .fallocate64 = ErrorFopFallocate64,
    .fsync = ErrorFopFsync,
    .readpage = ErrorFopReadpage,
    .unlink = ErrorFopUnlink,
};

static struct Mount* GetDevMountPoint(struct Vnode *dev)
{
    struct Mount *mnt = NULL;
    LIST_HEAD *mntList = GetMountList();
    if (mntList == NULL) {
        return NULL;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(mnt, mntList, struct Mount, mountList) {
        if (mnt->vnodeDev == dev) {
            return mnt;
        }
    }
    return NULL;
}

static void DirPreClose(struct fs_dirent_s *dirp)
{
    struct Vnode *node = NULL;
    if (dirp == NULL || dirp->fd_root == NULL) {
        return;
    }

    node = dirp->fd_root;
    if (node->vop && node->vop->Closedir) {
        node->vop->Closedir(node, dirp);
    }
}

static void FilePreClose(struct file *filep, const struct file_operations_vfs *ops)
{
    if (filep->f_oflags & O_DIRECTORY) {
        DirPreClose(filep->f_dir);
        return;
    }

    if (ops && ops->close) {
        ops->close(filep);
    }
}

static void FileDisableAndClean(struct Mount *mnt)
{
    struct filelist *flist = &tg_filelist;
    struct file *filep = NULL;
    const struct file_operations_vfs *originOps = NULL;

    for (int i = 3; i < CONFIG_NFILE_DESCRIPTORS; i++) {
        if (!get_bit(i)) {
            continue;
        }
        filep = &flist->fl_files[i];
        if (filep == NULL || filep->f_vnode == NULL) {
            continue;
        }
        if (filep->f_vnode->originMount != mnt) {
            continue;
        }
        originOps = filep->ops;
        filep->ops = &g_errorFileOps;
        FilePreClose(filep, originOps);
    }
}

static void VnodeTryFree(struct Vnode *vnode)
{
    if (vnode->useCount == 0) {
        VnodeFree(vnode);
        return;
    }

    VnodePathCacheFree(vnode);
    LOS_ListDelete(&(vnode->hashEntry));
    LOS_ListDelete(&vnode->actFreeEntry);

    if (vnode->vop->Reclaim) {
        vnode->vop->Reclaim(vnode);
    }
    vnode->vop = &g_errorVnodeOps;
    vnode->fop = &g_errorFileOps;
}

static void VnodeTryFreeAll(struct Mount *mount)
{
    struct Vnode *vnode = NULL;
    struct Vnode *nextVnode = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(vnode, nextVnode, GetVnodeActiveList(), struct Vnode, actFreeEntry) {
        if ((vnode->originMount != mount) || (vnode->flag & VNODE_FLAG_MOUNT_NEW)) {
            continue;
        }
        VnodeTryFree(vnode);
    }
}

int ForceUmountDev(struct Vnode *dev)
{
    int ret;
    struct Vnode *origin = NULL;
    struct filelist *flist = &tg_filelist;
    if (dev == NULL) {
        return -EINVAL;
    }

    (void)sem_wait(&flist->fl_sem);
    VnodeHold();

    struct Mount *mnt = GetDevMountPoint(dev);
    if (mnt == NULL) {
        VnodeDrop();
        (void)sem_post(&flist->fl_sem);
        return -ENXIO;
    }
    origin = mnt->vnodeBeCovered;

    FileDisableAndClean(mnt);
    VnodeTryFreeAll(mnt);
    ret = mnt->ops->Unmount(mnt, &dev);
    if (ret != OK) {
        PRINT_ERR("unmount in fs failed, ret = %d, errno = %d\n", ret, errno);
    }

    LOS_ListDelete(&mnt->mountList);
    free(mnt);
    origin->newMount = NULL;
    origin->flag &= ~(VNODE_FLAG_MOUNT_ORIGIN);

    VnodeDrop();
    (void)sem_post(&flist->fl_sem);

    return OK;
}
