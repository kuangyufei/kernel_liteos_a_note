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

#include "shcmd.h"
#define DEFAULT_SCREEN_WIDTH 80
#define MAX_CMD_KEY_WIDTH    12
#define CMD_ITEM_PER_LINE    (DEFAULT_SCREEN_WIDTH / (MAX_CMD_KEY_WIDTH + 1))

/*
help命令用于显示当前操作系统内所有操作指令

OHOS # help
*******************shell commands:*************************

arp           cat           cd            chgrp         chmod         chown         cp            cpup          
date          dhclient      dmesg         dns           format        free          help          hwi           
ifconfig      ipdebug       kill          log           ls            lsfd          memcheck      mkdir         
mount         netstat       oom           partinfo      partition     ping          ping6         pwd           
reset         rm            rmdir         sem           statfs        su            swtmr         sync          
systeminfo    task          telnet        tftp          touch         umount        uname         watch         
writeproc     
*/
UINT32 OsShellCmdHelp(UINT32 argc, const CHAR **argv)
{
    UINT32 loop = 0;
    CmdItemNode *curCmdItem = NULL;
    CmdModInfo *cmdInfo = OsCmdInfoGet();

    (VOID)argv;
    if (argc > 0) {
        PRINTK("\nUsage: help\n");
        return OS_ERROR;
    }

    PRINTK("*******************shell commands:*************************\n");
    LOS_DL_LIST_FOR_EACH_ENTRY(curCmdItem, &(cmdInfo->cmdList.list), CmdItemNode, list) {//遍历命令链表
        if ((loop % CMD_ITEM_PER_LINE) == 0) { /* just align print */
            PRINTK("\n");
        }
        PRINTK("%-12s ", curCmdItem->cmd->cmdKey);

        loop++;
    }
    PRINTK("\n\nAfter shell prompt \"OHOS # \":\n"
           "Use `<cmd> [args ...]` to run built-in shell commands listed above.\n"
           "Use `exec <cmd> [args ...]` or `./<cmd> [args ...]` to run external commands.\n");
    return 0;
}

SHELLCMD_ENTRY(help_shellcmd, CMD_TYPE_EX, "help", 0, (CmdCallBackFunc)OsShellCmdHelp);