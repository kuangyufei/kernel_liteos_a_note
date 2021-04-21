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

#include "vfs_zpfs.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/statfs.h>
#include <unistd.h>

#include "fs/dirent_fs.h"
#include "inode/inode.h"
#include "internal.h"
#include "los_tables.h"
#include "los_vm_filemap.h"

#ifdef LOSCFG_FS_ZPFS

static int CheckEntryExist(const struct ZpfsEntry *zpfsEntry, char **fPath, struct stat *buf)
{
    int ret;
    struct inode *inode = zpfsEntry->mountedInode;
    const char *path = zpfsEntry->mountedPath;
    const char *relPath = zpfsEntry->mountedRelpath;
    int len = strlen(path) + strlen(relPath) + 1;
    int fullLen = len  + 1;
    char *fullPath = (char *)malloc(fullLen);
    if (fullPath == NULL) {
        return -1;
    }

    ret = snprintf_s(fullPath, fullLen, len, "%s/%s", path, relPath);
    if (ret < 0) {
        free(fullPath);
        return -1;
    }
    if (inode->u.i_mops->stat != NULL) {
        ret = inode->u.i_mops->stat(inode, fullPath, buf);
    } else {
        ret = -ENOSYS;
    }

    if (fPath == NULL) {
        free(fullPath);
    } else {
        *fPath = fullPath;
    }

    return ret;
}

static struct inode *GetRelInodeFromVInode(const ZpfsConfig *zpfsCfg,
    const char* relPath, struct stat *buf, char **finalPath)
{
    int ret;
    struct ZpfsEntry entry;

    for (int i = 0; i < zpfsCfg->entryCount; i++) {
        entry.mountedPath    = zpfsCfg->orgEntry[i].mountedRelpath;
        entry.mountedRelpath = (char*)relPath;
        entry.mountedInode   = zpfsCfg->orgEntry[i].mountedInode;
        ret = CheckEntryExist(&entry, finalPath, buf);
        if (ret == OK) {
            return entry.mountedInode;
        }
    }
    return NULL;
}

/****************************************************************************
 * Name: VfsZpfsRealInode
 *
 * Description:
 *   Get the inode which is hidden.
 *
 * Input Parameters:
 *   The relative path.
 * Returned Value:
 *   finalPath   the relative path of the mount point.
 *   OK
 *   NULL  memory is not enough.
 ****************************************************************************/
static inline struct inode *VfsZpfsRealInode(const ZpfsConfig *zpfsCfg, const char *relPath, char **finalPath)
{
    struct stat buf;
    struct inode *inode = GetRelInodeFromVInode(zpfsCfg, relPath, &buf, finalPath);
    return inode;
}

static int VfsZpfsOpen(struct file *file, const char *relPath, int oflAgs, mode_t mode)
{
    char *finalPath = NULL;
    struct inode *swapInode = NULL;
    int ret;

    struct inode *inode = file->f_inode;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(inode);
    inode = VfsZpfsRealInode(zpfsCfg, relPath, &finalPath);
    if ((inode == NULL) || (!inode->u.i_mops->open)) {
        if (finalPath) {
            free(finalPath);
        }
        return -ENOENT;
    }

    swapInode = file->f_inode;
    file->f_inode = inode;
    ret = inode->u.i_mops->open(file, finalPath, oflAgs, mode);
    free(finalPath);
    finalPath = NULL;
    file->f_inode = swapInode;

    return ret;
}

static int VfsZpfsClose(struct file *file)
{
    struct inode *swapInode = NULL;
    int ret;

    struct inode *inode = file->f_inode;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(inode);
    inode = VfsZpfsRealInode(zpfsCfg, file->f_relpath, NULL);
    if ((inode == NULL) || (!inode->u.i_mops->close)) {
        return -ENOSYS;
    }

    swapInode = file->f_inode;
    file->f_inode = inode;
    ret = inode->u.i_mops->close(file);
    file->f_inode = swapInode;

    return ret;
}

static ssize_t VfsZpfsRead(struct file *file, char *buffer, size_t bufLen)
{
    struct inode *swapInode = NULL;
    ssize_t sret;

    struct inode *inode = file->f_inode;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(inode);
    inode = VfsZpfsRealInode(zpfsCfg, file->f_relpath, NULL);
    if ((inode == NULL) || (!inode->u.i_mops->read)) {
        return -ENOSYS;
    }

    swapInode = file->f_inode;
    file->f_inode = inode;
    sret = inode->u.i_mops->read(file, buffer, bufLen);
    file->f_inode = swapInode;

    return sret;
}

