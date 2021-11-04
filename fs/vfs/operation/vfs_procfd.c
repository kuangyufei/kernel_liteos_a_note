/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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

#include "fs/file.h"
#include "los_process_pri.h"
#include "fs/fd_table.h"
#include "mqueue.h"
#ifdef LOSCFG_NET_LWIP_SACK
#include "lwip/sockets.h"
#endif
//对进程文件表操作上锁
void FileTableLock(struct fd_table_s *fdt)
{
    /* Take the semaphore (perhaps waiting) */
    while (sem_wait(&fdt->ft_sem) != 0) {
        /*
        * The only case that an error should occur here is if the wait was
        * awakened by a signal.
        */
        LOS_ASSERT(errno == EINTR);
    }
}
///对进程文件表操作解锁
void FileTableUnLock(struct fd_table_s *fdt)
{
    int ret = sem_post(&fdt->ft_sem);
    if (ret == -1) {
        PRINTK("sem_post error, errno %d \n", get_errno());
    }
}
///分配进程描述符
static int AssignProcessFd(const struct fd_table_s *fdt, int minFd)
{
    if (minFd >= fdt->max_fds) {
        set_errno(EINVAL);
        return VFS_ERROR;
    }
	//从表中搜索未使用的 fd
    /* search unused fd from table */
    for (int i = minFd; i < fdt->max_fds; i++) {
        if (!FD_ISSET(i, fdt->proc_fds)) {
            return i;
        }
    }
    set_errno(EMFILE);
    return VFS_ERROR;
}
///获取进程文件描述符表
struct fd_table_s *GetFdTable(void)
{
    struct fd_table_s *fdt = NULL;
    struct files_struct *procFiles = OsCurrProcessGet()->files;//当前进程文件管理器

    if (procFiles == NULL) {
        return NULL;
    }

    fdt = procFiles->fdt;//进程文件表
    if ((fdt == NULL) || (fdt->ft_fds == NULL)) {
        return NULL;
    }

    return fdt;
}

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
///参数进程FD和参数系统FD进行绑定(关联)
void AssociateSystemFd(int procFd, int sysFd)
{
    struct fd_table_s *fdt = GetFdTable();//获取当前进程FD表

    if (!IsValidProcessFd(fdt, procFd)) {
        return;
    }

    if (sysFd < 0) {
        return;
    }

    FileTableLock(fdt);
    fdt->ft_fds[procFd].sysFd = sysFd;//绑定
    FileTableUnLock(fdt);
}

int CheckProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {
        return VFS_ERROR;
    }

    return OK;
}
///获取绑定的系统描述符
int GetAssociatedSystemFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {
        return VFS_ERROR;
    }

    FileTableLock(fdt);
    if (fdt->ft_fds[procFd].sysFd < 0) {
        FileTableUnLock(fdt);
        return VFS_ERROR;
    }
    int sysFd = fdt->ft_fds[procFd].sysFd;//进程FD捆绑系统FD
    FileTableUnLock(fdt);

    return sysFd;
}

/* Occupy the procFd, there are three circumstances:
 * 1.procFd is already associated, we need disassociate procFd with relevant sysfd.
 * 2.procFd is not allocated, we occupy it immediately.
 * 3.procFd is in open(), close(), dup() process, we return EBUSY immediately.
 */
