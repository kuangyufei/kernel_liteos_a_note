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
/**
 * @brief   创建文件操作错误处理函数
 * @param   parent     父目录vnode
 * @param   name       文件名
 * @param   mode       文件权限模式
 * @param   vnode      返回的vnode指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopCreate(struct Vnode *parent, const char *name, int mode, struct Vnode **vnode)
{
    (void)parent;  // 未使用的参数
    (void)name;    // 未使用的参数
    (void)mode;    // 未使用的参数
    (void)vnode;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   查找文件操作错误处理函数
 * @param   parent     父目录vnode
 * @param   name       文件名
 * @param   len        文件名长度
 * @param   vnode      返回的vnode指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopLookup(struct Vnode *parent, const char *name, int len, struct Vnode **vnode)
{
    (void)parent;  // 未使用的参数
    (void)name;    // 未使用的参数
    (void)len;     // 未使用的参数
    (void)vnode;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   打开文件操作错误处理函数
 * @param   vnode      文件vnode
 * @param   fd         文件描述符
 * @param   mode       打开模式
 * @param   flags      打开标志
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopOpen(struct Vnode *vnode, int fd, int mode, int flags)
{
    (void)vnode;   // 未使用的参数
    (void)fd;      // 未使用的参数
    (void)mode;    // 未使用的参数
    (void)flags;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   关闭文件操作错误处理函数
 * @param   vnode      文件vnode
 * @return  始终返回OK，强制卸载时已关闭
 */
static int ErrorVopClose(struct Vnode *vnode)
{
    (void)vnode;   // 未使用的参数
    /* 强制卸载时已关闭，此处不执行任何操作 */
    return OK;     // 返回成功
}

/**
 * @brief   回收vnode操作错误处理函数
 * @param   vnode      要回收的vnode
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopReclaim(struct Vnode *vnode)
{
    (void)vnode;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   删除文件操作错误处理函数
 * @param   parent     父目录vnode
 * @param   vnode      要删除的文件vnode
 * @param   fileName   文件名
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopUnlink(struct Vnode *parent, struct Vnode *vnode, const char *fileName)
{
    (void)parent;  // 未使用的参数
    (void)vnode;   // 未使用的参数
    (void)fileName;// 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   删除目录操作错误处理函数
 * @param   parent     父目录vnode
 * @param   vnode      要删除的目录vnode
 * @param   dirName    目录名
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopRmdir(struct Vnode *parent, struct Vnode *vnode, const char *dirName)
{
    (void)parent;  // 未使用的参数
    (void)vnode;   // 未使用的参数
    (void)dirName; // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   创建目录操作错误处理函数
 * @param   parent     父目录vnode
 * @param   dirName    目录名
 * @param   mode       目录权限模式
 * @param   vnode      返回的目录vnode指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopMkdir(struct Vnode *parent, const char *dirName, mode_t mode, struct Vnode **vnode)
{
    (void)parent;  // 未使用的参数
    (void)dirName; // 未使用的参数
    (void)mode;    // 未使用的参数
    (void)vnode;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   读取目录操作错误处理函数
 * @param   vnode      目录vnode
 * @param   dir        目录项结构
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopReaddir(struct Vnode *vnode, struct fs_dirent_s *dir)
{
    (void)vnode;   // 未使用的参数
    (void)dir;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   打开目录操作错误处理函数
 * @param   vnode      目录vnode
 * @param   dir        目录项结构
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopOpendir(struct Vnode *vnode, struct fs_dirent_s *dir)
{
    (void)vnode;   // 未使用的参数
    (void)dir;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   重绕目录操作错误处理函数
 * @param   vnode      目录vnode
 * @param   dir        目录项结构
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopRewinddir(struct Vnode *vnode, struct fs_dirent_s *dir)
{
    (void)vnode;   // 未使用的参数
    (void)dir;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   关闭目录操作错误处理函数
 * @param   vnode      目录vnode
 * @param   dir        目录项结构
 * @return  始终返回OK，强制卸载时已关闭
 */
static int ErrorVopClosedir(struct Vnode *vnode, struct fs_dirent_s *dir)
{
    (void)vnode;   // 未使用的参数
    (void)dir;     // 未使用的参数
    /* 强制卸载时已关闭，此处不执行任何操作 */
    return OK;     // 返回成功
}

