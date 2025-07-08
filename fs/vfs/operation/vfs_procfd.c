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
/**
 * @brief 对文件描述符表进行加锁，确保并发访问安全
 * @param fdt 文件描述符表指针
 */
void FileTableLock(struct fd_table_s *fdt)
{
    /* 获取信号量（可能需要等待） */
    while (sem_wait(&fdt->ft_sem) != 0) {  // 循环等待信号量，处理被信号中断的情况
        /*
        * 此处唯一可能的错误是等待被信号唤醒
        */
        LOS_ASSERT(errno == EINTR);  // 断言错误码为EINTR（被信号中断）
    }
}

/**
 * @brief 对文件描述符表进行解锁
 * @param fdt 文件描述符表指针
 */
void FileTableUnLock(struct fd_table_s *fdt)
{
    int ret = sem_post(&fdt->ft_sem);  // 释放信号量
    if (ret == -1) {  // 检查信号量释放是否成功
        PRINTK("sem_post error, errno %d \n", get_errno());  // 输出错误信息
    }
}

/**
 * @brief 分配进程文件描述符
 * @param fdt 文件描述符表指针
 * @param minFd 最小文件描述符值
 * @return 成功返回分配的文件描述符，失败返回VFS_ERROR
 */
static int AssignProcessFd(const struct fd_table_s *fdt, int minFd)
{
    if (minFd >= fdt->max_fds) {  // 检查最小文件描述符是否超出范围
        set_errno(EINVAL);  // 设置错误码为无效参数
        return VFS_ERROR;  // 返回错误
    }

    /* 从表中搜索未使用的文件描述符 */
    for (int i = minFd; i < fdt->max_fds; i++) {  // 从minFd开始遍历文件描述符表
        if (!FD_ISSET(i, fdt->proc_fds)) {  // 检查文件描述符是否未被使用
            return i;  // 返回找到的未使用文件描述符
        }
    }
    set_errno(EMFILE);  // 所有文件描述符均被使用，设置错误码
    return VFS_ERROR;  // 返回错误
}

/**
 * @brief 获取当前进程的文件描述符表
 * @return 成功返回文件描述符表指针，失败返回NULL
 */
struct fd_table_s *GetFdTable(void)
{
    struct fd_table_s *fdt = NULL;  // 文件描述符表指针
    struct files_struct *procFiles = OsCurrProcessGet()->files;  // 获取当前进程的文件结构

    if (procFiles == NULL) {  // 检查文件结构是否为空
        return NULL;  // 返回NULL
    }

    fdt = procFiles->fdt;  // 获取文件描述符表
    if ((fdt == NULL) || (fdt->ft_fds == NULL)) {  // 检查文件描述符表及其数组是否有效
        return NULL;  // 返回NULL
    }

    return fdt;  // 返回文件描述符表指针
}

/**
 * @brief 检查进程文件描述符是否有效
 * @param fdt 文件描述符表指针
 * @param procFd 进程文件描述符
 * @return 有效返回true，否则返回false
 */
static bool IsValidProcessFd(struct fd_table_s *fdt, int procFd)
{
    if (fdt == NULL) {  // 检查文件描述符表是否为空
        return false;  // 返回无效
    }
    if ((procFd < 0) || (procFd >= fdt->max_fds)) {  // 检查文件描述符是否在有效范围内
        return false;  // 返回无效
    }
    return true;  // 返回有效
}

/**
 * @brief 关联进程文件描述符与系统文件描述符
 * @param procFd 进程文件描述符
 * @param sysFd 系统文件描述符
 */
void AssociateSystemFd(int procFd, int sysFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查进程文件描述符是否有效
        return;  // 无效则返回
    }

    if (sysFd < 0) {  // 检查系统文件描述符是否有效
        return;  // 无效则返回
    }

    FileTableLock(fdt);  // 加锁保护
    fdt->ft_fds[procFd].sysFd = sysFd;  // 关联系统文件描述符
    FileTableUnLock(fdt);  // 解锁
}

/**
 * @brief 检查进程文件描述符是否有效
 * @param procFd 进程文件描述符
 * @return 成功返回OK，失败返回VFS_ERROR
 */
int CheckProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查文件描述符是否有效
        return VFS_ERROR;  // 返回错误
    }

    return OK;  // 返回成功
}