static off_t VfsZpfsLseek(struct file *file, off_t offset, int whence)
{
    off_t off;
    struct inode *swapInode = NULL;

    struct inode *inode = file->f_inode;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(inode);
    inode = VfsZpfsRealInode(zpfsCfg, file->f_relpath, NULL);
    if ((inode == NULL) || (!inode->u.i_mops->seek)) {
        return -ENOSYS;
    }

    swapInode = file->f_inode;
    file->f_inode = inode;
    off = inode->u.i_mops->seek(file, offset, whence);
    file->f_inode = swapInode;
    return off;
}

static loff_t VfsZpfsLseek64(struct file *file, loff_t offset, int whence)
{
    loff_t off;
    struct inode *swapInode = NULL;

    struct inode *inode = file->f_inode;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(inode);
    inode = VfsZpfsRealInode(zpfsCfg, file->f_relpath, NULL);
    if ((inode == NULL) || (!inode->u.i_mops->seek64)) {
        return -ENOSYS;
    }

    swapInode = file->f_inode;
    file->f_inode = inode;
    off = inode->u.i_mops->seek64(file, offset, whence);
    file->f_inode = swapInode;
    return off;
}

static int VfsZpfsDup(const struct file *oldFile, struct file *newFile)
{
    int ret;
    struct inode *inode = oldFile->f_inode;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(inode);
    inode = VfsZpfsRealInode(zpfsCfg, oldFile->f_relpath, NULL);
    if ((inode == NULL) || (!inode->u.i_mops->dup)) {
        return -ENOSYS;
    }

    ret = inode->u.i_mops->dup(oldFile, newFile);
    return ret;
}

static int VfsZpfsOpenDir(struct inode *mountpt, const char *relPath, struct fs_dirent_s *dir)
{
    ZpfsConfig *zpfsCfg = GetZpfsConfig(mountpt);
    if (!VfsZpfsRealInode(zpfsCfg, relPath, NULL)) {
        return -ENOENT;
    }
    struct ZpfsDir *zpfsDir = malloc(sizeof(struct ZpfsDir));
    if (zpfsDir == NULL) {
        return -ENOMEM;
    }
    zpfsDir->relPath = strdup(relPath);
    if (zpfsDir->relPath == NULL) {
        return -ENOMEM;
    }
    zpfsDir->index = zpfsCfg->entryCount;
    zpfsDir->openEntry = -1;
    dir->u.zpfs = (void*)zpfsDir;

    return OK;
}

static void CloseEntry(const struct ZpfsConfig *zpfsCfg, struct fs_dirent_s *dir, const int index)
{
    struct inode *inode = zpfsCfg->orgEntry[index].mountedInode;
    if (inode->u.i_mops->closedir != NULL) {
        inode->u.i_mops->closedir(inode, dir);
    }
}

static int VfsZpfsCloseDir(struct inode *mountpt, struct fs_dirent_s *dir)
{
    struct ZpfsDir *zpfsDir = (struct ZpfsDir *)(dir->u.zpfs);
    if (zpfsDir->relPath) {
        free(zpfsDir->relPath);
        zpfsDir->relPath = NULL;
    }
    free(zpfsDir);
    zpfsDir = NULL;
    return OK;
}

static int IsExistInEntries(const struct ZpfsConfig *zpfsCfg, const char* relPath, const int index)
{
    struct ZpfsEntry entry;
    struct stat buf;
    for (int i = index; i >= 0; i--) {
        entry.mountedPath    = zpfsCfg->orgEntry[i].mountedRelpath;
        entry.mountedRelpath = (char *)relPath;
        entry.mountedInode   = zpfsCfg->orgEntry[i].mountedInode;
        if (CheckEntryExist(&entry, NULL, &buf) == OK) {
            return OK;
        }
    }
    return -1;
}

static int OpenEntry(const struct ZpfsConfig *zpfsCfg,
    const struct ZpfsDir *zpfsDir, struct fs_dirent_s *dir, const int index)
{
    int ret;
    char *fullPath;
    char *path = (char *)zpfsCfg->orgEntry[index].mountedRelpath;
    char *relPath = zpfsDir->relPath;
    struct inode *curInode = zpfsCfg->orgEntry[index].mountedInode;
    int len = strlen(path) + strlen(relPath) + 1;
    int fullLen = len  + 1;
    fullPath = (char *)malloc(fullLen);
    if (fullPath == NULL) {
        return -ENOMEM;
    }
    ret = snprintf_s(fullPath, fullLen, len, "%s/%s", path, relPath);
    if (ret < 0) {
        free(fullPath);
        return -EINVAL;
    }
    ret = -ENOSYS;
    if (curInode->u.i_mops->opendir != NULL) {
        ret = curInode->u.i_mops->opendir(curInode, fullPath, dir);
    }
    free(fullPath);
    return ret;
}

