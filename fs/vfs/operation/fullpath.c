/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "limits.h"
#include "los_process_pri.h"
#include "fs/fd_table.h"
#include "fs/file.h"

#ifdef LOSCFG_SHELL
#include "shell.h"
#endif

#ifdef LOSCFG_SHELL
#define TEMP_PATH_MAX (PATH_MAX + SHOW_MAX_LEN)  // 当启用Shell时，临时路径最大长度 = 标准路径最大长度 + 显示最大长度
#else
#define TEMP_PATH_MAX  PATH_MAX  // 未启用Shell时，临时路径最大长度 = 标准路径最大长度
#endif

/**
 * @brief 带最大长度限制的字符串长度计算函数
 * @param str 待计算长度的字符串
 * @param maxlen 最大允许长度
 * @return 字符串实际长度（不超过maxlen）
 */
static unsigned int vfs_strnlen(const char *str, size_t maxlen)
{
    const char *p = NULL;  // 字符串遍历指针

    for (p = str; ((maxlen-- != 0) && (*p != '\0')); ++p) {}  // 遍历字符串直到结束符或达到最大长度

    return p - str;  // 返回字符串长度
}

/* 移除路径中多余的'/'，只保留一个 */

/**
 * @brief 清理路径中的连续斜杠
 * @param path 输入输出参数，待清理的路径字符串
 * @return 清理后的路径指针（与输入指针相同）
 */
static char *str_path(char *path)
{
    char *dest = path;  // 目标字符串指针
    char *src = path;   // 源字符串指针

    while (*src != '\0') {  // 遍历源字符串
        if (*src == '/') {  // 遇到斜杠时
            *dest++ = *src++;  // 复制当前斜杠
            while (*src == '/') {  // 跳过后续连续斜杠
                src++;
            }
            continue;
        }
        *dest++ = *src++;  // 复制非斜杠字符
    }
    *dest = '\0';  // 添加字符串结束符
    return path;  // 返回清理后的路径
}

/**
 * @brief 移除路径末尾的斜杠
 * @param dest 路径字符串末尾指针
 * @param fullpath 完整路径字符串起始指针
 */
static void str_remove_path_end_slash(char *dest, const char *fullpath)
{
    if ((*dest == '.') && (*(dest - 1) == '/')) {  // 处理"./"情况
        *dest = '\0';  // 截断字符串
        dest--;  // 调整指针
    }
    if ((dest != fullpath) && (*dest == '/')) {  // 如果路径非根目录且以斜杠结尾
        *dest = '\0';  // 移除末尾斜杠
    }
}

/**
 * @brief 规范化路径字符串（处理.和..）
 * @param fullpath 输入输出参数，待规范化的路径
 * @return 规范化后的路径末尾指针
 */
static char *str_normalize_path(char *fullpath)
{
    char *dest = fullpath;  // 目标字符串指针
    char *src = fullpath;   // 源字符串指针

    /* 2: 路径字符位置：/ 和结束符/0 */

    while (*src != '\0') {  // 遍历源字符串
        if (*src == '.') {  // 遇到.字符
            if (*(src + 1) == '/') {  // 处理"./"情况
                src += 2;  // 跳过"./"
                continue;
            } else if (*(src + 1) == '.') {  // 遇到".."情况
                if ((*(src + 2) == '/') || (*(src + 2) == '\0')) {  // 验证是否为"../"或".."
                    src += 2;  // 跳过".."
                } else {  // 不是有效的".."序列，视为普通文件名
                    while ((*src != '\0') && (*src != '/')) {  // 复制整个文件名
                        *dest++ = *src++;
                    }
                    continue;
                }
            } else {  // 单个"."且后面不是斜杠，视为普通字符
                *dest++ = *src++;
                continue;
            }
        } else {  // 非.字符，直接复制
            *dest++ = *src++;
            continue;
        }

        if ((dest - 1) != fullpath) {  // 不是路径起始位置
            dest--;  // 回退目标指针
        }

        while ((dest > fullpath) && (*(dest - 1) != '/')) {  // 回退到上一个斜杠
            dest--;
        }

        if (*src == '/') {  // 跳过斜杠
            src++;
        }
    }

    *dest = '\0';  // 添加字符串结束符

    /* 移除路径末尾的斜杠（如果存在） */

    dest--;  // 移动到最后一个字符

    str_remove_path_end_slash(dest, fullpath);  // 调用移除末尾斜杠函数
    return dest;  // 返回规范化后的路径末尾指针
}

