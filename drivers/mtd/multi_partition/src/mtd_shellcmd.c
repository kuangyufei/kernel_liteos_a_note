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

#include "mtd_partition.h"
#include "stdlib.h"
#include "stdio.h"
#include "los_config.h"

#ifdef LOSCFG_SHELL_CMD_DEBUG
#include "shcmd.h"
/***********************************************************
*	partition命令用来查看flash分区信息
*	partition [nand / spinor]

参数		参数说明						取值范围
nand	显示nand flash分区信息。			N/A
spinor	显示spinor flash分区信息。			N/A

partition命令用来查看flash分区信息。
仅当使能yaffs文件系统时才可以查看nand flash分区信息，
使能jffs或romfs文件系统时可以查看spinor flash分区信息。

举例：partition spinor
https://weharmony.github.io/openharmony/zh-cn/device-dev/kernel/partition.html
*
***********************************************************/
INT32 osShellCmdPartitionShow(INT32 argc, const CHAR **argv)
{
    mtd_partition *node = NULL;
    const CHAR *fs = NULL;
    partition_param *param = NULL;

    if (argc != 1) {//只接受一个参数
        PRINT_ERR("partition [nand/spinor]\n");
        return -EPERM;
    } else {
        fs = argv[0];
    }

    if (strcmp(fs, "nand") == 0) { // #partition nand
        param = GetNandPartParam(); //获取 nand flash 信息
    } else if (strcmp(fs, "spinor") == 0) { // #partition spinor
        param = GetSpinorPartParam(); //获取 spi nor flash 信息
    } else {
        PRINT_ERR("not supported!\n");
        return -EINVAL;
    }

    if ((param == NULL) || (param->flash_mtd == NULL)) {
        PRINT_ERR("no partition!\n");
        return -EINVAL;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(node, &param->partition_head->node_info, mtd_partition, node_info) {
        PRINTK("%s partition num:%u, blkdev name:%s, mountpt:%s, startaddr:0x%08x, length:0x%08x\n",
            fs, node->patitionnum, node->blockdriver_name, node->mountpoint_name,
            (node->start_block * param->block_size),
            ((node->end_block - node->start_block) + 1) * param->block_size);
    }
    return ENOERR;
}

SHELLCMD_ENTRY(partition_shellcmd, CMD_TYPE_EX, "partition", XARGS, (CmdCallBackFunc)osShellCmdPartitionShow);

#endif /* LOSCFG_SHELL */
