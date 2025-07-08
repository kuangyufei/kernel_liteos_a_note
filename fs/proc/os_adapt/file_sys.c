/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "fs/fs.h"
#include "proc_fs.h"
#include "proc_file.h"
#include "errno.h"
#include "sys/mount.h"

extern struct fsmap_t g_fsmap[];
extern struct fsmap_t g_fsmap_end;
/**
 * @brief /proc/filesystems文件的读取回调函数
 * @details 遍历文件系统映射表，将支持的文件系统类型信息格式化输出到序列缓冲区
 * @param seqBuf 序列缓冲区指针，用于存储文件系统信息
 * @param buf 未使用的参数
 * @return 0表示成功，非0表示错误码
 */
static int FsFileSysProcRead(struct SeqBuf *seqBuf, void *buf)
{
    (void)buf;  // 未使用参数，避免编译警告

    struct fsmap_t *m = NULL;  // 文件系统映射表迭代指针
    // 遍历全局文件系统映射表（从起始地址到结束地址）
    for (m = &g_fsmap[0]; m != &g_fsmap_end; ++m) {
        // 检查文件系统类型名称是否有效
        if (m->fs_filesystemtype) {
            // 根据是否为块设备文件系统输出不同格式
            if (m->is_bdfs == true) {
                // 块设备文件系统：缩进显示名称
                (void)LosBufPrintf(seqBuf, "\n       %s\n", m->fs_filesystemtype);
            } else {
                // 非块设备文件系统：前缀"nodev"标识
                (void)LosBufPrintf(seqBuf, "%s  %s\n", "nodev", m->fs_filesystemtype);
            }
        }
    }
    return 0;  // 成功完成读取操作
}

// /proc/filesystems文件的操作函数集
static const struct ProcFileOperations FILESYS_PROC_FOPS = {
    .read = FsFileSysProcRead,  // 读取回调函数，指向FsFileSysProcRead
};

/**
 * @brief 初始化/proc/filesystems节点
 * @details 创建/proc文件系统中的filesystems节点，并关联操作函数
 */
void ProcFileSysInit(void)
{
    // 创建/proc/filesystems节点，权限为0
    struct ProcDirEntry *pde = CreateProcEntry("filesystems", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("creat /proc/filesystems error!\n");  // 创建失败，输出错误信息
        return;
    }
    pde->procFileOps = &FILESYS_PROC_FOPS;  // 关联filesystems节点的操作函数集
}