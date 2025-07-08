/*!
 * @file    vfs_init.c
 * @brief
   @verbatim
   VFS是Virtual File System（虚拟文件系统）的缩写，它不是一个实际的文件系统，而是一个异构文件系统之上的软件粘合层，
   为用户提供统一的类Unix文件操作接口。
   
   由于不同类型的文件系统接口不统一，若系统中有多个文件系统类型，访问不同的文件系统就需要使用不同的非标准接口。
   而通过在系统中添加VFS层，提供统一的抽象接口，屏蔽了底层异构类型的文件系统的差异，使得访问文件系统的系统调用不用
   关心底层的存储介质和文件系统类型，提高开发效率。
   
   OpenHarmony内核中，VFS框架是通过在内存中的树结构来实现的，树的每个结点都是一个inode结构体。
   设备注册和文件系统挂载后会根据路径在树中生成相应的结点。VFS最主要是两个功能：
	   查找节点。
	   统一调用（标准）。
   

   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-22
 */
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

#include "disk_pri.h"
#include "fcntl.h"
#include "fs/file.h"
#include "fs/fs_operation.h"
#include "linux/spinlock.h"
#include "los_init.h"
#include "los_printf.h"
#include "fs/mount.h"
#include "path_cache.h"
#include "sys/statfs.h"
#include "unistd.h"
#include "vnode.h"

void los_vfs_init(void)
{
    uint retval;
    static bool g_vfs_init = false;
    if (g_vfs_init) {
        return;
    }

#ifdef LOSCFG_FS_FAT_DISK
    spin_lock_init(&g_diskSpinlock);
    spin_lock_init(&g_diskFatBlockSpinlock);
#endif
    files_initialize();
    files_initlist(&tg_filelist);

    retval = VnodesInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init VnodeInit failed error %d\n", retval);
        return;
    }

    retval = PathCacheInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init PathCacheInit failed error %d\n", retval);
        return;
    }
    retval = VnodeHashInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init VnodeHashInit failed error %d\n", retval);
        return;
    }

    retval = VnodeDevInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init VnodeDevInit failed error %d\n", retval);
        return;
    }

    g_vfs_init = true;
}

LOS_MODULE_INIT(los_vfs_init, LOS_INIT_LEVEL_KMOD_BASIC);
