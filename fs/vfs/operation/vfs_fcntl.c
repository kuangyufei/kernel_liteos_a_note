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
#include "fs/fd_table.h"
#include "fs/fs_operation.h"
#include "sys/types.h"
#include "sys/uio.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int FcntlDupFd(int procfd, int leastFd)
{
    int sysfd = GetAssociatedSystemFd(procfd);
    if ((sysfd < 0) || (sysfd >= CONFIG_NFILE_DESCRIPTORS)) {
        return -EBADF;
    }

    if (CheckProcessFd(leastFd) != OK) {
        return -EINVAL;
    }

    int dupFd = AllocLowestProcessFd(leastFd);
    if (dupFd < 0) {
        return -EMFILE;
    }

    files_refer(sysfd);
    AssociateSystemFd(dupFd, sysfd);

    return dupFd;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int VfsFcntl(int procfd, int cmd, ...)
{
    va_list ap;
    int ret = 0;

    va_start(ap, cmd);
    switch (cmd) {
        case F_DUPFD:
            {
                int arg = va_arg(ap, int);
                ret = FcntlDupFd(procfd, arg);
            }
            break;
        case F_GETFD:
            {
                bool isCloexec = CheckCloexecFlag(procfd);
                ret = isCloexec ? FD_CLOEXEC : 0;
            }
            break;
        case F_SETFD:
            {
                int oflags = va_arg(ap, int);
                if (oflags & FD_CLOEXEC) {
                    SetCloexecFlag(procfd);
                } else {
                    ClearCloexecFlag(procfd);
                }
            }
            break;
        default:
            ret = CONTINE_NUTTX_FCNTL;
            break;
    }

    va_end(ap);
    return ret;
}
