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

#include "stdio.h"
#include "stdlib.h"
#include "los_config.h"
#ifdef LOSCFG_SHELL_CMD_DEBUG
#include "disk.h"
#include "shcmd.h"
#include "shell.h"
#include "fs/path_cache.h"

INT32 osShellCmdPartInfo(INT32 argc, const CHAR **argv)
{
    struct Vnode *node = NULL;
    los_part *part = NULL;
    const CHAR *str = "/dev";
    int ret;

    if ((argc != 1) || (strncmp(argv[0], str, strlen(str)) != 0)) {
        PRINTK("Usage  :\n");
        PRINTK("        partinfo <dev_vnodename>\n");
        PRINTK("        dev_vnodename : the name of dev\n");
        PRINTK("Example:\n");
        PRINTK("        partinfo /dev/sdap0 \n");

        set_errno(EINVAL);
        return -LOS_NOK;
    }
    VnodeHold();
    ret = VnodeLookup(argv[0], &node, 0);
    if (ret < 0) {
        PRINT_ERR("no part found\n");
        VnodeDrop();
        set_errno(ENOENT);
        return -LOS_NOK;
    }

    part = los_part_find(node);
    VnodeDrop();
    show_part(part);

    return LOS_OK;
}

SHELLCMD_ENTRY(partinfo_shellcmd, CMD_TYPE_EX, "partinfo", XARGS, (CmdCallBackFunc)osShellCmdPartInfo);

#endif
