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
fd:文件描述符（file descriptor）也叫文件句柄.
为了高效管理已被打开的文件所创建的索引,是一个非负整数,本质上一个凭证.凭证上岗.
fd只是个索引, 顺藤摸瓜, fd -> file -> inode -> 块/字符驱动程序

当应用程序请求内核打开/新建一个文件时，内核会返回一个文件描述符用于对应这个打开/新建的文件，
读写文件也是需要使用这个文件描述符来指定待读写的文件的。
鸿蒙和LINUX一样,一切皆为文件,系统中，所有的文件操作，都是通过fd来定位资源和状态的
fd有两个好处, 1.安全,对用户透明,不需要知道具体怎么实现的,凭编号拿结果 2.方便扩展
-----------------------------------------------------------------------------

int select(int n, fd_set *restrict rfds, fd_set *restrict wfds, fd_set *restrict efds, struct timeval *restrict tv)
见于..\third_party\musl\src\select\select.c	
https://blog.csdn.net/starflame/article/details/7860091

理解select模型的关键在于理解fd_set，为说明方便，取fd_set长度为1字节，fd_set中的每一bit可以对应一个文件描述符fd。
则1字节长的fd_set最大可以对应8个fd。

typedef struct fd_set
{
  unsigned char fd_bits [(FD_SETSIZE+7)/8];//用一位来表示一个FD
} fd_set;

fd_set:
select()机制中提供一fd_set的数据结构，实际上是一long类型的数组，每一个数组元素都能与一打开的文件句柄
（不管是socket句柄，还是其他文件或命名管道或设备句柄）建立联系，建立联系的工作由程序员完成，当调用select()时，
由内核根据IO状态修改fd_set的内容，由此来通知执行了select()的进程哪一socket或文件发生了可读或可写事件。
****************************************************************************/

/* open file table for process fd */
struct file_table_s {
    signed short sysFd; /* system fd associate with the tg_filelist index */ //系统分配的fd,系统全局FD表由tg_filelist维护
};//sysFd的默认值是-1

struct fd_table_s {//fd表结构体
    unsigned int max_fds;//一个进程能打开的最大文件数量, 一般是 512 + 128 个,512指普通文件, 128指网络文件
    struct file_table_s *ft_fds; /* process fd array associate with system fd *///系统分配给进程的FD数组 ,fd 默认是 -1
    fd_set *open_fds;	//打开的fds 默认打开了 0,1,2	       (stdin,stdout,stderr)
    fd_set *proc_fds;	//处理的fds 默认打开了 0,1,2	       (stdin,stdout,stderr)
    sem_t ft_sem; /* manage access to the file table */ //管理对文件表的访问的信号量
};
//files_struct 为 进程 process->files 字段,包含一个进程的所有和VFS相关的内容 
struct files_struct {//进程文件表结构体
    int count;				//持有的文件数量
    struct fd_table_s *fdt; //持有的文件表
    unsigned int file_lock;	//文件互斥锁
    unsigned int next_fd;	//下一个fd
#ifdef VFS_USING_WORKDIR
    spinlock_t workdir_lock;	//工作区目录自旋锁
    char workdir[PATH_MAX];		//工作区路径,最大 256个字符
#endif
};

typedef struct ProcessCB LosProcessCB;
//以下函数见于 ..\third_party\NuttX\fs\inode\fs_files.c
void files_refer(int fd);

struct files_struct *dup_fd(struct files_struct *oldf);

struct files_struct *alloc_files(void);//为进程分配文件管理器，其中包含fd总数，(0,1,2)默认给了stdin,stdout,stderr

void delete_files(LosProcessCB *processCB, struct files_struct *files);//删除参数进程的文件管理器

struct files_struct *create_files_snapshot(const struct files_struct *oldf);//创建文件管理器快照，所谓快照就是一份拷贝

void delete_files_snapshot(struct files_struct *files);//删除文件管理器快照

int alloc_fd(int minfd);//分配一个系统fd，从全局tg_filelist中拿sysFd
#endif
