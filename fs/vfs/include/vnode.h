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

#ifndef _VNODE_H_
#define _VNODE_H_

#include <sys/stat.h>
#include "fs/fs_operation.h"
#include "fs/file.h"
#include "los_list.h"

typedef LOS_DL_LIST LIST_HEAD;  // 定义双向链表头类型
typedef LOS_DL_LIST LIST_ENTRY; // 定义双向链表节点类型

#define VNODE_FLAG_MOUNT_NEW      (1 << 0) /* new mount vnode */  // 新挂载节点标志位
#define VNODE_FLAG_MOUNT_ORIGIN   (1 << 1) /* origin vnode */   // 原始挂载节点标志位

#define V_CREATE     (1 << 0)  // 创建文件标志
#define V_DUMMY      (1 << 2)  // 虚拟节点标志

#ifndef VFS_ERROR
#define VFS_ERROR -1  // VFS操作错误返回值
#endif

#ifndef OK
#define OK 0  // 操作成功返回值
#endif

#define AT_REMOVEDIR 0x200  // 删除目录操作标志

#define DEV_PATH_LEN 5  // 设备路径长度

/* Permission flags */
#define READ_OP                 4  // 读操作权限位
#define WRITE_OP                2  // 写操作权限位
#define EXEC_OP                 1  // 执行操作权限位
#define UGO_NUMS                3  // 用户/组/其他权限数量
#define MODE_IXUGO              0111  // 所有用户的执行权限掩码
#define USER_MODE_SHIFT         6  // 用户权限位移量
#define GROUP_MODE_SHIFT        3  // 组权限位移量
#define UMASK_FULL              0777  // 完整权限掩码

/* Attribute flags. */
#define CHG_MODE 1    // 模式属性变更标志
#define CHG_UID 2     // UID属性变更标志
#define CHG_GID 4     // GID属性变更标志
#define CHG_SIZE 8    // 大小属性变更标志
#define CHG_ATIME 16  // 访问时间属性变更标志
#define CHG_MTIME 32  // 修改时间属性变更标志
#define CHG_CTIME 64  // 创建时间属性变更标志

struct IATTR {  // 用于记录vnode属性变更的结构体
    /* This structure is used for record vnode attr. */
    unsigned int attr_chg_valid;    // 属性变更有效性标志
    unsigned int attr_chg_flags;    // 属性变更标志位
    unsigned attr_chg_mode;         // 变更后的模式
    unsigned attr_chg_uid;          // 变更后的UID
    unsigned attr_chg_gid;          // 变更后的GID
    unsigned attr_chg_size;         // 变更后的大小
    unsigned attr_chg_atime;        // 变更后的访问时间
    unsigned attr_chg_mtime;        // 变更后的修改时间
    unsigned attr_chg_ctime;        // 变更后的创建时间
};

 /*
  * Vnode types.  VNODE_TYPE_UNKNOWN means no type.
  */
enum VnodeType {  // vnode类型枚举
    VNODE_TYPE_UNKNOWN,       /* unknown type */          // 未知类型
    VNODE_TYPE_REG,           /* regular fle */           // 普通文件
    VNODE_TYPE_DIR,           /* directory */             // 目录
    VNODE_TYPE_BLK,           /* block device */          // 块设备
    VNODE_TYPE_CHR,           /* char device */           // 字符设备
    VNODE_TYPE_BCHR,          /* block char mix device */ // 块字符混合设备
    VNODE_TYPE_FIFO,          /* pipe */                  // 管道
    VNODE_TYPE_LNK,           /* link */                  // 链接
#ifdef LOSCFG_PROC_PROCESS_DIR
    VNODE_TYPE_VIR_LNK,       /* virtual link */          // 虚拟链接
#endif
};

struct fs_dirent_s;  // 前向声明目录项结构体
struct VnodeOps;     // 前向声明vnode操作结构体
struct IATTR;        // 前向声明属性变更结构体

struct Vnode {  // vnode结构体，代表虚拟文件系统中的一个节点
    enum VnodeType type;                /* vnode type */               // vnode类型
    int useCount;                       /* ref count of users */       // 用户引用计数
    uint32_t hash;                      /* vnode hash */               // vnode哈希值
    uint uid;                           /* uid for dac */              // DAC权限的用户ID
    uint gid;                           /* gid for dac */              // DAC权限的组ID
    mode_t mode;                        /* mode for dac */             // DAC权限的模式
    LIST_HEAD parentPathCaches;         /* pathCaches point to parents */ // 指向父节点的路径缓存链表
    LIST_HEAD childPathCaches;          /* pathCaches point to children */ // 指向子节点的路径缓存链表
    struct Vnode *parent;               /* parent vnode */             // 父vnode指针
    struct VnodeOps *vop;               /* vnode operations */         // vnode操作函数集
    struct file_operations_vfs *fop;    /* file operations */          // 文件操作函数集
    void *data;                         /* private data */             // 文件系统私有数据
    uint32_t flag;                      /* vnode flag */               // vnode标志位
    LIST_ENTRY hashEntry;               /* list entry for bucket in hash table */ // 哈希表桶列表项
    LIST_ENTRY actFreeEntry;            /* vnode active/free list entry */ // vnode活跃/空闲列表项
    struct Mount *originMount;          /* fs info about this vnode */ // 该vnode所属的文件系统挂载信息
    struct Mount *newMount;             /* fs info about who mount on this vnode */ // 挂载在该vnode上的文件系统信息
    char *filePath;                     /* file path of the vnode */   // vnode的文件路径
    struct page_mapping mapping;        /* page mapping of the vnode */ // vnode的页面映射
#ifdef LOSCFG_MNT_CONTAINER
    int mntCount;                       /* ref count of mounts */      // 挂载引用计数
#endif
};

