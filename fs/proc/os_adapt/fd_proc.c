/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

#include <stdbool.h>
#include "fs/file.h"
#include "vfs_config.h"
#include "internal.h"

#include "fs/fd_table.h"
#include "los_process.h"
#include "capability_api.h"
#include "capability_type.h"


/*
 * Template: Pid    Fd  [SysFd ] Name
 */
static void FillFdInfo(struct SeqBuf *seqBuf, struct filelist *fileList, unsigned int pid, bool hasPrivilege)
{
    int fd;
    int sysFd;
    char *name = NULL;
    struct file *filp = NULL;

    struct fd_table_s *fdt = LOS_GetFdTable(pid);
    if ((fdt == NULL) || (fdt->proc_fds == NULL)) {
        return;
    }

    (void)sem_wait(&fdt->ft_sem);

    for (fd = MIN_START_FD; fd < fdt->max_fds; fd++) {
        if (FD_ISSET(fd, fdt->proc_fds)) {
            sysFd = fdt->ft_fds[fd].sysFd;
            if (sysFd < CONFIG_NFILE_DESCRIPTORS) {
                filp = &fileList->fl_files[sysFd];
                name = filp->f_path;
            } else if (sysFd < (CONFIG_NFILE_DESCRIPTORS + CONFIG_NSOCKET_DESCRIPTORS)) {
                name = "(socks)";
            } else if (sysFd < (FD_SETSIZE + CONFIG_NTIME_DESCRIPTORS)) {
                name = "(timer)";
            } else if (sysFd < (MQUEUE_FD_OFFSET + CONFIG_NQUEUE_DESCRIPTORS)) {
                name = "(mqueue)";
            } else {
                name = "(unknown)";
            }

            if (hasPrivilege) {
                (void)LosBufPrintf(seqBuf, "%u\t%d\t%d\t%s\n", pid, fd, sysFd, name);
            } else {
                (void)LosBufPrintf(seqBuf, "%u\t%d\t%s\n", pid, fd, name);
            }
        }
    }

    (void)sem_post(&fdt->ft_sem);
}

static int FdProcFill(struct SeqBuf *seqBuf, void *v)
{
    int pidNum;
    bool hasPrivilege;
    unsigned int pidMaxNum;
    unsigned int *pidList = NULL;

    /* privilege user */
    if (IsCapPermit(CAP_DAC_READ_SEARCH)) {
        pidMaxNum = LOS_GetSystemProcessMaximum();
        pidList = (unsigned int *)malloc(pidMaxNum * sizeof(unsigned int));
        if (pidList == NULL) {
            return -ENOMEM;
        }
        pidNum = LOS_GetUsedPIDList(pidList, pidMaxNum);
        hasPrivilege = true;
        (void)LosBufPrintf(seqBuf, "Pid\tFd\tSysFd\tName\n");
    } else {
        pidNum = 1;
        pidList = (unsigned int *)malloc(pidNum * sizeof(unsigned int));
        if (pidList == NULL) {
            return -ENOMEM;
        }
        pidList[0] = LOS_GetCurrProcessID();
        hasPrivilege = false;
        (void)LosBufPrintf(seqBuf, "Pid\tFd\tName\n");
    }

    struct filelist *fileList = &tg_filelist;
    (void)sem_wait(&fileList->fl_sem);

    for (int i = 0; i < pidNum; i++) {
        FillFdInfo(seqBuf, fileList, pidList[i], hasPrivilege);
    }

    free(pidList);
    (void)sem_post(&fileList->fl_sem);
    return 0;
}

static const struct ProcFileOperations FD_PROC_FOPS = {
    .read       = FdProcFill,
};

void ProcFdInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("fd", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("creat /proc/fd error.\n");
        return;
    }

    pde->procFileOps = &FD_PROC_FOPS;
}