/**
 * @brief   获取文件属性操作错误处理函数
 * @param   vnode      文件vnode
 * @param   st         stat结构指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopGetattr(struct Vnode *vnode, struct stat *st)
{
    (void)vnode;   // 未使用的参数
    (void)st;      // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   设置文件属性操作错误处理函数
 * @param   vnode      文件vnode
 * @param   st         stat结构指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopSetattr(struct Vnode *vnode, struct stat *st)
{
    (void)vnode;   // 未使用的参数
    (void)st;      // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   修改文件属性操作错误处理函数
 * @param   vnode      文件vnode
 * @param   attr       属性结构指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopChattr(struct Vnode *vnode, struct IATTR *attr)
{
    (void)vnode;   // 未使用的参数
    (void)attr;    // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   重命名文件操作错误处理函数
 * @param   src        源文件vnode
 * @param   dstParent  目标父目录vnode
 * @param   srcName    源文件名
 * @param   dstName    目标文件名
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopRename(struct Vnode *src, struct Vnode *dstParent, const char *srcName, const char *dstName)
{
    (void)src;     // 未使用的参数
    (void)dstParent;// 未使用的参数
    (void)srcName; // 未使用的参数
    (void)dstName; // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   截断文件操作错误处理函数
 * @param   vnode      文件vnode
 * @param   len        截断长度
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopTruncate(struct Vnode *vnode, off_t len)
{
    (void)vnode;   // 未使用的参数
    (void)len;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   64位截断文件操作错误处理函数
 * @param   vnode      文件vnode
 * @param   len        64位截断长度
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopTruncate64(struct Vnode *vnode, off64_t len)
{
    (void)vnode;   // 未使用的参数
    (void)len;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   文件系统检查操作错误处理函数
 * @param   vnode      文件vnode
 * @param   dir        目录项结构
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopFscheck(struct Vnode *vnode, struct fs_dirent_s *dir)
{
    (void)vnode;   // 未使用的参数
    (void)dir;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   创建链接操作错误处理函数
 * @param   src        源文件vnode
 * @param   dstParent  目标父目录vnode
 * @param   dst        返回的目标vnode指针
 * @param   dstName    目标文件名
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopLink(struct Vnode *src, struct Vnode *dstParent, struct Vnode **dst, const char *dstName)
{
    (void)src;     // 未使用的参数
    (void)dstParent;// 未使用的参数
    (void)dst;     // 未使用的参数
    (void)dstName; // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   创建符号链接操作错误处理函数
 * @param   parentVnode 父目录vnode
 * @param   newVnode    返回的新vnode指针
 * @param   path        符号链接路径
 * @param   target      目标路径
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorVopSymlink(struct Vnode *parentVnode, struct Vnode **newVnode, const char *path, const char *target)
{
    (void)parentVnode; // 未使用的参数
    (void)newVnode;    // 未使用的参数
    (void)path;        // 未使用的参数
    (void)target;      // 未使用的参数
    return -EIO;       // 返回I/O错误
}

/**
 * @brief   读取符号链接操作错误处理函数
 * @param   vnode      符号链接vnode
 * @param   buffer     缓冲区
 * @param   bufLen     缓冲区长度
 * @return  始终返回-EIO表示I/O错误
 */
