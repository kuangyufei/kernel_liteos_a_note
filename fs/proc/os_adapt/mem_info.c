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

#include "internal.h"
#include "proc_fs.h"
#include "vnode.h"
#include "los_memory.h"
#include "los_vm_filemap.h"
#include "los_memory_pri.h"
/**
 * @brief 填充系统内存信息到序列缓冲区
 * @param seqBuf 序列缓冲区指针，用于存储格式化的内存信息
 * @param arg 未使用的参数
 * @return 成功返回0，失败返回-EBADF
 */
static int SysMemInfoFill(struct SeqBuf *seqBuf, void *arg)
{
    (void)arg;                                  // 未使用的参数，避免编译警告
    LOS_MEM_POOL_STATUS mem = {0};              // 内存池状态结构体，用于存储内存信息
    // 获取系统内存池信息，若失败返回文件描述符错误
    if (LOS_MemInfoGet(m_aucSysMem0, &mem) == LOS_NOK) {
        return -EBADF;                          // 返回错误码：无效的文件描述符
    }
    // 向缓冲区写入已用内存大小（字节）
    (void)LosBufPrintf(seqBuf, "\nUsedSize:        %u byte\n", mem.totalUsedSize);
    // 向缓冲区写入空闲内存大小（字节）
    (void)LosBufPrintf(seqBuf, "FreeSize:        %u byte\n", mem.totalFreeSize);
    // 向缓冲区写入最大空闲节点大小（字节）
    (void)LosBufPrintf(seqBuf, "MaxFreeNodeSize: %u byte\n", mem.maxFreeNodeSize);
    // 向缓冲区写入已用节点数量
    (void)LosBufPrintf(seqBuf, "UsedNodeNum:     %u\n", mem.usedNodeNum);
    // 向缓冲区写入空闲节点数量
    (void)LosBufPrintf(seqBuf, "FreeNodeNum:     %u\n", mem.freeNodeNum);
#ifdef LOSCFG_MEM_WATERLINE
    // 当配置内存水位线功能时，写入内存使用水位线（字节）
    (void)LosBufPrintf(seqBuf, "UsageWaterLine:  %u byte\n", mem.usageWaterLine);
#endif
     return 0;                                  // 成功填充内存信息
}

/**
 * @brief /proc/meminfo文件的操作函数结构体
 * @note 仅实现read操作，指向SysMemInfoFill函数
 */
static const struct ProcFileOperations SYS_MEMINFO_PROC_FOPS = {
    .read = SysMemInfoFill,                     // 读取操作：填充内存信息到缓冲区
};

/**
 * @brief 初始化/proc/meminfo节点
 * @note 创建proc入口并关联文件操作函数
 */
void ProcSysMemInfoInit(void)
{
    // 创建名为"meminfo"的proc节点，权限0，父目录为NULL（根目录）
    struct ProcDirEntry *pde = CreateProcEntry("meminfo", 0, NULL);
    if (pde == NULL) {                          // 若节点创建失败
        PRINT_ERR("create mem_info error!\n");  // 打印错误信息
        return;                                 // 直接返回
    }

    pde->procFileOps = &SYS_MEMINFO_PROC_FOPS;  // 关联meminfo节点的文件操作函数
}