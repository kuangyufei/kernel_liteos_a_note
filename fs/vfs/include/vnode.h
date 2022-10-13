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

typedef LOS_DL_LIST LIST_HEAD;
typedef LOS_DL_LIST LIST_ENTRY;

#define VNODE_FLAG_MOUNT_NEW      (1 << 0) /* new mount vnode*/
#define VNODE_FLAG_MOUNT_ORIGIN   (1 << 1) /* origin vnode */

#define V_CREATE     (1 << 0)
#define V_DUMMY      (1 << 2)

#ifndef VFS_ERROR
#define VFS_ERROR -1
#endif

#ifndef OK
#define OK 0
#endif

#define AT_REMOVEDIR 0x200

#define DEV_PATH_LEN 5

/* Permission flags */
#define READ_OP                 4
#define WRITE_OP                2
#define EXEC_OP                 1
#define UGO_NUMS                3
#define MODE_IXUGO              0111
#define USER_MODE_SHIFT         6
#define GROUP_MODE_SHIFT        3
#define UMASK_FULL              0777

/* Attribute flags. */
#define CHG_MODE 1
#define CHG_UID 2
#define CHG_GID 4
#define CHG_SIZE 8
#define CHG_ATIME 16
#define CHG_MTIME 32
#define CHG_CTIME 64
/**
 * @brief 此结构用于记录 vnode 的属性
 */
struct IATTR {
  /* This structure is used for record vnode attr. */
  unsigned int attr_chg_valid;  ///< 节点改变有效性 (CHG_MODE | CHG_UID | ... )
  unsigned int attr_chg_flags;  ///< 额外的系统与用户标志（flag），用来保护该文件
  unsigned attr_chg_mode;	  ///< 确定了文件的类型，以及它的所有者、它的group、其它用户访问此文件的权限 (S_IWUSR | ...)
  unsigned attr_chg_uid;	///< 用户ID
  unsigned attr_chg_gid;	///< 组ID
  unsigned attr_chg_size;	///< 节点大小
  unsigned attr_chg_atime;	///< 节点最近访问时间
  unsigned attr_chg_mtime;	///< 节点对应的文件内容被修改时间
  unsigned attr_chg_ctime;	///<节点自身被修改时间
};

/**
 * @brief 
 * @verbatim
Linux系统使用struct inode作为数据结构名称。BSD派生的系统，使用vnode名称，其中v表示“virtual file system”
Linux 链接分两种，一种被称为硬链接（Hard Link），另一种被称为符号链接（Symbolic Link）。默认情况下，ln 命令产生硬链接。

硬连接
	硬连接指通过索引节点来进行连接。在 Linux 的文件系统中，保存在磁盘分区中的文件不管是什么类型都给它分配一个编号，
	称为索引节点号(Inode Index)。在 Linux 中，多个文件名指向同一索引节点是存在的。比如：A 是 B 的硬链接（A 和 B 都是文件名），
	则 A 的目录项中的 inode 节点号与 B 的目录项中的 inode 节点号相同，即一个 inode 节点对应两个不同的文件名，两个文件名指向同一个文件，
	A 和 B 对文件系统来说是完全平等的。删除其中任何一个都不会影响另外一个的访问。
	硬连接的作用是允许一个文件拥有多个有效路径名，这样用户就可以建立硬连接到重要文件，以防止“误删”的功能。其原因如上所述，
	因为对应该目录的索引节点有一个以上的连接。只删除一个连接并不影响索引节点本身和其它的连接，只有当最后一个连接被删除后，
	文件的数据块及目录的连接才会被释放。也就是说，文件真正删除的条件是与之相关的所有硬连接文件均被删除。
	# ln 源文件 目标文件
	
软连接
	另外一种连接称之为符号连接（Symbolic Link），也叫软连接。软链接文件有类似于 Windows 的快捷方式。
	它实际上是一个特殊的文件。在符号连接中，文件实际上是一个文本文件，其中包含的有另一文件的位置信息。
	比如：A 是 B 的软链接（A 和 B 都是文件名），A 的目录项中的 inode 节点号与 B 的目录项中的 inode 节点号不相同，
	A 和 B 指向的是两个不同的 inode，继而指向两块不同的数据块。但是 A 的数据块中存放的只是 B 的路径名（可以根据这个找到 B 的目录项）。
	A 和 B 之间是“主从”关系，如果 B 被删除了，A 仍然存在（因为两个是不同的文件），但指向的是一个无效的链接。
	# ln -s 源文文件或目录 目标文件或目录
	
软链接与硬链接最大的不同：文件A指向文件B的文件名，而不是文件B的inode号码，文件B的inode"链接数"不会因此发生变化。	

inode的特殊作用
由于inode号码与文件名分离，这种机制导致了一些Unix/Linux系统特有的现象。
　　1. 有时，文件名包含特殊字符，无法正常删除。这时，直接删除inode节点，就能起到删除文件的作用。
　　2. 移动文件或重命名文件，只是改变文件名，不影响inode号码。
　　3. 打开一个文件以后，系统就以inode号码来识别这个文件，不再考虑文件名。因此，通常来说，系统无法从inode号码得知文件名。
第3点使得软件更新变得简单，可以在不关闭软件的情况下进行更新，不需要重启。因为系统通过inode号码，识别运行中的文件，不通过文件名。
更新的时候，新版文件以同样的文件名，生成一个新的inode，不会影响到运行中的文件。等到下一次运行这个软件的时候，文件名就自动指向新版文件，
旧版文件的inode则被回收。
  @endverbatim
 */

 /*!
  * Vnode types.  VNODE_TYPE_UNKNOWN means no type. | 节点类型
  */
