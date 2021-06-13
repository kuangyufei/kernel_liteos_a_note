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
#include "fs/vfs_util.h"
#include "fs/path_cache.h"

#define VNODE_FLAG_MOUNT_NEW 1		//新装载点
#define VNODE_FLAG_MOUNT_ORIGIN 2	//原装载点
#define DEV_PATH_LEN 5
/******************************************************************
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
*******************************************************************/
 /*
  * Vnode types.  VNODE_TYPE_UNKNOWN means no type.
  */
enum VnodeType {//节点类型
    VNODE_TYPE_UNKNOWN,       /* unknown type */	//未知类型
    VNODE_TYPE_REG,           /* regular file */	//正则文件(普通文件)
    VNODE_TYPE_DIR,           /* directory */		//目录
    VNODE_TYPE_BLK,           /* block device */	//块设备
    VNODE_TYPE_CHR,           /* char device */		//字符设备
    VNODE_TYPE_BCHR,          /* block char mix device *///块和字符设备混合
    VNODE_TYPE_FIFO,          /* pipe */			//管道文件
    VNODE_TYPE_LNK,           /* link */			//链接,这里的链接指的是上层硬链接概念
};

struct fs_dirent_s;
struct VnodeOps;
struct IATTR;
/*
* linux下有多种权限控制的机制，常见的有：DAC(Discretionary Access Control)自主式权限控制和MAC(Mandatory Access Control)强制访问控制。
* linux 下使用 inode 中文意思是索引节点（index node）,从概念层面鸿蒙 Vnode是对标 inode 
* 这里顺便说一下目录文件的"链接数"。创建目录时，默认会生成两个目录项："."和".."。前者的inode号码就是当前目录的inode号码，
	等同于当前目录的"硬链接"；后者的inode号码就是当前目录的父目录的inode号码，等同于父目录的"硬链接"。
	所以，任何一个目录的"硬链接"总数，总是等于2加上它的子目录总数（含隐藏目录）
*/
struct Vnode {
    enum VnodeType type;                /* vnode type */	//节点类型
    int useCount;                       /* ref count of users *///节点引用(链接)数，即有多少文件名指向这个vnode,即上层理解的硬链接数   
    uint32_t hash;                      /* vnode hash */	//节点哈希值
    uint uid;                           /* uid for dac */	//DAC用户ID
    uint gid;                           /* gid for dac */	//DAC用户组ID
    mode_t mode;                        /* mode for dac */	//DAC的模式
    LIST_HEAD parentPathCaches;         /* pathCaches point to parents */	//指向父级路径缓存,上面的都是当了爸爸节点
    LIST_HEAD childPathCaches;          /* pathCaches point to children */	//指向子级路径缓存,上面都是当了别人儿子的节点
    struct Vnode *parent;               /* parent vnode */	//父节点
    struct VnodeOps *vop;               /* vnode operations */	//以 Vnode 方式访问数据(接口实现|驱动程序)
    struct file_operations_vfs *fop;    /* file operations */	//以 File 方式访问数据(接口实现|驱动程序)
    void *data;                         /* private data */		//私有数据 (drv_data),在 ( register_blockdriver | register_driver )中分配内存
    uint32_t flag;                      /* vnode flag */		//节点标签
    LIST_ENTRY hashEntry;               /* list entry for bucket in hash table */ //通过它挂入哈希表 g_vnodeHashEntrys[i], i:[0,g_vnodeHashMask]
    LIST_ENTRY actFreeEntry;            /* vnode active/free list entry */	//通过本节点挂到空闲链表和使用链表上
    struct Mount *originMount;          /* fs info about this vnode */ //关于这个节点的挂载信息
    struct Mount *newMount;             /* fs info about who mount on this vnode */	//挂载在这个节点上的文件系统
};
/*
	虚拟文件系统接口,具体的文件系统只需实现这些接口函数就完成了鸿蒙系统的接入.
	VnodeOps 系列函数是基于索引节点的操作.
*/
struct VnodeOps {
    int (*Create)(struct Vnode *parent, const char *name, int mode, struct Vnode **vnode);//创建
    int (*Lookup)(struct Vnode *parent, const char *name, int len, struct Vnode **vnode);//查询
    int (*Open)(struct Vnode *vnode, int fd, int mode, int flags);//打开
    int (*Close)(struct Vnode *vnode);//关闭
    int (*Reclaim)(struct Vnode *vnode);//回收
    int (*Unlink)(struct Vnode *parent, struct Vnode *vnode, char *fileName);//取消链接
    int (*Rmdir)(struct Vnode *parent, struct Vnode *vnode, char *dirName);//删除目录
    int (*Mkdir)(struct Vnode *parent, const char *dirName, mode_t mode, struct Vnode **vnode);//创建目录
    int (*Readdir)(struct Vnode *vnode, struct fs_dirent_s *dir);//读目录
    int (*Opendir)(struct Vnode *vnode, struct fs_dirent_s *dir);//打开目录
    int (*Rewinddir)(struct Vnode *vnode, struct fs_dirent_s *dir);//定位目录函数
    int (*Closedir)(struct Vnode *vnode, struct fs_dirent_s *dir);//关闭目录
    int (*Getattr)(struct Vnode *vnode, struct stat *st);//获取节点属性
    int (*Setattr)(struct Vnode *vnode, struct stat *st);//设置节点属性
    int (*Chattr)(struct Vnode *vnode, struct IATTR *attr);//改变节点属性(change attr)
    int (*Rename)(struct Vnode *src, struct Vnode *dstParent, const char *srcName, const char *dstName);//重命名
    int (*Truncate)(struct Vnode *vnode, off_t len);//缩减或扩展大小
    int (*Truncate64)(struct Vnode *vnode, off64_t len);//缩减或扩展大小
    int (*Fscheck)(struct Vnode *vnode, struct fs_dirent_s *dir);//检查功能
};
/*哈希比较指针函数,使用方法,例如:
* int VfsHashGet(const struct Mount *mount, uint32_t hash, struct Vnode **vnode, VfsHashCmp *fn, void *arg)
* VfsHashCmp *fn 等同于 int *fn, 此时 fn是个指针,指向了一个函数地址
* fn(vnode,arg)就是调用这个函数,返回一个int类型的值
*/
typedef int VfsHashCmp(struct Vnode *vnode, void *arg);

int VnodesInit(void);
int VnodeDevInit(void);
int VnodeAlloc(struct VnodeOps *vop, struct Vnode **vnode);
int VnodeFree(struct Vnode *vnode);
int VnodeLookup(const char *path, struct Vnode **vnode, uint32_t flags);
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
int VnodeDestory(struct Vnode *vnode);

#endif /* !_VNODE_H_ */
