/*!
 * @file    vfs_jffs2.c
 * @brief
 * @link jffs2 http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-bundles-fs-support-jffs2.html @endlink
   @verbatim
	基本概念
		JFFS2是Journalling Flash File System Version 2（日志文件系统）的缩写，是针对MTD设备的日志型文件系统。

		OpenHarmony内核的JFFS2主要应用于NOR FLASH闪存，其特点是：可读写、支持数据压缩、提供了崩溃/掉电安全保护、
		提供“写平衡”支持等。闪存与磁盘介质有许多差异，直接将磁盘文件系统运行在闪存设备上，会导致性能和安全问题。
		为解决这一问题，需要实现一个特别针对闪存的文件系统，JFFS2就是这样一种文件系统。

	运行机制
		关于JFFS2文件系统的在存储设备上的实际物理布局，及文件系统本身的规格说明，请参考JFFS2的官方规格说明文档。
		https://sourceware.org/jffs2/
		这里仅列举几个对开发者和使用者会有一定影响的JFFS2的重要机制/特征：

		1. Mount机制及速度问题：按照JFFS2的设计，所有的文件会按照一定的规则，切分成大小不等的节点，
			依次存储到flash设备上。在mount流程中，需要获取到所有的这些节点信息并缓存到内存里。
			因此，mount速度和flash设备的大小和文件数量的多少成线性比例关系。这是JFFS2的原生设计问题，
			对于mount速度非常介意的用户，可以在内核编译时开启“Enable JFFS2 SUMMARY”选项，可以极大提升mount的速度。
			这个选项的原理是将mount需要的信息提前存储到flash上，在mount时读取并解析这块内容，使得mount的速度变得相对恒定。
			这个实际是空间换时间的做法，会消耗8%左右的额外空间。
		2. 写平衡的支持：由于flash设备的物理属性，读写都只能基于某个特定大小的“块”进行，为了防止某些特定的块磨损过于严重，
			在JFFS2中需要对写入的块进行“平衡”的管理，保证所有的块的写入次数都是相对平均的，进而保证flash设备的整体寿命。
		3. GC(garbage collection)机制：在JFFS2里发生删除动作，实际的物理空间并不会立即释放，而是由独立的GC线程来做
			空间整理和搬移等GC动作，和所有的GC机制一样，在JFFS2里的GC会对瞬时的读写性能有一定影响。另外，为了有空间能
			被用来做空间整理，JFFS2会对每个分区预留3块左右的空间，这个空间是用户不可见的。
		4. 压缩机制：当前使用的JFFS2，底层会自动的在每次读/写时进行解压/压缩动作，实际IO的大小和用户请求读写的大小
			并不会一样。特别在写入时，不能通过写入大小来和flash剩余空间的大小来预估写入一定会成功或者失败。
		5. 硬链接机制：JFFS2支持硬链接，底层实际占用的物理空间是一份，对于同一个文件的多个硬连接，并不会增加空间的占用；
			反之，只有当删除了所有的硬链接时，实际物理空间才会被释放。
	开发指导
		对于基于JFFS2和nor flash的开发，总体而言，与其他文件系统非常相似，因为都有VFS层来屏蔽了具体文件系统的差异，
		对外接口体现也都是标准的POSIX接口。

		对于整个裸nor flash设备而言，没有集中的地方来管理和记录分区的信息。因此，需要通过其他的配置方式来传递这部分信息
		（当前使用的方式是在烧写镜像的时候，使用bootargs参数配置的），然后在代码中调用相应的接口来添加分区，再进行挂载动作。	
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-22
 */
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
#include "fs/driver.h"
#include "vnode.h"
#include "mtd_list.h"
#include "mtd_partition.h"
#include "jffs2_hash.h"

#include "os-linux.h"
#include "jffs2/nodelist.h"

#ifdef LOSCFG_FS_JFFS

/* forward define */
struct VnodeOps g_jffs2Vops;///< jffs2 关于vnode操作接口实现
struct file_operations_vfs g_jffs2Fops;///< jffs2 关于vfs接口实现

static LosMux g_jffs2FsLock;  /* lock for all jffs2 ops | 操作 jffs2文件系统锁*/

