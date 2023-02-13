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

static int SysMemInfoFill(struct SeqBuf *seqBuf, void *arg)
{
    (void)arg;
    LOS_MEM_POOL_STATUS mem = {0};
    if (LOS_MemInfoGet(m_aucSysMem0, &mem) == LOS_NOK) {
        return -EBADF;
    }
    (void)LosBufPrintf(seqBuf, "\nUsedSize:        %u byte\n", mem.totalUsedSize);
    (void)LosBufPrintf(seqBuf, "FreeSize:        %u byte\n", mem.totalFreeSize);
    (void)LosBufPrintf(seqBuf, "MaxFreeNodeSize: %u byte\n", mem.maxFreeNodeSize);
    (void)LosBufPrintf(seqBuf, "UsedNodeNum:     %u\n", mem.usedNodeNum);
    (void)LosBufPrintf(seqBuf, "FreeNodeNum:     %u\n", mem.freeNodeNum);
#ifdef LOSCFG_MEM_WATERLINE
    (void)LosBufPrintf(seqBuf, "UsageWaterLine:  %u byte\n", mem.usageWaterLine);
#endif
     return 0;
}

static const struct ProcFileOperations SYS_MEMINFO_PROC_FOPS = {
    .read = SysMemInfoFill,
};

void ProcSysMemInfoInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("meminfo", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("create mem_info error!\n");
        return;
    }

    pde->procFileOps = &SYS_MEMINFO_PROC_FOPS;
}
