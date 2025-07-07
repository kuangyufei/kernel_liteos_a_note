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
#include "syscall_pub.h"
/**
 * @brief 检查虚拟内存区域是否有效
 * @param space 虚拟内存空间指针
 * @param ptr 要检查的内存起始地址
 * @param len 要检查的内存长度
 * @return 0 - 内存区域有效，-1 - 内存区域无效
 */
int CheckRegion(const LosVmSpace *space, VADDR_T ptr, size_t len)
{
    LosVmMapRegion *region = LOS_RegionFind((LosVmSpace *)space, ptr);  // 查找包含ptr的内存区域
    if (region == NULL) {                                             // 如果未找到对应的内存区域
        return -1;                                                    // 返回无效
    }
    if (ptr + len <= region->range.base + region->range.size) {       // 检查当前区域是否能容纳整个长度
        return 0;                                                     // 能容纳，返回有效
    }
    // 当前区域不够，递归检查剩余长度
    return CheckRegion(space, region->range.base + region->range.size,
                       (ptr + len) - (region->range.base + region->range.size));
}

/**
 * @brief 复制用户空间内存到内核空间
 * @param ptr 用户空间内存指针
 * @param len 要复制的内存长度
 * @param needCopy 是否需要实际复制数据（1-需要，0-仅分配内存不复制）
 * @return 内核空间内存指针，失败时返回NULL并设置errno
 */
void *DupUserMem(const void *ptr, size_t len, int needCopy)
{
    void *p = LOS_MemAlloc(OS_SYS_MEM_ADDR, len);  // 在内核内存池中分配内存
    if (p == NULL) {                               // 分配失败
        set_errno(ENOMEM);                         // 设置错误码为内存不足
        return NULL;                               // 返回NULL
    }

    // 如果需要复制且从用户空间复制失败
    if (needCopy && LOS_ArchCopyFromUser(p, ptr, len) != 0) {
        LOS_MemFree(OS_SYS_MEM_ADDR, p);           // 释放已分配的内核内存
        set_errno(EFAULT);                         // 设置错误码为内存访问错误
        return NULL;                               // 返回NULL
    }

    return p;                                      // 返回分配/复制成功的内核内存指针
}

/**
 * @brief 获取文件的完整路径
 * @param fd 文件描述符（AT_FDCWD表示当前工作目录）
 * @param path 相对路径或绝对路径
 * @param fullpath 输出参数，用于存储完整路径
 * @return 0 - 成功，非0 - 错误码
 */
int GetFullpath(int fd, const char *path, char **fullpath)
{
    int ret = 0;                                   // 返回值
    char *pathRet = NULL;                          // 临时路径缓冲区
    struct file *file = NULL;                      // 文件结构体指针
    struct stat bufRet = {0};                      // 文件状态结构体

    if (path != NULL) {                            // 如果提供了路径参数
        ret = UserPathCopy(path, &pathRet);        // 复制用户空间路径到内核空间
        if (ret != 0) {                            // 复制失败
            goto OUT;                              // 跳转到清理代码
        }
    }

    if ((pathRet != NULL) && (*pathRet == '/')) {  // 如果路径是绝对路径
        *fullpath = pathRet;                       // 直接将绝对路径作为完整路径
        pathRet = NULL;                            // 避免后续释放
    } else {                                       // 相对路径处理
        if (fd != AT_FDCWD) {                      // 如果不是当前工作目录
            /* 将文件描述符转换为系统全局文件描述符 */
            fd = GetAssociatedSystemFd(fd);
        }
        ret = fs_getfilep(fd, &file);              // 通过文件描述符获取文件结构体
        if (ret < 0) {                             // 获取失败
            ret = -EPERM;                          // 设置错误码为权限不足
            goto OUT;                              // 跳转到清理代码
        }
        if (file) {                                // 如果文件结构体有效
            ret = stat(file->f_path, &bufRet);     // 获取文件状态
            if (!ret) {                            // 获取状态成功
                if (!S_ISDIR(bufRet.st_mode)) {    // 检查是否为目录
                    set_errno(ENOTDIR);            // 设置错误码为不是目录
                    ret = -ENOTDIR;                // 设置返回值
                    goto OUT;                      // 跳转到清理代码
                }
            }
        }
        // 规范化路径，获取完整路径
        ret = vfs_normalize_pathat(fd, pathRet, fullpath);
    }

OUT:
    PointerFree(pathRet);                          // 释放临时路径缓冲区
    return ret;                                    // 返回结果
}

/**
 * @brief 将用户空间路径复制到内核空间并检查长度
 * @param userPath 用户空间路径指针
 * @param pathBuf 输出参数，用于存储内核空间路径
 * @return 0 - 成功，非0 - 错误码
 */
int UserPathCopy(const char *userPath, char **pathBuf)
{
    int ret;

    // 分配PATH_MAX+1大小的缓冲区（包含null终止符）
    *pathBuf = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, PATH_MAX + 1);
    if (*pathBuf == NULL) {                        // 分配失败
        return -ENOMEM;                            // 返回内存不足错误
    }

    // 从用户空间复制路径到内核空间，最多复制PATH_MAX+1字节
    ret = LOS_StrncpyFromUser(*pathBuf, userPath, PATH_MAX + 1);
    if (ret < 0) {                                 // 复制失败
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *pathBuf);  // 释放缓冲区
        *pathBuf = NULL;                           // 清空指针
        return ret;                                // 返回错误码
    } else if (ret > PATH_MAX) {                   // 路径长度超过PATH_MAX
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *pathBuf);  // 释放缓冲区
        *pathBuf = NULL;                           // 清空指针
        return -ENAMETOOLONG;                      // 返回路径过长错误
    }
    (*pathBuf)[ret] = '\0';                        // 添加null终止符

    return 0;                                      // 返回成功
}