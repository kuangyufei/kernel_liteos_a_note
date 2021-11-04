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

#include "proc_file.h"

#include <sys/statfs.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "fs/dirent_fs.h"
#include "fs/mount.h"
#include "fs/fs.h"
#include "los_tables.h"
#include "internal.h"

/**
 * @brief 
 * @verbatim
鸿蒙的/proc目录是一种文件系统，即proc文件系统。与其它常见的文件系统不同的是，/proc是一种伪文件系统（也即虚拟文件系统），
存储的是当前内核运行状态的一系列特殊文件，用户可以通过这些文件查看有关系统硬件及当前正在运行进程的信息，
甚至可以通过更改其中某些文件来改变内核的运行状态。

基于/proc文件系统如上所述的特殊性，其内的文件也常被称作虚拟文件，并具有一些独特的特点。
例如，其中有些文件虽然使用查看命令查看时会返回大量信息，但文件本身的大小却会显示为0字节。
此外，这些特殊文件中大多数文件的时间及日期属性通常为当前系统时间和日期，这跟它们随时会被刷新（存储于RAM中）有关。

为了查看及使用上的方便，这些文件通常会按照相关性进行分类存储于不同的目录甚至子目录中，
如/proc/mounts 目录中存储的就是当前系统上所有装载点的相关信息，

大多数虚拟文件可以使用文件查看命令如cat、more或者less进行查看，有些文件信息表述的内容可以一目了然，
 * @endverbatim
 */
#ifdef LOSCFG_FS_PROC
static struct VnodeOps g_procfsVops; /// proc 文件系统
static struct file_operations_vfs g_procfsFops;

/// 通过节点获取私有内存对象,注意要充分理解 node->data 的作用,那是个可以通天的神奇变量. 
static struct ProcDirEntry *VnodeToEntry(struct Vnode *node)
{
    return (struct ProcDirEntry *)(node->data);
}
/// 创建节点,通过实体对象转成vnode节点,如此达到统一管理的目的.
static struct Vnode *EntryToVnode(struct ProcDirEntry *entry)
{
    struct Vnode *node = NULL;

    (void)VnodeAlloc(&g_procfsVops, &node);//申请一个 vnode节点,并设置节点操作
    node->fop = &g_procfsFops;//设置文件操作系列函数
    node->data = entry;//绑定实体
    node->type = entry->type;//实体类型 (文件|目录)
    node->uid = entry->uid;	//用户ID
    node->gid = entry->gid;	//组ID
    node->mode = entry->mode;//读/写/执行模式
    return node;
}
///实体匹配,通过名称匹配
static int EntryMatch(const char *name, int len, const struct ProcDirEntry *pn)
{
    if (len != pn->nameLen) {
        return 0;
    }
    return !strncmp(name, pn->name, len);
}

int VfsProcfsTruncate(struct Vnode *pVnode, off_t len)
{
    return 0;
}
///创建vnode节点,并绑定私有内容项
int VfsProcfsCreate(struct Vnode* parent, const char *name, int mode, struct Vnode **vnode)
{
    int ret;
    struct Vnode *vp = NULL;
    struct ProcDirEntry *curEntry = NULL;

    struct ProcDirEntry *parentEntry = VnodeToEntry(parent);
    if (parentEntry == NULL) {
        return -ENODATA;
    }

    ret = VnodeAlloc(&g_procfsVops, &vp);
    if (ret != 0) {
        return -ENOMEM;
    }

    curEntry = ProcCreate(name, mode, parentEntry, NULL);
    if (curEntry == NULL) {
        VnodeFree(vp);
        return -ENODATA;
    }

    vp->data = curEntry;
    vp->type = curEntry->type;
    if (vp->type == VNODE_TYPE_DIR) {
        vp->mode = S_IFDIR | PROCFS_DEFAULT_MODE;
    } else {
        vp->mode = S_IFREG | PROCFS_DEFAULT_MODE;
    }

    vp->vop = parent->vop;
    vp->fop = parent->fop;
    vp->parent = parent;
    vp->originMount = parent->originMount;

    *vnode = vp;

    return LOS_OK;
}

