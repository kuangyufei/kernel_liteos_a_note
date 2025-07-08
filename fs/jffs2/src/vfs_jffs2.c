/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd. All rights reserved.
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
 * @date    2025-07-08
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
// JFFS2文件系统的Vnode操作结构体
struct VnodeOps g_jffs2Vops;
// JFFS2文件系统的文件操作结构体
struct file_operations_vfs g_jffs2Fops;

// JFFS2文件系统操作互斥锁 (保护所有JFFS2操作的线程安全)
static LosMux g_jffs2FsLock;  /* lock for all jffs2 ops */

// JFFS2节点操作递归互斥锁 (支持递归加锁的节点操作保护)
static pthread_mutex_t g_jffs2NodeLock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
// JFFS2分区列表 (存储每个MTD分区对应的Vnode指针)
struct Vnode *g_jffs2PartList[CONFIG_MTD_PATTITION_NUM];

/**
 * @brief   根据inode模式设置Vnode类型
 * @param   node    [in]  JFFS2 inode结构体指针
 * @param   pVnode  [out] VFS Vnode结构体指针
 * @return  void
 * @details 根据inode的文件类型标志(S_IFMT)设置对应的Vnode类型
 */
static void Jffs2SetVtype(struct jffs2_inode *node, struct Vnode *pVnode)
{
    // 根据inode模式中的文件类型位判断文件类型
    switch (node->i_mode & S_IFMT) {
        case S_IFREG:                  // 普通文件
            pVnode->type = VNODE_TYPE_REG;  // 设置为普通文件Vnode类型
            break;
        case S_IFDIR:                  // 目录文件
            pVnode->type = VNODE_TYPE_DIR;  // 设置为目录Vnode类型
            break;
        case S_IFLNK:                  // 符号链接文件
            pVnode->type = VNODE_TYPE_LNK;  // 设置为链接Vnode类型
            break;
        default:                       // 未知类型
            pVnode->type = VNODE_TYPE_UNKNOWN;  // 设置为未知Vnode类型
            break;
    }
}

/**
 * @brief   获取当前系统时间(秒级)
 * @return  time_t 当前时间的秒数，获取失败返回0
 * @details 通过gettimeofday系统调用获取当前时间，并转换为秒级返回
 */
time_t Jffs2CurSec(void)
{
    struct timeval tv;                 // 时间结构体(秒+微秒)
    if (gettimeofday(&tv, NULL))       // 获取当前时间
        return 0;                      // 获取失败返回0
    return (uint32_t)(tv.tv_sec);      // 返回秒级时间
}

/**
 * @brief   加锁JFFS2节点互斥锁
 * @return  void
 * @details 对g_jffs2NodeLock递归互斥锁进行加锁操作
 */
void Jffs2NodeLock(void)
{
    (void)pthread_mutex_lock(&g_jffs2NodeLock);  // 加锁节点互斥锁
}

/**
 * @brief   解锁JFFS2节点互斥锁
 * @return  void
 * @details 对g_jffs2NodeLock递归互斥锁进行解锁操作
 */
void Jffs2NodeUnlock(void)
{
    (void)pthread_mutex_unlock(&g_jffs2NodeLock);  // 解锁节点互斥锁
}

/**
 * @brief   绑定并挂载JFFS2文件系统
 * @param   mnt        [in/out] 挂载点结构体指针
 * @param   blkDriver  [in]     块设备驱动Vnode指针
 * @param   data       [in]     挂载参数(未使用)
 * @return  int 成功返回0，失败返回错误码
 * @details 初始化JFFS2文件系统，创建根Vnode并关联挂载点
 */
int VfsJffs2Bind(struct Mount *mnt, struct Vnode *blkDriver, const void *data)
{
    int ret;                           // 函数返回值
    int partNo;                        // MTD分区号
    mtd_partition *p = NULL;           // MTD分区结构体指针
    struct MtdDev *mtd = NULL;         // MTD设备结构体指针
    struct Vnode *pv = NULL;           // Vnode指针(根节点)
    struct jffs2_inode *rootNode = NULL;  // JFFS2根inode指针

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁
    // 从块设备Vnode中获取MTD分区信息
    p = (mtd_partition *)((struct drv_data *)blkDriver->data)->priv;
    mtd = (struct MtdDev *)(p->mtd_info);  // 获取MTD设备信息

    /* 检查MTD设备是否有效且为NOR Flash类型 */
    if (mtd == NULL || mtd->type != MTD_NORFLASH) {
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return -EINVAL;                 // 返回无效参数错误
    }

    partNo = p->patitionnum;            // 获取分区号

    // 挂载JFFS2文件系统，获取根inode
    ret = jffs2_mount(partNo, &rootNode, mnt->mountFlags);
    if (ret != 0) {                     // 挂载失败
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return ret;                     // 返回挂载错误码
    }

    // 分配Vnode结构体
    ret = VnodeAlloc(&g_jffs2Vops, &pv);
    if (ret != 0) {                     // 分配失败
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        goto ERROR_WITH_VNODE;          // 跳转到错误处理
    }
    pv->type = VNODE_TYPE_DIR;          // 设置为目录类型
    pv->data = (void *)rootNode;        // 关联JFFS2根inode
    pv->originMount = mnt;              // 关联挂载点
    pv->fop = &g_jffs2Fops;             // 关联文件操作结构体
    mnt->data = p;                      // 挂载点存储分区信息
    mnt->vnodeCovered = pv;             // 挂载点关联根Vnode
    pv->uid = rootNode->i_uid;          // 设置用户ID
    pv->gid = rootNode->i_gid;          // 设置组ID
    pv->mode = rootNode->i_mode;        // 设置文件权限

    (void)VfsHashInsert(pv, rootNode->i_ino);  // 将Vnode插入VFS哈希表

    g_jffs2PartList[partNo] = blkDriver;  // 记录分区对应的块设备

    LOS_MuxUnlock(&g_jffs2FsLock);      // 释放文件系统锁

    return 0;                           // 挂载成功
ERROR_WITH_VNODE:
    return ret;                         // 返回错误码
}

/**
 * @brief   卸载JFFS2文件系统
 * @param   mnt        [in]  挂载点结构体指针
 * @param   blkDriver  [out] 块设备驱动Vnode指针的指针
 * @return  int 成功返回0，失败返回错误码
 * @details 执行JFFS2卸载操作，释放资源并返回块设备指针
 */
