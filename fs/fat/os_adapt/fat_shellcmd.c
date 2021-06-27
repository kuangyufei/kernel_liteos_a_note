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

#include "los_config.h"

#if defined(LOSCFG_SHELL) && defined(LOSCFG_FS_FAT)
#include "stdlib.h"
#include "stdio.h"
#include "errno.h"
#include "shcmd.h"
#include "shell.h"
#include "fatfs.h"

int osShellCmdFormat(int argc, char **argv)
{
    if (argc < 3) { /* 3, at least 3 params for this shell command. */
        perror("format error");
        PRINTK("Usage  :\n");
        PRINTK("        format <dev_vnodename> <sectors> <option> <label>\n");
        PRINTK("        dev_vnodename : the name of dev\n");
        PRINTK("        sectors       : Size of allocation unit in unit of byte or sector, ");
        PRINTK("0 instead of default size\n");
        PRINTK("        options       : Index of filesystem. 1 for FAT filesystem, ");
        PRINTK("2 for FAT32 filesystem, 7 for any, 8 for erase\n");
        PRINTK("        label         : The volume of device. It will be emptyed when this parameter is null\n");
        PRINTK("Example:\n");
        PRINTK("        format /dev/mmcblk0 128 2\n");

        set_errno(EINVAL);
        return 0;
    }

    if (argc == 4) { /* 4, if the params count is equal to 4, then do this. */
        /* 3, get the param and check. */
        if (strncmp(argv[3], "NULL", strlen(argv[3])) == 0 || strncmp(argv[3], "null", strlen(argv[3])) == 0) {
            set_label(NULL);
        } else {
            set_label(argv[3]);
        }
    }
        /* 0, 1, 2, get the param and check it. */
    if (format(argv[0], atoi(argv[1]), atoi(argv[2])) == 0) {
        PRINTK("format %s Success \n", argv[0]);
    } else {
        perror("format error");
    }

    return 0;
}

SHELLCMD_ENTRY(format_shellcmd, CMD_TYPE_EX, "format", XARGS, (CmdCallBackFunc)osShellCmdFormat);
#endif
