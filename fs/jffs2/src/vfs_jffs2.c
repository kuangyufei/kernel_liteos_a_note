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

#include "vfs_jffs2.h"

#include "fcntl.h"
#include "sys/stat.h"
#include "sys/statfs.h"
#include "errno.h"

#include "los_config.h"
#include "los_typedef.h"
#include "los_mux.h"
#include "los_tables.h"
#include "los_vm_filemap.h"
#include "los_crc32.h"
#include "capability_type.h"
#include "capability_api.h"

#include "fs/dirent_fs.h"
#include "fs/fs.h"
#include "fs/vnode.h"
#include "mtd_list.h"
#include "mtd_partition.h"
#include "jffs2_hash.h"

#include "os-linux.h"
#include "jffs2/nodelist.h"

#ifdef LOSCFG_FS_JFFS

/* forward define */
struct VnodeOps g_jffs2Vops;
struct file_operations_vfs g_jffs2Fops;

static LosMux g_jffs2FsLock;  /* lock for all jffs2 ops */

static pthread_mutex_t g_jffs2NodeLock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
struct Vnode *g_jffs2PartList[CONFIG_MTD_PATTITION_NUM];

static void Jffs2SetVtype(struct jffs2_inode *node, struct Vnode *pVnode)
{
    switch (node->i_mode & S_IFMT) {
        case S_IFREG:
            pVnode->type = VNODE_TYPE_REG;
            break;
        case S_IFDIR:
            pVnode->type = VNODE_TYPE_DIR;
            break;
        default:
            pVnode->type = VNODE_TYPE_UNKNOWN;
            break;
    }
}

time_t Jffs2CurSec(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
        return 0;
    return (uint32_t)(tv.tv_sec);
}

void Jffs2NodeLock(void)
{
    (void)pthread_mutex_lock(&g_jffs2NodeLock);
}

void Jffs2NodeUnlock(void)
{
    (void)pthread_mutex_unlock(&g_jffs2NodeLock);
}

int VfsJffs2Bind(struct Mount *mnt, struct Vnode *blkDriver, const void *data)
{
    int ret;
    int partNo;
    mtd_partition *p = NULL;
    struct MtdDev *mtd = NULL;
    struct Vnode *pv = NULL;
    struct jffs2_inode *rootNode = NULL;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);
    p = (mtd_partition *)((struct drv_data *)blkDriver->data)->priv;
    mtd = (struct MtdDev *)(p->mtd_info);

    /* find a empty mte in partition table */
    if (mtd == NULL || mtd->type != MTD_NORFLASH) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return -EINVAL;
    }

    partNo = p->patitionnum;

    ret = jffs2_mount(partNo, &rootNode);
    if (ret != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    ret = VnodeAlloc(&g_jffs2Vops, &pv);
    if (ret != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        goto ERROR_WITH_VNODE;
    }
    rootNode->i_vnode = pv;
    pv->type = VNODE_TYPE_DIR;
    pv->data = (void *)rootNode;
    pv->originMount = mnt;
    pv->fop = &g_jffs2Fops;
    mnt->data = p;
    mnt->vnodeCovered = pv;
    pv->uid = rootNode->i_uid;
    pv->gid = rootNode->i_gid;
    pv->mode = rootNode->i_mode;

    (void)VfsHashInsert(pv, rootNode->i_ino);

    g_jffs2PartList[partNo] = blkDriver;

    LOS_MuxUnlock(&g_jffs2FsLock);

    return 0;
ERROR_WITH_VNODE:
    return ret;
}

int VfsJffs2Unbind(struct Mount *mnt, struct Vnode **blkDriver)
{
    int ret;
    mtd_partition *p = NULL;
    int partNo;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    p = (mtd_partition *)mnt->data;
    if (p == NULL) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return -EINVAL;
    }

    partNo = p->patitionnum;
    ret = jffs2_umount((struct jffs2_inode *)mnt->vnodeCovered->data);
    if (ret) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    free(p->mountpoint_name);
    p->mountpoint_name = NULL;
    *blkDriver = g_jffs2PartList[partNo];

    LOS_MuxUnlock(&g_jffs2FsLock);
    return 0;
}

