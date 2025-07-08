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
// proc文件系统的Vnode操作结构体实例
static struct VnodeOps g_procfsVops;
// proc文件系统的文件操作结构体实例
static struct file_operations_vfs g_procfsFops;

/**
 * @brief 将Vnode转换为ProcDirEntry
 * @param node Vnode指针
 * @return 对应的ProcDirEntry指针
 */
struct ProcDirEntry *VnodeToEntry(struct Vnode *node)
{
    return (struct ProcDirEntry *)(node->data);  // 返回Vnode数据字段中存储的ProcDirEntry指针
}

/**
 * @brief 将ProcDirEntry转换为Vnode
 * @param entry ProcDirEntry指针
 * @return 新创建的Vnode指针
 */
static struct Vnode *EntryToVnode(struct ProcDirEntry *entry)
{
    struct Vnode *node = NULL;  // Vnode指针初始化

    (void)VnodeAlloc(&g_procfsVops, &node);  // 分配Vnode内存并关联Vnode操作
    node->fop = &g_procfsFops;               // 设置文件操作结构体
    node->data = entry;                      // 存储ProcDirEntry指针到Vnode数据字段
    node->type = entry->type;                // 设置Vnode类型
    node->uid = entry->uid;                  // 设置用户ID
    node->gid = entry->gid;                  // 设置组ID
    node->mode = entry->mode;                // 设置文件权限模式
    return node;                             // 返回创建的Vnode
}

/**
 * @brief 比较名称与ProcDirEntry的名称是否匹配
 * @param name 要比较的名称
 * @param len 名称长度
 * @param pn ProcDirEntry指针
 * @return 匹配返回1，否则返回0
 */
static int EntryMatch(const char *name, int len, const struct ProcDirEntry *pn)
{
    if (len != pn->nameLen) {                // 长度不匹配则直接返回不匹配
        return 0;
    }
    return !strncmp(name, pn->name, len);    // 比较名称字符串是否相等
}

/**
 * @brief 截断proc文件系统中的文件
 * @param pVnode Vnode指针
 * @param len 截断长度
 * @return 总是返回0（proc文件系统不支持截断操作）
 */
int VfsProcfsTruncate(struct Vnode *pVnode, off_t len)
{
    return 0;  // proc文件系统不支持截断操作，直接返回成功
}

/**
 * @brief 在proc文件系统中创建文件节点
 * @param parent 父Vnode指针
 * @param name 要创建的节点名称
 * @param mode 文件权限模式
 * @param vnode 输出参数，返回创建的Vnode指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsCreate(struct Vnode* parent, const char *name, int mode, struct Vnode **vnode)
{
    int ret;                                 // 返回值
    struct Vnode *vp = NULL;                 // 新Vnode指针
    struct ProcDirEntry *curEntry = NULL;    // 新ProcDirEntry指针

    struct ProcDirEntry *parentEntry = VnodeToEntry(parent);  // 获取父节点的ProcDirEntry
    if (parentEntry == NULL) {                                // 父节点ProcDirEntry无效
        return -ENODATA;                                      // 返回数据不存在错误
    }

    ret = VnodeAlloc(&g_procfsVops, &vp);    // 分配新Vnode
    if (ret != 0) {                          // 分配失败
        return -ENOMEM;                      // 返回内存不足错误
    }

    curEntry = ProcCreate(name, mode, parentEntry, NULL);  // 创建ProcDirEntry
    if (curEntry == NULL) {                                // 创建失败
        VnodeFree(vp);                                    // 释放已分配的Vnode
        return -ENODATA;                                  // 返回数据不存在错误
    }

    vp->data = curEntry;                     // 关联ProcDirEntry到Vnode
    vp->type = curEntry->type;               // 设置Vnode类型
    if (vp->type == VNODE_TYPE_DIR) {        // 判断是否为目录类型
        vp->mode = S_IFDIR | PROCFS_DEFAULT_MODE;  // 目录模式权限
    } else {
        vp->mode = S_IFREG | PROCFS_DEFAULT_MODE;  // 文件模式权限
    }

    vp->vop = parent->vop;                   // 继承父节点的Vnode操作
    vp->fop = parent->fop;                   // 继承父节点的文件操作
    vp->parent = parent;                     // 设置父Vnode
    vp->originMount = parent->originMount;   // 设置挂载点

    *vnode = vp;                             // 输出新创建的Vnode

    return LOS_OK;                           // 返回成功
}

/**
 * @brief 从proc文件系统读取数据
 * @param filep 文件结构体指针
 * @param buffer 数据缓冲区
 * @param buflen 缓冲区长度
 * @return 成功返回读取的字节数，失败返回错误码
 */