int VfsJffs2Unbind(struct Mount *mnt, struct Vnode **blkDriver)
{
    int ret;                           // 函数返回值
    mtd_partition *p = NULL;           // MTD分区结构体指针
    int partNo;                        // MTD分区号

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    p = (mtd_partition *)mnt->data;     // 从挂载点获取分区信息
    if (p == NULL) {                    // 分区信息无效
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return -EINVAL;                 // 返回无效参数错误
    }

    partNo = p->patitionnum;            // 获取分区号
    // 卸载JFFS2文件系统
    ret = jffs2_umount((struct jffs2_inode *)mnt->vnodeCovered->data);
    if (ret) {                          // 卸载失败
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return ret;                     // 返回卸载错误码
    }

    free(p->mountpoint_name);           // 释放挂载点名称内存
    p->mountpoint_name = NULL;          // 置空挂载点名称
    *blkDriver = g_jffs2PartList[partNo];  // 返回块设备指针

    LOS_MuxUnlock(&g_jffs2FsLock);      // 释放文件系统锁
    return 0;                           // 卸载成功
}

/**
 * @brief   在JFFS2文件系统中查找文件/目录
 * @param   parentVnode [in]  父目录Vnode指针
 * @param   path        [in]  要查找的路径名
 * @param   len         [in]  路径名长度
 * @param   ppVnode     [out] 查找到的Vnode指针的指针
 * @return  int 成功返回0，失败返回错误码
 * @details 在指定父目录下查找文件，若找到则创建或复用Vnode并返回
 */
int VfsJffs2Lookup(struct Vnode *parentVnode, const char *path, int len, struct Vnode **ppVnode)
{
    int ret;                           // 函数返回值
    struct Vnode *newVnode = NULL;     // 新Vnode指针
    struct jffs2_inode *node = NULL;   // JFFS2 inode指针
    struct jffs2_inode *parentNode = NULL;  // 父目录inode指针

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    parentNode = (struct jffs2_inode *)parentVnode->data;  // 获取父目录inode
    // 在父目录中查找指定路径的文件
    node = jffs2_lookup(parentNode, (const unsigned char *)path, len);
    if (!node) {                        // 未找到文件
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return -ENOENT;                 // 返回文件不存在错误
    }

    // 尝试从VFS哈希表中查找已存在的Vnode
    (void)VfsHashGet(parentVnode->originMount, node->i_ino, &newVnode, NULL, NULL);
    if (newVnode) {                     // 找到已存在的Vnode
        if (newVnode->data == NULL) {   // 检查Vnode数据是否有效
            LOS_Panic("#####VfsHashGet error#####\n");  // 触发断言
        }
        newVnode->parent = parentVnode;  // 设置父Vnode
        *ppVnode = newVnode;            // 返回找到的Vnode
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return 0;                       // 查找成功
    }
    // 分配新的Vnode
    ret = VnodeAlloc(&g_jffs2Vops, &newVnode);
    if (ret != 0) {                     // 分配失败
        PRINT_ERR("%s-%d, ret: %x\n", __FUNCTION__, __LINE__, ret);  // 打印错误
        (void)jffs2_iput(node);         // 减少inode引用计数
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return ret;                     // 返回错误码
    }

    Jffs2SetVtype(node, newVnode);      // 设置Vnode类型
    newVnode->fop = parentVnode->fop;   // 关联文件操作结构体
    newVnode->data = node;              // 关联JFFS2 inode
    newVnode->parent = parentVnode;     // 设置父Vnode
    newVnode->originMount = parentVnode->originMount;  // 关联挂载点
    newVnode->uid = node->i_uid;        // 设置用户ID
    newVnode->gid = node->i_gid;        // 设置组ID
    newVnode->mode = node->i_mode;      // 设置文件权限

    (void)VfsHashInsert(newVnode, node->i_ino);  // 将新Vnode插入哈希表

    *ppVnode = newVnode;                // 返回新创建的Vnode

    LOS_MuxUnlock(&g_jffs2FsLock);      // 释放文件系统锁
    return 0;                           // 查找成功
}

/**
 * @brief   在JFFS2文件系统中创建文件
 * @param   parentVnode [in]  父目录Vnode指针
 * @param   path        [in]  要创建的文件路径
 * @param   mode        [in]  文件创建模式
 * @param   ppVnode     [out] 新创建文件的Vnode指针的指针
 * @return  int 成功返回0，失败返回错误码
 * @details 在指定父目录下创建新文件，分配并初始化Vnode和JFFS2 inode
 */