int VfsJffs2Lookup(struct Vnode *parentVnode, const char *path, int len, struct Vnode **ppVnode)
{
    int ret;
    struct Vnode *newVnode = NULL;
    struct jffs2_inode *node = NULL;
    struct jffs2_inode *parentNode = NULL;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    parentNode = (struct jffs2_inode *)parentVnode->data;
    node = jffs2_lookup(parentNode, (const unsigned char *)path, len);
    if (!node) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return -ENOENT;
    }

    if (node->i_vnode) {
        *ppVnode = node->i_vnode;
        (void)VfsHashGet(parentVnode->originMount, node->i_ino, &newVnode, NULL, NULL);
        LOS_MuxUnlock(&g_jffs2FsLock);
        if (newVnode) {
            if (newVnode->data == NULL) {
                LOS_Panic("#####VfsHashGet error#####\n");
            }
            newVnode->parent = parentVnode;
            *ppVnode = newVnode;
            return 0;
        }
    }
    ret = VnodeAlloc(&g_jffs2Vops, &newVnode);
    if (ret != 0) {
        PRINT_ERR("%s-%d, ret: %x\n", __FUNCTION__, __LINE__, ret);
        (void)jffs2_iput(node);
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    Jffs2SetVtype(node, newVnode);
    node->i_vnode = newVnode;
    if (&g_jffs2Vops != parentVnode->vop) {
        LOS_Panic("jffs2 vop failed");
    }
    newVnode->vop = parentVnode->vop;
    newVnode->fop = parentVnode->fop;
    newVnode->data = node;
    newVnode->parent = parentVnode;
    newVnode->originMount = parentVnode->originMount;
    newVnode->uid = node->i_uid;
    newVnode->gid = node->i_gid;
    newVnode->mode = node->i_mode;

    (void)VfsHashInsert(newVnode, node->i_ino);

    *ppVnode = newVnode;

    LOS_MuxUnlock(&g_jffs2FsLock);
    return 0;
}

