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

#include "fs_file.h"
#include "los_process_pri.h"
#include "fs/fd_table.h"
#include "fs/file.h"
#include "fs/fs.h"

static void FileTableLock(struct fd_table_s *fdt)//对文件表操作前先拿锁
{
    /* Take the semaphore (perhaps waiting) */
    while (sem_wait(&fdt->ft_sem) != 0) { //拿锁，没有信号量就处于等待状态
        /*
        * The only case that an error should occur here is if the wait was
        * awakened by a signal.
        */ //这里发生错误的唯一情况是等待被信号唤醒。
        LOS_ASSERT(get_errno() == EINTR);//EINTR:系统调用被中断时产生的错误
    }//https://blog.csdn.net/tantion/article/details/86520806
}
//
static void FileTableUnLock(struct fd_table_s *fdt)//对文件表操作完后解锁
{
    int ret = sem_post(&fdt->ft_sem);//发送信号
    if (ret == -1) {
        PRINTK("sem_post error, errno %d \n", get_errno());
    }
}
//分配一个可用的进程fd,从minFd开始遍历
static int AssignProcessFd(const struct fd_table_s *fdt, int minFd)//分配进程fd
{
    if (fdt == NULL) {
        return VFS_ERROR;
    }

    /* search unused fd from table *///从表中搜索未使用的fd
    for (int i = minFd; i < fdt->max_fds; i++) {
        if (!FD_ISSET(i, fdt->proc_fds)) {//i尚未被设置，则返回i
            return i;
        }
    }

    return VFS_ERROR;
}
//获取当前的文件描述表,每个进程都维持一张FD表
static struct fd_table_s *GetFdTable(void)
{
    struct fd_table_s *fdt = NULL;
    struct files_struct *procFiles = OsCurrProcessGet()->files;//获取当前进程的文件管理器

    if (procFiles == NULL) {
        return NULL;
    }

    fdt = procFiles->fdt;//返回这个fdt
    if ((fdt == NULL) || (fdt->ft_fds == NULL)) {
        return NULL;
    }

    return fdt;
}
//是否为一个有效的进程fd
static bool IsValidProcessFd(struct fd_table_s *fdt, int procFd)
{
    if (fdt == NULL) {
        return false;
    }
    if ((procFd < 0) || (procFd >= fdt->max_fds)) {
        return false;
    }
    return true;
}
/****************************************************
关联系统分配的fd,形成<进程FD,系统FD>的绑定关系
这里要说明的是所有进程的文件句柄列表前三个都是一样的,(0,1,2)都默认贡献给了strin,strout,strerr
STDIN_FILENO:	接收键盘的输入
STDOUT_FILENO:	向屏幕输出
STDERR_FILENO:	向屏幕输出错误
fdt->ft_fds[STDIN_FILENO].sysFd = STDIN_FILENO; 
fdt->ft_fds[STDOUT_FILENO].sysFd = STDOUT_FILENO;
fdt->ft_fds[STDERR_FILENO].sysFd = STDERR_FILENO;
http://guoshaoguang.com/blog/tag/stderr_fileno/
****************************************************/
void AssociateSystemFd(int procFd, int sysFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {//先检查参数procFd的有效性
        return;//procFd 就是个索引,通过它找到 sysFd,真正在起作用的是 sysFd
    }

    if (sysFd < 0) {
        return;
    }

    FileTableLock(fdt);//操作临界区，先拿锁
    fdt->ft_fds[procFd].sysFd = sysFd;//形成<进程FD,系统FD>绑定关系
    FileTableUnLock(fdt);
}
//检查参数FD在当前进程的FD表中是否有效
int CheckProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {
        return VFS_ERROR;
    }

    return OK;
}
//获得系统FD的关联,参数为进程文件描述符(FD也叫文件句柄)
int GetAssociatedSystemFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();//获取当前进程的FD表

    if (!IsValidProcessFd(fdt, procFd)) {//参数FD是否在当前进程表中有效
        return VFS_ERROR;
    }

    FileTableLock(fdt);
    if (fdt->ft_fds[procFd].sysFd < 0) {//sysFd 为系统分配给进程的全局唯一FD
        FileTableUnLock(fdt);
        return VFS_ERROR;
    }
    int sysFd = fdt->ft_fds[procFd].sysFd;//sysFd怎么来的? 由 AssociateSystemFd 得到
    FileTableUnLock(fdt);

    return sysFd;
}

/* Occupy the procFd, there are three circumstances:  
 * 1.procFd is already associated, we need disassociate procFd with relevant sysfd. 
 * 2.procFd is not allocated, we occupy it immediately.
 * 3.procFd is in open(), close(), dup() process, we return EBUSY immediately.
 */