int VfsJffs2Create(struct Vnode *parentVnode, const char *path, int mode, struct Vnode **ppVnode)
{
    int ret;                           // 函数返回值
    struct jffs2_inode *newNode = NULL;  // 新JFFS2 inode指针
    struct Vnode *newVnode = NULL;     // 新Vnode指针

    // 分配新的Vnode
    ret = VnodeAlloc(&g_jffs2Vops, &newVnode);
    if (ret != 0) {                     // 分配失败
        return -ENOMEM;                 // 返回内存不足错误
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁
    // 在JFFS2中创建新文件
    ret = jffs2_create((struct jffs2_inode *)parentVnode->data, (const unsigned char *)path, mode, &newNode);
    if (ret != 0) {                     // 创建失败
        VnodeFree(newVnode);            // 释放已分配的Vnode
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return ret;                     // 返回创建错误码
    }

    newVnode->type = VNODE_TYPE_REG;    // 设置为普通文件类型
    newVnode->fop = parentVnode->fop;   // 关联文件操作结构体
    newVnode->data = newNode;           // 关联JFFS2 inode
    newVnode->parent = parentVnode;     // 设置父Vnode
    newVnode->originMount = parentVnode->originMount;  // 关联挂载点
    newVnode->uid = newNode->i_uid;     // 设置用户ID
    newVnode->gid = newNode->i_gid;     // 设置组ID
    newVnode->mode = newNode->i_mode;   // 设置文件权限

    (void)VfsHashInsert(newVnode, newNode->i_ino);  // 将新Vnode插入哈希表

    *ppVnode = newVnode;                // 返回新创建的Vnode

    LOS_MuxUnlock(&g_jffs2FsLock);      // 释放文件系统锁
    return 0;                           // 创建成功
}

/**
 * @brief   关闭JFFS2文件
 * @param   filep [in] 文件结构体指针
 * @return  int 成功返回0
 * @details JFFS2文件关闭操作(空实现，实际释放由VFS层处理)
 */
int VfsJffs2Close(struct file *filep)
{
    return 0;                           // 关闭成功
}

/**
 * @brief   读取JFFS2文件的一页数据
 * @param   vnode  [in]  文件Vnode指针
 * @param   buffer [out] 数据缓冲区指针
 * @param   off    [in]  读取偏移量
 * @return  ssize_t 成功返回读取字节数，失败返回错误码
 * @details 从指定偏移量读取一页大小的数据(通常为4KB)
 */
ssize_t VfsJffs2ReadPage(struct Vnode *vnode, char *buffer, off_t off)
{
    struct jffs2_inode *node = NULL;   // JFFS2 inode指针
    struct jffs2_inode_info *f = NULL;  // JFFS2 inode信息指针
    struct jffs2_sb_info *c = NULL;     // JFFS2超级块信息指针
    int ret;                           // 函数返回值

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    node = (struct jffs2_inode *)vnode->data;  // 获取JFFS2 inode
    f = JFFS2_INODE_INFO(node);         // 获取inode信息
    c = JFFS2_SB_INFO(node->i_sb);      // 获取超级块信息

    off_t pos = min(node->i_size, off);  // 计算实际读取起始位置(不超过文件大小)
    ssize_t len = min(PAGE_SIZE, (node->i_size - pos));  // 计算读取长度(不超过一页)
    // 读取inode数据
    ret = jffs2_read_inode_range(c, f, (unsigned char *)buffer, off, len);
    if (ret) {                          // 读取失败
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return ret;                     // 返回错误码
    }
    node->i_atime = Jffs2CurSec();      // 更新访问时间

    LOS_MuxUnlock(&g_jffs2FsLock);      // 释放文件系统锁

    return len;                         // 返回读取字节数
}

/**
 * @brief   读取JFFS2文件内容
 * @param   filep  [in]  文件结构体指针
 * @param   buffer [out] 数据缓冲区指针
 * @param   bufLen [in]  要读取的字节数
 * @return  ssize_t 成功返回读取字节数，失败返回错误码
 * @details 从当前文件偏移量读取指定长度的数据，并更新文件指针
 */
ssize_t VfsJffs2Read(struct file *filep, char *buffer, size_t bufLen)
{
    struct jffs2_inode *node = NULL;   // JFFS2 inode指针
    struct jffs2_inode_info *f = NULL;  // JFFS2 inode信息指针
    struct jffs2_sb_info *c = NULL;     // JFFS2超级块信息指针
    int ret;                           // 函数返回值

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁
    node = (struct jffs2_inode *)filep->f_vnode->data;  // 获取JFFS2 inode
    f = JFFS2_INODE_INFO(node);         // 获取inode信息
    c = JFFS2_SB_INFO(node->i_sb);      // 获取超级块信息

    off_t pos = min(node->i_size, filep->f_pos);  // 计算实际读取起始位置
    off_t len = min(bufLen, (node->i_size - pos));  // 计算实际可读取长度
    // 读取inode数据
    ret = jffs2_read_inode_range(c, f, (unsigned char *)buffer, filep->f_pos, len);
    if (ret) {                          // 读取失败
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return ret;                     // 返回错误码
    }
    node->i_atime = Jffs2CurSec();      // 更新访问时间
    filep->f_pos += len;                // 更新文件指针

    LOS_MuxUnlock(&g_jffs2FsLock);      // 释放文件系统锁

    return len;                         // 返回读取字节数
}

/**
 * @brief   写入一页数据到JFFS2文件
 * @param   vnode   [in]  文件Vnode指针
 * @param   buffer  [in]  数据缓冲区指针
 * @param   pos     [in]  写入偏移量
 * @param   buflen  [in]  要写入的字节数
 * @return  ssize_t 成功返回写入字节数，失败返回错误码
 * @details 将数据写入指定偏移量，若偏移量超过文件大小则扩展文件
 */
ssize_t VfsJffs2WritePage(struct Vnode *vnode, char *buffer, off_t pos, size_t buflen)
{
    struct jffs2_inode *node = NULL;   // JFFS2 inode指针
    struct jffs2_inode_info *f = NULL;  // JFFS2 inode信息指针
    struct jffs2_sb_info *c = NULL;     // JFFS2超级块信息指针
    struct jffs2_raw_inode ri = {0};    // JFFS2原始inode结构体
    struct IATTR attr = {0};            // inode属性结构体
    int ret;                           // 函数返回值
    uint32_t writtenLen;                // 实际写入长度

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    node = (struct jffs2_inode *)vnode->data;  // 获取JFFS2 inode
    f = JFFS2_INODE_INFO(node);         // 获取inode信息
    c = JFFS2_SB_INFO(node->i_sb);      // 获取超级块信息

    if (pos < 0) {                      // 检查偏移量是否有效
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return -EINVAL;                 // 返回无效参数错误
    }

    // 初始化原始inode结构体
    ri.ino = cpu_to_je32(f->inocache->ino);  // 设置inode号(小端转大端)
    ri.mode = cpu_to_jemode(node->i_mode);   // 设置文件模式
    ri.uid = cpu_to_je16(node->i_uid);       // 设置用户ID(小端转大端)
    ri.gid = cpu_to_je16(node->i_gid);       // 设置组ID(小端转大端)
    // 设置访问/创建/修改时间(小端转大端)
    ri.atime = ri.ctime = ri.mtime = cpu_to_je32(Jffs2CurSec());

    if (pos > node->i_size) {           // 若写入偏移量超过文件大小
        int err;
        attr.attr_chg_valid = CHG_SIZE;  // 标记需要修改大小
        attr.attr_chg_size = pos;        // 设置新大小
        err = jffs2_setattr(node, &attr);  // 更新inode属性
        if (err) {                       // 更新失败
            LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
            return err;                  // 返回错误码
        }
    }
    ri.isize = cpu_to_je32(node->i_size);  // 设置inode大小(小端转大端)

    // 写入inode数据
    ret = jffs2_write_inode_range(c, f, &ri, (unsigned char *)buffer, pos, buflen, &writtenLen);
    if (ret) {                          // 写入失败
        node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);  // 更新时间
        LOS_MuxUnlock(&g_jffs2FsLock);  // 释放文件系统锁
        return ret;                     // 返回错误码
    }

    node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);  // 更新修改和创建时间

    LOS_MuxUnlock(&g_jffs2FsLock);      // 释放文件系统锁

    return (ssize_t)writtenLen;         // 返回实际写入字节数
}
/**
 * @brief   JFFS2文件写入操作实现
 * @param   filep   文件描述符指针
 * @param   buffer  待写入数据缓冲区
 * @param   bufLen  待写入数据长度
 * @return  成功返回实际写入字节数，失败返回负错误码
 */
