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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "errno.h"
#include "vfs_config.h"
#include "sys/stat.h"
#include "vnode.h"
#include "fs/mount.h"
#include "string.h"
#include "stdlib.h"
#include "utime.h"
#include <fcntl.h>

/****************************************************************************
 * Global Functions
 ****************************************************************************/
/**
 * @brief 修改文件的访问时间和修改时间
 * @param path 文件路径
 * @param ptimes 指向utimbuf结构体的指针，包含新的访问时间和修改时间，为NULL时使用当前时间
 * @return 成功返回OK，失败返回VFS_ERROR并设置errno
 */
int utime(const char *path, const struct utimbuf *ptimes)
{
    int ret;                                  // 函数返回值，用于错误处理和状态判断
    char *fullpath = NULL;                    // 规范化后的文件全路径
    struct Vnode *vnode = NULL;               // 文件对应的vnode结构体指针
    time_t cur_sec;                           // 当前时间戳，用于ptimes为NULL时
    struct IATTR attr = {0};                  // 文件属性结构体，用于存储要修改的时间属性

    /* Sanity checks */                       // 参数合法性检查

    if (path == NULL) {                       // 检查路径是否为NULL指针
        ret = -EINVAL;                        // 设置错误码为无效参数
        goto errout;                          // 跳转到错误处理流程
    }

    if (!path[0]) {                           // 检查路径是否为空字符串
        ret = -ENOENT;                        // 设置错误码为文件不存在
        goto errout;                          // 跳转到错误处理流程
    }

    // 规范化文件路径，获取绝对路径
    ret = vfs_normalize_path((const char *)NULL, path, &fullpath);
    if (ret < 0) {                            // 检查路径规范化是否失败
        goto errout;                          // 跳转到错误处理流程
    }

    /* Get the vnode for this file */         // 获取文件对应的vnode
    VnodeHold();                              // 获取vnode全局锁，保护vnode操作
    // 查找文件对应的vnode
    ret = VnodeLookup(fullpath, &vnode, 0);
    if (ret != LOS_OK) {                      // 检查vnode查找是否失败
        VnodeDrop();                          // 释放vnode全局锁
        goto errout_with_path;                // 跳转到带路径释放的错误处理流程
    }

    // 检查文件所在文件系统是否为只读
    if ((vnode->originMount) && (vnode->originMount->mountFlags & MS_RDONLY)) {
        VnodeDrop();                          // 释放vnode全局锁
        ret = -EROFS;                         // 设置错误码为只读文件系统
        goto errout_with_path;                // 跳转到带路径释放的错误处理流程
    }

    // 检查vnode操作集和Chattr函数是否存在
    if (vnode->vop && vnode->vop->Chattr) {
        if (ptimes == NULL) {                 // 如果ptimes为NULL，使用当前时间
            /* get current seconds */         // 获取当前时间戳
            cur_sec = time(NULL);
            attr.attr_chg_atime = cur_sec;    // 设置访问时间为当前时间
            attr.attr_chg_mtime = cur_sec;    // 设置修改时间为当前时间
        } else {
            // 使用ptimes参数指定的访问时间和修改时间
            attr.attr_chg_atime = ptimes->actime;
            attr.attr_chg_mtime = ptimes->modtime;
        }
        // 设置属性修改标志，标识需要修改访问时间和修改时间
        attr.attr_chg_valid = CHG_ATIME | CHG_MTIME;
        // 调用vnode的Chattr方法修改文件时间属性
        ret = vnode->vop->Chattr(vnode, &attr);
        if (ret != OK) {                      // 检查属性修改是否失败
            VnodeDrop();                      // 释放vnode全局锁
            goto errout_with_path;            // 跳转到带路径释放的错误处理流程
        }
    } else {
        ret = -ENOSYS;                        // 设置错误码为不支持的操作
        VnodeDrop();                          // 释放vnode全局锁
        goto errout_with_path;                // 跳转到带路径释放的错误处理流程
    }
    VnodeDrop();                              // 释放vnode全局锁

    /* Successfully stat'ed the file */       // 文件时间属性修改成功
    free(fullpath);                           // 释放规范化路径内存

    return OK;                                // 返回成功

    /* Failure conditions always set the errno appropriately */

errout_with_path:                             // 带路径释放的错误处理标签
    free(fullpath);                           // 释放规范化路径内存
errout:                                      // 错误处理标签

    if (ret != 0) {                           // 如果存在错误码
        set_errno(-ret);                      // 设置全局错误码
    }
    return VFS_ERROR;                         // 返回错误
}