int VfsJffs2Create(struct Vnode *parentVnode, const char *path, int mode, struct Vnode **ppVnode)
{
    int ret;
    struct jffs2_inode *newNode = NULL;
    struct Vnode *newVnode = NULL;

    ret = VnodeAlloc(&g_jffs2Vops, &newVnode);
    if (ret != 0) {
        return -ENOMEM;
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);
    ret = jffs2_create((struct jffs2_inode *)parentVnode->data, (const unsigned char *)path, mode, &newNode);
    if (ret != 0) {
        VnodeFree(newVnode);
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    newVnode->type = VNODE_TYPE_REG;
    newNode->i_vnode = newVnode;
    newVnode->vop = parentVnode->vop;
    if (&g_jffs2Vops != parentVnode->vop) {
        LOS_Panic("jffs2 vop failed");
    }
    newVnode->fop = parentVnode->fop;
    newVnode->data = newNode;
    newVnode->parent = parentVnode;
    newVnode->originMount = parentVnode->originMount;
    newVnode->uid = newNode->i_uid;
    newVnode->gid = newNode->i_gid;
    newVnode->mode = newNode->i_mode;

    (void)VfsHashInsert(newVnode, newNode->i_ino);

    *ppVnode = newVnode;

    LOS_MuxUnlock(&g_jffs2FsLock);
    return 0;
}

int VfsJffs2Close(struct file *filep)
{
    return 0;
}

ssize_t VfsJffs2Read(struct file *filep, char *buffer, size_t bufLen)
{
    struct jffs2_inode *node = NULL;
    struct jffs2_inode_info *f = NULL;
    struct jffs2_sb_info *c = NULL;
    int ret;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = (struct jffs2_inode *)filep->f_vnode->data;
    f = JFFS2_INODE_INFO(node);
    c = JFFS2_SB_INFO(node->i_sb);

    off_t pos = min(node->i_size, filep->f_pos);
    off_t len = min(bufLen, (node->i_size - pos));
    ret = jffs2_read_inode_range(c, f, (unsigned char *)buffer, filep->f_pos, len);
    if (ret) {
        PRINTK("VfsJffs2Read(): read_inode_range failed %d\n", ret);
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }
    node->i_atime = Jffs2CurSec();
    filep->f_pos += len;

    LOS_MuxUnlock(&g_jffs2FsLock);

    return len;
}

ssize_t VfsJffs2Write(struct file *filep, const char *buffer, size_t bufLen)
{
    struct jffs2_inode *node = NULL;
    struct jffs2_inode_info *f = NULL;
    struct jffs2_sb_info *c = NULL;
    struct jffs2_raw_inode ri = {0};
    struct IATTR attr = {0};
    int ret;
    off_t pos;
    uint32_t writtenLen;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = (struct jffs2_inode *)filep->f_vnode->data;
    f = JFFS2_INODE_INFO(node);
    c = JFFS2_SB_INFO(node->i_sb);
    pos = filep->f_pos;

#if (LOSCFG_KERNEL_SMP == YES)
    struct super_block *sb = node->i_sb;
    UINT16 gcCpuMask = LOS_TaskCpuAffiGet(sb->s_gc_thread);
    UINT32 curTaskId = LOS_CurTaskIDGet();
    UINT16 curCpuMask = LOS_TaskCpuAffiGet(curTaskId);
    if (curCpuMask != gcCpuMask) {
        if (curCpuMask != LOSCFG_KERNEL_CPU_MASK) {
            (void)LOS_TaskCpuAffiSet(sb->s_gc_thread, curCpuMask);
        } else {
            (void)LOS_TaskCpuAffiSet(curTaskId, gcCpuMask);
        }
    }
#endif
    if (pos < 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return -EINVAL;
    }

    ri.ino = cpu_to_je32(f->inocache->ino);
    ri.mode = cpu_to_jemode(node->i_mode);
    ri.uid = cpu_to_je16(node->i_uid);
    ri.gid = cpu_to_je16(node->i_gid);
    ri.atime = ri.ctime = ri.mtime = cpu_to_je32(Jffs2CurSec());

    if (pos > node->i_size) {
        int err;
        attr.attr_chg_valid = CHG_SIZE;
        attr.attr_chg_size = pos;
        err = jffs2_setattr(node, &attr);
        if (err) {
            LOS_MuxUnlock(&g_jffs2FsLock);
            return err;
        }
    }
    ri.isize = cpu_to_je32(node->i_size);

    ret = jffs2_write_inode_range(c, f, &ri, (unsigned char *)buffer, pos, bufLen, &writtenLen);
    if (ret) {
        pos += writtenLen;

        node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);
        if (pos > node->i_size)
            node->i_size = pos;

        filep->f_pos = pos;

        LOS_MuxUnlock(&g_jffs2FsLock);

        return ret;
    }

    if (writtenLen != bufLen) {
        pos += writtenLen;

        node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);
        if (pos > node->i_size)
            node->i_size = pos;

        filep->f_pos = pos;

        LOS_MuxUnlock(&g_jffs2FsLock);

        return -ENOSPC;
    }

    pos += bufLen;

    node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);
    if (pos > node->i_size)
        node->i_size = pos;

    filep->f_pos = pos;

    LOS_MuxUnlock(&g_jffs2FsLock);

    return writtenLen;
}

off_t VfsJffs2Seek(struct file *filep, off_t offset, int whence)
{
    struct jffs2_inode *node = NULL;
    loff_t filePos;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = (struct jffs2_inode *)filep->f_vnode->data;
    filePos = filep->f_pos;

    switch (whence) {
        case SEEK_SET:
            filePos = offset;
            break;

        case SEEK_CUR:
            filePos += offset;
            break;

        case SEEK_END:
            filePos = node->i_size + offset;
            break;

        default:
            LOS_MuxUnlock(&g_jffs2FsLock);
            return -EINVAL;
    }

    LOS_MuxUnlock(&g_jffs2FsLock);

    if (filePos < 0)
        return -EINVAL;

    return filePos;
}