ssize_t VfsJffs2Write(struct file *filep, const char *buffer, size_t bufLen)
{
    struct jffs2_inode *node = NULL;          // JFFS2 inode结构体指针
    struct jffs2_inode_info *f = NULL;        // JFFS2 inode信息结构体指针
    struct jffs2_sb_info *c = NULL;           // JFFS2超级块信息结构体指针
    struct jffs2_raw_inode ri = {0};          // 原始inode结构体，用于存储inode元数据
    struct IATTR attr = {0};                  // 文件属性结构体，用于修改文件大小
    int ret;                                  // 函数返回值
    off_t pos;                                // 文件当前写入位置
    uint32_t writtenLen;                      // 实际写入长度

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统全局互斥锁，确保线程安全

    node = (struct jffs2_inode *)filep->f_vnode->data;  // 从vnode中获取JFFS2 inode
    f = JFFS2_INODE_INFO(node);                         // 获取inode信息
    c = JFFS2_SB_INFO(node->i_sb);                      // 获取超级块信息
    pos = filep->f_pos;                                 // 获取当前文件指针位置

#ifdef LOSCFG_KERNEL_SMP  // SMP系统条件编译块
    struct super_block *sb = node->i_sb;                // 获取超级块
    UINT16 gcCpuMask = LOS_TaskCpuAffiGet(sb->s_gc_thread);  // 获取垃圾回收线程的CPU亲和性掩码
    UINT32 curTaskId = LOS_CurTaskIDGet();              // 获取当前任务ID
    UINT16 curCpuMask = LOS_TaskCpuAffiGet(curTaskId);  // 获取当前任务的CPU亲和性掩码
    if (curCpuMask != gcCpuMask) {                      // 若当前任务与GC线程CPU亲和性不同
        if (curCpuMask != LOSCFG_KERNEL_CPU_MASK) {      // 当前任务不是全CPU掩码
            (void)LOS_TaskCpuAffiSet(sb->s_gc_thread, curCpuMask);  // 将GC线程亲和性设置为当前任务的CPU
        } else {
            (void)LOS_TaskCpuAffiSet(curTaskId, gcCpuMask);  // 将当前任务亲和性设置为GC线程的CPU
        }
    }
#endif
    if (pos < 0) {                               // 检查写入位置是否合法
        LOS_MuxUnlock(&g_jffs2FsLock);           // 释放文件系统锁
        return -EINVAL;                          // 返回无效参数错误
    }

    ri.ino = cpu_to_je32(f->inocache->ino);      // 设置inode号（CPU字节序转JFFS2字节序）
    ri.mode = cpu_to_jemode(node->i_mode);       // 设置文件模式（权限位）
    ri.uid = cpu_to_je16(node->i_uid);           // 设置用户ID
    ri.gid = cpu_to_je16(node->i_gid);           // 设置组ID
    ri.atime = ri.ctime = ri.mtime = cpu_to_je32(Jffs2CurSec());  // 设置访问/创建/修改时间为当前时间

    if (pos > node->i_size) {                    // 若写入位置超过文件当前大小（需要扩展文件）
        int err;
        attr.attr_chg_valid = CHG_SIZE;         // 标记需要修改文件大小
        attr.attr_chg_size = pos;               // 设置新文件大小为当前写入位置
        err = jffs2_setattr(node, &attr);       // 调用JFFS2设置属性函数扩展文件
        if (err) {                              // 扩展失败
            LOS_MuxUnlock(&g_jffs2FsLock);       // 释放锁
            return err;                         // 返回错误码
        }
    }
    ri.isize = cpu_to_je32(node->i_size);        // 设置inode大小

    // 调用JFFS2核心函数写入数据
    ret = jffs2_write_inode_range(c, f, &ri, (unsigned char *)buffer, pos, bufLen, &writtenLen);
    if (ret) {                                   // 写入部分成功（可能空间不足）
        pos += writtenLen;                       // 更新写入位置

        node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);  // 更新文件修改/创建时间
        if (pos > node->i_size)
            node->i_size = pos;                  // 更新文件大小

        filep->f_pos = pos;                      // 更新文件指针

        LOS_MuxUnlock(&g_jffs2FsLock);           // 释放锁

        return ret;                              // 返回错误码（非致命错误）
    }

    if (writtenLen != bufLen) {                  // 写入长度与请求长度不匹配（空间不足）
        pos += writtenLen;                       // 更新写入位置

        node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);  // 更新时间戳
        if (pos > node->i_size)
            node->i_size = pos;                  // 更新文件大小

        filep->f_pos = pos;                      // 更新文件指针

        LOS_MuxUnlock(&g_jffs2FsLock);           // 释放锁

        return -ENOSPC;                          // 返回空间不足错误
    }

    pos += bufLen;                               // 更新写入位置

    node->i_mtime = node->i_ctime = je32_to_cpu(ri.mtime);  // 更新时间戳
    if (pos > node->i_size)
        node->i_size = pos;                      // 更新文件大小

    filep->f_pos = pos;                          // 更新文件指针

    LOS_MuxUnlock(&g_jffs2FsLock);               // 释放文件系统锁

    return writtenLen;                           // 返回成功写入的字节数
}

/**
 * @brief   JFFS2文件定位操作实现
 * @param   filep   文件描述符指针
 * @param   offset  偏移量
 * @param   whence  定位基准（SEEK_CUR/SEEK_SET/SEEK_END）
 * @return  成功返回新的文件位置，失败返回负错误码
 */
off_t VfsJffs2Seek(struct file *filep, off_t offset, int whence)
{
    struct jffs2_inode *node = NULL;          // JFFS2 inode结构体指针
    loff_t filePos;                           // 计算后的文件位置

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    node = (struct jffs2_inode *)filep->f_vnode->data;  // 获取inode
    filePos = filep->f_pos;                             // 获取当前文件位置

    switch (whence) {                         // 根据定位基准计算新位置
        case SEEK_CUR:                        // 从当前位置开始
            filePos += offset;
            break;

        case SEEK_SET:                        // 从文件开头开始
            filePos = offset;
            break;

        case SEEK_END:                        // 从文件末尾开始
            filePos = node->i_size + offset;
            break;

        default:                              // 无效的定位基准
            LOS_MuxUnlock(&g_jffs2FsLock);   // 释放锁
            return -EINVAL;                  // 返回错误
    }

    LOS_MuxUnlock(&g_jffs2FsLock);           // 释放锁

    if (filePos < 0)                         // 检查位置是否合法
        return -EINVAL;

    return filePos;                          // 返回新的文件位置
}

/**
 * @brief   JFFS2 Ioctl操作（未实现）
 * @param   filep   文件描述符指针
 * @param   cmd     控制命令
 * @param   arg     命令参数
 * @return  始终返回-ENOSYS（不支持该操作）
 */
int VfsJffs2Ioctl(struct file *filep, int cmd, unsigned long arg)
{
    PRINT_DEBUG("%s NOT SUPPORT\n", __FUNCTION__);  // 打印调试信息：不支持该操作
    return -ENOSYS;                                 // 返回不支持错误
}

/**
 * @brief   JFFS2文件同步操作
 * @param   filep   文件描述符指针
 * @return  始终返回0（JFFS2直接写入Flash，无需额外同步）
 * @note    注：启用页缓存后需添加页刷新逻辑
 */
int VfsJffs2Fsync(struct file *filep)
{
    /* jffs2_write directly write to flash, sync is OK.
        BUT after pagecache enabled, pages need to be flushed to flash */
    return 0;  // JFFS2直接写入Flash，同步操作始终成功
}

