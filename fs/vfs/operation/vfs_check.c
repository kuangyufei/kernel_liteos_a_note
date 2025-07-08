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

#include "errno.h"
#include "stdlib.h"
#include "string.h"
#include "dirent.h"
#include "unistd.h"
#include "sys/select.h"
#include "sys/stat.h"
#include "sys/prctl.h"
#include "fs/dirent_fs.h"
#include "vnode.h"

/****************************************************************************
 * Name: fscheck
 ****************************************************************************/
/**
 * @brief   文件系统检查函数
 * @param   path  要检查的文件系统路径
 * @return  成功返回0，失败返回VFS_ERROR并设置errno
 */
int fscheck(const char *path)
{
    int ret;                         // 用于存储函数调用返回值
    struct Vnode *vnode = NULL;      // 用于存储查找到的vnode结构体指针
    struct fs_dirent_s *dir = NULL;  // 目录项结构体指针，用于文件系统检查

    /* Find the node matching the path. */
    VnodeHold();                     // 获取vnode操作锁，确保线程安全
    // 查找路径对应的vnode，第三个参数0表示不创建文件
    ret = VnodeLookup(path, &vnode, 0);
    if (ret != OK) {                 // 检查vnode查找是否成功
        VnodeDrop();                 // 释放vnode操作锁
        goto errout;                 // 跳转到错误处理
    }

    // 分配目录项结构体内存，zalloc会初始化为0
    dir = (struct fs_dirent_s *)zalloc(sizeof(struct fs_dirent_s));
    if (!dir) {                      // 检查内存分配是否成功
        /* Insufficient memory to complete the operation. */
        ret = -ENOMEM;               // 设置返回值为内存不足错误
        VnodeDrop();                 // 释放vnode操作锁
        goto errout;                 // 跳转到错误处理
    }

    // 检查vnode操作集是否存在且包含Fscheck方法
    if (vnode->vop && vnode->vop->Fscheck) {
        // 调用具体文件系统的Fscheck实现进行文件系统检查
        ret = vnode->vop->Fscheck(vnode, dir);
        if (ret != OK) {             // 检查文件系统检查是否成功
            VnodeDrop();             // 释放vnode操作锁
            goto errout_with_direntry;  // 跳转到带目录项的错误处理
        }
    } else {
        ret = -ENOSYS;               // 设置返回值为操作未实现错误
        VnodeDrop();                 // 释放vnode操作锁
        goto errout_with_direntry;   // 跳转到带目录项的错误处理
    }
    VnodeDrop();                     // 释放vnode操作锁

    free(dir);                       // 释放目录项结构体内存
    return 0;                        // 文件系统检查成功，返回0

errout_with_direntry:                // 带目录项的错误处理标签
    free(dir);                       // 释放目录项结构体内存
errout:                             // 错误处理标签
    set_errno(-ret);                 // 将返回值转换为errno并设置
    return VFS_ERROR;                // 返回VFS错误
}