struct VnodeOps {  // vnode操作函数集结构体
    int (*Create)(struct Vnode *parent, const char *name, int mode, struct Vnode **vnode);  // 创建文件
    int (*Lookup)(struct Vnode *parent, const char *name, int len, struct Vnode **vnode);   // 查找文件
    int (*Open)(struct Vnode *vnode, int fd, int mode, int flags);  // 打开文件
    ssize_t (*ReadPage)(struct Vnode *vnode, char *buffer, off_t pos);  // 读取页面
    ssize_t (*WritePage)(struct Vnode *vnode, char *buffer, off_t pos, size_t buflen);  // 写入页面
    int (*Close)(struct Vnode *vnode);  // 关闭文件
    int (*Reclaim)(struct Vnode *vnode);  // 回收vnode
    int (*Unlink)(struct Vnode *parent, struct Vnode *vnode, const char *fileName);  // 删除文件
    int (*Rmdir)(struct Vnode *parent, struct Vnode *vnode, const char *dirName);  // 删除目录
    int (*Mkdir)(struct Vnode *parent, const char *dirName, mode_t mode, struct Vnode **vnode);  // 创建目录
    int (*Readdir)(struct Vnode *vnode, struct fs_dirent_s *dir);  // 读取目录
    int (*Opendir)(struct Vnode *vnode, struct fs_dirent_s *dir);  // 打开目录
    int (*Rewinddir)(struct Vnode *vnode, struct fs_dirent_s *dir);  // 重绕目录
    int (*Closedir)(struct Vnode *vnode, struct fs_dirent_s *dir);  // 关闭目录
    int (*Getattr)(struct Vnode *vnode, struct stat *st);  // 获取文件属性
    int (*Setattr)(struct Vnode *vnode, struct stat *st);  // 设置文件属性
    int (*Chattr)(struct Vnode *vnode, struct IATTR *attr);  // 更改文件属性
    int (*Rename)(struct Vnode *src, struct Vnode *dstParent, const char *srcName, const char *dstName);  // 重命名文件
    int (*Truncate)(struct Vnode *vnode, off_t len);  // 截断文件
    int (*Truncate64)(struct Vnode *vnode, off64_t len);  // 64位截断文件
    int (*Fscheck)(struct Vnode *vnode, struct fs_dirent_s *dir);  // 文件系统检查
    int (*Link)(struct Vnode *src, struct Vnode *dstParent, struct Vnode **dst, const char *dstName);  // 创建硬链接
    int (*Symlink)(struct Vnode *parentVnode, struct Vnode **newVnode, const char *path, const char *target);  // 创建符号链接
    ssize_t (*Readlink)(struct Vnode *vnode, char *buffer, size_t bufLen);  // 读取符号链接
};

typedef int VfsHashCmp(struct Vnode *vnode, void *arg);  // vnode哈希比较函数指针类型

int VnodesInit(void);
int VnodeDevInit(void);
int VnodeAlloc(struct VnodeOps *vop, struct Vnode **newVnode);
int VnodeFree(struct Vnode *vnode);
int VnodeLookup(const char *path, struct Vnode **vnode, uint32_t flags);
int VnodeLookupFullpath(const char *fullpath, struct Vnode **vnode, uint32_t flags);
int VnodeLookupAt(const char *path, struct Vnode **vnode, uint32_t flags, struct Vnode *orgVnode);
int VnodeHold(void);
int VnodeDrop(void);
void VnodeRefDec(struct Vnode *vnode);
int VnodeFreeAll(const struct Mount *mnt);
int VnodeHashInit(void);
uint32_t VfsHashIndex(struct Vnode *vnode);
int VfsHashGet(const struct Mount *mount, uint32_t hash, struct Vnode **vnode, VfsHashCmp *fun, void *arg);
void VfsHashRemove(struct Vnode *vnode);
int VfsHashInsert(struct Vnode *vnode, uint32_t hash);
void ChangeRoot(struct Vnode *newRoot);
BOOL VnodeInUseIter(const struct Mount *mount);
struct Vnode *VnodeGetRoot(void);
void VnodeMemoryDump(void);
mode_t GetUmask(void);
int VfsPermissionCheck(uint fuid, uint fgid, mode_t fileMode, int accMode);
int VfsVnodePermissionCheck(const struct Vnode *node, int accMode);
LIST_HEAD* GetVnodeFreeList(void);
LIST_HEAD* GetVnodeActiveList(void);
LIST_HEAD* GetVnodeVirtualList(void);
int VnodeClearCache(void);
struct Vnode *GetCurrRootVnode(void);
#ifdef LOSCFG_PROC_PROCESS_DIR
struct Vnode *VnodeFind(int fd);
#endif
#endif /* !_VNODE_H_ */
