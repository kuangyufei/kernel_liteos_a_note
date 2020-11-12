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

#ifndef __INCLUDE_FS_FDTABLE_H
#define __INCLUDE_FS_FDTABLE_H

#include <limits.h>
#include <sys/select.h>
#include <semaphore.h>
#include "linux/spinlock.h"


/****************************************************************************

fd 文件描述符（file descriptor）为了高效管理已被打开的文件所创建的索引。本质上就是一个非负整数
当应用程序请求内核打开/新建一个文件时，内核会返回一个文件描述符用于对应这个打开/新建的文件，
读写文件也是需要使用这个文件描述符来指定待读写的文件的。
在鸿蒙系统中，所有的文件操作，都是通过fd来定位资源和状态的

****************************************************************************/

/* open file table for process fd */
struct file_table_s {
    signed short sysFd; /* system fd associate with the tg_filelist index */ //与tg_filelist索引关联的系统fd
};

struct fd_table_s {//fd表结构体
    unsigned int max_fds;
    struct file_table_s *ft_fds; /* process fd array associate with system fd *///进程的FD数组，与系统fd相关联
    fd_set *open_fds;	//打开的fds 默认打开了 0,1,2	       (stdin,stdout,stderr)
    fd_set *proc_fds;	//处理的fds 默认打开了 0,1,2	       (stdin,stdout,stderr)
    sem_t ft_sem; /* manage access to the file table */ //管理对文件表的访问
};
//files_struct 一般用于进程 process->files 
struct files_struct {//进程文件表结构体
    int count;				//持有的文件数量
    struct fd_table_s *fdt; //持有的文件表
    unsigned int file_lock;	//文件互斥锁
    unsigned int next_fd;	//下一个fd
#ifdef VFS_USING_WORKDIR
    spinlock_t workdir_lock;	//工作区目录自旋锁
    char workdir[PATH_MAX];		//工作区路径
#endif
};

typedef struct ProcessCB LosProcessCB;
//以下函数见于 ..\third_party\NuttX\fs\inode\fs_files.c
void files_refer(int fd);

struct files_struct *dup_fd(struct files_struct *oldf);

struct files_struct *alloc_files(void);//返回分配好的files_struct，其中包含fd总数，stdin,stdout,stderr等信息 stdin也是一个文件

void delete_files(LosProcessCB *processCB, struct files_struct *files);//删除进程的文件管理器

struct files_struct *create_files_snapshot(const struct files_struct *oldf);//创建文件管理器快照，所谓快照就是一份拷贝

void delete_files_snapshot(struct files_struct *files);//删除文件管理器快照

int alloc_fd(int minfd);//分配一个fd，当然是从files_struct->fdt 数组中拿一个未被使用(分配)的fd，默认都是 -1

#endif
