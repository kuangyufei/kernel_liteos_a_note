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

#include "capability_api.h"
#include "errno.h"
#include "fs/fs_operation.h"
#include "fs/file.h"
#include "string.h"
#include "stdlib.h"
#include "sys/stat.h"
#include "vnode.h"
#include "fs/mount.h"
#include <fcntl.h>

/****************************************************************************
 * Static Functions
 ****************************************************************************/
/****************************************************************************
 * Name: chattr
 *
 * Returned Value:
 *   Zero on success; -1 on failure with errno set:
 *
 ****************************************************************************/
/**
 * @brief   修改文件属性
 * @param   pathname  文件路径名
 * @param   attr      指向IATTR结构体的指针，包含要设置的文件属性
 * @return  成功返回OK，失败返回VFS_ERROR并设置errno
 */
int chattr(const char *pathname, struct IATTR *attr)
{
    struct Vnode *vnode = NULL;  // 用于存储查找到的vnode结构体指针
    int ret;                     // 用于存储函数调用返回值

    // 参数合法性检查：路径名或属性结构体为空则返回无效参数错误
    if (pathname == NULL || attr == NULL) {
        set_errno(EINVAL);       // 设置errno为无效参数
        return VFS_ERROR;        // 返回VFS错误
    }

    VnodeHold();                 // 获取vnode操作锁，确保线程安全
    // 查找路径对应的vnode，第三个参数0表示不创建文件
    ret = VnodeLookup(pathname, &vnode, 0);
    if (ret != LOS_OK) {         // 检查vnode查找是否成功
        goto errout_with_lock;   // 查找失败，跳转到带锁错误处理
    }

    // 检查文件系统是否为只读：如果挂载点存在且标记为只读
    if ((vnode->originMount) && (vnode->originMount->mountFlags & MS_RDONLY)) {
        ret = -EROFS;            // 设置返回值为只读文件系统错误
        goto errout_with_lock;   // 跳转到带锁错误处理
    }

    /* The way we handle the stat depends on the type of vnode that we
     * are dealing with.
     */

    // 检查vnode操作集是否存在且包含Chattr方法
    if (vnode->vop != NULL && vnode->vop->Chattr != NULL) {
        ret = vnode->vop->Chattr(vnode, attr);  // 调用具体文件系统的Chattr实现
    } else {
        ret = -ENOSYS;           // 操作未实现错误
    }
    VnodeDrop();                 // 释放vnode操作锁

    if (ret < 0) {               // 检查属性修改是否失败
        goto errout;             // 跳转到错误处理
    }

    return OK;                   // 属性修改成功，返回OK

    /* Failure conditions always set the errno appropriately */

errout_with_lock:                // 带锁状态下的错误处理标签
    VnodeDrop();                 // 释放vnode操作锁
errout:                         // 错误处理标签
    set_errno(-ret);             // 将返回值转换为errno并设置
    return VFS_ERROR;            // 返回VFS错误
}