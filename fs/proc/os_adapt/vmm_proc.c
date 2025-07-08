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

#include <sys/statfs.h>
#include <sys/mount.h>
#include "proc_fs.h"

#ifdef LOSCFG_SHELL_CMD_DEBUG
#include "los_vm_map.h"
#include "los_vm_dump.h"
#include "los_vm_lock.h"
#include "los_process_pri.h"

#ifdef LOSCFG_KERNEL_VM

/**
 * @brief   将虚拟内存空间信息输出到序列缓冲区
 * @param   seqBuf [out] 用于存储输出内容的缓冲区结构体指针
 * @return  VOID
 * @details 遍历系统中所有虚拟内存空间及其映射区域，收集并格式化输出内存使用信息，包括进程ID、空间大小、区域属性等
 */
STATIC VOID OsVmDumpSeqSpaces(struct SeqBuf *seqBuf)
{
    LosVmSpace *space = NULL;               // 虚拟内存空间结构体指针
    LosVmMapRegion *region = NULL;          // 虚拟内存映射区域结构体指针
    LosRbNode *pstRbNode = NULL;            // 红黑树节点指针，用于遍历区域树
    LosRbNode *pstRbNodeNext = NULL;        // 下一个红黑树节点指针，用于安全遍历
    UINT32 pssPages = 0;                    // 比例集大小(按页计数)，衡量物理内存使用
    UINT32 spacePages;                      // 虚拟内存空间总页数
    UINT32 regionPages;                     // 内存区域页数
    LosProcessCB *pcb = NULL;               // 进程控制块指针
    LOS_DL_LIST *aspaceList = LOS_GetVmSpaceList();  // 获取所有虚拟内存空间链表
    LosMux *aspaceListMux = OsGVmSpaceMuxGet();      // 获取虚拟内存空间链表互斥锁

    (VOID)LOS_MuxAcquire(aspaceListMux);    // 获取虚拟内存空间链表锁，保护遍历过程
    // 遍历系统中所有虚拟内存空间
    LOS_DL_LIST_FOR_EACH_ENTRY(space, aspaceList, LosVmSpace, node) {
        (VOID)LOS_MuxAcquire(&space->regionMux);  // 获取当前空间的区域树互斥锁
        spacePages = OsCountAspacePages(space);   // 计算当前虚拟内存空间总页数
        pcb = OsGetPIDByAspace(space);            // 通过虚拟内存空间获取对应进程控制块
        if (pcb == NULL) {                        // 跳过无效进程(可能已退出)
            (VOID)LOS_MuxRelease(&space->regionMux);  // 释放区域树锁
            continue;                             // 继续下一个内存空间
        }
        // 打印内存空间标题行
        (VOID)LosBufPrintf(seqBuf, "\r\n PID    aspace     name       base       size     pages \n");
        (VOID)LosBufPrintf(seqBuf, " ----   ------     ----       ----       -----     ----\n");
        // 打印进程ID、内存空间地址、进程名、基地址、大小和总页数
        (VOID)LosBufPrintf(seqBuf, " %-4d %#010x %-10.10s %#010x %#010x %d\n",
                           pcb->processID, space, pcb->processName, space->base, space->size, spacePages);
        // 打印内存区域标题行
        (VOID)LosBufPrintf(seqBuf,
            "\r\n\t region      name                base       size       mmu_flags      pages   pg/ref\n");
        (VOID)LosBufPrintf(seqBuf,
            "\t ------      ----                ----       ----        ---------      -----   -----\n");

        // 安全遍历当前内存空间的红黑树区域节点
        RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)
            region = (LosVmMapRegion *)pstRbNode;  // 将红黑树节点转换为内存区域指针
            // 计算区域页数和PSS(比例集大小)页数
            regionPages = OsCountRegionPages(space, region, &pssPages);
            // 将区域标志转换为可读字符串(如"rwx")
            CHAR *flagsStr = OsArchFlagsToStr(region->regionFlags);
            if (flagsStr == NULL) {                // 内存分配失败时跳出循环
                break;
            }
            // 打印区域详细信息：区域地址、名称/路径、基地址、大小、标志、页数和PSS页数
            (VOID)LosBufPrintf(seqBuf, "\t %#010x  %-19.19s %#010x %#010x  %-15.15s %4d    %4d\n",
                region, OsGetRegionNameOrFilePath(region), region->range.base,
                region->range.size, flagsStr, regionPages, pssPages);
            (VOID)LOS_MemFree(m_aucSysMem0, flagsStr);  // 释放标志字符串内存
            (VOID)OsRegionOverlapCheck(space, region);  // 检查区域是否重叠(调试用)
        RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)
        (VOID)LOS_MuxRelease(&space->regionMux);  // 释放当前空间的区域树互斥锁
    }
    (VOID)LOS_MuxRelease(aspaceListMux);      // 释放虚拟内存空间链表锁
}

/**
 * @brief   填充/proc/vmm文件内容
 * @param   m   [out] 用于存储输出内容的缓冲区结构体指针
 * @param   v   [in]  未使用的参数
 * @return  int 成功返回0
 * @details 调用OsVmDumpSeqSpaces函数生成虚拟内存使用信息并写入缓冲区
 */
static int VmmProcFill(struct SeqBuf *m, void *v)
{
    (void)v;                                 // 未使用参数，避免编译警告
    OsVmDumpSeqSpaces(m);                    // 生成并输出虚拟内存空间信息

    return 0;                                // 成功返回0
}

// VMM proc文件操作结构体，仅支持读操作
static const struct ProcFileOperations VMM_PROC_FOPS = {
    .write      = NULL,                      // 写操作未实现
    .read       = VmmProcFill,               // 关联读操作回调函数
};

/**
 * @brief   初始化/proc/vmm文件节点
 * @return  void
 * @details 创建/proc/vmm文件节点并关联文件操作结构体，用于查看系统虚拟内存使用情况
 */
void ProcVmmInit(void)
{
    // 创建/proc/vmm文件节点，权限0，父目录为NULL(即/proc目录)
    struct ProcDirEntry *pde = CreateProcEntry("vmm", 0, NULL);
    if (pde == NULL) {                       // 检查节点创建是否失败
        PRINT_ERR("create /proc/vmm error!\n");  // 打印错误信息
        return;                              // 创建失败直接返回
    }

    pde->procFileOps = &VMM_PROC_FOPS;       // 关联vmm文件操作结构体
}
#endif
#endif