static pthread_mutex_t g_jffs2NodeLock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
struct Vnode *g_jffs2PartList[CONFIG_MTD_PATTITION_NUM]; ///< jffs2 分区列表
/// 设置vnode节点的文件类型
static void Jffs2SetVtype(struct jffs2_inode *node, struct Vnode *pVnode)
{
    switch (node->i_mode & S_IFMT) {
        case S_IFREG:
            pVnode->type = VNODE_TYPE_REG; // 普通文件
            break;
        case S_IFDIR:
            pVnode->type = VNODE_TYPE_DIR; // 目录/文件夹
            break;
        case S_IFLNK:
            pVnode->type = VNODE_TYPE_LNK; // 软链接
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

/*!
 * @brief VfsJffs2Bind	挂载JFFS2分区
 @verbatim
	 运行命令：
	 	OHOS # mount /dev/spinorblk1 /jffs1 jffs2
	 将从串口得到如下回应信息，表明挂载成功。
		OHOS # mount /dev/spinorblk1 /jffs1 jffs2
		mount OK
	 挂载成功后，用户就能对norflash进行读写操作。	
 @endverbatim	
 * @param blkDriver	
 * @param data	
 * @param mnt	
 * @return	
 *
 * @see
 */
int VfsJffs2Bind(struct Mount *mnt, struct Vnode *blkDriver, const void *data)
{
    int ret;
    int partNo;
    mtd_partition *p = NULL;
    struct MtdDev *mtd = NULL;
    struct Vnode *pv = NULL;
    struct jffs2_inode *rootNode = NULL;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);
    p = (mtd_partition *)((struct drv_data *)blkDriver->data)->priv; //分区结构
    mtd = (struct MtdDev *)(p->mtd_info);//分区信息

    /* find a empty mte in partition table */
    if (mtd == NULL || mtd->type != MTD_NORFLASH) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return -EINVAL;
    }

    partNo = p->patitionnum;

    ret = jffs2_mount(partNo, &rootNode, mnt->mountFlags);
    if (ret != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    ret = VnodeAlloc(&g_jffs2Vops, &pv);
    if (ret != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        goto ERROR_WITH_VNODE;
    }
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

/*!
 * @brief VfsJffs2Unbind 卸载JFFS2分区	 
 @verbatim	
 	调用int umount(const char *target)函数卸载分区，只需要正确给出挂载点即可。
    运行命令：
	OHOS # umount /jffs1
	将从串口得到如下回应信息，表明卸载成功。
		OHOS # umount /jffs1
		umount ok
 @endverbatim			
 * @param blkDriver	
 * @param mnt	
 * @return	
 *
 * @see
 */
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

    parentNode = (struct jffs2_inode *)parentVnode->data;//获取私有数据
    node = jffs2_lookup(parentNode, (const unsigned char *)path, len);
    if (!node) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return -ENOENT;
    }

    (void)VfsHashGet(parentVnode->originMount, node->i_ino, &newVnode, NULL, NULL);
    if (newVnode) {
        if (newVnode->data == NULL) {
            LOS_Panic("#####VfsHashGet error#####\n");
         }
        newVnode->parent = parentVnode;
        *ppVnode = newVnode;
        LOS_MuxUnlock(&g_jffs2FsLock);
        return 0;
    }
    ret = VnodeAlloc(&g_jffs2Vops, &newVnode);
    if (ret != 0) {
        PRINT_ERR("%s-%d, ret: %x\n", __FUNCTION__, __LINE__, ret);
        (void)jffs2_iput(node);
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    Jffs2SetVtype(node, newVnode);
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
///创建一个jffs2 索引节点
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

ssize_t VfsJffs2ReadPage(struct Vnode *vnode, char *buffer, off_t off)
{
    struct jffs2_inode *node = NULL;
    struct jffs2_inode_info *f = NULL;
    struct jffs2_sb_info *c = NULL;
    int ret;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = (struct jffs2_inode *)vnode->data;
    f = JFFS2_INODE_INFO(node);
    c = JFFS2_SB_INFO(node->i_sb);

    off_t pos = min(node->i_size, off);
    ssize_t len = min(PAGE_SIZE, (node->i_size - pos));
    ret = jffs2_read_inode_range(c, f, (unsigned char *)buffer, off, len);
    if (ret) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }
    node->i_atime = Jffs2CurSec();

    LOS_MuxUnlock(&g_jffs2FsLock);

    return len;
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
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }
    node->i_atime = Jffs2CurSec();
    filep->f_pos += len;

    LOS_MuxUnlock(&g_jffs2FsLock);

    return len;
}