/* 占用procFd，有三种情况：
* 1.procFd 已经关联，我们需要将 procFd 与相关的 sysfd 解除关联。
* 2.procFd 未分配，我们立即占用。
* 3.procFd在open()、close()、dup()过程中，我们立即返回EBUSY。
*/
int AllocSpecifiedProcessFd(int procFd)//分配指定的进程Fd
{
    struct fd_table_s *fdt = GetFdTable();//获取进程FD表

    if (!IsValidProcessFd(fdt, procFd)) {
        return -EBADF;
    }

    FileTableLock(fdt);
    if (fdt->ft_fds[procFd].sysFd >= 0) {//第一种情况
        /* Disassociate procFd */
        fdt->ft_fds[procFd].sysFd = -1;//解除关联
        FileTableUnLock(fdt);
        return OK;
    }

    if (FD_ISSET(procFd, fdt->proc_fds)) {//还在使用中
        /* procFd in race condition */
        FileTableUnLock(fdt);
        return -EBUSY;
    } else {//未分配情况
        /* Unused procFd */
        FD_SET(procFd, fdt->proc_fds);//立即占用
    }

    FileTableUnLock(fdt);
    return OK;
}
///释放进程文件描述符
void FreeProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {
        return;
    }

    FileTableLock(fdt);
    FD_CLR(procFd, fdt->proc_fds);	//相应位清0
    FD_CLR(procFd, fdt->cloexec_fds);
    fdt->ft_fds[procFd].sysFd = -1;	//解绑系统文件描述符
    FileTableUnLock(fdt);
}
///解绑系统文件描述符,返回系统文件描述符
int DisassociateProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (!IsValidProcessFd(fdt, procFd)) {
        return VFS_ERROR;
    }

    FileTableLock(fdt);
    if (fdt->ft_fds[procFd].sysFd < 0) {//无系统文件描述符
        FileTableUnLock(fdt);
        return VFS_ERROR;//解绑失败
    }
    int sysFd = fdt->ft_fds[procFd].sysFd;//存在绑定关系
    if (procFd >= MIN_START_FD) {//必须大于2
        fdt->ft_fds[procFd].sysFd = -1;//解绑
    }
    FileTableUnLock(fdt);

    return sysFd;
}
///分配文件描述符
int AllocProcessFd(void)
{
    return AllocLowestProcessFd(MIN_START_FD);
}
///分配文件描述符,从3号开始
int AllocLowestProcessFd(int minFd)
{
    struct fd_table_s *fdt = GetFdTable();

    if (fdt == NULL) {
        return VFS_ERROR;
    }
	//minFd 应该是一个正数，并且 0,1,2 已经分配给 stdin,stdout,stderr
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

    /* occupy the fd set */
    FD_SET(procFd, fdt->proc_fds);//占用该进程文件描述符
    FileTableUnLock(fdt);

    return procFd;
}
///分配和绑定进程描述符
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

    /* occupy the fd set */
    FD_SET(procFd, fdt->proc_fds);
    fdt->ft_fds[procFd].sysFd = sysFd;
    FileTableUnLock(fdt);

    return procFd;
}
///分配和绑定系统描述符
int AllocAndAssocSystemFd(int procFd, int minFd)
{
    struct fd_table_s *fdt = GetFdTable();//获取当前进程文件表

    if (!IsValidProcessFd(fdt, procFd)) {
        return VFS_ERROR;
    }

    int sysFd = alloc_fd(minFd);//1.分配一个系统描述符,从全局文件描述符 tg_filelist 表中分配
    if (sysFd < 0) {
        return VFS_ERROR;
    }

    FileTableLock(fdt);
    fdt->ft_fds[procFd].sysFd = sysFd;//2.将进程描述符和系统描述符绑定
    FileTableUnLock(fdt);

    return sysFd;
}
///进程FD引用数改变
static void FdRefer(int sysFd)
{
    if ((sysFd > STDERR_FILENO) && (sysFd < CONFIG_NFILE_DESCRIPTORS)) {
        files_refer(sysFd);//增加系统FD引用次数
    }
#if defined(LOSCFG_NET_LWIP_SACK)
    if ((sysFd >= CONFIG_NFILE_DESCRIPTORS) && (sysFd < (CONFIG_NFILE_DESCRIPTORS + CONFIG_NSOCKET_DESCRIPTORS))) {
        socks_refer(sysFd);//增加socket引用次数
    }
#endif
#if defined(LOSCFG_COMPAT_POSIX)
    if ((sysFd >= MQUEUE_FD_OFFSET) && (sysFd < (MQUEUE_FD_OFFSET + CONFIG_NQUEUE_DESCRIPTORS))) {
        MqueueRefer(sysFd);
    }
#endif
}
///关闭FD
static void FdClose(int sysFd, unsigned int targetPid)
{
    UINT32 intSave;

    if ((sysFd > STDERR_FILENO) && (sysFd < CONFIG_NFILE_DESCRIPTORS)) {
        LosProcessCB *processCB = OS_PCB_FROM_PID(targetPid);//获取目标进程
        SCHEDULER_LOCK(intSave);
        if (OsProcessIsInactive(processCB)) {
            SCHEDULER_UNLOCK(intSave);
            return;
        }
        SCHEDULER_UNLOCK(intSave);

        files_close_internal(sysFd, processCB);//减少文件引用数(进程和系统的两个引用数)
    }
#if defined(LOSCFG_NET_LWIP_SACK)
    if ((sysFd >= CONFIG_NFILE_DESCRIPTORS) && (sysFd < (CONFIG_NFILE_DESCRIPTORS + CONFIG_NSOCKET_DESCRIPTORS))) {
        socks_close(sysFd);//减少sockert引用数
    }
#endif
#if defined(LOSCFG_COMPAT_POSIX)
    if ((sysFd >= MQUEUE_FD_OFFSET) && (sysFd < (MQUEUE_FD_OFFSET + CONFIG_NQUEUE_DESCRIPTORS))) {
        mq_close((mqd_t)sysFd);//减少mqpersonal引用数
    }
#endif
}
///获取参数进程FD表
static struct fd_table_s *GetProcessFTable(unsigned int pid, sem_t *semId)
{
    UINT32 intSave;
    struct files_struct *procFiles = NULL;
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);

    SCHEDULER_LOCK(intSave);
    if (OsProcessIsInactive(processCB)) {//参数进程必须处于激活状态
        SCHEDULER_UNLOCK(intSave);
        return NULL;
    }

    procFiles = processCB->files;
    if (procFiles == NULL || procFiles->fdt == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return NULL;
    }

    *semId = procFiles->fdt->ft_sem;
    SCHEDULER_UNLOCK(intSave);

    return procFiles->fdt;
}
///拷贝一个进程FD给指定的进程
int CopyFdToProc(int fd, unsigned int targetPid)
{
#if !defined(LOSCFG_NET_LWIP_SACK) && !defined(LOSCFG_COMPAT_POSIX) && !defined(LOSCFG_FS_VFS)
    return -ENOSYS;
#else
    int sysFd;
    struct fd_table_s *fdt = NULL;
    int procFd;
    sem_t semId;

    if (OS_PID_CHECK_INVALID(targetPid)) {
        return -EINVAL;
    }

    sysFd = GetAssociatedSystemFd(fd);//找到当前进程FD绑定的系统FD
    if (sysFd < 0) {
        return -EBADF;
    }

    FdRefer(sysFd);//引用数要增加了.
    fdt = GetProcessFTable(targetPid, &semId);//获取目标进程的FD表
    if (fdt == NULL || fdt->ft_fds == NULL) {
        FdClose(sysFd, targetPid);
        return -EPERM;
    }

    /* Take the semaphore (perhaps waiting) */
    if (sem_wait(&semId) != 0) {
        /* Target process changed */
        FdClose(sysFd, targetPid);
        return -ESRCH;
    }

    procFd = AssignProcessFd(fdt, 3);//从目标进程FD表中分配一个FD出来,注意这个FD编号不一定和当前进程的编号相同,但他们都将绑定在同一个系统FD上
    if (procFd < 0) {
        if (sem_post(&semId) == -1) {
            PRINT_ERR("sem_post error, errno %d \n", get_errno());
        }
        FdClose(sysFd, targetPid);
        return -EPERM;
    }

    /* occupy the fd set */
    FD_SET(procFd, fdt->proc_fds);//申请到了等啥呀,赶紧占用这个FD
    fdt->ft_fds[procFd].sysFd = sysFd;//绑定,这句话代表的意思是有两个进程的FD都帮到同一个系统FD上
    if (sem_post(&semId) == -1) {
        PRINTK("sem_post error, errno %d \n", get_errno());
    }

    return procFd;
#endif
}
///关闭进程FD
int CloseProcFd(int procFd, unsigned int targetPid)
{
#if !defined(LOSCFG_NET_LWIP_SACK) && !defined(LOSCFG_COMPAT_POSIX) && !defined(LOSCFG_FS_VFS)
    return -ENOSYS;
#else
    int sysFd;
    struct fd_table_s *fdt = NULL;
    sem_t semId;

    if (OS_PID_CHECK_INVALID(targetPid)) {
        return -EINVAL;
    }

    fdt = GetProcessFTable(targetPid, &semId);//获取进程文件描述表
    if (fdt == NULL || fdt->ft_fds == NULL) {
        return -EPERM;
    }

    /* Take the semaphore (perhaps waiting) */
    if (sem_wait(&semId) != 0) {
        /* Target process changed */
        return -ESRCH;
    }

    if (!IsValidProcessFd(fdt, procFd)) {
        if (sem_post(&semId) == -1) {
            PRINTK("sem_post error, errno %d \n", get_errno());
        }
        return -EPERM;
    }

    sysFd = fdt->ft_fds[procFd].sysFd;//获取参数进程描述符绑定的系统文件描述符
    if (sysFd < 0) {
        if (sem_post(&semId) == -1) {
            PRINTK("sem_post error, errno %d \n", get_errno());
        }
        return -EPERM;
    }

    /* clean the fd set */
    FD_CLR(procFd, fdt->proc_fds);//进程FD重置
    FD_CLR(procFd, fdt->cloexec_fds);
    fdt->ft_fds[procFd].sysFd = -1;//解绑
    if (sem_post(&semId) == -1) {
        PRINTK("sem_post error, errno %d \n", get_errno());
    }
    FdClose(sysFd, targetPid);//注意这个操作只是让对应的引用数量减少

    return 0;
#endif
}
