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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "fs/file.h"
#include "fs/fs_operation.h"
#include "fs/fd_table.h"
#include "unistd.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/
void CloseOnExec(struct files_struct *files)
{
    int sysFd;
    if ((files == NULL) || (files->fdt == NULL)) {
        return;
    }

    for (int i = 0; i < files->fdt->max_fds; i++) {
        if (FD_ISSET(i, files->fdt->proc_fds) &&
            FD_ISSET(i, files->fdt->cloexec_fds)) {
            sysFd = DisassociateProcessFd(i);
            close(sysFd);
            FreeProcessFd(i);
        }
    }
}

void SetCloexecFlag(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();
    if (fdt == NULL) {
        return;
    }

    FileTableLock(fdt);
    FD_SET(procFd, fdt->cloexec_fds);
    FileTableUnLock(fdt);
    return;
}

bool CheckCloexecFlag(int procFd)
{
    bool isCloexec = 0;
    struct fd_table_s *fdt = GetFdTable();
    if (fdt == NULL) {
        return false;
    }

    FileTableLock(fdt);
    isCloexec = FD_ISSET(procFd, fdt->cloexec_fds);
    FileTableUnLock(fdt);
    return isCloexec;
}

void ClearCloexecFlag(int procFd)
{
    struct fd_table_s *fdt = GetFdTable();
    if (fdt == NULL) {
        return;
    }

    FileTableLock(fdt);
    FD_CLR(procFd, fdt->cloexec_fds);
    FileTableUnLock(fdt);
    return;
}