int VfsProcfsRead(struct file *filep, char *buffer, size_t buflen)
{
    ssize_t size;                            // 读取大小
    struct ProcDirEntry *entry = NULL;       // ProcDirEntry指针
    if ((filep == NULL) || (filep->f_vnode == NULL) || (buffer == NULL)) {  // 参数有效性检查
        return -EINVAL;                      // 返回无效参数错误
    }

    VnodeHold();                             // 持有Vnode锁
    entry = VnodeToEntry(filep->f_vnode);    // 获取文件对应的ProcDirEntry
    if (entry == NULL) {                     // ProcDirEntry无效
        VnodeDrop();                         // 释放Vnode锁
        return -EPERM;                       // 返回权限错误
    }

    size = (ssize_t)ReadProcFile(entry, (void *)buffer, buflen);  // 读取proc文件内容
    filep->f_pos = entry->pf->fPos;                               // 更新文件位置
    VnodeDrop();                                                  // 释放Vnode锁
    return size;                                                  // 返回读取大小
}

/**
 * @brief 向proc文件系统写入数据
 * @param filep 文件结构体指针
 * @param buffer 数据缓冲区
 * @param buflen 写入长度
 * @return 成功返回写入的字节数，失败返回错误码
 */
int VfsProcfsWrite(struct file *filep, const char *buffer, size_t buflen)
{
    ssize_t size;                            // 写入大小
    struct ProcDirEntry *entry = NULL;       // ProcDirEntry指针
    if ((filep == NULL) || (filep->f_vnode == NULL) || (buffer == NULL)) {  // 参数有效性检查
        return -EINVAL;                      // 返回无效参数错误
    }

    VnodeHold();                             // 持有Vnode锁
    entry = VnodeToEntry(filep->f_vnode);    // 获取文件对应的ProcDirEntry
    if (entry == NULL) {                     // ProcDirEntry无效
        VnodeDrop();                         // 释放Vnode锁
        return -EPERM;                       // 返回权限错误
    }

    size = (ssize_t)WriteProcFile(entry, (void *)buffer, buflen);  // 写入proc文件内容
    filep->f_pos = entry->pf->fPos;                                // 更新文件位置
    VnodeDrop();                                                   // 释放Vnode锁
    return size;                                                   // 返回写入大小
}

