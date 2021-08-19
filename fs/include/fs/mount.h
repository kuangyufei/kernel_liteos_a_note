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

#ifndef _MOUNT_H_
#define _MOUNT_H_

#include <sys/stat.h>
#include <sys/statfs.h>
#include "vnode.h"

#define MS_RDONLY 1
#define MS_NOSYNC 2

struct MountOps;
/****************************************************************** 
将一个设备（通常是存储设备）挂接到一个已存在的目录上,操作系统将所有的设备都看作文件，
它将整个计算机的资源都整合成一个大的文件目录。我们要访问存储设备中的文件，必须将文件所在的分区挂载到一个已存在的目录上，
然后通过访问这个目录来访问存储设备。挂载就是把设备放在一个目录下，让系统知道怎么管理这个设备里的文件，
了解这个存储设备的可读写特性之类的过程。

linux下面所有的文件、目录、设备都有一个路径，这个路径永远以/开头，用/分隔，如果一个路径是另一个路径的前缀，
则这两个路径有逻辑上的父子关系。但是并不是所有逻辑上的父子关系都必须要是同一个设备，决定不同路径对应到哪个设备的机制就叫做mount（挂载）。
通过mount，可以设置当前的路径与设备的对应关系。每个设备会设置一个挂载点，挂载点是一个空目录。一般来说必须有一个设备挂载在/这个根路径下面，
叫做rootfs。其他挂载点可以是/tmp，/boot，/dev等等，通过在rootfs上面创建一个空目录然后用mount命令就可以将设备挂载到这个目录上。挂载之后，
这个目录下的子路径，就会映射到被挂载的设备里面。当访问一个路径时，会选择一个能最大匹配当前路径前缀的挂载点。
比如说，有/var的挂载点，也有/var/run的挂载点的情况下，访问/var/run/test.pid，就会匹配到/var/run挂载点设备下面的/test.pid。
同一个设备可以有多个挂载点，同一个挂载点同时只能加载一个设备。访问非挂载点的路径的时候，按照前面所说，其实是访问最接近的一个挂载点，
如果没有其他挂载点那么就是rootfs上的目录或者文件了。

https://www.zhihu.com/question/266907637/answer/315386532
*******************************************************************/
//举例: mount /dev/mmcblk0p0 /bin1/vs/sd vfat 将/dev/mmcblk0p0 挂载到/bin1/vs/sd目录
//注意: 同时可以 mount /dev/mmcblk0p0 /home/vs vfat 也就是说一个文件系统可以有多个挂载点
struct Mount {
    LIST_ENTRY mountList;              /* mount list */			 //通过本节点将Mount挂到全局Mount链表上
    const struct MountOps *ops;        /* operations of mount */ //挂载操作函数	
    struct Vnode *vnodeBeCovered;      /* vnode we mounted on */ //要被挂载的节点 即 /bin1/vs/sd 对应的 vnode节点
    struct Vnode *vnodeCovered;        /* syncer vnode */		 //要挂载的节点	即/dev/mmcblk0p0 对应的 vnode节点
    struct Vnode *vnodeDev;            /* dev vnode */
    LIST_HEAD vnodeList;               /* list of vnodes */		//链表表头
    int vnodeSize;                     /* size of vnode list */	//节点数量
    LIST_HEAD activeVnodeList;         /* list of active vnodes */	//激活的节点链表
    int activeVnodeSize;               /* szie of active vnodes list *///激活的节点数量
    void *data;                        /* private data */	//私有数据,可使用这个成员作为一个指向它们自己内部数据的指针
    uint32_t hashseed;                 /* Random seed for vfs hash */ //vfs 哈希随机种子
    unsigned long mountFlags;          /* Flags for mount */	//挂载标签
    char pathName[PATH_MAX];           /* path name of mount point */	//挂载点路径名称  /bin1/vs/sd
    char devName[PATH_MAX];            /* path name of dev point */		//设备名称 /dev/mmcblk0p0
};
//挂载操作
struct MountOps {
    int (*Mount)(struct Mount *mount, struct Vnode *vnode, const void *data);//挂载
    int (*Unmount)(struct Mount *mount, struct Vnode **blkdriver);//卸载
    int (*Statfs)(struct Mount *mount, struct statfs *sbp);//统计文件系统的信息，如该文件系统类型、总大小、可用大小等信息
};

typedef int (*foreach_mountpoint_t)(const char *devpoint,
                                    const char *mountpoint,
                                    struct statfs *statbuf,
                                    void *arg);
struct Mount* MountAlloc(struct Vnode* vnode, struct MountOps* mop);
LIST_HEAD* GetMountList(void);
int foreach_mountpoint(foreach_mountpoint_t handler, void *arg);
int ForceUmountDev(struct Vnode *dev);
#endif
