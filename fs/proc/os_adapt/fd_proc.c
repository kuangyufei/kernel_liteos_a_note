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

#include "proc_fs.h"

#include <stdbool.h>
#include "fs/file.h"
#include "vfs_config.h"
#include "internal.h"

#include "fs/fd_table.h"
#include "los_process.h"
#include "capability_api.h"
#include "capability_type.h"


/*
 * Template: Pid    Fd  [SysFd ] Name
 */
/**
 * @brief 填充文件描述符信息到序列缓冲区
 * @param seqBuf 序列缓冲区指针，用于存储格式化后的文件描述符信息
 * @param fileList 文件列表指针，包含系统中打开的文件信息
 * @param pid 进程ID，指定要查询的进程
 * @param hasPrivilege 是否有特权标志，决定输出信息的详细程度
 */
static void FillFdInfo(struct SeqBuf *seqBuf, struct filelist *fileList, unsigned int pid, bool hasPrivilege)
{
    int fd;                  // 进程级文件描述符
    int sysFd;               // 系统级文件描述符
    char *name = NULL;       // 文件路径或类型名称
    struct file *filp = NULL;// 文件结构体指针

    // 获取指定进程的文件描述符表
    struct fd_table_s *fdt = LOS_GetFdTable(pid);
    // 检查文件描述符表是否有效
    if ((fdt == NULL) || (fdt->proc_fds == NULL)) {
        return;  // 文件描述符表无效，直接返回
    }

    // 等待文件描述符表信号量，确保线程安全
    (void)sem_wait(&fdt->ft_sem);

    // 遍历进程所有可能的文件描述符（从最小起始值到最大值）
    for (fd = MIN_START_FD; fd < fdt->max_fds; fd++) {
        // 检查当前文件描述符是否被设置（打开状态）
        if (FD_ISSET(fd, fdt->proc_fds)) {
            sysFd = fdt->ft_fds[fd].sysFd;  // 获取对应的系统级文件描述符

            // 根据系统文件描述符范围判断文件类型
            if (sysFd < CONFIG_NFILE_DESCRIPTORS) {
                // 普通文件描述符：获取文件路径
                filp = &fileList->fl_files[sysFd];
                name = filp->f_path;
            } else if (sysFd < (CONFIG_NFILE_DESCRIPTORS + CONFIG_NSOCKET_DESCRIPTORS)) {
                name = "(socks)";          // 套接字类型
            } else if (sysFd < (FD_SETSIZE + CONFIG_NTIME_DESCRIPTORS)) {
                name = "(timer)";          // 定时器类型
            } else if (sysFd < (MQUEUE_FD_OFFSET + CONFIG_NQUEUE_DESCRIPTORS)) {
                name = "(mqueue)";         // 消息队列类型
            } else if (sysFd < (EPOLL_FD_OFFSET + CONFIG_EPOLL_DESCRIPTORS)) {
                name = "(epoll)";          // epoll类型
            } else {
                name = "(unknown)";        // 未知类型
            }

            // 根据权限输出不同详细程度的信息
            if (hasPrivilege) {
                // 特权用户：输出PID、进程FD、系统FD、引用计数和文件名称
                (void)LosBufPrintf(seqBuf, "%u\t%d\t%6d <%d>\t%s\n", pid, fd, sysFd, filp ? filp->f_refcount : 1, name);
            } else {
                // 普通用户：仅输出PID、进程FD和文件名称
                (void)LosBufPrintf(seqBuf, "%u\t%d\t%s\n", pid, fd, name);
            }
        }
    }

    // 释放文件描述符表信号量
    (void)sem_post(&fdt->ft_sem);
}

/**
 * @brief 填充/proc/fd文件内容的回调函数
 * @param seqBuf 序列缓冲区指针，用于存储文件内容
 * @param v 未使用的参数
 * @return 0表示成功，非0表示错误码
 */
static int FdProcFill(struct SeqBuf *seqBuf, void *v)
{
    int pidNum;               // 进程ID数量
    bool hasPrivilege;        // 是否有读取权限标志
    unsigned int pidMaxNum;   // 最大进程ID数量
    unsigned int *pidList = NULL;  // 进程ID列表指针

    /* 特权用户检查 */
    if (IsCapPermit(CAP_DAC_READ_SEARCH)) {
        // 特权用户：获取系统最大进程数并分配PID列表内存
        pidMaxNum = LOS_GetSystemProcessMaximum();
        pidList = (unsigned int *)malloc(pidMaxNum * sizeof(unsigned int));
        if (pidList == NULL) {
            return -ENOMEM;  // 内存分配失败，返回内存不足错误
        }
        // 获取所有已使用的进程ID列表
        pidNum = LOS_GetUsedPIDList(pidList, pidMaxNum);
        hasPrivilege = true;
        // 输出特权用户表头（包含详细信息列）
        (void)LosBufPrintf(seqBuf, "%s\t%s\t%6s %s\t%s\n", "Pid", "Fd", "SysFd", "<ref>", "Name");
    } else {
        // 普通用户：仅获取当前进程ID
        pidNum = 1;
        pidList = (unsigned int *)malloc(pidNum * sizeof(unsigned int));
        if (pidList == NULL) {
            return -ENOMEM;  // 内存分配失败，返回内存不足错误
        }
        pidList[0] = LOS_GetCurrProcessID();  // 获取当前进程ID
        hasPrivilege = false;
        // 输出普通用户表头（仅基础信息列）
        (void)LosBufPrintf(seqBuf, "Pid\tFd\tName\n");
    }

    // 获取全局文件列表并加锁
    struct filelist *fileList = &tg_filelist;
    (void)sem_wait(&fileList->fl_sem);

    // 遍历所有进程ID，填充文件描述符信息
    for (int i = 0; i < pidNum; i++) {
        FillFdInfo(seqBuf, fileList, pidList[i], hasPrivilege);
    }

    // 释放资源
    free(pidList);
    (void)sem_post(&fileList->fl_sem);  // 释放文件列表信号量
    return 0;                           // 成功完成填充
}

// /proc/fd文件的操作函数集，仅实现读操作
static const struct ProcFileOperations FD_PROC_FOPS = {
    .read       = FdProcFill,  // 读取回调函数，指向FdProcFill
};

/**
 * @brief 初始化/proc/fd节点
 * @details 创建/proc文件系统中的fd节点，并关联操作函数
 */
void ProcFdInit(void)
{
    // 创建/proc/fd节点，权限为0
    struct ProcDirEntry *pde = CreateProcEntry("fd", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("create /proc/fd error.\n");  // 创建失败，输出错误信息
        return;
    }

    pde->procFileOps = &FD_PROC_FOPS;  // 关联fd节点的操作函数集
}