/**
 * @brief   JFFS2文件描述符复制（未实现）
 * @param   oldFile 源文件描述符
 * @param   newFile 目标文件描述符
 * @return  始终返回-ENOSYS（不支持该操作）
 */
int VfsJffs2Dup(const struct file *oldFile, struct file *newFile)
{
    PRINT_DEBUG("%s NOT SUPPORT\n", __FUNCTION__);  // 打印调试信息：不支持该操作
    return -ENOSYS;                                 // 返回不支持错误
}

/**
 * @brief   JFFS2目录打开操作
 * @param   pVnode  目录vnode指针
 * @param   dir     目录项结构体指针
 * @return  始终返回0（成功）
 */
int VfsJffs2Opendir(struct Vnode *pVnode, struct fs_dirent_s *dir)
{
    dir->fd_int_offset = 0;  // 初始化目录内部偏移量为0
    return 0;
}

/**
 * @brief   JFFS2目录读取操作
 * @param   pVnode  目录vnode指针
 * @param   dir     目录项结构体指针（用于存储读取结果）
 * @return  成功返回读取到的目录项数量，失败返回负错误码
 */
int VfsJffs2Readdir(struct Vnode *pVnode, struct fs_dirent_s *dir)
{
    int ret;                                  // 函数返回值
    int i = 0;                                // 目录项计数

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    /* set jffs2_d */
    while (i < dir->read_cnt) {               // 循环读取，直到达到请求的目录项数量
        // 调用JFFS2目录读取函数
        ret = jffs2_readdir((struct jffs2_inode *)pVnode->data, &dir->fd_position, &dir->fd_int_offset, &dir->fd_dir[i]);
        if (ret) {                            // 读取失败或到达目录末尾
            break;
        }

        i++;                                  // 增加目录项计数
    }

    LOS_MuxUnlock(&g_jffs2FsLock);           // 释放锁

    return i;                                 // 返回实际读取到的目录项数量
}

/**
 * @brief   JFFS2目录定位操作（空实现）
 * @param   pVnode  目录vnode指针
 * @param   dir     目录项结构体指针
 * @param   offset  偏移量
 * @return  始终返回0
 */
int VfsJffs2Seekdir(struct Vnode *pVnode, struct fs_dirent_s *dir, unsigned long offset)
{
    return 0;  // 未实现具体定位逻辑，直接返回成功
}

/**
 * @brief   JFFS2目录重绕操作
 * @param   pVnode  目录vnode指针
 * @param   dir     目录项结构体指针
 * @return  始终返回0
 */
int VfsJffs2Rewinddir(struct Vnode *pVnode, struct fs_dirent_s *dir)
{
    dir->fd_int_offset = 0;  // 重置目录内部偏移量为0（回到目录开头）

    return 0;
}

/**
 * @brief   JFFS2目录关闭操作（空实现）
 * @param   node    目录vnode指针
 * @param   dir     目录项结构体指针
 * @return  始终返回0
 */
int VfsJffs2Closedir(struct Vnode *node, struct fs_dirent_s *dir)
{
    return 0;  // 无需特殊处理，直接返回成功
}

/**
 * @brief   JFFS2创建目录操作
 * @param   parentNode 父目录vnode指针
 * @param   dirName    目录名称
 * @param   mode       目录权限模式
 * @param   ppVnode    输出参数，用于返回新目录vnode
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Mkdir(struct Vnode *parentNode, const char *dirName, mode_t mode, struct Vnode **ppVnode)
{
    int ret;                                  // 函数返回值
    struct jffs2_inode *node = NULL;          // JFFS2 inode结构体指针
    struct Vnode *newVnode = NULL;            // 新目录的vnode

    ret = VnodeAlloc(&g_jffs2Vops, &newVnode);  // 分配新的vnode
    if (ret != 0) {                            // 分配失败
        return -ENOMEM;                        // 返回内存不足错误
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    // 调用JFFS2创建目录函数
    ret = jffs2_mkdir((struct jffs2_inode *)parentNode->data, (const unsigned char *)dirName, mode, &node);
    if (ret != 0) {                            // 创建失败
        LOS_MuxUnlock(&g_jffs2FsLock);         // 释放锁
        VnodeFree(newVnode);                   // 释放已分配的vnode
        return ret;                            // 返回错误码
    }

    newVnode->type = VNODE_TYPE_DIR;           // 设置vnode类型为目录
    newVnode->fop = parentNode->fop;           // 继承父目录的文件操作集
    newVnode->data = node;                     // 关联JFFS2 inode
    newVnode->parent = parentNode;             // 设置父vnode
    newVnode->originMount = parentNode->originMount;  // 设置挂载点
    newVnode->uid = node->i_uid;               // 设置用户ID
    newVnode->gid = node->i_gid;               // 设置组ID
    newVnode->mode = node->i_mode;             // 设置权限模式

    *ppVnode = newVnode;                       // 输出新vnode

    (void)VfsHashInsert(newVnode, node->i_ino);  // 将新vnode插入VFS哈希表

    LOS_MuxUnlock(&g_jffs2FsLock);             // 释放锁

    return 0;                                  // 返回成功
}

/**
 * @brief   JFFS2文件截断操作核心实现（静态辅助函数）
 * @param   pVnode  文件/目录的Vnode指针
 * @param   len     截断后的目标长度
 * @return  成功返回0，失败返回负错误码
 */
static int Jffs2Truncate(struct Vnode *pVnode, unsigned int len)
{
    int ret;                                  // 函数返回值
    struct IATTR attr = {0};                  // 文件属性结构体，用于设置文件大小

    attr.attr_chg_size = len;                 // 设置目标大小
    attr.attr_chg_valid = CHG_SIZE;           // 标记需要修改文件大小属性

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁
    ret = jffs2_setattr((struct jffs2_inode *)pVnode->data, &attr);  // 调用JFFS2设置属性函数执行截断
    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放文件系统锁
    return ret;                               // 返回操作结果
}

/**
 * @brief   VFS接口：文件截断操作（32位偏移量）
 * @param   pVnode  文件/目录的Vnode指针
 * @param   len     截断后的目标长度（off_t类型）
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Truncate(struct Vnode *pVnode, off_t len)
{
    // 调用静态辅助函数执行截断，将off_t类型长度转换为unsigned int
    int ret = Jffs2Truncate(pVnode, (unsigned int)len);
    return ret;                               // 返回操作结果
}

/**
 * @brief   VFS接口：文件截断操作（64位偏移量）
 * @param   pVnode  文件/目录的Vnode指针
 * @param   len     截断后的目标长度（off64_t类型）
 * @return  成功返回0，失败返回负错误码
 * @note    实际实现中仍使用32位截断逻辑，可能存在大文件截断风险
 */