enum VnodeType {
    VNODE_TYPE_UNKNOWN,       /*! unknown type | 未知类型*/
    VNODE_TYPE_REG,           /*! regular file | 正则文件(普通文件)*/	
    VNODE_TYPE_DIR,           /*! directory | 目录*/		
    VNODE_TYPE_BLK,           /*! block device | 块设备*/	
    VNODE_TYPE_CHR,           /*! char device | 字符设备*/		
    VNODE_TYPE_BCHR,          /*! block char mix device | 块和字符设备混合*/
    VNODE_TYPE_FIFO,          /*! pipe | 管道文件*/			
    VNODE_TYPE_LNK,           /*! link | 链接,这里的链接指的是上层硬链接概念*/
};

struct fs_dirent_s;
struct VnodeOps;
struct IATTR;

/*!
* @brief vnode并不包含文件名,因为 vnode和文件名是 1:N 的关系
  @verbatim
linux下有多种权限控制的机制，常见的有：DAC(Discretionary Access Control)自主式权限控制和MAC(Mandatory Access Control)强制访问控制。
linux 下使用 inode 中文意思是索引节点（index node）,从概念层面鸿蒙 Vnode是对标 inode 
这里顺便说一下目录文件的"链接数"。创建目录时，默认会生成两个目录项："."和".."。前者的inode号码就是当前目录的inode号码，
	等同于当前目录的"硬链接"；后者的inode号码就是当前目录的父目录的inode号码，等同于父目录的"硬链接"。
	所以，任何一个目录的"硬链接"总数，总是等于2加上它的子目录总数（含隐藏目录）
	
由于 vnode 是对所有设备的一个抽象，因此不同类型的设备，他们的操作方法也不一样，
因此 vop ,fop 都是接口, data 因设备不同而不同.

如果底层是磁盘存储，Inode结构会保存到磁盘。当需要时从磁盘读取到内存中进行缓存。
  @endverbatim
*/
struct Vnode {
    enum VnodeType type;                /* vnode type | 节点类型 (文件|目录|链接...)*/	
    int useCount;                       /* ref count of users | 节点引用(链接)数，即有多少文件名指向这个vnode,即上层理解的硬链接数*/   
    uint32_t hash;                      /* vnode hash | 节点哈希值*/	
    uint uid;                           /* uid for dac | 文件拥有者的User ID*/	
    uint gid;                           /* gid for dac | 文件的Group ID*/	
    mode_t mode;                        /* mode for dac | chmod 文件的读、写、执行权限*/	
    LIST_HEAD parentPathCaches;         /* pathCaches point to parents | 指向父级路径缓存,上面的都是当了爸爸节点*/
    LIST_HEAD childPathCaches;          /* pathCaches point to children | 指向子级路径缓存,上面都是当了别人儿子的节点*/	
    struct Vnode *parent;               /* parent vnode | 父节点*/	
    struct VnodeOps *vop;               /* vnode operations | 相当于指定操作Vnode方式 (接口实现|驱动程序)*/	
    struct file_operations_vfs *fop;    /* file operations | 相当于指定文件系统*/	
    void *data;                         /* private data | 文件数据block的位置,指向每种具体设备私有的成员，例如 ( drv_data | nfsnode | ....)*/		
    uint32_t flag;                      /* vnode flag | 节点标签*/		
    LIST_ENTRY hashEntry;               /* list entry for bucket in hash table | 通过它挂入哈希表 g_vnodeHashEntrys[i], i:[0,g_vnodeHashMask]*/ 
    LIST_ENTRY actFreeEntry;            /* vnode active/free list entry | 通过本节点挂到空闲链表和使用链表上*/	
    struct Mount *originMount;          /* fs info about this vnode | 自己所在的文件系统挂载信息*/ 
    struct Mount *newMount;             /* fs info about who mount on this vnode | 其他挂载在这个节点上文件系统信息*/	
    char *filePath;                     /* file path of the vnode */
    struct page_mapping mapping;        /* page mapping of the vnode */
};
/*!
	虚拟节点操作接口,具体的文件系统只需实现这些接口函数来操作vnode.
	VnodeOps 系列函数是对节点本身的操作.
*/
struct VnodeOps {
    int (*Create)(struct Vnode *parent, const char *name, int mode, struct Vnode **vnode);///< 创建节点
    int (*Lookup)(struct Vnode *parent, const char *name, int len, struct Vnode **vnode);///<查询节点
    //Lookup向底层文件系统查找获取inode信息
    int (*Open)(struct Vnode *vnode, int fd, int mode, int flags);///< 打开节点
    ssize_t (*ReadPage)(struct Vnode *vnode, char *buffer, off_t pos);
    ssize_t (*WritePage)(struct Vnode *vnode, char *buffer, off_t pos, size_t buflen);
    int (*Close)(struct Vnode *vnode);///< 关闭节点
    int (*Reclaim)(struct Vnode *vnode);///<回 收节点
    int (*Unlink)(struct Vnode *parent, struct Vnode *vnode, const char *fileName);///< 取消硬链接
    int (*Rmdir)(struct Vnode *parent, struct Vnode *vnode, const char *dirName);///< 删除目录节点
    int (*Mkdir)(struct Vnode *parent, const char *dirName, mode_t mode, struct Vnode **vnode);///< 创建目录节点
    /*!
    创建一个目录时，实际做了3件事：在其“父目录文件”中增加一个条目；分配一个inode；再分配一个存储块，
    用来保存当前被创建目录包含的文件与子目录。被创建的“目录文件”中自动生成两个子目录的条目，名称分别是：“.”和“..”。
    前者与该目录具有相同的inode号码，因此是该目录的一个“硬链接”。后者的inode号码就是该目录的父目录的inode号码。
    所以，任何一个目录的"硬链接"总数，总是等于它的子目录总数（含隐藏目录）加2。即每个“子目录文件”中的“..”条目，
    加上它自身的“目录文件”中的“.”条目，再加上“父目录文件”中的对应该目录的条目。
    */
    int (*Readdir)(struct Vnode *vnode, struct fs_dirent_s *dir);///< 读目录节点
    int (*Opendir)(struct Vnode *vnode, struct fs_dirent_s *dir);///< 打开目录节点
    int (*Rewinddir)(struct Vnode *vnode, struct fs_dirent_s *dir);///< 定位目录节点
    int (*Closedir)(struct Vnode *vnode, struct fs_dirent_s *dir);///< 关闭目录节点
    int (*Getattr)(struct Vnode *vnode, struct stat *st);///< 获取节点属性
    int (*Setattr)(struct Vnode *vnode, struct stat *st);///< 设置节点属性
    int (*Chattr)(struct Vnode *vnode, struct IATTR *attr);///< 改变节点属性(change attr)
    int (*Rename)(struct Vnode *src, struct Vnode *dstParent, const char *srcName, const char *dstName);///< 重命名
    int (*Truncate)(struct Vnode *vnode, off_t len);///< 缩减或扩展大小
    int (*Truncate64)(struct Vnode *vnode, off64_t len);///< 缩减或扩展大小
    int (*Fscheck)(struct Vnode *vnode, struct fs_dirent_s *dir);///< 检查功能
    int (*Link)(struct Vnode *src, struct Vnode *dstParent, struct Vnode **dst, const char *dstName);
    int (*Symlink)(struct Vnode *parentVnode, struct Vnode **newVnode, const char *path, const char *target);
    ssize_t (*Readlink)(struct Vnode *vnode, char *buffer, size_t bufLen);
};
/*! 哈希比较指针函数,使用方法,例如:
* int VfsHashGet(const struct Mount *mount, uint32_t hash, struct Vnode **vnode, VfsHashCmp *fn, void *arg)
* VfsHashCmp *fn 等同于 int *fn, 此时 fn是个指针,指向了一个函数地址
* fn(vnode,arg)就是调用这个函数,返回一个int类型的值
*/
typedef int VfsHashCmp(struct Vnode *vnode, void *arg);

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

#endif /* !_VNODE_H_ */