/**
 * @brief 获取与进程文件描述符关联的系统文件描述符
 * @param procFd 进程文件描述符
 * @return 成功返回系统文件描述符，失败返回VFS_ERROR
 */
int GetAssociatedSystemFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查文件描述符是否有效
        return VFS_ERROR;  // 返回错误
    }

    FileTableLock(fdt);  // 加锁保护
    if (fdt->ft_fds[procFd].sysFd < 0) {  // 检查是否有关联的系统文件描述符
        FileTableUnLock(fdt);  // 解锁
        return VFS_ERROR;  // 返回错误
    }
    int sysFd = fdt->ft_fds[procFd].sysFd;  // 获取系统文件描述符
    FileTableUnLock(fdt);  // 解锁

    return sysFd;  // 返回系统文件描述符
}

/* 占用进程文件描述符，存在三种情况：
 * 1.procFd已关联，需要解除与相关sysfd的关联
 * 2.procFd未分配，立即占用
 * 3.procFd正在open()、close()、dup()过程中，返回EBUSY
 */
/**
 * @brief 分配指定的进程文件描述符
 * @param procFd 指定的进程文件描述符
 * @return 成功返回OK，失败返回错误码
 */
int AllocSpecifiedProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查文件描述符是否有效
        return -EBADF;  // 返回错误码：无效文件描述符
    }

    FileTableLock(fdt);  // 加锁保护
    if (fdt->ft_fds[procFd].sysFd >= 0) {  // 检查是否已关联系统文件描述符
        /* 解除procFd关联 */
        fdt->ft_fds[procFd].sysFd = -1;  // 重置系统文件描述符
        FileTableUnLock(fdt);  // 解锁
        return OK;  // 返回成功
    }

    if (FD_ISSET(procFd, fdt->proc_fds)) {  // 检查文件描述符是否处于竞争状态
        /* procFd处于竞争状态 */
        FileTableUnLock(fdt);  // 解锁
        return -EBUSY;  // 返回错误码：资源忙
    } else {
        /* 未使用的procFd */
        FD_SET(procFd, fdt->proc_fds);  // 标记文件描述符为已使用
    }

    FileTableUnLock(fdt);  // 解锁
    return OK;  // 返回成功
}

/**
 * @brief 释放进程文件描述符
 * @param procFd 进程文件描述符
 */
void FreeProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查文件描述符是否有效
        return;  // 无效则返回
    }

    FileTableLock(fdt);  // 加锁保护
    FD_CLR(procFd, fdt->proc_fds);  // 清除文件描述符使用标记
    FD_CLR(procFd, fdt->cloexec_fds);  // 清除CLOEXEC标记
    fdt->ft_fds[procFd].sysFd = -1;  // 重置系统文件描述符
    FileTableUnLock(fdt);  // 解锁
}

/**
 * @brief 解除进程文件描述符与系统文件描述符的关联
 * @param procFd 进程文件描述符
 * @return 成功返回系统文件描述符，失败返回VFS_ERROR
 */
int DisassociateProcessFd(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查文件描述符是否有效
        return VFS_ERROR;  // 返回错误
    }

    FileTableLock(fdt);  // 加锁保护
    if (fdt->ft_fds[procFd].sysFd < 0) {  // 检查是否有关联的系统文件描述符
        FileTableUnLock(fdt);  // 解锁
        return VFS_ERROR;  // 返回错误
    }
    int sysFd = fdt->ft_fds[procFd].sysFd;  // 获取系统文件描述符
    if (procFd >= MIN_START_FD) {  // 检查是否为非标准文件描述符（0,1,2为标准输入输出错误）
        fdt->ft_fds[procFd].sysFd = -1;  // 重置系统文件描述符
    }
    FileTableUnLock(fdt);  // 解锁

    return sysFd;  // 返回系统文件描述符
}

/**
 * @brief 分配进程文件描述符（从最小可用开始）
 * @return 成功返回文件描述符，失败返回VFS_ERROR
 */
int AllocProcessFd(void)
{
    return AllocLowestProcessFd(MIN_START_FD);  // 调用分配最低可用文件描述符函数
}