/**
 * @brief 在proc文件系统中查找节点
 * @param parent 父Vnode指针
 * @param name 要查找的节点名称
 * @param len 名称长度
 * @param vpp 输出参数，返回找到的Vnode指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsLookup(struct Vnode *parent, const char *name, int len, struct Vnode **vpp)
{
    if (parent == NULL || name == NULL || len <= 0 || vpp == NULL) {  // 参数有效性检查
        return -EINVAL;                                              // 返回无效参数错误
    }
    struct ProcDirEntry *entry = VnodeToEntry(parent);                // 获取父节点的ProcDirEntry
    if (entry == NULL) {                                             // ProcDirEntry无效
        return -ENODATA;                                             // 返回数据不存在错误
    }

    entry = entry->subdir;                  // 从子目录开始查找
    while (1) {                             // 循环遍历子目录
        if (entry == NULL) {                // 未找到匹配节点
            return -ENOENT;                 // 返回文件不存在错误
        }
        if (EntryMatch(name, len, entry)) { // 找到匹配的节点
            break;                          // 跳出循环
        }
        entry = entry->next;                // 继续查找下一个节点
    }

    *vpp = EntryToVnode(entry);             // 将找到的ProcDirEntry转换为Vnode
    if ((*vpp) == NULL) {                   // 转换失败
        return -ENOMEM;                     // 返回内存不足错误
    }
    (*vpp)->originMount = parent->originMount;  // 设置挂载点
    (*vpp)->parent = parent;                    // 设置父Vnode
    return LOS_OK;                              // 返回成功
}

/**
 * @brief 挂载proc文件系统
 * @param mnt 挂载结构体指针
 * @param device 设备Vnode指针（未使用）
 * @param data 挂载数据（未使用）
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsMount(struct Mount *mnt, struct Vnode *device, const void *data)
{
    struct Vnode *vp = NULL;                // Vnode指针
    int ret;                                // 返回值

    spin_lock_init(&procfsLock);            // 初始化procfs锁
    procfsInit = true;                      // 标记procfs已初始化

    ret = VnodeAlloc(&g_procfsVops, &vp);    // 分配Vnode
    if (ret != 0) {                          // 分配失败
        return -ENOMEM;                      // 返回内存不足错误
    }

    struct ProcDirEntry *root = GetProcRootEntry();  // 获取proc根目录入口
    vp->data = root;                                 // 关联根目录入口到Vnode
    vp->originMount = mnt;                           // 设置挂载点
    vp->fop = &g_procfsFops;                         // 设置文件操作
    mnt->data = NULL;                                // 挂载数据置空
    mnt->vnodeCovered = vp;                          // 设置挂载覆盖的Vnode
    vp->type = root->type;                           // 设置Vnode类型
    if (vp->type == VNODE_TYPE_DIR) {                // 判断是否为目录类型
        vp->mode = S_IFDIR | PROCFS_DEFAULT_MODE;    // 目录模式权限
    } else {
        vp->mode = S_IFREG | PROCFS_DEFAULT_MODE;    // 文件模式权限
    }

    return LOS_OK;                               // 返回成功
}

/**
 * @brief 卸载proc文件系统（不支持）
 * @param handle 挂载句柄
 * @param blkdriver 块设备驱动（未使用）
 * @return 总是返回-EPERM（不允许卸载）
 */
int VfsProcfsUnmount(void *handle, struct Vnode **blkdriver)
{
    (void)handle;                            // 未使用参数
    (void)blkdriver;                         // 未使用参数
    return -EPERM;                           // proc文件系统不支持卸载，返回权限错误
}

