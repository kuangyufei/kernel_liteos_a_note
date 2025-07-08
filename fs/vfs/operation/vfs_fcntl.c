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
#include "fs/fd_table.h"
#include "fs/fs_operation.h"
#include "sys/types.h"
#include "sys/uio.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/
/**
 * @brief   复制文件描述符
 * @param   procfd     进程文件描述符
 * @param   leastFd    新文件描述符的最小值
 * @return  成功返回新的文件描述符，失败返回错误码
 */
static int FcntlDupFd(int procfd, int leastFd)
{
    int sysfd = GetAssociatedSystemFd(procfd);  // 获取与进程文件描述符关联的系统文件描述符
    if ((sysfd < 0) || (sysfd >= CONFIG_NFILE_DESCRIPTORS)) {  // 检查系统文件描述符是否有效
        return -EBADF;  // 返回文件描述符错误
    }

    if (CheckProcessFd(leastFd) != OK) {  // 检查最小文件描述符是否合法
        return -EINVAL;  // 返回参数无效错误
    }

    int dupFd = AllocLowestProcessFd(leastFd);  // 分配最小可用的进程文件描述符
    if (dupFd < 0) {  // 检查分配是否成功
        return -EMFILE;  // 返回打开文件过多错误
    }

    files_refer(sysfd);  // 增加系统文件描述符的引用计数
    AssociateSystemFd(dupFd, sysfd);  // 将新的进程文件描述符与系统文件描述符关联

    return dupFd;  // 返回新的文件描述符
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
/**
 * @brief   文件控制操作
 * @param   procfd     进程文件描述符
 * @param   cmd        控制命令
 * @param   ...        可变参数，根据命令不同而不同
 * @return  成功返回0或相应值，失败返回错误码
 */
int VfsFcntl(int procfd, int cmd, ...)
{
    va_list ap;  // 可变参数列表
    int ret = 0;  // 返回值

    va_start(ap, cmd);  // 初始化可变参数
    switch (cmd) {  // 根据命令类型处理
        case F_DUPFD:  // 复制文件描述符命令
            {
                int arg = va_arg(ap, int);  // 获取最小文件描述符参数
                ret = FcntlDupFd(procfd, arg);  // 调用复制文件描述符函数
            }
            break;
        case F_GETFD:  // 获取文件描述符标志命令
            {
                bool isCloexec = CheckCloexecFlag(procfd);  // 检查CLOEXEC标志
                ret = isCloexec ? FD_CLOEXEC : 0;  // 返回标志状态
            }
            break;
        case F_SETFD:  // 设置文件描述符标志命令
            {
                int oflags = va_arg(ap, int);  // 获取标志参数
                if (oflags & FD_CLOEXEC) {  // 检查是否设置CLOEXEC标志
                    SetCloexecFlag(procfd);  // 设置CLOEXEC标志
                } else {
                    ClearCloexecFlag(procfd);  // 清除CLOEXEC标志
                }
            }
            break;
        default:  // 不支持的命令
            ret = CONTINE_NUTTX_FCNTL;  // 继续执行NUTTX的fcntl处理
            break;
    }

    va_end(ap);  // 结束可变参数处理
    return ret;  // 返回结果
}