ssize_t VfsJffs2WritePage(struct Vnode *vnode, char *buffer, off_t pos, size_t buflen)
{
    struct jffs2_inode *node = NULL;
    struct jffs2_inode_info *f = NULL;
    struct jffs2_sb_info *c = NULL;
    struct jffs2_raw_inode ri = {0};
    struct IATTR attr = {0};
    int ret;
    uint32_t writtenLen;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    node = (struct jffs2_inode *)vnode->data;
    f = JFFS2_INODE_INFO(node);
    c = JFFS2_SB_INFO(node->i_sb);

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

    ret = jffs2_write_inode_range(c, f, &ri, (unsigned char *)buffer, pos, buflen, &writtenLen);
    if (ret) {
        node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);
        LOS_MuxUnlock(&g_jffs2FsLock);
        return ret;
    }

    node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);

    LOS_MuxUnlock(&g_jffs2FsLock);

    return (ssize_t)writtenLen;
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

#ifdef LOSCFG_KERNEL_SMP
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
        case SEEK_CUR:
            filePos += offset;
            break;

        case SEEK_SET:
            filePos = offset;
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
        LOS_MuxUnlock(&g_jffs2FsLock);
        VnodeFree(newVnode);
        return ret;
    }

    newVnode->type = VNODE_TYPE_DIR;
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

int VfsJffs2Rmdir(struct Vnode *parentVnode, struct Vnode *targetVnode, const char *path)
{
    int ret;
    struct jffs2_inode *parentInode = NULL;
    struct jffs2_inode *targetInode = NULL;

    if (!parentVnode || !targetVnode) {
        return -EINVAL;
    }

    parentInode = (struct jffs2_inode *)parentVnode->data;
    targetInode = (struct jffs2_inode *)targetVnode->data;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    ret = jffs2_rmdir(parentInode, targetInode, (const unsigned char *)path);

    if (ret == 0) {
        (void)jffs2_iput(targetInode);
    }

    LOS_MuxUnlock(&g_jffs2FsLock);
    return ret;
}

int VfsJffs2Link(struct Vnode *oldVnode, struct Vnode *newParentVnode, struct Vnode **newVnode, const char *newName)
{
    int ret;
    struct jffs2_inode *oldInode = oldVnode->data;
    struct jffs2_inode *newParentInode = newParentVnode->data;
    struct Vnode *pVnode = NULL;

    ret = VnodeAlloc(&g_jffs2Vops, &pVnode);
    if (ret != 0) {
        return -ENOMEM;
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);
    ret = jffs2_link(oldInode, newParentInode, (const unsigned char *)newName);
    if (ret != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        VnodeFree(pVnode);
        return ret;
    }

    pVnode->type = VNODE_TYPE_REG;
    pVnode->fop = &g_jffs2Fops;
    pVnode->parent = newParentVnode;
    pVnode->originMount = newParentVnode->originMount;
    pVnode->data = oldInode;
    pVnode->uid = oldVnode->uid;
    pVnode->gid = oldVnode->gid;
    pVnode->mode = oldVnode->mode;

    *newVnode = pVnode;
    (void)VfsHashInsert(*newVnode, oldInode->i_ino);

    LOS_MuxUnlock(&g_jffs2FsLock);
    return ret;
}