int VfsJffs2Truncate64(struct Vnode *pVnode, off64_t len)
{
    // 调用静态辅助函数执行截断，将off64_t类型长度转换为unsigned int
    int ret = Jffs2Truncate(pVnode, (unsigned int)len);
    return ret;                               // 返回操作结果
}

/**
 * @brief   VFS接口：修改文件属性
 * @param   pVnode  文件/目录的Vnode指针
 * @param   attr    待设置的属性结构体指针
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Chattr(struct Vnode *pVnode, struct IATTR *attr)
{
    int ret;                                  // 函数返回值
    struct jffs2_inode *node = NULL;          // JFFS2 inode结构体指针

    if (pVnode == NULL) {                     // 参数合法性检查：Vnode不能为空
        return -EINVAL;                       // 返回无效参数错误
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    node = pVnode->data;                      // 获取Vnode关联的JFFS2 inode
    ret = jffs2_setattr(node, attr);          // 调用JFFS2设置属性函数
    if (ret == 0) {                           // 属性设置成功
        pVnode->uid = node->i_uid;            // 更新Vnode的用户ID
        pVnode->gid = node->i_gid;            // 更新Vnode的组ID
        pVnode->mode = node->i_mode;          // 更新Vnode的权限模式
    }
    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放文件系统锁
    return ret;                               // 返回操作结果
}

/**
 * @brief   VFS接口：删除目录
 * @param   parentVnode 父目录Vnode指针
 * @param   targetVnode 目标目录Vnode指针
 * @param   path        目标目录路径名
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Rmdir(struct Vnode *parentVnode, struct Vnode *targetVnode, const char *path)
{
    int ret;                                  // 函数返回值
    struct jffs2_inode *parentInode = NULL;   // 父目录inode指针
    struct jffs2_inode *targetInode = NULL;   // 目标目录inode指针

    if (!parentVnode || !targetVnode) {       // 参数合法性检查：父目录和目标目录Vnode不能为空
        return -EINVAL;                       // 返回无效参数错误
    }

    parentInode = (struct jffs2_inode *)parentVnode->data;  // 获取父目录inode
    targetInode = (struct jffs2_inode *)targetVnode->data;  // 获取目标目录inode

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    // 调用JFFS2删除目录函数
    ret = jffs2_rmdir(parentInode, targetInode, (const unsigned char *)path);
    if (ret == 0) {                           // 删除成功
        (void)jffs2_iput(targetInode);        // 减少目标inode引用计数（释放inode）
    }

    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放文件系统锁
    return ret;                               // 返回操作结果
}

/**
 * @brief   VFS接口：创建硬链接
 * @param   oldVnode     源文件Vnode指针
 * @param   newParentVnode 新链接的父目录Vnode指针
 * @param   newVnode     输出参数，新链接的Vnode指针
 * @param   newName      新链接的名称
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Link(struct Vnode *oldVnode, struct Vnode *newParentVnode, struct Vnode **newVnode, const char *newName)
{
    int ret;                                  // 函数返回值
    struct jffs2_inode *oldInode = oldVnode->data;  // 源文件inode
    struct jffs2_inode *newParentInode = newParentVnode->data;  // 新父目录inode
    struct Vnode *pVnode = NULL;              // 临时Vnode指针

    ret = VnodeAlloc(&g_jffs2Vops, &pVnode);  // 分配新的Vnode
    if (ret != 0) {                           // 分配失败
        return -ENOMEM;                       // 返回内存不足错误
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁
    // 调用JFFS2创建硬链接函数
    ret = jffs2_link(oldInode, newParentInode, (const unsigned char *)newName);
    if (ret != 0) {                           // 创建链接失败
        LOS_MuxUnlock(&g_jffs2FsLock);        // 释放锁
        VnodeFree(pVnode);                    // 释放已分配的Vnode
        return ret;                           // 返回错误码
    }

    pVnode->type = VNODE_TYPE_REG;            // 设置Vnode类型为普通文件
    pVnode->fop = &g_jffs2Fops;               // 关联JFFS2文件操作集
    pVnode->parent = newParentVnode;          // 设置父Vnode
    pVnode->originMount = newParentVnode->originMount;  // 设置挂载点
    pVnode->data = oldInode;                  // 关联源文件inode（硬链接共享inode）
    pVnode->uid = oldVnode->uid;              // 继承源文件UID
    pVnode->gid = oldVnode->gid;              // 继承源文件GID
    pVnode->mode = oldVnode->mode;            // 继承源文件权限

    *newVnode = pVnode;                       // 输出新Vnode
    (void)VfsHashInsert(*newVnode, oldInode->i_ino);  // 将新Vnode插入VFS哈希表

    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放锁
    return ret;                               // 返回成功
}

/**
 * @brief   VFS接口：创建符号链接
 * @param   parentVnode  父目录Vnode指针
 * @param   newVnode     输出参数，新符号链接的Vnode指针
 * @param   path         符号链接名称
 * @param   target       符号链接指向的目标路径
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Symlink(struct Vnode *parentVnode, struct Vnode **newVnode, const char *path, const char *target)
{
    int ret;                                  // 函数返回值
    struct jffs2_inode *inode = NULL;         // 新符号链接的inode指针
    struct Vnode *pVnode = NULL;              // 临时Vnode指针

    ret = VnodeAlloc(&g_jffs2Vops, &pVnode);  // 分配新的Vnode
    if (ret != 0) {                           // 分配失败
        return -ENOMEM;                       // 返回内存不足错误
    }

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁
    // 调用JFFS2创建符号链接函数
    ret = jffs2_symlink((struct jffs2_inode *)parentVnode->data, &inode, (const unsigned char *)path, target);
    if (ret != 0) {                           // 创建失败
        LOS_MuxUnlock(&g_jffs2FsLock);        // 释放锁
        VnodeFree(pVnode);                    // 释放Vnode
        return ret;                           // 返回错误码
    }

    pVnode->type = VNODE_TYPE_LNK;            // 设置Vnode类型为符号链接
    pVnode->fop = &g_jffs2Fops;               // 关联JFFS2文件操作集
    pVnode->parent = parentVnode;             // 设置父Vnode
    pVnode->originMount = parentVnode->originMount;  // 设置挂载点
    pVnode->data = inode;                     // 关联新创建的inode
    pVnode->uid = inode->i_uid;               // 设置UID
    pVnode->gid = inode->i_gid;               // 设置GID
    pVnode->mode = inode->i_mode;             // 设置权限模式

    *newVnode = pVnode;                       // 输出新Vnode
    (void)VfsHashInsert(*newVnode, inode->i_ino);  // 插入VFS哈希表

    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放锁
    return ret;                               // 返回成功
}

/**
 * @brief   VFS接口：读取符号链接目标路径
 * @param   vnode    符号链接的Vnode指针
 * @param   buffer   用户空间缓冲区，用于存储目标路径
 * @param   bufLen   缓冲区长度
 * @return  成功返回读取的字节数，失败返回负错误码
 */
