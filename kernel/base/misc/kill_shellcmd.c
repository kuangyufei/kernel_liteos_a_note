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
#include "los_signal.h"
#include "los_printf.h"
#include "los_task_pri.h"
#include "los_process_pri.h"

#ifdef LOSCFG_BASE_CORE_HILOG
#include "log.h"
#else
#define HILOG_INFO(type, fmt, ...)    PRINT_INFO(fmt, __VA_ARGS__)
#define HILOG_ERROR(type, fmt, ...)   PRINT_ERR(fmt, __VA_ARGS__)
#endif

#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif

LITE_OS_SEC_TEXT_MINOR VOID OsPrintKillUsage(VOID)
{
    PRINTK("\nkill: usage: kill [sigspec] [pid]\n");
}
/********************************************* 
命令功能
命令用于发送特定信号给指定进程。

命令格式
kill [signo | -signo] [pid]

参数		参数说明		取值范围
signo	信号ID	[1,30]
pid		进程ID	[1,MAX_INT]

须知： signo有效范围为[0,64]，建议取值范围为[1,30]，其余为保留内容。

使用指南
必须指定发送的信号编号及进程号。
进程编号取值范围根据系统配置变化，例如系统最大支持pid为256，则取值范围缩小为[1-256]。
*********************************************/
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdKill(INT32 argc, const CHAR **argv)
{
#define  ARG_NUM 2
    INT32 sigNo = 0;
    INT32 pidNo = 0;
    INT32 ret;
    CHAR *endPtr = NULL;

    if (argc == ARG_NUM) {
        sigNo = strtoul(argv[0], &endPtr, 0);
        if (*endPtr != 0) {
            PRINT_ERR("\nsigNo can't access %s.\n", argv[0]);
            goto ERROR;
        }
        endPtr = NULL;
        pidNo = strtoul(argv[1], &endPtr, 0);
        if (*endPtr != 0) {
            PRINT_ERR("\npidNo can't access %s.\n", argv[1]);
            goto ERROR;
        }

        ret = OsKill(pidNo, abs(sigNo), OS_USER_KILL_PERMISSION);
        HILOG_INFO(LOG_CORE, "Send signal(%d) to pidNo = %d!\n", abs(sigNo), pidNo);
        if (ret == -1) {
            HILOG_ERROR(LOG_CORE, "Kill fail ret = %d! Operation not permitted\n", ret);
            goto ERROR;
        }
        if (ret < 0) {
            PRINT_ERR("\n Kill fail ret = %d! process not exist or sigNo is invalid\n", ret);
            goto ERROR;
        }
    } else {
        PRINT_ERR("\nPara number errno!\n");
        goto ERROR;
    }
    return 0;
ERROR:
    OsPrintKillUsage();
    return 0;
}

#ifdef LOSCFG_SHELL
SHELLCMD_ENTRY(kill_shellcmd, CMD_TYPE_EX, "kill", 2, (CmdCallBackFunc)OsShellCmdKill);
#endif