/**
 * @brief 分配最低可用的进程文件描述符
 * @param minFd 最小文件描述符值
 * @return 成功返回文件描述符，失败返回VFS_ERROR
 */
int AllocLowestProcessFd(int minFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (fdt == NULL) {  // 检查文件描述符表是否有效
        return VFS_ERROR;  // 返回错误
    }

    /* minFd应为正数，0,1,2已分配给stdin,stdout,stderr */
    if (minFd < MIN_START_FD) {  // 确保最小文件描述符不小于起始值
        minFd = MIN_START_FD;  // 设置为起始值
    }

    FileTableLock(fdt);  // 加锁保护

    int procFd = AssignProcessFd(fdt, minFd);  // 分配文件描述符
    if (procFd == VFS_ERROR) {  // 检查分配是否成功
        FileTableUnLock(fdt);  // 解锁
        return VFS_ERROR;  // 返回错误
    }

    /* 占用文件描述符集 */
    FD_SET(procFd, fdt->proc_fds);  // 标记文件描述符为已使用
    FileTableUnLock(fdt);  // 解锁

    return procFd;  // 返回分配的文件描述符
}

/**
 * @brief 分配并关联进程文件描述符与系统文件描述符
 * @param sysFd 系统文件描述符
 * @param minFd 最小文件描述符值
 * @return 成功返回进程文件描述符，失败返回VFS_ERROR
 */
int AllocAndAssocProcessFd(int sysFd, int minFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (fdt == NULL) {  // 检查文件描述符表是否有效
        return VFS_ERROR;  // 返回错误
    }

    /* minFd应为正数，0,1,2已分配给stdin,stdout,stderr */
    if (minFd < MIN_START_FD) {  // 确保最小文件描述符不小于起始值
        minFd = MIN_START_FD;  // 设置为起始值
    }

    FileTableLock(fdt);  // 加锁保护

    int procFd = AssignProcessFd(fdt, minFd);  // 分配文件描述符
    if (procFd == VFS_ERROR) {  // 检查分配是否成功
        FileTableUnLock(fdt);  // 解锁
        return VFS_ERROR;  // 返回错误
    }

    /* 占用文件描述符集 */
    FD_SET(procFd, fdt->proc_fds);  // 标记文件描述符为已使用
    fdt->ft_fds[procFd].sysFd = sysFd;  // 关联系统文件描述符
    FileTableUnLock(fdt);  // 解锁

    return procFd;  // 返回进程文件描述符
}

/**
 * @brief 分配并关联系统文件描述符与进程文件描述符
 * @param procFd 进程文件描述符
 * @param minFd 最小系统文件描述符值
 * @return 成功返回系统文件描述符，失败返回VFS_ERROR
 */
int AllocAndAssocSystemFd(int procFd, int minFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查进程文件描述符是否有效
        return VFS_ERROR;  // 返回错误
    }

    int sysFd = alloc_fd(minFd);  // 分配系统文件描述符
    if (sysFd < 0) {  // 检查分配是否成功
        return VFS_ERROR;  // 返回错误
    }

    FileTableLock(fdt);  // 加锁保护
    fdt->ft_fds[procFd].sysFd = sysFd;  // 关联系统文件描述符
    FileTableUnLock(fdt);  // 解锁

    return sysFd;  // 返回系统文件描述符
}

/**
 * @brief 增加文件描述符引用计数
 * @param sysFd 系统文件描述符
 */
static void FdRefer(int sysFd)
{
    if ((sysFd > STDERR_FILENO) && (sysFd < CONFIG_NFILE_DESCRIPTORS)) {  // 检查是否为普通文件描述符
        files_refer(sysFd);  // 增加文件引用计数
    }
#if defined(LOSCFG_NET_LWIP_SACK)  // 如果配置了LWIP SACK
    if ((sysFd >= CONFIG_NFILE_DESCRIPTORS) && (sysFd < (CONFIG_NFILE_DESCRIPTORS + CONFIG_NSOCKET_DESCRIPTORS))) {  // 检查是否为套接字描述符
        socks_refer(sysFd);  // 增加套接字引用计数
    }
#endif
#if defined(LOSCFG_COMPAT_POSIX)  // 如果配置了POSIX兼容
    if ((sysFd >= MQUEUE_FD_OFFSET) && (sysFd < (MQUEUE_FD_OFFSET + CONFIG_NQUEUE_DESCRIPTORS))) {  // 检查是否为消息队列描述符
        MqueueRefer(sysFd);  // 增加消息队列引用计数
    }
#endif
}

