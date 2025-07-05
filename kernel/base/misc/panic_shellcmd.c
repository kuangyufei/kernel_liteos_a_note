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
#include "shcmd.h"
#include "shell.h"
#endif
#include "los_swtmr_pri.h"


#ifdef LOSCFG_SHELL_CMD_DEBUG
// 系统异常重置标志：TRUE表示启用异常重置功能，FALSE表示禁用
STATIC BOOL systemExcReset = FALSE;  // 系统异常重置状态全局变量

/**
 * @brief  获取系统异常重置状态
 * @return BOOL - 系统异常重置状态（TRUE/FALSE）
 */
LITE_OS_SEC_TEXT_MINOR BOOL OsSystemExcIsReset(VOID)
{
    return systemExcReset;  // 返回当前系统异常重置状态
}

#ifdef LOSCFG_DRIVERS_HDF_PLATFORM_WATCHDOG  // 条件编译：如果启用HDF看门狗驱动配置
#include "watchdog_if.h"                     // 包含看门狗接口头文件

// 看门狗定时器间隔（单位：秒）
#define WATCHDOG_TIMER_INTERVAL 5          // 看门狗超时时间：5秒
// 看门狗喂狗间隔（单位：秒）
#define WATCHDOG_TIMER_INTERVAL_HALF (WATCHDOG_TIMER_INTERVAL / 2)  // 喂狗间隔：超时时间的一半

STATIC UINT16 g_swtmrID;                    // 软件定时器ID（用于周期性喂狗）
STATIC BOOL g_wdStarted = FALSE;            // 看门狗启动状态标志
STATIC DevHandle g_wdHandle;                // 看门狗设备句柄

/**
 * @brief  启动看门狗功能
 * @details 打开看门狗设备、设置超时时间、创建周期性喂狗定时器并启动
 */
STATIC void StartWatchdog(void)
{
    int32_t ret;                            // 函数返回值
    if (g_wdStarted) {                      // 如果看门狗已启动
        return;                             // 直接返回
    }

    // 打开看门狗设备（设备索引0）
    ret = WatchdogOpen(0, &g_wdHandle);
    if (ret != HDF_SUCCESS) {               // 如果打开设备失败
        return;                             // 返回
    }
    // 设置看门狗超时时间（单位：秒）
    WatchdogSetTimeout(g_wdHandle, WATCHDOG_TIMER_INTERVAL);

    // 创建周期性喂狗软件定时器
    // 参数：超时 ticks（每秒tick数 * 喂狗间隔秒数）、周期模式、喂狗回调函数、定时器ID指针、回调参数
    if (LOS_SwtmrCreate(LOSCFG_BASE_CORE_TICK_PER_SECOND * WATCHDOG_TIMER_INTERVAL_HALF, LOS_SWTMR_MODE_PERIOD,
                        (SWTMR_PROC_FUNC)WatchdogFeed, &g_swtmrID, (UINTPTR)g_wdHandle) != LOS_OK) {
        WatchdogClose(g_wdHandle);          // 创建定时器失败，关闭看门狗设备
        g_wdHandle = NULL;                  // 重置设备句柄
        return;                             // 返回
    }

    WatchdogStart(g_wdHandle);              // 启动看门狗硬件计时
    LOS_SwtmrStart(g_swtmrID);              // 启动喂狗软件定时器
    g_wdStarted = TRUE;                     // 更新看门狗启动状态标志
}

/**
 * @brief  停止看门狗功能
 * @details 停止并删除喂狗定时器、停止并关闭看门狗设备
 */
STATIC void StopWatchdog(void)
{
    if (!g_wdStarted) {                     // 如果看门狗未启动
        return;                             // 直接返回
    }

    LOS_SwtmrStop(g_swtmrID);               // 停止喂狗软件定时器
    LOS_SwtmrDelete(g_swtmrID);             // 删除喂狗软件定时器
    g_swtmrID = 0;                          // 重置定时器ID

    WatchdogStop(g_wdHandle);               // 停止看门狗硬件计时
    WatchdogClose(g_wdHandle);              // 关闭看门狗设备
    g_wdHandle = NULL;                      // 重置设备句柄

    g_wdStarted = FALSE;                    // 更新看门狗启动状态标志
}

/**
 * @brief  系统异常重置shell命令处理函数
 * @param[in]  argc - 命令参数个数
 * @param[in]  argv - 命令参数列表
 * @return UINT32 - 执行结果（LOS_OK表示成功）
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdSystemExcReset(INT32 argc, const CHAR **argv)
{
    if (argc != 1) {                        // 如果参数个数不为1
        goto EXC_RESET_HELP;                // 跳转到帮助信息输出
    }

    if (strcmp(argv[0], "on") == 0) {       // 如果参数为"on"
        systemExcReset = TRUE;              // 启用系统异常重置
        StartWatchdog();                    // 启动看门狗
        PRINTK("panicreset on!\n");         // 输出开启提示
        return LOS_OK;                      // 返回成功
    }

    if (strcmp(argv[0], "off") == 0) {      // 如果参数为"off"
        systemExcReset = FALSE;             // 禁用系统异常重置
        StopWatchdog();                     // 停止看门狗
        PRINTK("panicreset off!\n");        // 输出关闭提示
        return LOS_OK;                      // 返回成功
    }

EXC_RESET_HELP:                             // 帮助信息标签
    PRINTK("usage: panicreset [on/off]\n"); // 输出命令用法
    return LOS_OK;                          // 返回成功
}
#endif  // LOSCFG_DRIVERS_HDF_PLATFORM_WATCHDOG

SHELLCMD_ENTRY(panic_reset_shellcmd, CMD_TYPE_EX, "panicreset", 1, (CmdCallBackFunc)OsShellCmdSystemExcReset);
#endif
#endif

