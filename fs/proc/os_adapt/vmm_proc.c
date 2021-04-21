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

STATIC VOID OsVmDumpSeqSpaces(struct SeqBuf *seqBuf)
{
    LosVmSpace *space = NULL;
    LosVmMapRegion *region = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeNext = NULL;
    UINT32 pssPages = 0;
    UINT32 spacePages;
    UINT32 regionPages;
    LosProcessCB *pcb = NULL;
    LOS_DL_LIST *aspaceList = LOS_GetVmSpaceList();
    LosMux *aspaceListMux = OsGVmSpaceMuxGet();

    (VOID)LOS_MuxAcquire(aspaceListMux);
    LOS_DL_LIST_FOR_EACH_ENTRY(space, aspaceList, LosVmSpace, node) {
        (VOID)LOS_MuxAcquire(&space->regionMux);
        spacePages = OsCountAspacePages(space);
        pcb = OsGetPIDByAspace(space);
        if (pcb == NULL) {
            (VOID)LOS_MuxRelease(&space->regionMux);
            continue;
        }
        (VOID)LosBufPrintf(seqBuf, "\r\n PID    aspace     name       base       size     pages \n");
        (VOID)LosBufPrintf(seqBuf, " ----   ------     ----       ----       -----     ----\n");
        (VOID)LosBufPrintf(seqBuf, " %-4d %#010x %-10.10s %#010x %#010x %d\n",
                           pcb->processID, space, pcb->processName, space->base, space->size, spacePages);
        (VOID)LosBufPrintf(seqBuf,
            "\r\n\t region      name                base       size       mmu_flags      pages   pg/ref\n");
        (VOID)LosBufPrintf(seqBuf,
            "\t ------      ----                ----       ----        ---------      -----   -----\n");

        RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)
            region = (LosVmMapRegion *)pstRbNode;
            regionPages = OsCountRegionPages(space, region, &pssPages);
            CHAR *flagsStr = OsArchFlagsToStr(region->regionFlags);
            if (flagsStr == NULL) {
                break;
            }
            (VOID)LosBufPrintf(seqBuf, "\t %#010x  %-19.19s %#010x %#010x  %-15.15s %4d    %4d\n",
                region, OsGetRegionNameOrFilePath(region), region->range.base,
                region->range.size, flagsStr, regionPages, pssPages);
            (VOID)LOS_MemFree(m_aucSysMem0, flagsStr);
            (VOID)OsRegionOverlapCheck(space, region);
        RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)
        (VOID)LOS_MuxRelease(&space->regionMux);
    }
    (VOID)LOS_MuxRelease(aspaceListMux);
}

static int VmmProcFill(struct SeqBuf *m, void *v)
{
    (void)v;
    OsVmDumpSeqSpaces(m);

    return 0;
}

static const struct ProcFileOperations VMM_PROC_FOPS = {
    .write      = NULL,
    .read       = VmmProcFill,
};

void ProcVmmInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("vmm", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("create /proc/vmm error!\n");
        return;
    }

    pde->procFileOps = &VMM_PROC_FOPS;
}
#endif
#endif