int VfsProcfsRead(struct file *filep, char *buffer, size_t buflen)
{
    ssize_t size;
    struct ProcDirEntry *entry = NULL;
    if ((filep == NULL) || (filep->f_vnode == NULL) || (buffer == NULL)) {
        return -EINVAL;
    }

    entry = VnodeToEntry(filep->f_vnode);
    size = (ssize_t)ReadProcFile(entry, (void *)buffer, buflen);
    filep->f_pos = entry->pf->fPos;

    return size;
}

int VfsProcfsWrite(struct file *filep, const char *buffer, size_t buflen)
{
    ssize_t size;
    struct ProcDirEntry *entry = NULL;
    if ((filep == NULL) || (filep->f_vnode == NULL) || (buffer == NULL)) {
        return -EINVAL;
    }

    entry = VnodeToEntry(filep->f_vnode);
    size = (ssize_t)WriteProcFile(entry, (void *)buffer, buflen);
    filep->f_pos = entry->pf->fPos;

    return size;
}

int VfsProcfsLookup(struct Vnode *parent, const char *name, int len, struct Vnode **vpp)
{
    if (parent == NULL || name == NULL || len <= 0 || vpp == NULL) {
        return -EINVAL;
    }
    struct ProcDirEntry *entry = VnodeToEntry(parent);
    if (entry == NULL) {
        return -ENODATA;
    }
    entry = entry->subdir;
    while (1) {
        if (entry == NULL) {
            return -ENOENT;
        }
        if (EntryMatch(name, len, entry)) {
            break;
        }
        entry = entry->next;
    }

    *vpp = EntryToVnode(entry);
    if ((*vpp) == NULL) {
        return -ENOMEM;
    }
    (*vpp)->originMount = parent->originMount;//继承挂载信息
    (*vpp)->parent = parent;
    return LOS_OK;
}
///挂s载实现,找个vnode节点挂上去 
int VfsProcfsMount(struct Mount *mnt, struct Vnode *device, const void *data)
{
    struct Vnode *vp = NULL;
    int ret;

    spin_lock_init(&procfsLock);
    procfsInit = true;		//已初始化 /proc 模块

    ret = VnodeAlloc(&g_procfsVops, &vp);//分配一个节点
    if (ret != 0) {
        return -ENOMEM;
    }

    struct ProcDirEntry *root = GetProcRootEntry();
    vp->data = root;
    vp->originMount = mnt;//绑定mount
    vp->fop = &g_procfsFops;//指定文件系统
    mnt->data = NULL;
    mnt->vnodeCovered = vp;
    vp->type = root->type;
    if (vp->type == VNODE_TYPE_DIR) {//目录节点
        vp->mode = S_IFDIR | PROCFS_DEFAULT_MODE;//贴上目录标签
    } else {
        vp->mode = S_IFREG | PROCFS_DEFAULT_MODE;//贴上文件标签
    }

    return LOS_OK;
}

int VfsProcfsUnmount(void *handle, struct Vnode **blkdriver)
{
    (void)handle;
    (void)blkdriver;
    return -EPERM;
}

int VfsProcfsStat(struct Vnode *node, struct stat *buf)
{
    struct ProcDirEntry *entry = VnodeToEntry(node);

    (void)memset_s(buf, sizeof(struct stat), 0, sizeof(struct stat));
    buf->st_mode = entry->mode;

    return LOS_OK;
}

