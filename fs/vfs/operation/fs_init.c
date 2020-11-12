/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "los_printf.h"
#include "fs/fs.h"
#include "inode/inode.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/statfs.h"
#include "linux/spinlock.h"
#include "disk_pri.h"
/************************************************************************
VFS是Virtual File System（虚拟文件系统）的缩写，它不是一个实际的文件系统，而是一个异构文件系统之上的软件粘合层
虚拟文件系统初始化   los_vfs_init()只能调用一次，多次调用将会造成文件系统异常

inode->i_mode 的标签如下:

#define S_IFMT  0170000 //文件类型位域掩码
#define S_IFDIR 0040000 //目录
#define S_IFCHR 0020000	//字符设备
#define S_IFBLK 0060000	//块设备
#define S_IFREG 0100000	//普通文件
#define S_IFIFO 0010000	//FIFO
#define S_IFLNK 0120000 //符号链接
#define S_IFSOCK 0140000 //套接字

#define S_ISUID 04000 	//设置用户ID
#define S_ISGID 02000 	//设置组ID
#define S_ISVTX 01000	//粘住位 此扩展功能的应用在/tmp目录下有所体现，我们可以在该目录下任意创建自己的文件，但就是不能删除或更名其他用户的文件
#define S_IRUSR 0400	//所有者有读权限
#define S_IWUSR 0200	//所有者有写权限
#define S_IXUSR 0100	//所有者有执行权限
#define S_IRWXU 0700	//所有者有读，写，执行权限
#define S_IRGRP 0040	//组有读权限
#define S_IWGRP 0020	//组有写权限
#define S_IXGRP 0010	//组有执行权限
#define S_IRWXG 0070	//组有读，写，执行权限
#define S_IROTH 0004	//其他有读权限
#define S_IWOTH 0002	//其他有写权限
#define S_IXOTH 0001	//其他有执行权限
#define S_IRWXO 0007	//其他有读，写，执行权限

例如：chmod 777 (S_IRWXU S_IRWXG S_IRWXO)
************************************************************************/
void los_vfs_init(void)
{
    int err;
    uint retval;
    static bool g_vfs_init = false;
    struct inode *dev = NULL;

    if (g_vfs_init) {
        return;
    }

    spin_lock_init(&g_diskSpinlock);
    spin_lock_init(&g_diskFatBlockSpinlock); //此处连拿两把锁
    files_initlist(&tg_filelist);//主要初始化列表访问互斥体
    fs_initialize();//初始inode、文件和VFS数据结构
    if ((err = inode_reserve("/", &g_root_inode)) < 0) {//为伪文件系统保留一个（初始化的）索引节点。 "/"为根节点
        PRINT_ERR("los_vfs_init failed error %d\n", -err);
        return;
    }
    g_root_inode->i_mode |= S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    if ((err = inode_reserve("/dev", &dev)) < 0) {//为伪文件系统保留一个（初始化的）索引节点。 "/dev"
        PRINT_ERR("los_vfs_init failed error %d\n", -err);
        return;
    }
    dev->i_mode |= S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    retval = init_file_mapping();//初始化文件映射
    if (retval != LOS_OK) {
        PRINT_ERR("Page cache file map init failed\n");
        return;
    }

    g_vfs_init = true;
}