int VfsJffs2Ioctl(struct file *filep, int cmd, unsigned long arg)
{
    PRINT_DEBUG("%s NOT SUPPORT\n", __FUNCTION__);
    return -ENOSYS;
}

int VfsJffs2Fsync(struct file *filep)
{
    /* jffs2_write directly write to flash, sync is OK.
        BUT after pagecache enabled, pages need to be flushed to flash */
    return 0;
}

int VfsJffs2Dup(const struct file *oldFile, struct file *newFile)
{
    PRINT_DEBUG("%s NOT SUPPORT\n", __FUNCTION__);
    return -ENOSYS;
}

int VfsJffs2Opendir(struct Vnode *pVnode, struct fs_dirent_s *dir)
{
    dir->fd_int_offset = 0;
    return 0;
}

int VfsJffs2Readdir(struct Vnode *pVnode, struct fs_dirent_s *dir)
{
    int ret;
    int i = 0;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    /* set jffs2_d */
    while (i < dir->read_cnt) {
        ret = jffs2_readdir((struct jffs2_inode *)pVnode->data, &dir->fd_position,
                            &dir->fd_int_offset, &dir->fd_dir[i]);
        if (ret) {
            break;
        }

        i++;
    }

    LOS_MuxUnlock(&g_jffs2FsLock);

    return i;
}

int VfsJffs2Seekdir(struct Vnode *pVnode, struct fs_dirent_s *dir, unsigned long offset)
{
    return 0;
}

int VfsJffs2Rewinddir(struct Vnode *pVnode, struct fs_dirent_s *dir)
{
    dir->fd_int_offset = 0;

    return 0;
}

int VfsJffs2Closedir(struct Vnode *node, struct fs_dirent_s *dir)
{
    return 0;
}