/**
 * @brief 路径规范化参数检查
 * @param filename 待检查的文件名
 * @param pathname 输出参数，规范化后的路径指针
 * @return 成功返回文件名长度，失败返回错误码
 */
static int vfs_normalize_path_parame_check(const char *filename, char **pathname)
{
    int namelen;  // 文件名长度
    char *name = NULL;  // 文件名指针

    if (pathname == NULL) {  // 检查输出参数有效性
        return -EINVAL;  // 返回无效参数错误
    }

    /* 检查参数 */

    if (filename == NULL) {  // 检查文件名是否为空
        *pathname = NULL;  // 清空输出路径
        return -EINVAL;  // 返回无效参数错误
    }

    namelen = vfs_strnlen(filename, PATH_MAX);  // 获取文件名长度
    if (!namelen) {  // 空文件名
        *pathname = NULL;  // 清空输出路径
        return -EINVAL;  // 返回无效参数错误
    } else if (namelen >= PATH_MAX) {  // 文件名过长
        *pathname = NULL;  // 清空输出路径
        return -ENAMETOOLONG;  // 返回文件名过长错误
    }

    // 检查路径中每个分量是否超过最大长度
    for (name = (char *)filename + namelen; ((name != filename) && (*name != '/')); name--) {
        if (strlen(name) > NAME_MAX) {  // 单个路径分量过长
            *pathname = NULL;  // 清空输出路径
            return -ENAMETOOLONG;  // 返回文件名过长错误
        }
    }

    return namelen;  // 返回文件名长度
}

/**
 * @brief 处理相对路径（拼接目录和文件名）
 * @param directory 目录路径
 * @param filename 文件名
 * @param pathname 输出参数，拼接后的路径指针
 * @param namelen 文件名长度
 * @return 成功返回拼接后的完整路径，失败返回NULL
 */
static char *vfs_not_absolute_path(const char *directory, const char *filename, char **pathname, int namelen)
{
    int ret;  // 函数返回值
    char *fullpath = NULL;  // 完整路径指针

    /* 2: 路径字符位置：/ 和结束符/0 */

    if ((namelen > 1) && (filename[0] == '.') && (filename[1] == '/')) {  // 处理"./"开头的相对路径
        filename += 2;  // 跳过"./"
    }

    // 分配内存：目录长度 + 文件名长度 + 2（斜杠和结束符）
    fullpath = (char *)malloc(strlen(directory) + namelen + 2);
    if (fullpath == NULL) {  // 内存分配失败
        *pathname = NULL;  // 清空输出路径
        set_errno(ENOMEM);  // 设置内存不足错误
        return (char *)NULL;  // 返回NULL
    }

    /* 拼接路径和文件名 */

    // 格式化字符串，拼接目录和文件名
    ret = snprintf_s(fullpath, strlen(directory) + namelen + 2, strlen(directory) + namelen + 1,
                     "%s/%s", directory, filename);
    if (ret < 0) {  // 格式化失败
        *pathname = NULL;  // 清空输出路径
        free(fullpath);  // 释放已分配内存
        set_errno(ENAMETOOLONG);  // 设置文件名过长错误
        return (char *)NULL;  // 返回NULL
    }

    return fullpath;  // 返回拼接后的完整路径
}

/**
 * @brief 规范化完整路径（区分绝对路径和相对路径）
 * @param directory 目录路径（相对路径时使用）
 * @param filename 文件名
 * @param pathname 输出参数，规范化后的路径指针
 * @param namelen 文件名长度
 * @return 成功返回规范化后的路径，失败返回NULL
 */
static char *vfs_normalize_fullpath(const char *directory, const char *filename, char **pathname, int namelen)
{
    char *fullpath = NULL;  // 完整路径指针

    if (filename[0] != '/') {  // 不是绝对路径
        /* 处理相对路径 */

        fullpath = vfs_not_absolute_path(directory, filename, pathname, namelen);  // 拼接路径
        if (fullpath == NULL) {  // 拼接失败
            return (char *)NULL;  // 返回NULL
        }
    } else {  // 是绝对路径
        /* 直接使用绝对路径 */

        fullpath = strdup(filename); /* 复制字符串 */
        if (fullpath == NULL) {  // 内存分配失败
            *pathname = NULL;  // 清空输出路径
            set_errno(ENOMEM);  // 设置内存不足错误
            return (char *)NULL;  // 返回NULL
        }
        if (filename[1] == '/') {  // 检查是否有连续斜杠（如"//"开头）
            *pathname = NULL;  // 清空输出路径
            free(fullpath);  // 释放已分配内存
            set_errno(EINVAL);  // 设置无效参数错误
            return (char *)NULL;  // 返回NULL
        }
    }

    return fullpath;  // 返回完整路径
}

