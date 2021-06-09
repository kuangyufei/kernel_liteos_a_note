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

#include "los_mux.h"
#include "fs/vfs_util.h"
#include "fs/vnode.h"
#include <sys/stat.h>
#include <limits.h>

struct MountOps;
/* 将一个设备（通常是存储设备）挂接到一个已存在的目录上,操作系统将所有的设备都看作文件，
* 它将整个计算机的资源都整合成一个大的文件目录。我们要访问存储设备中的文件，必须将文件所在的分区挂载到一个已存在的目录上，
* 然后通过访问这个目录来访问存储设备。挂载就是把设备放在一个目录下，让系统知道怎么管理这个设备里的文件，
* 了解这个存储设备的可读写特性之类的过程。
*/
struct Mount {//装载
    LIST_ENTRY mountList;              /* mount list */			 //通过本节点将Mount挂到全局Mount链表上
    const struct MountOps *ops;        /* operations of mount */ //装载操作函数	
    struct Vnode *vnodeBeCovered;      /* vnode we mounted on */ //设备装载点
    struct Vnode *vnodeCovered;        /* syncer vnode */		//暂时不知作何用途	
    LIST_HEAD vnodeList;               /* list of vnodes */		//链表表头
    int vnodeSize;                     /* size of vnode list */	//节点数量
    LIST_HEAD activeVnodeList;         /* list of active vnodes */	//激活的节点链表
    int activeVnodeSize;               /* szie of active vnodes list *///激活的节点数量
    void *data;                        /* private data */	//私有数据
    uint32_t hashseed;                 /* Random seed for vfs hash */ //vfs 哈希随机种子
    unsigned long mountFlags;          /* Flags for mount */	//装载标签
    char pathName[PATH_MAX];           /* path name of mount point */	//装载点路径名称
};
//装载操作
struct MountOps {
    int (*Mount)(struct Mount *mount, struct Vnode *vnode, const void *data);//装载
    int (*Unmount)(struct Mount *mount, struct Vnode **blkdriver);//卸载
    int (*Statfs)(struct Mount *mount, struct statfs *sbp);//状态
};

struct Mount* MountAlloc(struct Vnode* vnode, struct MountOps* mop);
LIST_HEAD* GetMountList(void);
#endif
