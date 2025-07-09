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
/**
 * @brief 处理shell格式化命令的入口函数
 * @details 解析格式化命令参数，验证参数合法性，并调用底层格式化函数
 * @param argc 命令参数个数
 * @param argv 命令参数字符串数组
 * @return 0 - 执行成功，非0 - 执行失败
 */
int osShellCmdFormat(int argc, char **argv)
{
    if (argc < 3) { /* 参数数量检查：至少需要3个参数 */
        perror("format error");  // 输出错误信息
        PRINTK("Usage  :\n");  // 打印使用说明
        PRINTK("        format <dev_vnodename> <sectors> <option> <label>\n");  // 命令格式
        PRINTK("        dev_vnodename : the name of dev\n");  // 设备节点名参数说明
        PRINTK("        sectors       : Size of allocation unit in unit of byte or sector, ");  // 扇区大小参数说明
        PRINTK("0 instead of default size\n");  // 0表示使用默认大小
        PRINTK("        options       : Index of filesystem. 1 for FAT filesystem, ");  // 文件系统类型参数说明
        PRINTK("2 for FAT32 filesystem, 7 for any, 8 for erase\n");  // 各数值对应的文件系统类型
        PRINTK("        label         : The volume of device. It will be emptyed when this parameter is null\n");  // 卷标参数说明
        PRINTK("Example:\n");  // 示例用法
        PRINTK("        format /dev/mmcblk0 128 2\n");  // 格式化示例命令

        set_errno(EINVAL);  // 设置错误号为参数无效
        return 0;  // 返回
    }

    if (argc == 4) { /* 参数数量为4时处理卷标 */
        /* 检查卷标参数是否为NULL或null */
        if (strncmp(argv[3], "NULL", strlen(argv[3])) == 0 || strncmp(argv[3], "null", strlen(argv[3])) == 0) {
            set_label(NULL);  // 设置卷标为空
        } else {
            set_label(argv[3]);  // 设置卷标为输入值
        }
    }
        /* 调用格式化函数，传入设备名、扇区大小和文件系统类型 */
    if (format(argv[0], atoi(argv[1]), atoi(argv[2])) == 0) {
        PRINTK("format %s Success \n", argv[0]);  // 格式化成功提示
    } else {
        perror("format error");  // 格式化失败提示
    }

    return 0;  // 函数返回
}

/**
 * @brief 注册shell格式化命令
 * @details 将format命令添加到shell命令表，指定命令类型和回调函数
 */
SHELLCMD_ENTRY(format_shellcmd, CMD_TYPE_EX, "format", XARGS, (CmdCallBackFunc)osShellCmdFormat);  // 注册format命令到shell
#endif