int AllocSpecifiedProcessFd(int procFd)//分配指定的procFd
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {//参数检查,主要判断是否在有效范围内
        return -EBADF;
    }

    FileTableLock(fdt);
    if (fdt->ft_fds[procFd].sysFd >= 0) {//1.procFd已经关联，解除procFd与相关sysfd的关联
        /* Disassociate procFd */
        fdt->ft_fds[procFd].sysFd = -1;//重置sysFd
        FileTableUnLock(fdt);
        return OK;
    }

    if (FD_ISSET(procFd, fdt->proc_fds)) {//3. open(), close(), dup()正在使用中,返回busy
        /* procFd in race condition */ //procFd处于竞争状态下
        FileTableUnLock(fdt);
        return -EBUSY;
    } else {//2.procFd没有被分配，我们马上占有它
        /* Unused procFd */
        FD_SET(procFd, fdt->proc_fds);//由此来通知执行了select()的进程哪一socket或文件发生了可读或可写事件
    }

    FileTableUnLock(fdt);
    return OK;
}

void FreeProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {
        return;
    }

    FileTableLock(fdt);
    FD_CLR(procFd, fdt->proc_fds);//清除select()对此的通知,之后procFd又可用于再分配
    fdt->ft_fds[procFd].sysFd = -1;//重置sysFd
    FileTableUnLock(fdt);
}
//解除<系统FD,进程FD>的关联
int DisassociateProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();//获取当前进程的FD表

    if (!IsValidProcessFd(fdt, procFd)) {
        return VFS_ERROR;
    }

    FileTableLock(fdt);
    if (fdt->ft_fds[procFd].sysFd < 0) {//系统FD是否有效,默认为-1,有效时必大于0
        FileTableUnLock(fdt);
        return VFS_ERROR;
    }
    int sysFd = fdt->ft_fds[procFd].sysFd;
    if (procFd >= MIN_START_FD) {//procFd 必大于2 
        fdt->ft_fds[procFd].sysFd = -1; //解除关联关系
    }
    FileTableUnLock(fdt);

    return sysFd;
}
//分配一个进程fd
int AllocProcessFd(void)
{
    return AllocLowestProcessFd(MIN_START_FD);//0,1,2已经分配给了 stdin，stdout，stderr，所以从3开始
}
//分配最小文件句柄,对进程来说可分配的FD范围在 [2,512/512+128]之间
int AllocLowestProcessFd(int minFd)
{
    struct fd_table_s *fdt = GetFdTable();//获取当前进程的文件管理器

    if (fdt == NULL) {
        return VFS_ERROR;
    }
	//minFd应该是一个正数，0,1,2已经被分配到stdin，stdout，stderr
    /* minFd should be a positive number,and 0,1,2 had be distributed to stdin,stdout,stderr */
    if (minFd < MIN_START_FD) {//由进程分配的FD,最小不能小过3
        minFd = MIN_START_FD;
    }

    FileTableLock(fdt);//操作临界区，必须上锁

    int procFd = AssignProcessFd(fdt, minFd);
    if (procFd == VFS_ERROR) {
        FileTableUnLock(fdt);
        return VFS_ERROR;
    }

    // occupy the fd set
    FD_SET(procFd, fdt->proc_fds);//占有这个procFd
    FileTableUnLock(fdt);//释放锁

    return procFd;
}
//两个动作:1.分配一个进程fd 2.进程fd 和 系统fd的关联
int AllocAndAssocProcessFd(int sysFd, int minFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (fdt == NULL) {
        return VFS_ERROR;
    }

    /* minFd should be a positive number,and 0,1,2 had be distributed to stdin,stdout,stderr */
    if (minFd < MIN_START_FD) {
        minFd = MIN_START_FD;
    }

    FileTableLock(fdt);

    int procFd = AssignProcessFd(fdt, minFd);
    if (procFd == VFS_ERROR) {
        FileTableUnLock(fdt);
        return VFS_ERROR;
    }

    // occupy the fd set
    FD_SET(procFd, fdt->proc_fds);
    fdt->ft_fds[procFd].sysFd = sysFd;//关联
    FileTableUnLock(fdt);

    return procFd;
}
//两个动作:1.分配一个进程fd 2.进程fd 和 系统fd的关联
int AllocAndAssocSystemFd(int procFd, int minFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {
        return VFS_ERROR;
    }

    int sysFd = alloc_fd(minFd);//分配一个系统fd
    if (sysFd < 0) {
        return VFS_ERROR;
    }

    FileTableLock(fdt);
    fdt->ft_fds[procFd].sysFd = sysFd;
    FileTableUnLock(fdt);

    return sysFd;
}