ssize_t VfsJffs2Readlink(struct Vnode *vnode, char *buffer, size_t bufLen)
{
    struct jffs2_inode *inode = NULL;         // 符号链接inode指针
    struct jffs2_inode_info *f = NULL;        // inode信息结构体指针
    ssize_t targetLen;                        // 目标路径长度
    ssize_t cnt;                              // 实际读取长度

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    inode = (struct jffs2_inode *)vnode->data;  // 获取inode
    f = JFFS2_INODE_INFO(inode);               // 获取inode信息
    targetLen = strlen((const char *)f->target);  // 计算目标路径长度
    if (bufLen == 0) {                        // 缓冲区长度为0（仅获取长度）
        LOS_MuxUnlock(&g_jffs2FsLock);        // 释放锁
        return 0;                             // 返回0（不读取数据）
    }

    // 计算可读取的最大长度（避免缓冲区溢出，预留1字节给终止符）
    cnt = (bufLen - 1) < targetLen ? (bufLen - 1) : targetLen;
    // 将目标路径从内核空间复制到用户空间
    if (LOS_CopyFromKernel(buffer, bufLen, (const char *)f->target, cnt) != 0) {
        LOS_MuxUnlock(&g_jffs2FsLock);        // 复制失败，释放锁
        return -EFAULT;                       // 返回内存访问错误
    }
    buffer[cnt] = '\0';                       // 添加字符串终止符

    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放锁

    return cnt;                               // 返回实际读取的字节数
}

/**
 * @brief   VFS接口：删除文件（非目录）
 * @param   parentVnode 父目录Vnode指针
 * @param   targetVnode 目标文件Vnode指针
 * @param   path        目标文件路径名
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Unlink(struct Vnode *parentVnode, struct Vnode *targetVnode, const char *path)
{
    int ret;                                  // 函数返回值
    struct jffs2_inode *parentInode = NULL;   // 父目录inode指针
    struct jffs2_inode *targetInode = NULL;   // 目标文件inode指针

    if (!parentVnode || !targetVnode) {       // 参数合法性检查
        PRINTK("%s-%d parentVnode=%x, targetVnode=%x\n", __FUNCTION__, __LINE__, parentVnode, targetVnode);  // 打印调试信息
        return -EINVAL;                       // 返回无效参数错误
    }

    parentInode = (struct jffs2_inode *)parentVnode->data;  // 获取父目录inode
    targetInode = (struct jffs2_inode *)targetVnode->data;  // 获取目标文件inode

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    // 调用JFFS2删除文件函数
    ret = jffs2_unlink(parentInode, targetInode, (const unsigned char *)path);
    if (ret == 0) {                           // 删除成功
        (void)jffs2_iput(targetInode);        // 减少inode引用计数
    }

    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放锁
    return ret;                               // 返回操作结果
}

/**
 * @brief   VFS接口：重命名文件/目录
 * @param   fromVnode    源文件/目录的Vnode指针
 * @param   toParentVnode 目标父目录的Vnode指针
 * @param   fromName     源名称
 * @param   toName       目标名称
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Rename(struct Vnode *fromVnode, struct Vnode *toParentVnode, const char *fromName, const char *toName)
{
    int ret;                                  // 函数返回值
    struct Vnode *fromParentVnode = NULL;     // 源文件的父目录Vnode
    struct Vnode *toVnode = NULL;             // 目标路径已存在的Vnode
    struct jffs2_inode *fromNode = NULL;      // 源文件inode

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁
    fromParentVnode = fromVnode->parent;      // 获取源文件的父目录

    // 检查目标路径是否已存在
    ret = VfsJffs2Lookup(toParentVnode, toName, strlen(toName), &toVnode);
    if (ret == 0) {                           // 目标路径已存在
        if (toVnode->type == VNODE_TYPE_DIR) {  // 目标是目录
            ret = VfsJffs2Rmdir(toParentVnode, toVnode, (char *)toName);  // 删除目标目录
        } else {
            ret = VfsJffs2Unlink(toParentVnode, toVnode, (char *)toName);  // 删除目标文件
        }
        if (ret) {                            // 删除目标失败
            PRINTK("%s-%d remove newname(%s) failed ret=%d\n", __FUNCTION__, __LINE__, toName, ret);  // 打印错误信息
            LOS_MuxUnlock(&g_jffs2FsLock);    // 释放锁
            return ret;                       // 返回错误码
        }
    }
    fromNode = (struct jffs2_inode *)fromVnode->data;  // 获取源文件inode
    // 调用JFFS2重命名函数
    ret = jffs2_rename((struct jffs2_inode *)fromParentVnode->data, fromNode,
        (const unsigned char *)fromName, (struct jffs2_inode *)toParentVnode->data, (const unsigned char *)toName);
    fromVnode->parent = toParentVnode;        // 更新源文件Vnode的父目录
    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放锁

    if (ret) {                                // 重命名失败
        return ret;                           // 返回错误码
    }

    return 0;                                 // 返回成功
}

/**
 * @brief   VFS接口：获取文件状态信息
 * @param   pVnode  文件/目录的Vnode指针
 * @param   buf     stat结构体指针，用于存储状态信息
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Stat(struct Vnode *pVnode, struct stat *buf)
{
    struct jffs2_inode *node = NULL;          // JFFS2 inode结构体指针

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    node = (struct jffs2_inode *)pVnode->data;  // 获取inode
    // 根据inode模式设置文件类型
    switch (node->i_mode & S_IFMT) {
        case S_IFREG:                         // 普通文件
        case S_IFDIR:                         // 目录
        case S_IFLNK:                         // 符号链接
            buf->st_mode = node->i_mode;      // 设置完整权限模式
            break;

        default:                              // 未知类型
            buf->st_mode = DT_UNKNOWN;        // 设置为未知类型
            break;
    }

    // 填充stat结构体字段
    buf->st_dev = 0;                          // 设备号（未使用）
    buf->st_ino = node->i_ino;                // inode号
    buf->st_nlink = node->i_nlink;            // 链接数
    buf->st_uid = node->i_uid;                // 用户ID
    buf->st_gid = node->i_gid;                // 组ID
    buf->st_size = node->i_size;              // 文件大小（字节）
    buf->st_blksize = BLOCK_SIZE;             // 块大小
    buf->st_blocks = buf->st_size / buf->st_blksize;  // 块数（向上取整）
    buf->st_atime = node->i_atime;            // 访问时间
    buf->st_mtime = node->i_mtime;            // 修改时间
    buf->st_ctime = node->i_ctime;            // 创建时间

    /* Adapt to kstat member long tv_sec */
    buf->__st_atim32.tv_sec = (long)node->i_atime;  // 适配kstat的32位时间字段
    buf->__st_mtim32.tv_sec = (long)node->i_mtime;
    buf->__st_ctim32.tv_sec = (long)node->i_ctime;

    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放锁

    return 0;                                 // 返回成功
}

