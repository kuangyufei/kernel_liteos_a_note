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
/**
 * @brief 根据文件系统类型显示挂载信息
 * @param devPoint 设备挂载点路径
 * @param mountPoint 文件系统挂载路径
 * @param statBuf 文件系统状态缓冲区指针
 * @param arg 传递的参数，此处为SeqBuf序列缓冲区指针
 * @return 成功返回0，不支持的文件系统类型返回0
 */
static int ShowType(const char *devPoint, const char *mountPoint, struct statfs *statBuf, void *arg)
{
    struct SeqBuf *seqBuf = (struct SeqBuf *)arg;  // 序列缓冲区指针，用于存储格式化输出
    char *type = NULL;                             // 文件系统类型字符串指针

    // 根据文件系统魔数判断文件系统类型
    switch (statBuf->f_type) {
        case PROCFS_MAGIC:                         // Proc文件系统魔数
            type = "proc";                         // 设置类型为proc
            break;
        case JFFS2_SUPER_MAGIC:                    // JFFS2文件系统魔数
            type = "jffs2";                        // 设置类型为jffs2
            break;
        case NFS_SUPER_MAGIC:                      // NFS文件系统魔数
            type = "nfs";                         // 设置类型为nfs
            break;
        case TMPFS_MAGIC:                          // TMPFS文件系统魔数
            type = "tmpfs";                       // 设置类型为tmpfs
            break;
        case MSDOS_SUPER_MAGIC:                    // MSDOS文件系统魔数
            type = "vfat";                        // 设置类型为vfat
            break;
        case ZPFS_MAGIC:                           // ZPFS文件系统魔数
            type = "zpfs";                        // 设置类型为zpfs
            break;
        default:                                   // 不支持的文件系统类型
            return 0;                              // 直接返回，不输出信息
    }

    // 根据设备挂载点是否为空字符串，格式化输出不同的挂载信息
    if (strlen(devPoint) == 0) {
        // 设备挂载点为空时，使用文件系统类型作为第一列
        (void)LosBufPrintf(seqBuf, "%s %s %s %s %d %d\n", type, mountPoint, type, "()", 0, 0);
    } else {
        // 设备挂载点非空时，使用实际设备路径作为第一列
        (void)LosBufPrintf(seqBuf, "%s %s %s %s %d %d\n", devPoint, mountPoint, type, "()", 0, 0);
    }

    return 0;                                      // 成功输出挂载信息
}

/**
 * @brief 填充挂载信息到序列缓冲区
 * @param m 序列缓冲区指针
 * @param v 未使用的参数
 * @return 成功返回0
 */
static int MountsProcFill(struct SeqBuf *m, void *v)
{
    foreach_mountpoint_t handler = ShowType;       // 挂载点遍历处理函数：ShowType
    // 遍历所有挂载点，调用ShowType处理并填充信息到缓冲区
    (void)foreach_mountpoint(handler, (void *)m);

    return 0;                                      // 成功填充挂载信息
}

/**
 * @brief /proc/mounts文件的操作函数结构体
 * @note 仅实现read操作，指向MountsProcFill函数
 */
static const struct ProcFileOperations MOUNTS_PROC_FOPS = {
    .read = MountsProcFill,                        // 读取操作：填充挂载信息到缓冲区
};

/**
 * @brief 初始化/proc/mounts节点
 * @note 创建proc入口并关联文件操作函数
 */
void ProcMountsInit(void)
{
    // 创建名为"mounts"的proc节点，权限0，父目录为NULL（根目录）
    struct ProcDirEntry *pde = CreateProcEntry("mounts", 0, NULL);
    if (pde == NULL) {                             // 若节点创建失败
        PRINT_ERR("create mounts error!\n");       // 打印错误信息
        return;                                    // 直接返回
    }

    pde->procFileOps = &MOUNTS_PROC_FOPS;          // 关联mounts节点的文件操作函数
}