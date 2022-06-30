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

#include "proc_fs.h"

#include <stdio.h>
#include <sys/mount.h>
#include <sys/statfs.h>

#include "fs/mount.h"
#include "internal.h"
//显示文件系统类型,将被作为回调函数回调           cat /proc/mounts 显示的每项内容
static int ShowType(const char *devPoint, const char *mountPoint, struct statfs *statBuf, void *arg)
{
    struct SeqBuf *seqBuf = (struct SeqBuf *)arg;
    char *type = NULL;

    switch (statBuf->f_type) {//目前鸿蒙支持的文件系统
        case PROCFS_MAGIC:	//虚拟文件系统，通过它可以使鸿蒙在内核空间和用户空间之间进行通信
            type = "proc";
            break;
        case JFFS2_SUPER_MAGIC:
            type = "jffs2";	//基于 nor flash的文件系统
            break;
        case NFS_SUPER_MAGIC://网络文件系统
            type = "nfs";
            break;
        case TMPFS_MAGIC://tmpfs /mnt/wsl  windows10的 wsl 实现是 tmpfs方式
            type = "tmpfs";	//内存文件系统
            break;
        case MSDOS_SUPER_MAGIC:
            type = "vfat";
            break;
        case ZPFS_MAGIC:
            type = "zpfs";
            break;
        default:
            return 0;
    }

    if (strlen(devPoint) == 0) {
        (void)LosBufPrintf(seqBuf, "%s %s %s %s %d %d\n", type, mountPoint, type, "()", 0, 0);
    } else {
        (void)LosBufPrintf(seqBuf, "%s %s %s %s %d %d\n", devPoint, mountPoint, type, "()", 0, 0);
    }

    return 0;
}
/// 读取 mount 接口实现
static int MountsProcFill(struct SeqBuf *m, void *v)
{
    foreach_mountpoint_t handler = ShowType;
    (void)foreach_mountpoint(handler, (void *)m);

    return 0;
}
///实现 操作proc file 接口,也可理解为驱动程序不同
static const struct ProcFileOperations MOUNTS_PROC_FOPS = {
    .read = MountsProcFill,// /proc/mounts 只能被读取
};

void ProcMountsInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("mounts", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("create mounts error!\n");
        return;
    }

    pde->procFileOps = &MOUNTS_PROC_FOPS;//操作 /proc/mounts 的具体实现
}