/**
 * @brief   VFS接口：Vnode回收操作（空实现）
 * @param   pVnode  待回收的Vnode指针
 * @return  始终返回0（成功）
 * @note    JFFS2未实现Vnode主动回收机制，依赖系统自动管理
 */
int VfsJffs2Reclaim(struct Vnode *pVnode)
{
    return 0;  // 空实现，直接返回成功
}

/**
 * @brief   VFS接口：获取文件系统统计信息
 * @param   mnt     文件系统挂载点结构体指针
 * @param   buf     statfs结构体指针，用于存储统计信息
 * @return  成功返回0，失败返回负错误码
 */
int VfsJffs2Statfs(struct Mount *mnt, struct statfs *buf)
{
    unsigned long freeSize;                   // 可用空间总量（空闲+脏数据区域）
    struct jffs2_sb_info *c = NULL;           // JFFS2超级块信息结构体指针
    struct jffs2_inode *rootNode = NULL;      // 根目录inode指针

    LOS_MuxLock(&g_jffs2FsLock, (uint32_t)JFFS2_WAITING_FOREVER);  // 获取文件系统锁

    rootNode = (struct jffs2_inode *)mnt->vnodeCovered->data;  // 从挂载点获取根目录inode
    c = JFFS2_SB_INFO(rootNode->i_sb);        // 获取JFFS2超级块信息

    freeSize = c->free_size + c->dirty_size;  // 计算可用空间（空闲块+可回收脏块）
    buf->f_type = JFFS2_SUPER_MAGIC;          // 文件系统类型（JFFS2魔数）
    buf->f_bsize = PAGE_SIZE;                 // 块大小（系统页大小）
    // 计算总块数：(总块数 * 每块扇区大小) / 页大小
    buf->f_blocks = (((uint64_t)c->nr_blocks) * c->sector_size) / PAGE_SIZE;
    buf->f_bfree = freeSize / PAGE_SIZE;      // 空闲块数
    buf->f_bavail = buf->f_bfree;             // 可用块数（与空闲块数相同）
    buf->f_namelen = NAME_MAX;                // 最大文件名长度
    buf->f_fsid.__val[0] = JFFS2_SUPER_MAGIC; // 文件系统ID（低32位）
    buf->f_fsid.__val[1] = 1;                 // 文件系统ID（高32位）
    buf->f_frsize = BLOCK_SIZE;               // 基本文件系统块大小
    buf->f_files = 0;                         // 文件总数（未实现）
    buf->f_ffree = 0;                         // 空闲文件节点数（未实现）
    buf->f_flags = mnt->mountFlags;           // 挂载标志

    LOS_MuxUnlock(&g_jffs2FsLock);            // 释放文件系统锁
    return 0;                                 // 返回成功
}

/**
 * @brief   初始化JFFS2文件系统互斥锁
 * @return  成功返回0，失败返回-1
 */
int Jffs2MutexCreate(void)
{
    // 初始化全局文件系统互斥锁
    if (LOS_MuxInit(&g_jffs2FsLock, NULL) != LOS_OK) {
        PRINT_ERR("%s, LOS_MuxCreate failed\n", __FUNCTION__);  // 打印初始化失败信息
        return -1;                                               // 返回错误
    } else {
        return 0;                                                // 返回成功
    }
}

/**
 * @brief   销毁JFFS2文件系统互斥锁
 */
void Jffs2MutexDelete(void)
{
    (void)LOS_MuxDestroy(&g_jffs2FsLock);     // 销毁全局文件系统互斥锁
}

/**
 * @brief   JFFS2文件系统挂载操作集合
 * @note    实现VFS与JFFS2之间的挂载接口适配
 */
const struct MountOps jffs_operations = {
    .Mount = VfsJffs2Bind,                    // 挂载文件系统
    .Unmount = VfsJffs2Unbind,                // 卸载文件系统
    .Statfs = VfsJffs2Statfs,                 // 获取文件系统统计信息
};

/**
 * @brief   JFFS2 Vnode操作集合
 * @note    实现VFS层Vnode操作到JFFS2具体实现的映射
 */
struct VnodeOps g_jffs2Vops = {
    .Lookup = VfsJffs2Lookup,                 // 查找文件/目录
    .Create = VfsJffs2Create,                 // 创建文件
    .ReadPage = VfsJffs2ReadPage,             // 读取页数据
    .WritePage = VfsJffs2WritePage,           // 写入页数据
    .Rename = VfsJffs2Rename,                 // 重命名文件/目录
    .Mkdir = VfsJffs2Mkdir,                   // 创建目录
    .Getattr = VfsJffs2Stat,                  // 获取文件属性
    .Opendir = VfsJffs2Opendir,               // 打开目录
    .Readdir = VfsJffs2Readdir,               // 读取目录
    .Closedir = VfsJffs2Closedir,             // 关闭目录
    .Rewinddir = VfsJffs2Rewinddir,           // 重绕目录（回到开头）
    .Unlink = VfsJffs2Unlink,                 // 删除文件
    .Rmdir = VfsJffs2Rmdir,                   // 删除目录
    .Chattr = VfsJffs2Chattr,                 // 修改文件属性
    .Reclaim = VfsJffs2Reclaim,               // 回收Vnode
    .Truncate = VfsJffs2Truncate,             // 截断文件（32位）
    .Truncate64 = VfsJffs2Truncate64,         // 截断文件（64位）
    .Link = VfsJffs2Link,                     // 创建硬链接
    .Symlink = VfsJffs2Symlink,               // 创建符号链接
    .Readlink = VfsJffs2Readlink,             // 读取符号链接目标
};

/**
 * @brief   JFFS2文件操作集合
 * @note    实现VFS层文件操作到JFFS2具体实现的映射
 */
struct file_operations_vfs g_jffs2Fops = {
    .read = VfsJffs2Read,                     // 读取文件内容
    .write = VfsJffs2Write,                   // 写入文件内容
    .mmap = OsVfsFileMmap,                    // 内存映射（使用通用VFS实现）
    .seek = VfsJffs2Seek,                     // 文件定位
    .close = VfsJffs2Close,                   // 关闭文件
    .fsync = VfsJffs2Fsync,                   // 文件同步
};


// 文件系统注册宏：将JFFS2文件系统注册到VFS
// 参数说明：
//   jffs_fsmap   - 映射表项名称
//   "jffs2"      - 文件系统名称
//   jffs_operations - 挂载操作集合
//   TRUE         - 支持格式化
//   TRUE         - 支持自动挂载
FSMAP_ENTRY(jffs_fsmap, "jffs2", jffs_operations, TRUE, TRUE);

#endif
