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
/**
 * @brief  shell帮助命令实现，用于显示所有支持的shell命令
 * @param  argc [IN] 命令参数个数
 * @param  argv [IN] 命令参数列表
 * @return UINT32 成功返回0，失败返回OS_ERROR
 */
UINT32 OsShellCmdHelp(UINT32 argc, const CHAR **argv)
{
    UINT32 loop = 0;                                 // 循环计数器，用于命令列表格式化输出
    CmdItemNode *curCmdItem = NULL;                  // 命令项节点指针，用于遍历命令列表
    CmdModInfo *cmdInfo = OsCmdInfoGet();            // 获取命令模块信息，包含所有已注册的命令

    (VOID)argv;                                      // 未使用参数，避免编译警告
    if (argc > 0) {                                  // 检查是否有多余参数
        PRINTK("\nUsage: help\n");                   // 输出命令使用方法
        return OS_ERROR;                             // 参数错误，返回错误码
    }

    PRINTK("*******************shell commands:*************************\n");  // 打印命令列表标题
    // 遍历命令链表，获取每个命令项节点
    LOS_DL_LIST_FOR_EACH_ENTRY(curCmdItem, &(cmdInfo->cmdList.list), CmdItemNode, list) {
        if ((loop % CMD_ITEM_PER_LINE) == 0) {       // 每CMD_ITEM_PER_LINE个命令换行，实现格式化输出
            PRINTK("\n");
        }
        PRINTK("%-12s ", curCmdItem->cmd->cmdKey);   // 左对齐输出命令关键字，占12个字符宽度

        loop++;
    }
    // 打印shell使用说明
    PRINTK("\n\nAfter shell prompt \"OHOS # \":\n"
           "Use `<cmd> [args ...]` to run built-in shell commands listed above.\n"
           "Use `exec <cmd> [args ...]` or `./<cmd> [args ...]` to run external commands.\n");
    return 0;                                       // 成功返回
}

SHELLCMD_ENTRY(help_shellcmd, CMD_TYPE_EX, "help", 0, (CmdCallBackFunc)OsShellCmdHelp);