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
/**
 * @brief   显示指定类型Flash的分区信息
 * @param   argc    命令参数个数
 * @param   argv    命令参数列表，需指定"nand"或"spinor"
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 osShellCmdPartitionShow(INT32 argc, const CHAR **argv)
{
    mtd_partition *node = NULL;        // 分区节点指针
    const CHAR *fs = NULL;             // Flash类型字符串指针
    partition_param *param = NULL;     // 分区参数结构体指针

    // 检查参数个数是否正确
    if (argc != 1) {
        PRINT_ERR("partition [nand/spinor]\n");  // 打印命令使用方法
        return -EPERM;                            // 返回权限错误
    } else {
        fs = argv[0];                             // 获取Flash类型参数
    }

    // 根据Flash类型获取对应的分区参数
    if (strcmp(fs, "nand") == 0) {
        param = GetNandPartParam();               // 获取NAND Flash分区参数
    } else if (strcmp(fs, "spinor") == 0) {
        param = GetSpinorPartParam();             // 获取SPINOR Flash分区参数
    } else {
        PRINT_ERR("not supported!\n");           // 打印不支持的类型错误
        return -EINVAL;                           // 返回无效参数错误
    }

    // 检查分区参数是否有效
    if ((param == NULL) || (param->flash_mtd == NULL)) {
        PRINT_ERR("no partition!\n");            // 打印无分区错误
        return -EINVAL;                           // 返回无效参数错误
    }

    // 遍历分区链表并打印分区信息
    LOS_DL_LIST_FOR_EACH_ENTRY(node, &param->partition_head->node_info, mtd_partition, node_info) {
        PRINTK("%s partition num:%u, blkdev name:%s, mountpt:%s, startaddr:0x%08x, length:0x%08x\n",
            fs, node->patitionnum, node->blockdriver_name, node->mountpoint_name,
            (node->start_block * param->block_size),  // 计算分区起始地址
            ((node->end_block - node->start_block) + 1) * param->block_size);  // 计算分区大小
    }
    return ENOERR;  // 返回成功
}

// 注册分区显示命令到shell
SHELLCMD_ENTRY(partition_shellcmd, CMD_TYPE_EX, "partition", XARGS, (CmdCallBackFunc)osShellCmdPartitionShow);
#endif /* LOSCFG_SHELL */
