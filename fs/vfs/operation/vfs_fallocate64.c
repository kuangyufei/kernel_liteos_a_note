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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include "fs/file.h"
#include "sched.h"
#include "sys/types.h"
#include "unistd.h"
#include "vfs_config.h"

/****************************************************************************
 * Name: file_fallocate
 ****************************************************************************/
/**
 * @brief   64位文件预分配辅助函数
 * @param   filep 文件结构体指针
 * @param   mode  操作模式（仅支持FALLOC_FL_KEEP_SIZE）
 * @param   offset 64位文件内偏移量
 * @param   len    64位要分配的长度
 * @return  成功返回0，失败返回VFS_ERROR并设置errno
 */
ssize_t file_fallocate64(struct file *filep, int mode, off64_t offset, off64_t len)
{
    int ret;   // 用于存储函数调用返回值
    int err;   // 用于存储错误码

    // 检查分配长度是否合法（必须大于0）
    if (len <= 0) {
        err = EINVAL;             // 设置无效参数错误
        goto errout;              // 跳转到错误处理
    }

    /* Was this file opened for write access? */

    // 检查文件是否以可写模式打开
    if (((unsigned int)(filep->f_oflags) & O_ACCMODE) == O_RDONLY) {
        err = EACCES;             // 设置权限拒绝错误
        goto errout;              // 跳转到错误处理
    }

    /* Is a driver registered? Does it support the fallocate method? */
    // 检查文件操作集是否存在且包含fallocate64方法
    if (!filep->ops || !filep->ops->fallocate64) {
        err = EBADF;              // 设置错误的文件描述符错误
        goto errout;              // 跳转到错误处理
    }

    /* Yes, then let the driver perform the fallocate */

    // 调用具体文件系统的fallocate64实现进行空间预分配
    ret = filep->ops->fallocate64(filep, mode, offset, len);
    if (ret < 0) {                // 检查预分配是否成功
        err = -ret;               // 将返回的负错误码转换为正数
        goto errout;              // 跳转到错误处理
    }

    return ret;                   // 预分配成功，返回0

errout:                         // 错误处理标签
    set_errno(err);              // 设置errno为对应的错误码
    return VFS_ERROR;             // 返回VFS错误
}

/**
 * @brief   64位文件空间预分配函数
 * @param   fd     文件描述符
 * @param   mode   操作模式（仅支持FALLOC_FL_KEEP_SIZE）
 * @param   offset 64位文件内偏移量
 * @param   len    64位要分配的长度
 * @return  成功返回0，失败返回-1并设置errno
 * @note    该函数为文件预分配连续的数据区域，支持64位大文件操作
 */
int fallocate64(int fd, int mode, off64_t offset, off64_t len)
{
#if CONFIG_NFILE_DESCRIPTORS > 0
    struct file *filep = NULL;    // 文件结构体指针
#endif

    /* Did we get a valid file descriptor? */

    // 检查文件描述符是否在有效范围内
#if CONFIG_NFILE_DESCRIPTORS > 0
    if ((unsigned int)fd >= CONFIG_NFILE_DESCRIPTORS)
#endif
    {
        set_errno(EBADF);         // 设置错误的文件描述符错误
        return VFS_ERROR;         // 返回VFS错误
    }

#if CONFIG_NFILE_DESCRIPTORS > 0

    /* The descriptor is in the right range to be a file descriptor... write
     * to the file.
     */

    // 通过文件描述符获取文件结构体指针
    int ret = fs_getfilep(fd, &filep);
    if (ret < 0) {                // 检查获取是否成功
        /* The errno value has already been set */
        return VFS_ERROR;         // 返回VFS错误
    }

    // 检查文件是否为目录（目录不支持预分配操作）
    if ((unsigned int)filep->f_oflags & O_DIRECTORY) {
        set_errno(EBADF);         // 设置错误的文件描述符错误
        return VFS_ERROR;         // 返回VFS错误
    }

    /* Perform the fallocate operation using the file descriptor as an index */
    // 调用64位辅助函数执行实际的预分配操作
    return file_fallocate64(filep, mode, offset, len);
#endif
}