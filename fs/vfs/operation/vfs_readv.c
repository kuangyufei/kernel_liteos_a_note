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

#include "sys/types.h"
#include "sys/uio.h"
#include "unistd.h"
#include "string.h"
#include "stdlib.h"
#include "fs/file.h"
#include "user_copy.h"
#include "stdio.h"
#include "limits.h"
/**
 * @brief 预读缓冲区并进行合法性检查
 * @param fd 文件描述符
 * @param iov iovec结构体数组，包含多个缓冲区的地址和长度
 * @param iovcnt iovec结构体数组元素个数
 * @param totalbytesread 用于返回总读取字节数的指针
 * @param offset 读取偏移量指针（NULL表示使用当前文件指针）
 * @return 成功返回分配的缓冲区指针，失败返回NULL
 */
static char *pread_buf_and_check(int fd, const struct iovec *iov, int iovcnt, ssize_t *totalbytesread, off_t *offset)
{
    char *buf = NULL;          // 临时缓冲区指针
    size_t buflen = 0;         // 缓冲区总长度
    int i;                     // 循环计数器

    if ((iov == NULL) || (iovcnt > IOV_MAX)) {  // 检查输入参数合法性
        *totalbytesread = VFS_ERROR;  // 设置错误返回值
        return NULL;  // 返回NULL
    }

    for (i = 0; i < iovcnt; ++i) {  // 计算所有缓冲区总长度
        if (SSIZE_MAX - buflen < iov[i].iov_len) {  // 检查是否溢出
            set_errno(EINVAL);  // 设置无效参数错误
            return NULL;  // 返回NULL
        }
        buflen += iov[i].iov_len;  // 累加缓冲区长度
    }

    if (buflen == 0) {  // 检查总长度是否为0
        *totalbytesread = 0;  // 返回0字节读取
        return NULL;  // 返回NULL
    }

#ifdef LOSCFG_KERNEL_VM  // 如果配置了内核虚拟内存
    buf = (char *)LOS_VMalloc(buflen * sizeof(char));  // 使用内核内存分配
#else
    buf = (char *)malloc(buflen * sizeof(char));  // 使用标准内存分配
#endif
    if (buf == NULL) {  // 检查内存分配是否成功
        set_errno(ENOMEM);  // 设置内存不足错误
        *totalbytesread = VFS_ERROR;  // 设置错误返回值
        return buf;  // 返回NULL
    }

    *totalbytesread = (offset == NULL) ? read(fd, buf, buflen)
                                       : pread(fd, buf, buflen, *offset);
    if ((*totalbytesread == VFS_ERROR) || (*totalbytesread == 0)) {  // 检查读取结果
#ifdef LOSCFG_KERNEL_VM
        LOS_VFree(buf);  // 释放内核内存
#else
        free(buf);  // 释放标准内存
#endif
        return NULL;  // 返回NULL
    }

    return buf;  // 返回成功分配的缓冲区
}

/**
 * @brief 实现分散读取功能（核心函数）
 * @param fd 文件描述符
 * @param iov iovec结构体数组，包含多个缓冲区的地址和长度
 * @param iovcnt iovec结构体数组元素个数
 * @param offset 读取偏移量指针（NULL表示使用当前文件指针）
 * @return 成功返回读取的总字节数，失败返回VFS_ERROR
 */
ssize_t vfs_readv(int fd, const struct iovec *iov, int iovcnt, off_t *offset)
{
    int i;                      // 循环计数器
    int ret;                    // 系统调用返回值
    char *buf = NULL;           // 临时缓冲区指针
    char *curbuf = NULL;        // 当前缓冲区位置指针
    ssize_t bytestoread;        // 当前iov元素需要读取的字节数
    ssize_t totalbytesread = 0; // 总读取字节数
    ssize_t bytesleft;          // 剩余未复制的字节数

    // 调用辅助函数预读数据并检查
    buf = pread_buf_and_check(fd, iov, iovcnt, &totalbytesread, offset);
    if (buf == NULL) {  // 检查缓冲区是否有效
        return totalbytesread;  // 返回读取结果或错误码
    }

    curbuf = buf;  // 初始化当前缓冲区指针
    bytesleft = totalbytesread;  // 初始化剩余字节数
    for (i = 0; i < iovcnt; ++i) {  // 遍历所有iov缓冲区
        bytestoread = iov[i].iov_len;  // 获取当前缓冲区长度
        if (bytestoread == 0) {  // 跳过空缓冲区
            continue;
        }

        if (bytesleft <= bytestoread) {  // 剩余字节不足当前缓冲区
            // 复制剩余所有字节
            ret = LOS_CopyFromKernel(iov[i].iov_base, bytesleft, curbuf, bytesleft);
            bytesleft = ret;  // 更新剩余字节数
            goto out;  // 跳出循环
        }

        // 复制当前缓冲区所需的全部字节
        ret = LOS_CopyFromKernel(iov[i].iov_base, bytestoread, curbuf, bytestoread);
        if (ret != 0) {  // 检查复制是否成功
            bytesleft = bytesleft - (bytestoread - ret);  // 计算实际复制字节数
            goto out;  // 跳出循环
        }
        bytesleft -= bytestoread;  // 更新剩余字节数
        curbuf += bytestoread;  // 移动当前缓冲区指针
    }

out:  // 释放资源并返回结果
#ifdef LOSCFG_KERNEL_VM
    LOS_VFree(buf);  // 释放内核内存
#else
    free(buf);  // 释放标准内存
#endif
    if ((i == 0) && (ret == iov[i].iov_len)) {  // 检查首次复制是否失败
        /* 第一个iovec复制失败，且复制了0字节 */
        set_errno(EFAULT);  // 设置内存访问错误
        return VFS_ERROR;  // 返回错误
    }

    return totalbytesread - bytesleft;  // 返回实际复制的字节数
}

/**
 * @brief 系统调用readv的实现（不指定偏移量）
 * @param fd 文件描述符
 * @param iov iovec结构体数组，包含多个缓冲区的地址和长度
 * @param iovcnt iovec结构体数组元素个数
 * @return 成功返回读取的总字节数，失败返回-1并设置errno
 */
ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    return vfs_readv(fd, iov, iovcnt, NULL);  // 调用vfs_readv，偏移量为NULL
}