/**
 * @brief 关闭系统文件描述符
 * @param sysFd 系统文件描述符
 * @param targetPid 目标进程PID
 */
static void FdClose(int sysFd, unsigned int targetPid)
{
    UINT32 intSave;  // 中断状态保存变量

    if ((sysFd > STDERR_FILENO) && (sysFd < CONFIG_NFILE_DESCRIPTORS)) {  // 检查是否为普通文件描述符
        LosProcessCB *processCB = OS_PCB_FROM_PID(targetPid);  // 获取目标进程控制块
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        if (OsProcessIsInactive(processCB)) {  // 检查进程是否已终止
            SCHEDULER_UNLOCK(intSave);  // 恢复调度器
            return;  // 进程已终止，直接返回
        }
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器

        files_close_internal(sysFd, processCB);  // 关闭文件描述符
    }
#if defined(LOSCFG_NET_LWIP_SACK)  // 如果配置了LWIP SACK
    if ((sysFd >= CONFIG_NFILE_DESCRIPTORS) && (sysFd < (CONFIG_NFILE_DESCRIPTORS + CONFIG_NSOCKET_DESCRIPTORS))) {  // 检查是否为套接字描述符
        socks_close(sysFd);  // 关闭套接字
    }
#endif
#if defined(LOSCFG_COMPAT_POSIX)  // 如果配置了POSIX兼容
    if ((sysFd >= MQUEUE_FD_OFFSET) && (sysFd < (MQUEUE_FD_OFFSET + CONFIG_NQUEUE_DESCRIPTORS))) {  // 检查是否为消息队列描述符
        mq_close((mqd_t)sysFd);  // 关闭消息队列
    }
#endif
}

/**
 * @brief 获取目标进程的文件描述符表
 * @param pid 目标进程PID
 * @param semId 信号量ID指针，用于返回文件描述符表的信号量
 * @return 成功返回文件描述符表指针，失败返回NULL
 */
static struct fd_table_s *GetProcessFTable(unsigned int pid, sem_t *semId)
{
    UINT32 intSave;  // 中断状态保存变量
    struct files_struct *procFiles = NULL;  // 进程文件结构指针
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 获取目标进程控制块

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    if (OsProcessIsInactive(processCB)) {  // 检查进程是否已终止
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        return NULL;  // 返回NULL
    }

    procFiles = processCB->files;  // 获取进程文件结构
    if (procFiles == NULL || procFiles->fdt == NULL) {  // 检查文件结构和文件描述符表是否有效
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        return NULL;  // 返回NULL
    }

    *semId = procFiles->fdt->ft_sem;  // 获取文件描述符表的信号量
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器

    return procFiles->fdt;  // 返回文件描述符表指针
}

/**
 * @brief 将文件描述符复制到目标进程
 * @param fd 源进程文件描述符
 * @param targetPid 目标进程PID
 * @return 成功返回目标进程中的文件描述符，失败返回错误码
 */
