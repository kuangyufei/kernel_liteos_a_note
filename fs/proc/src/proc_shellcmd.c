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
/*****************************************************************
鸿蒙内核提供了一种通过 /proc 文件系统，在运行时访问内核内部数据结构、改变内核设置的机制。
proc文件系统是一个伪文件系统，它只存在内存当中，而不占用外存空间。它以文件系统的方式为
访问系统内核数据的操作提供接口。
用户和应用程序可以通过proc得到系统的信息，并可以改变内核的某些参数。由于系统的信息，
如进程，是动态改变的，所以用户或应用程序读取proc文件时，proc文件系统是动态从系统内核读出
所需信息并提交的。

proc fs支持传入字符串参数，需要每个文件实现自己的写方法。
命令格式
writeproc <data> >> /proc/<filename>

参数说明
参数			参数说明		
data		要输入的字符串，以空格为结束符，如需输入空格，请用""包裹。
filename	data要传入的proc文件。

proc文件实现自身的write函数，调用writeproc命令后会将入参传入write函数。
	注意:procfs暂不支持多线程访问
	
OHOS # writeproc test >> /proc/uptime

[INFO]write buf is: test
test >> /proc/uptime
 说明： uptime proc文件临时实现write函数，INFO日志为实现的测试函数打印的日志。

*****************************************************************/
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

    if (argc == WRITEPROC_ARGC) {//argv[0] = ">>"
        value = argv[0];//test
        path = argv[2];///proc/uptime
        len = strlen(value) + 1;  /* +1:add the \0 */
        if (strncmp(argv[1], ">>", strlen(">>")) == 0) { //  第二个参数必须得是 >>
            if ((realpath(path, realPath) == NULL) || (strncmp(realPath, rootProcDir, strlen(rootProcDir)) != 0)) {
                PRINT_ERR("No such file or directory\n");
                return PROC_ERROR;
            }

            handle = OpenProcFile(realPath, O_TRUNC);//打开 proc 文件
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