static int VfsZpfsReadDir(struct inode *mountpt, struct fs_dirent_s *dir)
{
    int ret;
    struct inode *oldInode = NULL;
    struct inode *curInode = NULL;
    struct ZpfsDir *zpfsDir = (struct ZpfsDir *)dir->u.zpfs;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(mountpt);

    int index = zpfsDir->index;
    do {
        if (zpfsDir->openEntry == -1) {
            zpfsDir->index--;
            index = zpfsDir->index;
            if (index < 0) {
                ret = -1;
                break;
            }
            ret = OpenEntry(zpfsCfg, zpfsDir, dir, index);
            if (ret != OK) {
                break;
            }
            zpfsDir->openEntry = 1;
        }

        curInode = zpfsCfg->orgEntry[index].mountedInode;
        oldInode = dir->fd_root;
        dir->fd_root = curInode;
        ret = curInode->u.i_mops->readdir(curInode, dir);
        dir->fd_root = oldInode;
        if (ret != OK) {
            if (index >= 0) {
                CloseEntry(zpfsCfg, dir, index);
                zpfsDir->openEntry = -1;
                continue;
            }
        } else if (IsExistInEntries(zpfsCfg, dir->fd_dir[0].d_name, (index - 1)) == OK) {
            continue;
        }
        ret = 1; // 1 means current op return one file
        dir->fd_position++;
        dir->fd_dir[0].d_off = dir->fd_position;
        dir->fd_dir[0].d_reclen = (uint16_t)sizeof(struct dirent);
        break;
    } while (1);

    return ret;
}

static int VfsZpfsRewindDir(struct inode *mountpt, struct fs_dirent_s *dir)
{
    PRINT_DEBUG("%s NOT support!\n", __FUNCTION__);
    return OK;
}

static int VfsZpfsBind(struct inode *blkDriver, const void *data, void **handle, const char *relPath)
{
    if (data == NULL) {
        return -1;
    }
    (*handle) = (void*)data;
    return OK;
}

static int VfsZpfsUnbind(void *handle, struct inode **blkDriver)
{
    struct inode *node = NULL;
    ZpfsConfig *zpfsCfg = NULL;
    if (handle != NULL) {
        zpfsCfg = (ZpfsConfig *)handle;
        node = inode_unlink(zpfsCfg->patchTarget);
        INODE_SET_TYPE(node, FSNODEFLAG_DELETED);
        if (node != zpfsCfg->patchInode) {
            return -EINVAL;
        }
    }
    return OK;
}

static int VfsZpfsStatFs(struct inode *mountpt, struct statfs *buf)
{
    if (buf == NULL) {
        return -EINVAL;
    }
    (void)memset_s(buf, sizeof(struct statfs), 0, sizeof(struct statfs));
    buf->f_type = ZPFS_MAGIC;
    return OK;
}

static int VfsZpfsStat(struct inode *mountpt, const char *relPath, struct stat *buf)
{
    struct inode *inode = NULL;
    ZpfsConfig *zpfsCfg = GetZpfsConfig(mountpt);
    inode = GetRelInodeFromVInode(zpfsCfg, relPath, buf, NULL);
    if (inode == NULL) {
        return -ENOENT;
    }
    return OK;
}

const struct mountpt_operations zpfsOperations = {
    VfsZpfsOpen,        /* open */
    VfsZpfsClose,       /* close */
    VfsZpfsRead,        /* read */
    NULL,               /* write */
    VfsZpfsLseek,       /* seek */
    NULL,               /* ioctl */
    OsVfsFileMmap,      /* mmap */
    NULL,               /* sync */
    VfsZpfsDup,         /* dup */
    NULL,               /* fstat */
    NULL,               /* truncate */
    VfsZpfsOpenDir,     /* opendir */
    VfsZpfsCloseDir,    /* closedir */
    VfsZpfsReadDir,     /* readdir */
    VfsZpfsRewindDir,   /* rewinddir */
    VfsZpfsBind,        /* bind */
    VfsZpfsUnbind,      /* unbind */
    VfsZpfsStatFs,      /* statfs */
    NULL,               /* virstatfs */
    NULL,               /* unlink */
    NULL,               /* mkdir */
    NULL,               /* rmdir */
    NULL,               /* rename */
    VfsZpfsStat,        /* stat */
    NULL,               /* utime */
    NULL,               /* chattr */
    VfsZpfsLseek64,     /* seek64 */
    NULL,               /* getlabel */
    NULL,               /* fallocate */
    NULL,               /* fallocate64 */
    NULL,               /* truncate64 */
    NULL,               /* fscheck */
    NULL,               /* map_pages */
    NULL,               /* readpage */
    NULL,               /* writepage */
};

FSMAP_ENTRY(zpfs_fsmap, ZPFS_NAME, zpfsOperations, FALSE, FALSE);

#endif // LOSCFG_FS_ZPFS