/**
 * @brief 获取proc文件系统节点的状态信息
 * @param node Vnode指针
 * @param buf stat结构体指针，用于存储状态信息
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsStat(struct Vnode *node, struct stat *buf)
{
    VnodeHold();                             // 持有Vnode锁
    struct ProcDirEntry *entry = VnodeToEntry(node);  // 获取节点对应的ProcDirEntry
    if (entry == NULL) {                              // ProcDirEntry无效
        VnodeDrop();                                  // 释放Vnode锁
        return -EPERM;                                // 返回权限错误
    }
    (void)memset_s(buf, sizeof(struct stat), 0, sizeof(struct stat));  // 清空stat结构体
    buf->st_mode = entry->mode;                                       // 设置文件权限模式
    VnodeDrop();                                                      // 释放Vnode锁
    return LOS_OK;                                                    // 返回成功
}

#ifdef LOSCFG_KERNEL_PLIMITS  // 如果启用了内核限制器功能
/**
 * @brief 在proc文件系统中创建目录
 * @param parent 父Vnode指针
 * @param dirName 目录名称
 * @param mode 目录权限模式
 * @param vnode 输出参数，返回创建的目录Vnode
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsMkdir(struct Vnode *parent, const char *dirName, mode_t mode, struct Vnode **vnode)
{
    struct ProcDirEntry *parentEntry = VnodeToEntry(parent);  // 获取父节点的ProcDirEntry
    struct ProcDirEntry *pde = NULL;                          // 新目录的ProcDirEntry
    if ((parentEntry->procDirOps == NULL) || (parentEntry->procDirOps->mkdir == NULL)) {  // 检查是否支持创建目录操作
        return -ENOSYS;                                                                   // 返回不支持的系统调用错误
    }

    int ret = parentEntry->procDirOps->mkdir(parentEntry, dirName, mode, &pde);  // 调用目录操作创建目录
    if ((ret < 0) || (pde == NULL)) {                                            // 创建失败
        return ret;                                                              // 返回错误码
    }

    *vnode = EntryToVnode(pde);                     // 将ProcDirEntry转换为Vnode
    (*vnode)->vop = parent->vop;                   // 继承父节点的Vnode操作
    (*vnode)->parent = parent;                     // 设置父Vnode
    (*vnode)->originMount = parent->originMount;   // 设置挂载点
    if ((*vnode)->type == VNODE_TYPE_DIR) {        // 判断是否为目录类型
        (*vnode)->mode = S_IFDIR | PROCFS_DEFAULT_MODE;  // 目录模式权限
    } else {
        (*vnode)->mode = S_IFREG | PROCFS_DEFAULT_MODE;  // 文件模式权限
    }
    return ret;                                   // 返回结果
}

/**
 * @brief 从proc文件系统中删除目录
 * @param parent 父Vnode指针
 * @param vnode 要删除的目录Vnode
 * @param dirName 目录名称
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsRmdir(struct Vnode *parent, struct Vnode *vnode, const char *dirName)
{
    if (parent == NULL) {                         // 父节点无效
        return -EINVAL;                          // 返回无效参数错误
    }

    struct ProcDirEntry *parentEntry = VnodeToEntry(parent);  // 获取父节点的ProcDirEntry
    if ((parentEntry->procDirOps == NULL) || (parentEntry->procDirOps->rmdir == NULL)) {  // 检查是否支持删除目录操作
        return -ENOSYS;                                                                   // 返回不支持的系统调用错误
    }

    struct ProcDirEntry *dirEntry = VnodeToEntry(vnode);  // 获取要删除目录的ProcDirEntry
    int ret = parentEntry->procDirOps->rmdir(parentEntry, dirEntry, dirName);  // 调用目录操作删除目录
    if (ret < 0) {                                                             // 删除失败
        return ret;                                                           // 返回错误码
    }
    vnode->data = NULL;  // 清除Vnode关联的数据
    return 0;            // 返回成功
}
#endif  // LOSCFG_KERNEL_PLIMITS

/**
 * @brief 读取proc目录项
 * @param node 目录Vnode指针
 * @param dir 目录项结构体指针
 * @return 成功返回读取的目录项数量，失败返回错误码
 */
int VfsProcfsReaddir(struct Vnode *node, struct fs_dirent_s *dir)
{
    int result;                              // 操作结果
    char *buffer = NULL;                     // 缓冲区指针
    unsigned int minSize, dstNameSize;       // 大小变量
    struct ProcDirEntry *pde = NULL;         // ProcDirEntry指针
    int i = 0;                               // 循环计数器

    if (dir == NULL) {                       // 目录项结构体无效
        return -EINVAL;                      // 返回无效参数错误
    }
    if (node->type != VNODE_TYPE_DIR) {      // 检查是否为目录类型
        return -ENOTDIR;                     // 返回不是目录错误
    }
    VnodeHold();                             // 持有Vnode锁
    pde = VnodeToEntry(node);                // 获取目录对应的ProcDirEntry
    if (pde == NULL) {                       // ProcDirEntry无效
        VnodeDrop();                         // 释放Vnode锁
        return -EPERM;                       // 返回权限错误
    }

    while (i < dir->read_cnt) {              // 循环读取目录项
        buffer = (char *)zalloc(sizeof(char) * NAME_MAX);  // 分配缓冲区
        if (buffer == NULL) {                              // 分配失败
            VnodeDrop();                                  // 释放Vnode锁
            PRINT_ERR("malloc failed\n");                  // 打印错误信息
            return -ENOMEM;                               // 返回内存不足错误
        }

        result = ReadProcFile(pde, (void *)buffer, NAME_MAX);  // 读取目录项名称
        if (result != ENOERR) {                                // 读取失败
            free(buffer);                                      // 释放缓冲区
            break;                                             // 跳出循环
        }
        dstNameSize = sizeof(dir->fd_dir[i].d_name);           // 目标名称缓冲区大小
        minSize = (dstNameSize < NAME_MAX) ? dstNameSize : NAME_MAX;  // 计算最小大小
        result = strncpy_s(dir->fd_dir[i].d_name, dstNameSize, buffer, minSize);  // 复制名称
        if (result != EOK) {                                      // 复制失败
            VnodeDrop();                                          // 释放Vnode锁
            free(buffer);                                         // 释放缓冲区
            return -ENAMETOOLONG;                                 // 返回名称过长错误
        }
        dir->fd_dir[i].d_name[dstNameSize - 1] = '\0';  // 确保字符串以null结尾
        dir->fd_position++;                             // 更新目录位置
        dir->fd_dir[i].d_off = dir->fd_position;        // 设置目录项偏移
        dir->fd_dir[i].d_reclen = (uint16_t)sizeof(struct dirent);  // 设置目录项长度

        i++;                                            // 增加计数器
        free(buffer);                                   // 释放缓冲区
    }
    VnodeDrop();                                        // 释放Vnode锁
    return i;                                           // 返回读取的目录项数量
}