/**
 * @brief 路径规范化主函数
 * @param directory 基础目录（相对路径时使用，NULL表示使用当前工作目录）
 * @param filename 待规范化的文件名
 * @param pathname 输出参数，规范化后的完整路径
 * @return 成功返回ENOERR，失败返回错误码
 */
int vfs_normalize_path(const char *directory, const char *filename, char **pathname)
{
    char *fullpath = NULL;  // 完整路径指针
    int namelen;  // 文件名长度
#ifdef VFS_USING_WORKDIR
    UINTPTR lock_flags;  // 自旋锁标志
    LosProcessCB *curr = OsCurrProcessGet();  // 当前进程控制块
    BOOL dir_flags = (directory == NULL) ? TRUE : FALSE;  // 是否使用工作目录标志
#endif

    namelen = vfs_normalize_path_parame_check(filename, pathname);  // 参数检查
    if (namelen < 0) {  // 参数检查失败
        return namelen;  // 返回错误码
    }

#ifdef VFS_USING_WORKDIR
    if (directory == NULL) {  // 如果未指定目录，使用当前工作目录
        spin_lock_irqsave(&curr->files->workdir_lock, lock_flags);  // 加锁保护工作目录
        directory = curr->files->workdir;  // 获取当前工作目录
    }
#else
    if ((directory == NULL) && (filename[0] != '/')) {  // 未启用工作目录且不是绝对路径
        PRINT_ERR("NO_WORKING_DIR\n");  // 打印错误信息
        *pathname = NULL;  // 清空输出路径
        return -EINVAL;  // 返回无效参数错误
    }
#endif

    /* 2: 路径字符位置：/ 和结束符/0 */

    // 检查路径长度是否超过限制
    if ((filename[0] != '/') && (strlen(directory) + namelen + 2 > TEMP_PATH_MAX)) {
#ifdef VFS_USING_WORKDIR
        if (dir_flags == TRUE) {  // 如果使用了工作目录
            spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);  // 解锁
        }
#endif
        return -ENAMETOOLONG;  // 返回路径过长错误
    }

    fullpath = vfs_normalize_fullpath(directory, filename, pathname, namelen);  // 获取完整路径
#ifdef VFS_USING_WORKDIR
    if (dir_flags == TRUE) {  // 如果使用了工作目录
        spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);  // 解锁
    }
#endif
    if (fullpath == NULL) {  // 获取路径失败
        return -get_errno();  // 返回错误码
    }

    (void)str_path(fullpath);  // 清理路径中的连续斜杠
    (void)str_normalize_path(fullpath);  // 规范化路径（处理.和..）
    if (strlen(fullpath) >= PATH_MAX) {  // 检查规范化后的路径长度
        *pathname = NULL;  // 清空输出路径
        free(fullpath);  // 释放内存
        return -ENAMETOOLONG;  // 返回路径过长错误
    }

    *pathname = fullpath;  // 设置输出路径
    return ENOERR;  // 返回成功
}

/**
 * @brief 根据文件描述符规范化路径
 * @param dirfd 目录文件描述符
 * @param filename 待规范化的文件名
 * @param pathname 输出参数，规范化后的完整路径
 * @return 成功返回ENOERR，失败返回错误码
 */
int vfs_normalize_pathat(int dirfd, const char *filename, char **pathname)
{
    /* 通过dirfd获取路径 */
    char *relativeoldpath = NULL;  // 相对路径指针
    char *fullpath = NULL;  // 完整路径指针
    int ret = 0;  // 函数返回值

    ret = get_path_from_fd(dirfd, &relativeoldpath);  // 从文件描述符获取路径
    if (ret < 0) {  // 获取失败
        return ret;  // 返回错误码
    }

    ret = vfs_normalize_path((const char *)relativeoldpath, filename, &fullpath);  // 规范化路径
    if (relativeoldpath) {  // 如果获取到了相对路径
        free(relativeoldpath);  // 释放内存
    }

    if (ret < 0) {  // 规范化失败
        return ret;  // 返回错误码
    }

    *pathname = fullpath;  // 设置输出路径
    return ret;  // 返回成功
}
