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
#ifdef LOSCFG_SHELL
#include "stdlib.h"
#include "los_cpup_pri.h"
#include "los_process_pri.h"
#include "shcmd.h"
#include "shell.h"


VOID OsCmdCpupOperateOneParam(UINT32 mode)
{
    UINT32 ret;

    if (mode == CPUP_LAST_TEN_SECONDS) {
        PRINTK("\nSysCpuUsage in 10s: ");
    } else if (mode == CPUP_LAST_ONE_SECONDS) {
        PRINTK("\nSysCpuUsage in 1s: ");
    } else {
        PRINTK("\nSysCpuUsage in all time: ");
    }
    ret = LOS_HistorySysCpuUsage(mode);
    PRINTK("%u.%u\n", ret / LOS_CPUP_PRECISION_MULT, ret % LOS_CPUP_PRECISION_MULT);
}

VOID OsCmdCpupOperateTwoParam(UINT32 mode, UINT32 pid)
{
    UINT32 ret;

    if (mode == CPUP_LAST_TEN_SECONDS) {
        PRINTK("\npid %u CpuUsage in 10s: ", pid);
    } else if (mode == CPUP_LAST_ONE_SECONDS) {
        PRINTK("\npid %u CpuUsage in 1s: ", pid);
    } else {
        PRINTK("\npid %u CpuUsage in all time: ", pid);
    }
    ret = LOS_HistoryProcessCpuUsage(pid, mode);
    PRINTK("%u.%u\n", ret / LOS_CPUP_PRECISION_MULT, ret % LOS_CPUP_PRECISION_MULT);
}

LITE_OS_SEC_TEXT STATIC VOID OsCpupCmdHelp(VOID)
{
    PRINTK("usage:\n");
    PRINTK("      cpup\n"
           "      cpup [MODE]\n"
           "      cpup [MODE] [PID] \n");
    PRINTK("\r\nMode parameter description:\n"
           "  0       SysCpuUsage in 10s\n"
           "  1       SysCpuUsage in 1s\n"
           "  others  SysCpuUsage in all time\n");
}
/******************************************************
命令功能
cpup命令用于查询系统CPU的占用率。
命令格式
cpup [mode] [taskID]
参数		参数说明		取值范围
mode
		● 缺省：显示系统最近10s内的CPU占用率。
		● 0：显示系统最近10s内的CPU占用率。
		● 1：显示系统最近1s内的CPU占用率。
		● 其他数字：显示系统启动至今总的CPU 占用率。
taskID	任务ID号		[0,0xFFFFFFFF]

使用指南
参数缺省时，显示系统10s前的CPU占用率。
只有一个参数时，该参数为mode，显示系统相应时间前的CPU占用率。
输入两个参数时，第一个参数为mode，第二个参数为taskID，显示对应ID号任务的相应时间前的CPU占用率。

举例：输入cpup 1 5 表示 显示5号任务最近1s内的CPU占用率 
******************************************************/
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdCpup(INT32 argc, const CHAR **argv)
{
    size_t mode, pid;
    CHAR *bufMode = NULL;
    CHAR *bufID = NULL;
    UINT32 ret;

    if (argc == 0) {
        ret = LOS_HistorySysCpuUsage(CPUP_LAST_TEN_SECONDS);
        PRINTK("\nSysCpuUsage in 10s: %u.%u\n", ret / LOS_CPUP_PRECISION_MULT, ret % LOS_CPUP_PRECISION_MULT);
        return LOS_OK;
    }

    if (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help")) {
        OsCpupCmdHelp();
        return LOS_OK;
    }

    mode = strtoul(argv[0], &bufMode, 0);
    if ((bufMode == NULL) || (*bufMode != 0)) {
        PRINTK("\nUnknown mode: %s\n", argv[0]);
        OsCpupCmdHelp();
        return LOS_OK;
    }

    if (mode > CPUP_ALL_TIME) {
        mode = CPUP_ALL_TIME;//统计所有CPU的耗时
    }

    if (argc == 1) {
        OsCmdCpupOperateOneParam(mode);//处理只有一个参数的情况 例如 cpup 1
        return LOS_OK;
    }

    pid = strtoul(argv[1], &bufID, 0);//musl标准库函数,将字符串转成无符号整型
    if (OsProcessIDUserCheckInvalid(pid) || (*bufID != 0)) {
        PRINTK("\nUnknown pid: %s\n", argv[1]);
        return LOS_OK;
    }

    if (OsProcessIsDead(OS_PCB_FROM_PID(pid))) {
        PRINTK("\nUnknown pid: %u\n", pid);
        return LOS_OK;
    }

    /* when the parameters number is two */
    if (argc == 2) {
        OsCmdCpupOperateTwoParam(mode, pid);//处理有两个参数的情况 例如 cpup 1 5
        return LOS_OK;
    }

    OsCpupCmdHelp();
    return LOS_OK;
}

SHELLCMD_ENTRY(cpup_shellcmd, CMD_TYPE_EX, "cpup", XARGS, (CmdCallBackFunc)OsShellCmdCpup);//采用shell命令静态注册方式
#endif /* LOSCFG_SHELL */