int VfsProcfsReaddir(struct Vnode *node, struct fs_dirent_s *dir)
{
    int result;
    char *buffer = NULL;
    int buflen = NAME_MAX;
    unsigned int min_size;
    unsigned int dst_name_size;
    struct ProcDirEntry *pde = NULL;
    int i = 0;

    if (dir == NULL) {
        return -EINVAL;
    }
    if (node->type != VNODE_TYPE_DIR) {
        return -ENOTDIR;
    }
    pde = VnodeToEntry(node);

    while (i < dir->read_cnt) {
        buffer = (char *)zalloc(sizeof(char) * NAME_MAX);
        if (buffer == NULL) {
            PRINT_ERR("malloc failed\n");
            return -ENOMEM;
        }

        result = ReadProcFile(pde, (void *)buffer, buflen);
        if (result != ENOERR) {
            free(buffer);
            break;
        }
        dst_name_size = sizeof(dir->fd_dir[i].d_name);
        min_size = (dst_name_size < NAME_MAX) ? dst_name_size : NAME_MAX;
        result = strncpy_s(dir->fd_dir[i].d_name, dst_name_size, buffer, min_size);
        if (result != EOK) {
            free(buffer);
            return -ENAMETOOLONG;
        }
        dir->fd_dir[i].d_name[dst_name_size - 1] = '\0';
        dir->fd_position++;
        dir->fd_dir[i].d_off = dir->fd_position;
        dir->fd_dir[i].d_reclen = (uint16_t)sizeof(struct dirent);

        i++;
        free(buffer);
    }

    return i;
}
///proc 打开目录
int VfsProcfsOpendir(struct Vnode *node,  struct fs_dirent_s *dir)
{
    struct ProcDirEntry *pde = VnodeToEntry(node);
    if (pde == NULL) {
        return -EINVAL;
    }
    pde->pdirCurrent = pde->subdir;
    pde->pf->fPos = 0;

    return LOS_OK;
}
///proc 打开文件
int VfsProcfsOpen(struct file *filep)
{
    if (filep == NULL) {
        return -EINVAL;
    }
    struct Vnode *node = filep->f_vnode;//找到vnode节点
    struct ProcDirEntry *pde = VnodeToEntry(node);//拿到私有数据(内存对象)
    if (ProcOpen(pde->pf) != OK) {
        return -ENOMEM;
    }
    if (S_ISREG(pde->mode) && (pde->procFileOps != NULL) && (pde->procFileOps->open != NULL)) {
        (void)pde->procFileOps->open((struct Vnode *)pde, pde->pf);
    }
    if (S_ISDIR(pde->mode)) {
        pde->pdirCurrent = pde->subdir;
        pde->pf->fPos = 0;
    }
    filep->f_priv = (void *)pde;
    return LOS_OK;
}

int VfsProcfsClose(struct file *filep)
{
    int result = 0;
    if (filep == NULL) {
        return -EINVAL;
    }
    struct Vnode *node = filep->f_vnode;
    struct ProcDirEntry *pde = VnodeToEntry(node);
    pde->pf->fPos = 0;
    if ((pde->procFileOps != NULL) && (pde->procFileOps->release != NULL)) {
        result = pde->procFileOps->release((struct Vnode *)pde, pde->pf);
    }
    LosBufRelease(pde->pf->sbuf);
    pde->pf->sbuf = NULL;

    return result;
}
///统计信息接口,简单实现
int VfsProcfsStatfs(struct Mount *mnt, struct statfs *buf)
{
    (void)memset_s(buf, sizeof(struct statfs), 0, sizeof(struct statfs));
    buf->f_type = PROCFS_MAGIC;

    return LOS_OK;
}

int VfsProcfsClosedir(struct Vnode *vp, struct fs_dirent_s *dir)
{
    return LOS_OK;
}
/// proc 对 MountOps 接口实现
const struct MountOps procfs_operations = {
    .Mount = VfsProcfsMount,//装载
    .Unmount = NULL,
    .Statfs = VfsProcfsStatfs,//统计信息
};
// proc 对 VnodeOps 接口实现,暂没有实现创建节点的功能.
static struct VnodeOps g_procfsVops = { 
    .Lookup = VfsProcfsLookup,
    .Getattr = VfsProcfsStat,
    .Readdir = VfsProcfsReaddir,
    .Opendir = VfsProcfsOpendir,
    .Closedir = VfsProcfsClosedir,
    .Truncate = VfsProcfsTruncate
};
// proc 对 file_operations_vfs 接口实现
static struct file_operations_vfs g_procfsFops = {
    .read = VfsProcfsRead,	// 最终调用 ProcFileOperations -> read
    .write = VfsProcfsWrite,// 最终调用 ProcFileOperations -> write
    .open = VfsProcfsOpen,	// 最终调用 ProcFileOperations -> open
    .close = VfsProcfsClose	// 最终调用 ProcFileOperations -> release
};
//文件系统注册入口
FSMAP_ENTRY(procfs_fsmap, "procfs", procfs_operations, FALSE, FALSE);
#endif