static ssize_t ErrorVopReadlink(struct Vnode *vnode, char *buffer, size_t bufLen)
{
    (void)vnode;   // 未使用的参数
    (void)buffer;  // 未使用的参数
    (void)bufLen;  // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   错误处理vnode操作集
 * @details 所有操作均返回-EIO错误，用于文件系统强制卸载后禁用vnode操作
 */
static struct VnodeOps g_errorVnodeOps = {
    .Create = ErrorVopCreate,        // 创建文件操作
    .Lookup = ErrorVopLookup,        // 查找文件操作
    .Open = ErrorVopOpen,            // 打开文件操作
    .Close = ErrorVopClose,          // 关闭文件操作
    .Reclaim = ErrorVopReclaim,      // 回收vnode操作
    .Unlink = ErrorVopUnlink,        // 删除文件操作
    .Rmdir = ErrorVopRmdir,          // 删除目录操作
    .Mkdir = ErrorVopMkdir,          // 创建目录操作
    .Readdir = ErrorVopReaddir,      // 读取目录操作
    .Opendir = ErrorVopOpendir,      // 打开目录操作
    .Rewinddir = ErrorVopRewinddir,  // 重绕目录操作
    .Closedir = ErrorVopClosedir,    // 关闭目录操作
    .Getattr = ErrorVopGetattr,      // 获取文件属性操作
    .Setattr = ErrorVopSetattr,      // 设置文件属性操作
    .Chattr = ErrorVopChattr,        // 修改文件属性操作
    .Rename = ErrorVopRename,        // 重命名文件操作
    .Truncate = ErrorVopTruncate,    // 截断文件操作
    .Truncate64 = ErrorVopTruncate64,// 64位截断文件操作
    .Fscheck = ErrorVopFscheck,      // 文件系统检查操作
    .Link = ErrorVopLink,            // 创建链接操作
    .Symlink = ErrorVopSymlink,      // 创建符号链接操作
    .Readlink = ErrorVopReadlink,    // 读取符号链接操作
};

/* 文件操作返回EIO错误 */
/**
 * @brief   打开文件操作错误处理函数
 * @param   filep      文件结构体指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopOpen(struct file *filep)
{
    (void)filep;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   关闭文件操作错误处理函数
 * @param   filep      文件结构体指针
 * @return  始终返回OK，强制卸载时已关闭
 */
static int ErrorFopClose(struct file *filep)
{
    (void)filep;   // 未使用的参数
    /* 强制卸载时已关闭，此处不执行任何操作 */
    return OK;     // 返回成功
}

/**
 * @brief   读取文件操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   buffer     缓冲区
 * @param   buflen     读取长度
 * @return  始终返回-EIO表示I/O错误
 */
static ssize_t ErrorFopRead(struct file *filep, char *buffer, size_t buflen)
{
    (void)filep;   // 未使用的参数
    (void)buffer;  // 未使用的参数
    (void)buflen;  // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   写入文件操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   buffer     缓冲区
 * @param   buflen     写入长度
 * @return  始终返回-EIO表示I/O错误
 */
static ssize_t ErrorFopWrite(struct file *filep, const char *buffer, size_t buflen)
{
    (void)filep;   // 未使用的参数
    (void)buffer;  // 未使用的参数
    (void)buflen;  // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   文件定位操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   offset     偏移量
 * @param   whence     定位基准
 * @return  始终返回-EIO表示I/O错误
 */
static off_t ErrorFopSeek(struct file *filep, off_t offset, int whence)
{
    (void)filep;   // 未使用的参数
    (void)offset;  // 未使用的参数
    (void)whence;  // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   IO控制操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   cmd        控制命令
 * @param   arg        命令参数
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopIoctl(struct file *filep, int cmd, unsigned long arg)
{
    (void)filep;   // 未使用的参数
    (void)cmd;     // 未使用的参数
    (void)arg;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   内存映射操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   region     内存映射区域
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopMmap(struct file* filep, struct VmMapRegion *region)
{
    (void)filep;   // 未使用的参数
    (void)region;  // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   轮询操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   fds        轮询表
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopPoll(struct file *filep, poll_table *fds)
{
    (void)filep;   // 未使用的参数
    (void)fds;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   获取文件状态操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   st         stat结构指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopStat(struct file *filep, struct stat* st)
{
    (void)filep;   // 未使用的参数
    (void)st;      // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   文件空间分配操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   mode       分配模式
 * @param   offset     偏移量
 * @param   len        长度
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopFallocate(struct file* filep, int mode, off_t offset, off_t len)
{
    (void)filep;   // 未使用的参数
    (void)mode;    // 未使用的参数
    (void)offset;  // 未使用的参数
    (void)len;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   64位文件空间分配操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   mode       分配模式
 * @param   offset     64位偏移量
 * @param   len        64位长度
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopFallocate64(struct file *filep, int mode, off64_t offset, off64_t len)
{
    (void)filep;   // 未使用的参数
    (void)mode;    // 未使用的参数
    (void)offset;  // 未使用的参数
    (void)len;     // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   文件同步操作错误处理函数
 * @param   filep      文件结构体指针
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopFsync(struct file *filep)
{
    (void)filep;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   读取页面操作错误处理函数
 * @param   filep      文件结构体指针
 * @param   buffer     缓冲区
 * @param   buflen     读取长度
 * @return  始终返回-EIO表示I/O错误
 */
static ssize_t ErrorFopReadpage(struct file *filep, char *buffer, size_t buflen)
{
    (void)filep;   // 未使用的参数
    (void)buffer;  // 未使用的参数
    (void)buflen;  // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   文件删除操作错误处理函数
 * @param   vnode      文件vnode
 * @return  始终返回-EIO表示I/O错误
 */
static int ErrorFopUnlink(struct Vnode *vnode)
{
    (void)vnode;   // 未使用的参数
    return -EIO;   // 返回I/O错误
}

/**
 * @brief   错误处理文件操作集
 * @details 所有操作均返回-EIO错误，用于文件系统强制卸载后禁用文件操作
 */
static struct file_operations_vfs g_errorFileOps = {
    .open = ErrorFopOpen,            // 打开文件操作
    .close = ErrorFopClose,          // 关闭文件操作
    .read = ErrorFopRead,            // 读取文件操作
    .write = ErrorFopWrite,          // 写入文件操作
    .seek = ErrorFopSeek,            // 文件定位操作
    .ioctl = ErrorFopIoctl,          // IO控制操作
    .mmap = ErrorFopMmap,            // 内存映射操作
    .poll = ErrorFopPoll,            // 轮询操作
    .stat = ErrorFopStat,            // 获取文件状态操作
    .fallocate = ErrorFopFallocate,  // 文件空间分配操作
    .fallocate64 = ErrorFopFallocate64,// 64位文件空间分配操作
    .fsync = ErrorFopFsync,          // 文件同步操作
    .readpage = ErrorFopReadpage,    // 读取页面操作
    .unlink = ErrorFopUnlink,        // 文件删除操作
};

/**
 * @brief   获取设备挂载点
 * @param   dev        设备vnode
 * @return  成功返回挂载点结构体指针，失败返回NULL
 */
static struct Mount* GetDevMountPoint(const struct Vnode *dev)
{
    struct Mount *mnt = NULL;  // 挂载点结构体指针
    LIST_HEAD *mntList = GetMountList();  // 获取挂载列表
    if (mntList == NULL)
    {
        return NULL;  // 挂载列表为空，返回NULL
    }

    // 遍历挂载列表查找匹配的设备
    LOS_DL_LIST_FOR_EACH_ENTRY(mnt, mntList, struct Mount, mountList)
    {
        if (mnt->vnodeDev == dev)  // 找到匹配的设备
        {
            return mnt;  // 返回挂载点结构体指针
        }
    }
    return NULL;  // 未找到匹配的设备，返回NULL
}

/**
 * @brief   目录预关闭处理
 * @param   dirp       目录项结构指针
 */
static void DirPreClose(struct fs_dirent_s *dirp)
{
    struct Vnode *node = NULL;  // vnode指针
    if (dirp == NULL || dirp->fd_root == NULL)  // 参数检查
    {
        return;  // 参数无效，直接返回
    }

    node = dirp->fd_root;  // 获取目录根vnode
    if (node->vop && node->vop->Closedir)  // 检查是否有Closedir操作
    {
        node->vop->Closedir(node, dirp);  // 调用Closedir操作
    }
}

/**
 * @brief   文件预关闭处理
 * @param   filep      文件结构体指针
 * @param   ops        文件操作集
 */
static void FilePreClose(struct file *filep, const struct file_operations_vfs *ops)
{
    if (filep->f_oflags & O_DIRECTORY)  // 判断是否为目录文件
    {
        DirPreClose(filep->f_dir);  // 调用目录预关闭处理
        return;
    }

    if (ops && ops->close)  // 检查是否有关闭操作
    {
        ops->close(filep);  // 调用关闭操作
    }
}

/**
 * @brief   文件禁用与清理
 * @param   mnt        挂载点结构体指针
 */
static void FileDisableAndClean(const struct Mount *mnt)
{
    struct filelist *flist = &tg_filelist;  // 文件列表
    struct file *filep = NULL;  // 文件结构体指针
    const struct file_operations_vfs *originOps = NULL;  // 原始文件操作集

    // 遍历所有文件描述符
    for (int i = 3; i < CONFIG_NFILE_DESCRIPTORS; i++)
    {
        if (!get_bit(i))  // 检查文件描述符是否使用
        {
            continue;  // 未使用，跳过
        }
        filep = &flist->fl_files[i];  // 获取文件结构体
        if (filep == NULL || filep->f_vnode == NULL)  // 参数检查
        {
            continue;  // 参数无效，跳过
        }
        if (filep->f_vnode->originMount != mnt)  // 检查是否属于当前挂载点
        {
            continue;  // 不属于，跳过
        }
        originOps = filep->ops;  // 保存原始文件操作集
        filep->ops = &g_errorFileOps;  // 设置错误处理文件操作集
        FilePreClose(filep, originOps);  // 执行文件预关闭处理
    }
}

/**
 * @brief   尝试释放vnode
 * @param   vnode      要释放的vnode
 */
static void VnodeTryFree(struct Vnode *vnode)
{
    if (vnode->useCount == 0)  // 检查引用计数是否为0
    {
        VnodeFree(vnode);  // 释放vnode
        return;
    }

    VnodePathCacheFree(vnode);  // 释放vnode路径缓存
    LOS_ListDelete(&(vnode->hashEntry));  // 从哈希表中删除
    LOS_ListDelete(&vnode->actFreeEntry);  // 从活动列表中删除

    if (vnode->vop->Reclaim)  // 检查是否有Reclaim操作
    {
        vnode->vop->Reclaim(vnode);  // 调用Reclaim操作
    }
    vnode->vop = &g_errorVnodeOps;  // 设置错误处理vnode操作集
    vnode->fop = &g_errorFileOps;  // 设置错误处理文件操作集
}

/**
 * @brief   尝试释放所有vnode
 * @param   mount      挂载点结构体指针
 */
static void VnodeTryFreeAll(const struct Mount *mount)
{
    struct Vnode *vnode = NULL;  // vnode指针
    struct Vnode *nextVnode = NULL;  // 下一个vnode指针

    // 遍历活动vnode列表
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(vnode, nextVnode, GetVnodeActiveList(), struct Vnode, actFreeEntry)
    {
        // 检查是否属于当前挂载点且不是新挂载点
        if ((vnode->originMount != mount) || (vnode->flag & VNODE_FLAG_MOUNT_NEW))
        {
            continue;  // 不符合条件，跳过
        }
        VnodeTryFree(vnode);  // 尝试释放vnode
    }
}

/**
 * @brief   强制卸载设备
 * @param   dev        设备vnode
 * @return  成功返回OK，失败返回错误码
 */
int ForceUmountDev(struct Vnode *dev)
{
    int ret;  // 返回值
    struct Vnode *origin = NULL;  // 原始vnode
    struct filelist *flist = &tg_filelist;  // 文件列表
    if (dev == NULL)  // 参数检查
    {
        return -EINVAL;  // 参数无效，返回错误
    }

    (void)sem_wait(&flist->fl_sem);  // 获取文件列表信号量
    VnodeHold();  // 持有vnode锁

    struct Mount *mnt = GetDevMountPoint(dev);  // 获取设备挂载点
    if (mnt == NULL)  // 检查挂载点是否存在
    {
        VnodeDrop();  // 释放vnode锁
        (void)sem_post(&flist->fl_sem);  // 释放文件列表信号量
        return -ENXIO;  // 挂载点不存在，返回错误
    }
    origin = mnt->vnodeBeCovered;  // 获取被覆盖的原始vnode

    FileDisableAndClean(mnt);  // 禁用并清理文件
    VnodeTryFreeAll(mnt);  // 尝试释放所有vnode
    ret = mnt->ops->Unmount(mnt, &dev);  // 调用文件系统卸载操作
    if (ret != OK)  // 检查卸载是否成功
    {
        PRINT_ERR("unmount in fs failed, ret = %d, errno = %d\n", ret, errno);  // 打印错误信息
    }

    LOS_ListDelete(&mnt->mountList);  // 从挂载列表中删除
    free(mnt);  // 释放挂载点内存
    origin->newMount = NULL;  // 清除新挂载点指针
    origin->flag &= ~(VNODE_FLAG_MOUNT_ORIGIN);  // 清除挂载点标志

    VnodeDrop();  // 释放vnode锁
    (void)sem_post(&flist->fl_sem);  // 释放文件列表信号量

    return OK;  // 返回成功
}