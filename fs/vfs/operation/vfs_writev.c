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
#include "limits.h"
/**
 * @brief 将分散的iovec数据复制到连续缓冲区
 * @param buf 目标连续缓冲区
 * @param totallen 目标缓冲区总长度
 * @param iov iovec结构体数组，包含分散的数据
 * @param iovcnt iovec结构体数量
 * @return 成功复制的字节数，失败返回VFS_ERROR
 */
static int iov_trans_to_buf(char *buf, ssize_t totallen, const struct iovec *iov, int iovcnt)
{
    int i;                                  // 循环计数器，用于遍历iovec数组
    size_t ret, writepart;                  // ret:复制结果，writepart:部分复制的字节数
    size_t bytestowrite;                    // 当前iovec元素需要写入的字节数
    char *writebuf = NULL;                  // 指向当前iovec元素的数据缓冲区
    char *curbuf = buf;                     // 指向目标缓冲区的当前位置

    // 遍历所有iovec元素
    for (i = 0; i < iovcnt; ++i) {
        writebuf = (char *)iov[i].iov_base; // 获取当前iovec元素的数据指针
        bytestowrite = iov[i].iov_len;      // 获取当前iovec元素的数据长度
        if (bytestowrite == 0) {            // 跳过空的iovec元素
            continue;
        }

        if (totallen == 0) {                // 如果目标缓冲区已写满，跳出循环
            break;
        }

        // 计算实际可写入的字节数（不超过剩余缓冲区空间）
        bytestowrite = (totallen < bytestowrite) ? totallen : bytestowrite;
        // 将用户空间数据复制到内核空间缓冲区
        ret = LOS_CopyToKernel(curbuf, bytestowrite, writebuf, bytestowrite);
        if (ret != 0) {                     // 检查复制是否失败
            if (ret == bytestowrite) {      // 完全复制失败
                set_errno(EFAULT);          // 设置错误码为内存访问错误
                return VFS_ERROR;           // 返回错误
            } else {                        // 部分复制失败
                writepart = bytestowrite - ret; // 计算成功复制的字节数
                curbuf += writepart;        // 移动当前缓冲区指针
                break;                      // 跳出循环，不再继续处理
            }
        }
        curbuf += bytestowrite;             // 移动当前缓冲区指针
        totallen -= bytestowrite;           // 减少剩余可写入字节数
    }

    // 返回成功复制的总字节数
    return (int)((intptr_t)curbuf - (intptr_t)buf);
}

/**
 * @brief 向量写入函数，支持分散输入/集中输出
 * @param fd 文件描述符
 * @param iov iovec结构体数组，包含要写入的数据
 * @param iovcnt iovec结构体数量
 * @param offset 文件偏移量指针，为NULL时使用当前文件偏移
 * @return 成功写入的字节数，失败返回VFS_ERROR
 */
ssize_t vfs_writev(int fd, const struct iovec *iov, int iovcnt, off_t *offset)
{
    int i, ret;                             // i:循环计数器，ret:函数返回值
    char *buf = NULL;                       // 临时缓冲区，用于合并分散的iovec数据
    size_t buflen = 0;                      // 所有iovec数据的总长度
    size_t bytestowrite;                    // 实际要写入的字节数
    ssize_t totalbyteswritten;              // 成功写入的总字节数
    size_t totallen;                        // 缓冲区总大小

    // 检查输入参数合法性
    if ((iov == NULL) || (iovcnt > IOV_MAX)) {
        return VFS_ERROR;                   // 参数无效，返回错误
    }

    // 计算所有iovec数据的总长度
    for (i = 0; i < iovcnt; ++i) {
        // 检查是否超过SSIZE_MAX限制
        if (SSIZE_MAX - buflen < iov[i].iov_len) {
            set_errno(EINVAL);              // 设置错误码为无效参数
            return VFS_ERROR;               // 返回错误
        }
        buflen += iov[i].iov_len;           // 累加iovec长度
    }

    if (buflen == 0) {                      // 如果没有数据要写入
        return 0;                           // 返回0，表示成功写入0字节
    }

    totallen = buflen * sizeof(char);       // 计算缓冲区总大小
#ifdef LOSCFG_KERNEL_VM                     // 如果启用了内核虚拟内存
    buf = (char *)LOS_VMalloc(totallen);    // 使用内核虚拟内存分配
#else                                       // 否则
    buf = (char *)malloc(totallen);         // 使用标准内存分配
#endif
    if (buf == NULL) {                      // 检查内存分配是否失败
        return VFS_ERROR;                   // 返回错误
    }

    // 将分散的iovec数据复制到连续缓冲区
    ret = iov_trans_to_buf(buf, totallen, iov, iovcnt);
    if (ret <= 0) {                         // 检查数据复制是否失败
#ifdef LOSCFG_KERNEL_VM                     // 如果启用了内核虚拟内存
        LOS_VFree(buf);                     // 释放内核虚拟内存
#else                                       // 否则
        free(buf);                          // 释放标准内存
#endif
        return VFS_ERROR;                   // 返回错误
    }

    bytestowrite = (ssize_t)ret;            // 获取实际要写入的字节数
    // 根据offset是否为NULL选择write或pwrite
    totalbyteswritten = (offset == NULL) ? write(fd, buf, bytestowrite)
                                         : pwrite(fd, buf, bytestowrite, *offset);
#ifdef LOSCFG_KERNEL_VM                     // 如果启用了内核虚拟内存
    LOS_VFree(buf);                         // 释放内核虚拟内存
#else                                       // 否则
    free(buf);                              // 释放标准内存
#endif
    return totalbyteswritten;               // 返回成功写入的字节数
}

/**
 * @brief 系统调用writev的实现
 * @param fd 文件描述符
 * @param iov iovec结构体数组，包含要写入的数据
 * @param iovcnt iovec结构体数量
 * @return 成功写入的字节数，失败返回VFS_ERROR
 */
ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    // 调用vfs_writev，offset参数为NULL表示使用当前文件偏移
    return vfs_writev(fd, iov, iovcnt, NULL);
}