/**
 * @brief 打开proc目录
 * @param node 目录Vnode指针
 * @param dir 目录项结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsOpendir(struct Vnode *node,  struct fs_dirent_s *dir)
{
    VnodeHold();                             // 持有Vnode锁
    struct ProcDirEntry *pde = VnodeToEntry(node);  // 获取目录对应的ProcDirEntry
    if (pde == NULL) {                              // ProcDirEntry无效
        VnodeDrop();                               // 释放Vnode锁
        return -EINVAL;                            // 返回无效参数错误
    }

    pde->pdirCurrent = pde->subdir;         // 设置当前目录项为第一个子目录
    if (pde->pf == NULL) {                  // 检查文件结构体是否存在
        VnodeDrop();                        // 释放Vnode锁
        return -EINVAL;                     // 返回无效参数错误
    }
    pde->pf->fPos = 0;                      // 重置文件位置
    VnodeDrop();                            // 释放Vnode锁
    return LOS_OK;                          // 返回成功
}

/**
 * @brief 打开proc文件
 * @param filep 文件结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsOpen(struct file *filep)
{
    if (filep == NULL) {                    // 文件结构体无效
        return -EINVAL;                     // 返回无效参数错误
    }
    VnodeHold();                            // 持有Vnode锁
    struct Vnode *node = filep->f_vnode;    // 获取文件对应的Vnode
    struct ProcDirEntry *pde = VnodeToEntry(node);  // 获取ProcDirEntry
    if (pde == NULL) {                              // ProcDirEntry无效
        VnodeDrop();                               // 释放Vnode锁
        return -EPERM;                             // 返回权限错误
    }

    if (ProcOpen(pde->pf) != OK) {          // 打开proc文件
        return -ENOMEM;                     // 返回内存不足错误
    }
    if (S_ISREG(pde->mode) && (pde->procFileOps != NULL) && (pde->procFileOps->open != NULL)) {  // 如果是普通文件且有打开操作
        (void)pde->procFileOps->open((struct Vnode *)pde, pde->pf);  // 调用文件打开操作
    }
    if (S_ISDIR(pde->mode)) {               // 如果是目录
        pde->pdirCurrent = pde->subdir;    // 设置当前目录项为第一个子目录
        pde->pf->fPos = 0;                 // 重置文件位置
    }
    filep->f_priv = (void *)pde;            // 存储ProcDirEntry到私有数据
    VnodeDrop();                            // 释放Vnode锁
    return LOS_OK;                          // 返回成功
}

/**
 * @brief 关闭proc文件
 * @param filep 文件结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsClose(struct file *filep)
{
    int result = 0;                         // 操作结果
    if (filep == NULL) {                    // 文件结构体无效
        return -EINVAL;                     // 返回无效参数错误
    }

    VnodeHold();                            // 持有Vnode锁
    struct Vnode *node = filep->f_vnode;    // 获取文件对应的Vnode
    struct ProcDirEntry *pde = VnodeToEntry(node);  // 获取ProcDirEntry
    if ((pde == NULL) || (pde->pf == NULL)) {       // ProcDirEntry或文件结构体无效
        VnodeDrop();                               // 释放Vnode锁
        return -EPERM;                             // 返回权限错误
    }

    pde->pf->fPos = 0;                      // 重置文件位置
    if ((pde->procFileOps != NULL) && (pde->procFileOps->release != NULL)) {  // 如果有释放操作
        result = pde->procFileOps->release((struct Vnode *)pde, pde->pf);    // 调用释放操作
    }
    LosBufRelease(pde->pf->sbuf);           // 释放缓冲区
    pde->pf->sbuf = NULL;                   // 清空缓冲区指针
    VnodeDrop();                            // 释放Vnode锁
    return result;                          // 返回操作结果
}

/**
 * @brief 获取proc文件系统的状态信息
 * @param mnt 挂载结构体指针
 * @param buf statfs结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
int VfsProcfsStatfs(struct Mount *mnt, struct statfs *buf)
{
    (void)memset_s(buf, sizeof(struct statfs), 0, sizeof(struct statfs));  // 清空statfs结构体
    buf->f_type = PROCFS_MAGIC;                                           // 设置文件系统魔数

    return LOS_OK;                          // 返回成功
}

/**
 * @brief 关闭proc目录
 * @param vp 目录Vnode指针
 * @param dir 目录项结构体指针
 * @return 总是返回LOS_OK
 */