int VfsJffs2Mkdir(struct Vnode *parentNode, const char *dirName, mode_t mode, struct Vnode **ppVnode)
{
    int ret;
    struct jffs2_inode *node = NULL;
    struct Vnode *newVnode = NULL;

    ret = VnodeAlloc(&g_jffs2Vops, &newVnode);
    if (ret != 0) {
        return -ENOMEM;
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    ret = jffs2_mkdir((struct jffs2_inode *)parentNode->data, (const unsigned char *)dirName, mode, &node);
    if (ret != 0) {
        VnodeFree(newVnode);
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    newVnode->type = VNODE_TYPE_DIR;
    node->i_vnode = newVnode;
    newVnode->vop = parentNode->vop;
    if (&g_jffs2Vops != parentNode->vop) {
        LOS_Panic("jffs2 vop failed");
    }
    newVnode->fop = parentNode->fop;
    newVnode->data = node;
    newVnode->parent = parentNode;
    newVnode->originMount = parentNode->originMount;
    newVnode->uid = node->i_uid;
    newVnode->gid = node->i_gid;
    newVnode->mode = node->i_mode;

    *ppVnode = newVnode;

    (void)VfsHashInsert(newVnode, node->i_ino);

    LOS_MuxUnlock(&g_jffs2FsLock);

    return 0;
}

static int Jffs2Truncate(struct Vnode *pVnode, unsigned int len)
{
    int ret;
    struct IATTR attr = {0};

    attr.attr_chg_size = len;
    attr.attr_chg_valid = CHG_SIZE;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);
    ret = jffs2_setattr((struct jffs2_inode *)pVnode->data, &attr);
    LOS_MuxUnlock(&g_jffs2FsLock);
    return ret;
}

int VfsJffs2Truncate(struct Vnode *pVnode, off_t len)
{
    int ret = Jffs2Truncate(pVnode, (unsigned int)len);
    return ret;
}

int VfsJffs2Truncate64(struct Vnode *pVnode, off64_t len)
{
    int ret = Jffs2Truncate(pVnode, (unsigned int)len);
    return ret;
}

int VfsJffs2Chattr(struct Vnode *pVnode, struct IATTR *attr)
{
    int ret;
    struct jffs2_inode *node = NULL;

    if (pVnode == NULL) {
        return -EINVAL;
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = pVnode->data;
    ret = jffs2_setattr(node, attr);
    if (ret == 0) {
        pVnode->uid = node->i_uid;
        pVnode->gid = node->i_gid;
        pVnode->mode = node->i_mode;
    }
    LOS_MuxUnlock(&g_jffs2FsLock);
    return ret;
}

int VfsJffs2Rmdir(struct Vnode *parentVnode, struct Vnode *targetVnode, char *path)
{
    int ret;

    if (!parentVnode || !targetVnode) {
        return -EINVAL;
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    ret = jffs2_rmdir((struct jffs2_inode *)parentVnode->data, (struct jffs2_inode *)targetVnode->data,
                      (const unsigned char *)path);

    LOS_MuxUnlock(&g_jffs2FsLock);
    return ret;
}

int VfsJffs2Unlink(struct Vnode *parentVnode, struct Vnode *targetVnode, char *path)
{
    int ret;

    if (!parentVnode || !targetVnode) {
        PRINTK("%s-%d parentVnode=%x, targetVnode=%x\n", __FUNCTION__, __LINE__, parentVnode, targetVnode);
        return -EINVAL;
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    ret = jffs2_unlink((struct jffs2_inode *)parentVnode->data, (struct jffs2_inode *)targetVnode->data,
                       (const unsigned char *)path);

    LOS_MuxUnlock(&g_jffs2FsLock);
    return ret;
}

int VfsJffs2Rename(struct Vnode *fromVnode, struct Vnode *toParentVnode, const char *fromName, const char *toName)
{
    int ret;
    struct Vnode *fromParentVnode = NULL;
    struct Vnode *toVnode = NULL;
    struct jffs2_inode *fromNode = NULL;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);
    fromParentVnode = fromVnode->parent;

    ret = VfsJffs2Lookup(toParentVnode, toName, strlen(toName), &toVnode);
    if (ret == 0) {
        if (toVnode->type == VNODE_TYPE_DIR) {
            ret = VfsJffs2Rmdir(toParentVnode, toVnode, (char *)toName);
        } else {
            ret = VfsJffs2Unlink(toParentVnode, toVnode, (char *)toName);
        }
        if (ret) {
            PRINTK("%s-%d remove newname(%s) failed ret=%d\n", __FUNCTION__, __LINE__, toName, ret);
            LOS_MuxUnlock(&g_jffs2FsLock);
            return ret;
        }
    }
    fromNode = (struct jffs2_inode *)fromVnode->data;
    ret = jffs2_rename((struct jffs2_inode *)fromParentVnode->data, fromNode,
        (const unsigned char *)fromName, (struct jffs2_inode *)toParentVnode->data, (const unsigned char *)toName);
    /* Careful with this: we can safely free the fromVnode AND toVnode but not fromNode, so reset the i_vnode field OR
       it will be a jungle field. With a new lookup process, we'll allocate a new vnode for it. */
    fromVnode->parent = toParentVnode;
    LOS_MuxUnlock(&g_jffs2FsLock);

    if (ret) {
        return ret;
    }

    return 0;
}

int VfsJffs2Stat(struct Vnode *pVnode, struct stat *buf)
{
    struct jffs2_inode *node = NULL;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = (struct jffs2_inode *)pVnode->data;
    switch (node->i_mode & S_IFMT) {
        case S_IFREG:
        case S_IFDIR:
            buf->st_mode = node->i_mode;
            break;

        default:
            buf->st_mode = DT_UNKNOWN;
            break;
    }

    buf->st_dev = 0;
    buf->st_ino = node->i_ino;
    buf->st_nlink = node->i_nlink;
    buf->st_uid = node->i_uid;
    buf->st_gid = node->i_gid;
    buf->st_size = node->i_size;
    buf->st_blksize = BLOCK_SIZE;
    buf->st_blocks = buf->st_size / buf->st_blksize;
    buf->st_atime = node->i_atime;
    buf->st_mtime = node->i_mtime;
    buf->st_ctime = node->i_ctime;

    LOS_MuxUnlock(&g_jffs2FsLock);

    return 0;
}

int VfsJffs2Reclaim(struct Vnode *pVnode)
{
    int ret;
    struct jffs2_inode *node = NULL;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = pVnode->data;
    if (node == NULL) {
        return LOS_OK;
    }

    node->i_vnode = NULL;
    ret = jffs2_iput(node);

    LOS_MuxUnlock(&g_jffs2FsLock);

    return ret;
}

int VfsJffs2Statfs(struct Mount *mnt, struct statfs *buf)
{
    unsigned long freeSize;
    struct jffs2_sb_info *c = NULL;
    struct jffs2_inode *rootNode = NULL;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    rootNode = (struct jffs2_inode *)mnt->vnodeCovered->data;
    c = JFFS2_SB_INFO(rootNode->i_sb);

    freeSize = c->free_size + c->dirty_size;
    buf->f_type = JFFS2_SUPER_MAGIC;
    buf->f_bsize = PAGE_SIZE;
    buf->f_blocks = (((uint64_t)c->nr_blocks) * c->sector_size) / PAGE_SIZE;
    buf->f_bfree = freeSize / PAGE_SIZE;
    buf->f_bavail = buf->f_bfree;
    buf->f_namelen = NAME_MAX;
    buf->f_fsid.__val[0] = JFFS2_SUPER_MAGIC;
    buf->f_fsid.__val[1] = 1;
    buf->f_frsize = BLOCK_SIZE;
    buf->f_files = 0;
    buf->f_ffree = 0;
    buf->f_flags = mnt->mountFlags;

    LOS_MuxUnlock(&g_jffs2FsLock);
    return 0;
}

int Jffs2MutexCreate(void)
{
    if (LOS_MuxInit(&g_jffs2FsLock, NULL) != LOS_OK) {
        PRINT_ERR("%s, LOS_MuxCreate failed\n", __FUNCTION__);
        return -1;
    } else {
        return 0;
    }
}

void Jffs2MutexDelete(void)
{
    (void)LOS_MuxDestroy(&g_jffs2FsLock);
}

const struct MountOps jffs_operations = {
    .Mount = VfsJffs2Bind,
    .Unmount = VfsJffs2Unbind,
    .Statfs = VfsJffs2Statfs,
};

struct VnodeOps g_jffs2Vops = {
    .Lookup = VfsJffs2Lookup,
    .Create = VfsJffs2Create,
    .Rename = VfsJffs2Rename,
    .Mkdir = VfsJffs2Mkdir,
    .Getattr = VfsJffs2Stat,
    .Opendir = VfsJffs2Opendir,
    .Readdir = VfsJffs2Readdir,
    .Closedir = VfsJffs2Closedir,
    .Rewinddir = VfsJffs2Rewinddir,
    .Unlink = VfsJffs2Unlink,
    .Rmdir = VfsJffs2Rmdir,
    .Chattr = VfsJffs2Chattr,
    .Reclaim = VfsJffs2Reclaim,
    .Truncate = VfsJffs2Truncate,
    .Truncate64 = VfsJffs2Truncate64,
};

struct file_operations_vfs g_jffs2Fops = {
    .read = VfsJffs2Read,
    .write = VfsJffs2Write,
    .mmap = OsVfsFileMmap,
    .seek = VfsJffs2Seek,
    .close = VfsJffs2Close,
    .fsync = VfsJffs2Fsync,
};


FSMAP_ENTRY(jffs_fsmap, "jffs2", jffs_operations, TRUE, TRUE);

#endif
