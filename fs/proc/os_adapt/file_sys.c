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

static int FsFileSysProcRead(struct SeqBuf *seqBuf, void *buf)
{
    (void)buf;

    struct fsmap_t *m = NULL;
    for (m = &g_fsmap[0]; m != &g_fsmap_end; ++m) {
        if (m->fs_filesystemtype) {
            if (m->is_bdfs == true) {
                (void)LosBufPrintf(seqBuf, "\n       %s\n", m->fs_filesystemtype);
            } else {
                (void)LosBufPrintf(seqBuf, "%s  %s\n", "nodev", m->fs_filesystemtype);
            }
        }
    }
    return 0;
}

static const struct ProcFileOperations FILESYS_PROC_FOPS = {
    .read = FsFileSysProcRead,
};

void ProcFileSysInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("filesystems", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("creat /proc/filesystems error!\n");
        return;
    }
    pde->procFileOps = &FILESYS_PROC_FOPS;
}