int CopyFdToProc(int fd, unsigned int targetPid)
{
#if !defined(LOSCFG_NET_LWIP_SACK) && !defined(LOSCFG_COMPAT_POSIX) && !defined(LOSCFG_FS_VFS)  // 如果未配置网络、POSIX兼容和VFS
    return -ENOSYS;  // 返回不支持的系统调用错误
#else
    int sysFd;  // 系统文件描述符
    struct fd_table_s *fdt = NULL;  // 文件描述符表指针
    int procFd;  // 进程文件描述符
    sem_t semId;  // 信号量ID

    if (OS_PID_CHECK_INVALID(targetPid)) {  // 检查目标PID是否有效
        return -EINVAL;  // 返回无效参数错误
    }

    sysFd = GetAssociatedSystemFd(fd);  // 获取源进程文件描述符关联的系统文件描述符
    if (sysFd < 0) {  // 检查系统文件描述符是否有效
        return -EBADF;  // 返回无效文件描述符错误
    }

    FdRefer(sysFd);  // 增加系统文件描述符引用计数
    fdt = GetProcessFTable(targetPid, &semId);  // 获取目标进程的文件描述符表
    if (fdt == NULL || fdt->ft_fds == NULL) {  // 检查文件描述符表是否有效
        FdClose(sysFd, targetPid);  // 关闭系统文件描述符
        return -EPERM;  // 返回权限错误
    }

    /* 获取信号量（可能需要等待） */
    if (sem_wait(&semId) != 0) {  // 等待目标进程文件描述符表的信号量
        /* 目标进程已改变 */
        FdClose(sysFd, targetPid);  // 关闭系统文件描述符
        return -ESRCH;  // 返回进程不存在错误
    }

    procFd = AssignProcessFd(fdt, 3);  // 分配目标进程文件描述符（最小为3）
    if (procFd < 0) {  // 检查分配是否成功
        if (sem_post(&semId) == -1) {  // 释放信号量
            PRINT_ERR("sem_post error, errno %d \n", get_errno());  // 输出错误信息
        }
        FdClose(sysFd, targetPid);  // 关闭系统文件描述符
        return -EPERM;  // 返回权限错误
    }

    /* 占用文件描述符集 */
    FD_SET(procFd, fdt->proc_fds);  // 标记文件描述符为已使用
    fdt->ft_fds[procFd].sysFd = sysFd;  // 关联系统文件描述符
    if (sem_post(&semId) == -1) {  // 释放信号量
        PRINTK("sem_post error, errno %d \n", get_errno());  // 输出错误信息
    }

    return procFd;  // 返回目标进程文件描述符
#endif
}

/**
 * @brief 关闭目标进程的文件描述符
 * @param procFd 进程文件描述符
 * @param targetPid 目标进程PID
 * @return 成功返回0，失败返回错误码
 */
int CloseProcFd(int procFd, unsigned int targetPid)
{
#if !defined(LOSCFG_NET_LWIP_SACK) && !defined(LOSCFG_COMPAT_POSIX) && !defined(LOSCFG_FS_VFS)  // 如果未配置网络、POSIX兼容和VFS
    return -ENOSYS;  // 返回不支持的系统调用错误
#else
    int sysFd;  // 系统文件描述符
    struct fd_table_s *fdt = NULL;  // 文件描述符表指针
    sem_t semId;  // 信号量ID

    if (OS_PID_CHECK_INVALID(targetPid)) {  // 检查目标PID是否有效
        return -EINVAL;  // 返回无效参数错误
    }

    fdt = GetProcessFTable(targetPid, &semId);  // 获取目标进程的文件描述符表
    if (fdt == NULL || fdt->ft_fds == NULL) {  // 检查文件描述符表是否有效
        return -EPERM;  // 返回权限错误
    }

    /* 获取信号量（可能需要等待） */
    if (sem_wait(&semId) != 0) {  // 等待目标进程文件描述符表的信号量
        /* 目标进程已改变 */
        return -ESRCH;  // 返回进程不存在错误
    }

    if (!IsValidProcessFd(fdt, procFd)) {  // 检查进程文件描述符是否有效
        if (sem_post(&semId) == -1) {  // 释放信号量
            PRINTK("sem_post error, errno %d \n", get_errno());  // 输出错误信息
        }
        return -EPERM;  // 返回权限错误
    }

    sysFd = fdt->ft_fds[procFd].sysFd;  // 获取关联的系统文件描述符
    if (sysFd < 0) {  // 检查系统文件描述符是否有效
        if (sem_post(&semId) == -1) {  // 释放信号量
            PRINTK("sem_post error, errno %d \n", get_errno());  // 输出错误信息
        }
        return -EPERM;  // 返回权限错误
    }

    /* 清除文件描述符集 */
    FD_CLR(procFd, fdt->proc_fds);  // 清除文件描述符使用标记
    FD_CLR(procFd, fdt->cloexec_fds);  // 清除CLOEXEC标记
    fdt->ft_fds[procFd].sysFd = -1;  // 重置系统文件描述符
    if (sem_post(&semId) == -1) {  // 释放信号量
        PRINTK("sem_post error, errno %d \n", get_errno());  // 输出错误信息
    }
    FdClose(sysFd, targetPid);  // 关闭系统文件描述符

    return 0;  // 返回成功
#endif
}