int VfsProcfsClosedir(struct Vnode *vp, struct fs_dirent_s *dir)
{
    return LOS_OK;  // 关闭目录成功
}

/**
 * @brief 读取符号链接
 * @param vnode Vnode指针
 * @param buffer 缓冲区
 * @param bufLen 缓冲区长度
 * @return 成功返回读取的字节数，失败返回错误码
 */
ssize_t VfsProcfsReadlink(struct Vnode *vnode, char *buffer, size_t bufLen)
{
    int result = -EINVAL;                   // 操作结果
    if (vnode == NULL) {                    // Vnode无效
        return result;                      // 返回无效参数错误
    }

    struct ProcDirEntry *pde = VnodeToEntry(vnode);  // 获取ProcDirEntry
    if (pde == NULL) {                              // ProcDirEntry无效
        return -EPERM;                             // 返回权限错误
    }

    if ((pde->procFileOps != NULL) && (pde->procFileOps->readLink != NULL)) {  // 如果有读取链接操作
        result = pde->procFileOps->readLink(pde, buffer, bufLen);             // 调用读取链接操作
    }
    return result;                          // 返回操作结果
}

// proc文件系统的挂载操作结构体
const struct MountOps procfs_operations = {
    .Mount = VfsProcfsMount,                // 挂载函数
    .Unmount = NULL,                        // 卸载函数（不支持）
    .Statfs = VfsProcfsStatfs,              // 获取文件系统状态函数
};

// proc文件系统的Vnode操作结构体实例初始化
static struct VnodeOps g_procfsVops = {
    .Lookup = VfsProcfsLookup,              // 查找节点
    .Getattr = VfsProcfsStat,               // 获取属性
    .Readdir = VfsProcfsReaddir,            // 读取目录
    .Opendir = VfsProcfsOpendir,            // 打开目录
    .Closedir = VfsProcfsClosedir,          // 关闭目录
    .Truncate = VfsProcfsTruncate,          // 截断文件
    .Readlink = VfsProcfsReadlink,          // 读取链接
#ifdef LOSCFG_KERNEL_PLIMITS
    .Mkdir = VfsProcfsMkdir,                // 创建目录（仅限制器功能启用时）
    .Rmdir = VfsProcfsRmdir,                // 删除目录（仅限制器功能启用时）
#endif
};

// proc文件系统的文件操作结构体实例初始化
static struct file_operations_vfs g_procfsFops = {
    .read = VfsProcfsRead,                  // 读取文件
    .write = VfsProcfsWrite,                // 写入文件
    .open = VfsProcfsOpen,                  // 打开文件
    .close = VfsProcfsClose                 // 关闭文件
};

// 注册proc文件系统到文件系统映射
FSMAP_ENTRY(procfs_fsmap, "procfs", procfs_operations, FALSE, FALSE);
#endif
