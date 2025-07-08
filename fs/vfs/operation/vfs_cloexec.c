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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "fs/file.h"
#include "fs/fs_operation.h"
#include "fs/fd_table.h"
#include "unistd.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/
/**
 * @brief   执行exec时关闭设置了CLOEXEC标志的文件描述符
 * @param   files 指向进程文件描述符集合的指针
 * @return  无返回值
 */
void CloseOnExec(struct files_struct *files)
{
    int sysFd;  // 系统文件描述符
    // 参数合法性检查：文件描述符集合或其表为空则直接返回
    if ((files == NULL) || (files->fdt == NULL)) {
        return;
    }

    // 遍历所有可能的文件描述符
    for (int i = 0; i < files->fdt->max_fds; i++) {
        // 检查文件描述符是否有效且设置了CLOEXEC标志
        if (FD_ISSET(i, files->fdt->proc_fds) &&
            FD_ISSET(i, files->fdt->cloexec_fds)) {
            sysFd = DisassociateProcessFd(i);  // 解除进程文件描述符与系统文件描述符的关联
            if (sysFd >= 0) {                 // 检查系统文件描述符是否有效
                close(sysFd);                 // 关闭系统文件描述符
            }

            FreeProcessFd(i);  // 释放进程文件描述符
        }
    }
}

/**
 * @brief   为指定进程文件描述符设置CLOEXEC标志
 * @param   procFd 进程文件描述符
 * @return  无返回值
 */
void SetCloexecFlag(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表
    if (fdt == NULL) {                      // 检查文件描述符表是否获取成功
        return;
    }

    FileTableLock(fdt);                     // 加锁保护文件描述符表操作
    FD_SET(procFd, fdt->cloexec_fds);       // 设置CLOEXEC标志
    FileTableUnLock(fdt);                   // 解锁
    return;
}

/**
 * @brief   检查指定进程文件描述符是否设置了CLOEXEC标志
 * @param   procFd 进程文件描述符
 * @return  设置了返回true，否则返回false
 */
bool CheckCloexecFlag(int procFd)
{
    bool isCloexec = 0;                     // 用于存储检查结果
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表
    if (fdt == NULL) {                      // 检查文件描述符表是否获取成功
        return false;
    }

    FileTableLock(fdt);                     // 加锁保护文件描述符表操作
    isCloexec = FD_ISSET(procFd, fdt->cloexec_fds);  // 检查CLOEXEC标志状态
    FileTableUnLock(fdt);                   // 解锁
    return isCloexec;
}

/**
 * @brief   清除指定进程文件描述符的CLOEXEC标志
 * @param   procFd 进程文件描述符
 * @return  无返回值
 */
void ClearCloexecFlag(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();  // 获取文件描述符表
    if (fdt == NULL) {                      // 检查文件描述符表是否获取成功
        return;
    }

    FileTableLock(fdt);                     // 加锁保护文件描述符表操作
    FD_CLR(procFd, fdt->cloexec_fds);       // 清除CLOEXEC标志
    FileTableUnLock(fdt);                   // 解锁
    return;
}