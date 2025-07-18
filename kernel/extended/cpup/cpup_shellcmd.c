/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

/**
 * @brief 根据模式参数查询并打印系统CPU使用率
 * @param mode 查询模式：CPUP_LAST_TEN_SECONDS(10秒内)、CPUP_LAST_ONE_SECONDS(1秒内)、其他值(所有时间)
 * @return 无
 */
VOID OsCmdCpupOperateOneParam(UINT32 mode)
{
    UINT32 ret;  /* CPU使用率查询结果 */

    if (mode == CPUP_LAST_TEN_SECONDS) {                       // 查询最近10秒系统CPU使用率
        PRINTK("\nSysCpuUsage in 10s: ");
    } else if (mode == CPUP_LAST_ONE_SECONDS) {                // 查询最近1秒系统CPU使用率
        PRINTK("\nSysCpuUsage in 1s: ");
    } else {                                                   // 查询所有时间系统CPU使用率
        PRINTK("\nSysCpuUsage in all time: ");
    }
    ret = LOS_HistorySysCpuUsage(mode);                        // 调用系统CPU使用率查询接口
    PRINTK("%u.%u\n", ret / LOS_CPUP_PRECISION_MULT, ret % LOS_CPUP_PRECISION_MULT);  // 打印格式化的使用率结果
}

/**
 * @brief 根据模式和进程ID查询并打印指定进程CPU使用率
 * @param mode 查询模式：CPUP_LAST_TEN_SECONDS(10秒内)、CPUP_LAST_ONE_SECONDS(1秒内)、其他值(所有时间)
 * @param pid 进程ID
 * @return 无
 */
VOID OsCmdCpupOperateTwoParam(UINT32 mode, UINT32 pid)
{
    UINT32 ret;  /* CPU使用率查询结果 */

    if (mode == CPUP_LAST_TEN_SECONDS) {                       // 查询最近10秒进程CPU使用率
        PRINTK("\npid %u CpuUsage in 10s: ", pid);
    } else if (mode == CPUP_LAST_ONE_SECONDS) {                // 查询最近1秒进程CPU使用率
        PRINTK("\npid %u CpuUsage in 1s: ", pid);
    } else {                                                   // 查询所有时间进程CPU使用率
        PRINTK("\npid %u CpuUsage in all time: ", pid);
    }
    ret = LOS_HistoryProcessCpuUsage(pid, mode);               // 调用进程CPU使用率查询接口
    PRINTK("%u.%u\n", ret / LOS_CPUP_PRECISION_MULT, ret % LOS_CPUP_PRECISION_MULT);  // 打印格式化的使用率结果
}

/**
 * @brief 打印cpup命令帮助信息
 * @return 无
 */
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

/**
 * @brief cpup shell命令处理函数
 * @param argc 命令参数个数
 * @param argv 命令参数数组
 * @return 操作结果，LOS_OK表示成功
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdCpup(INT32 argc, const CHAR **argv)
{
    size_t mode, pid;       /* mode:查询模式, pid:进程ID */
    CHAR *bufMode = NULL;   /* 模式字符串转换缓冲区 */
    CHAR *bufID = NULL;     /* PID字符串转换缓冲区 */
    UINT32 ret;             /* 函数返回值 */

    if (argc == 0) {                                            // 无参数时默认查询10秒内系统CPU使用率
        ret = LOS_HistorySysCpuUsage(CPUP_LAST_TEN_SECONDS);
        PRINTK("\nSysCpuUsage in 10s: %u.%u\n", ret / LOS_CPUP_PRECISION_MULT, ret % LOS_CPUP_PRECISION_MULT);
        return LOS_OK;
    }

    if (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help")) {  // 处理帮助命令
        OsCpupCmdHelp();
        return LOS_OK;
    }

    mode = strtoul(argv[0], &bufMode, 0);                       // 将字符串参数转换为数值模式
    if ((bufMode == NULL) || (*bufMode != 0)) {                 // 检查模式参数是否合法
        PRINTK("\nUnknown mode: %s\n", argv[0]);
        OsCpupCmdHelp();
        return LOS_OK;
    }

    if (mode > CPUP_ALL_TIME) {                                 // 模式值超出范围时默认设为所有时间
        mode = CPUP_ALL_TIME;
    }

    if (argc == 1) {                                            // 单参数时查询系统CPU使用率
        OsCmdCpupOperateOneParam(mode);
        return LOS_OK;
    }

    pid = strtoul(argv[1], &bufID, 0);                          // 将字符串参数转换为进程ID
    if (OsProcessIDUserCheckInvalid(pid) || (*bufID != 0)) {    // 检查PID是否合法
        PRINTK("\nUnknown pid: %s\n", argv[1]);
        return LOS_OK;
    }

    if (OsProcessIsUnused(OS_PCB_FROM_PID(pid)) || OsProcessIsDead(OS_PCB_FROM_PID(pid))) {  // 检查进程是否存在且活跃
        PRINTK("\nUnknown pid: %u\n", pid);
        return LOS_OK;
    }

    /* when the parameters number is two */
    if (argc == 2) {                                            // 双参数时查询指定进程CPU使用率
        OsCmdCpupOperateTwoParam(mode, pid);
        return LOS_OK;
    }

    OsCpupCmdHelp();                                            // 参数个数错误时打印帮助信息
    return LOS_OK;
}

// 注册cpup shell命令，命令类型为CMD_TYPE_EX，参数为XARGS，回调函数为OsShellCmdCpup
SHELLCMD_ENTRY(cpup_shellcmd, CMD_TYPE_EX, "cpup", XARGS, (CmdCallBackFunc)OsShellCmdCpup);
#endif /* LOSCFG_SHELL */