int VfsJffs2Symlink(struct Vnode *parentVnode, struct Vnode **newVnode, const char *path, const char *target)
{
    int ret;
    struct jffs2_inode *inode = NULL;
    struct Vnode *pVnode = NULL;

    ret = VnodeAlloc(&g_jffs2Vops, &pVnode);
    if (ret != 0) {
        return -ENOMEM;
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);
    ret = jffs2_symlink((struct jffs2_inode *)parentVnode->data, &inode, (const unsigned char *)path, target);
    if (ret != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        VnodeFree(pVnode);
        return ret;
    }

    pVnode->type = VNODE_TYPE_LNK;
    pVnode->fop = &g_jffs2Fops;
    pVnode->parent = parentVnode;
    pVnode->originMount = parentVnode->originMount;
    pVnode->data = inode;
    pVnode->uid = inode->i_uid;
    pVnode->gid = inode->i_gid;
    pVnode->mode = inode->i_mode;

    *newVnode = pVnode;
    (void)VfsHashInsert(*newVnode, inode->i_ino);

    LOS_MuxUnlock(&g_jffs2FsLock);
    return ret;
}

ssize_t VfsJffs2Readlink(struct Vnode *vnode, char *buffer, size_t bufLen)
{
    struct jffs2_inode *inode = NULL;
    struct jffs2_inode_info *f = NULL;
    ssize_t targetLen;
    ssize_t cnt;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    inode = (struct jffs2_inode *)vnode->data;
    f = JFFS2_INODE_INFO(inode);
    targetLen = strlen((const char *)f->target);
    if (bufLen == 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return 0;
    }

    cnt = (bufLen - 1) < targetLen ? (bufLen - 1) : targetLen;
    if (LOS_CopyFromKernel(buffer, bufLen, (const char *)f->target, cnt) != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);
        return -EFAULT;
    }
    buffer[cnt] = '\0';

    LOS_MuxUnlock(&g_jffs2FsLock);

    return cnt;
}

int VfsJffs2Unlink(struct Vnode *parentVnode, struct Vnode *targetVnode, const char *path)
{
    int ret;
    struct jffs2_inode *parentInode = NULL;
    struct jffs2_inode *targetInode = NULL;

    if (!parentVnode || !targetVnode) {
        PRINTK("%s-%d parentVnode=%x, targetVnode=%x\n", __FUNCTION__, __LINE__, parentVnode, targetVnode);
        return -EINVAL;
    }

    parentInode = (struct jffs2_inode *)parentVnode->data;
    targetInode = (struct jffs2_inode *)targetVnode->data;

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);

    ret = jffs2_unlink(parentInode, targetInode, (const unsigned char *)path);

    if (ret == 0) {
        (void)jffs2_iput(targetInode);
    }

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
        case S_IFLNK:
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

    /* Adapt to kstat member long tv_sec */
    buf->__st_atim32.tv_sec = (long)node->i_atime;
    buf->__st_mtim32.tv_sec = (long)node->i_mtime;
    buf->__st_ctim32.tv_sec = (long)node->i_ctime;

    LOS_MuxUnlock(&g_jffs2FsLock);

    return 0;
}

int VfsJffs2Reclaim(struct Vnode *pVnode)
{
    return 0;
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

const struct MountOps jffs_operations = {//jffs对mount接口实现
    .Mount = VfsJffs2Bind,		//挂载JFFS2分区
    .Unmount = VfsJffs2Unbind,	//卸载JFFS2分区
    .Statfs = VfsJffs2Statfs,
};
//jffs2 节点视角的操作实现
struct VnodeOps g_jffs2Vops = {
    .Lookup = VfsJffs2Lookup,
    .Create = VfsJffs2Create,
    .ReadPage = VfsJffs2ReadPage,
    .WritePage = VfsJffs2WritePage,
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
    .Link = VfsJffs2Link,
    .Symlink = VfsJffs2Symlink,
    .Readlink = VfsJffs2Readlink,
};
//jffs2 文件视角的操作实现
struct file_operations_vfs g_jffs2Fops = {
    .read = VfsJffs2Read,
    .write = VfsJffs2Write,
    .mmap = OsVfsFileMmap,
    .seek = VfsJffs2Seek,
    .close = VfsJffs2Close,
    .fsync = VfsJffs2Fsync,
};

//文件系统与内存的映射
FSMAP_ENTRY(jffs_fsmap, "jffs2", jffs_operations, TRUE, TRUE);

#endif
