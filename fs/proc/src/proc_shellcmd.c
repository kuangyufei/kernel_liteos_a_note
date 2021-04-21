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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statfs.h>
#include <errno.h>

#include "los_config.h"

#if defined(LOSCFG_SHELL_CMD_DEBUG) && defined(LOSCFG_FS_PROC)
#include "los_typedef.h"
#include "shell.h"
#include "shcmd.h"
#include "proc_file.h"
#include "dirent.h"
#include "fs/fs.h"
#include "proc_fs.h"

#define WRITEPROC_ARGC  3

int OsShellCmdWriteProc(int argc, char **argv)
{
    int i;
    int ret;
    const char *path = NULL;
    const char *value = NULL;
    unsigned int len;
    struct ProcDirEntry *handle = NULL;
    char realPath[PATH_MAX] = {'\0'};
    const char *rootProcDir = "/proc/";

    if (argc == WRITEPROC_ARGC) {
        value = argv[0];
        path = argv[2];
        len = strlen(value) + 1;  /* +1:add the \0 */
        if (strncmp(argv[1], ">>", strlen(">>")) == 0) {
            if ((realpath(path, realPath) == NULL) || (strncmp(realPath, rootProcDir, strlen(rootProcDir)) != 0)) {
                PRINT_ERR("No such file or directory\n");
                return PROC_ERROR;
            }

            handle = OpenProcFile(realPath, O_TRUNC);
            if (handle == NULL) {
                PRINT_ERR("No such file or directory\n");
                return PROC_ERROR;
            }

            ret = WriteProcFile(handle, value, len);
            if (ret < 0) {
                (void)CloseProcFile(handle);
                PRINT_ERR("write error\n");
                return PROC_ERROR;
            }
            for (i = 0; i < argc; i++) {
                PRINTK("%s%s", i > 0 ? " " : "", argv[i]);
            }
            PRINTK("\n");
            (void)CloseProcFile(handle);
            return LOS_OK;
        }
    }
    PRINT_ERR("writeproc [data] [>>] [path]\n");
    return PROC_ERROR;
}

SHELLCMD_ENTRY(writeproc_shellcmd, CMD_TYPE_EX, "writeproc", XARGS, (CmdCallBackFunc)OsShellCmdWriteProc);
#endif
