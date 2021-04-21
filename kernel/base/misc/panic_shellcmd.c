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
#include "watchdog_if.h"


#ifdef LOSCFG_SHELL_CMD_DEBUG
STATIC BOOL systemExcReset = FALSE;

LITE_OS_SEC_TEXT_MINOR BOOL OsSystemExcIsReset(VOID)
{
    return systemExcReset;
}
#ifdef LOSCFG_DRIVERS_HDF_PLATFORM_WATCHDOG

#define WATCHDOG_TIMER_INTERVAL 5 // 5 seconds
#define WATCHDOG_TIMER_INTERVAL_HALF (WATCHDOG_TIMER_INTERVAL / 2)

STATIC UINT16 g_swtmrID;
STATIC BOOL g_wdStarted = FALSE;
STATIC DevHandle g_wdHandle;

STATIC void StartWatchdog(void)
{
    if (g_wdStarted) {
        return;
    }

    g_wdHandle = WatchdogOpen(0);
    WatchdogSetTimeout(g_wdHandle, WATCHDOG_TIMER_INTERVAL);

    if (LOS_SwtmrCreate(LOSCFG_BASE_CORE_TICK_PER_SECOND * WATCHDOG_TIMER_INTERVAL_HALF, LOS_SWTMR_MODE_PERIOD,
                        (SWTMR_PROC_FUNC)WatchdogFeed, &g_swtmrID, (UINTPTR)g_wdHandle) != LOS_OK) {
        WatchdogClose(g_wdHandle);
        g_wdHandle = NULL;
        return;
    }

    WatchdogStart(g_wdHandle);
    LOS_SwtmrStart(g_swtmrID);
    g_wdStarted = TRUE;
}

STATIC void StopWatchdog(void)
{
    if (!g_wdStarted) {
        return;
    }

    LOS_SwtmrStop(g_swtmrID);
    LOS_SwtmrDelete(g_swtmrID);
    g_swtmrID = 0;

    WatchdogStop(g_wdHandle);
    WatchdogClose(g_wdHandle);
    g_wdHandle = NULL;

    g_wdStarted = FALSE;
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdSystemExcReset(INT32 argc, const CHAR **argv)
{
    if (argc != 1) {
        goto EXC_RESET_HELP;
    }

    if (strcmp(argv[0], "on") == 0) {
        systemExcReset = TRUE;
        StartWatchdog();
        PRINTK("panicreset on!\n");
        return LOS_OK;
    }

    if (strcmp(argv[0], "off") == 0) {
        systemExcReset = FALSE;
        StopWatchdog();
        PRINTK("panicreset off!\n");
        return LOS_OK;
    }

EXC_RESET_HELP:
    PRINTK("usage: panicreset [on/off]\n");
    return LOS_OK;
}

SHELLCMD_ENTRY(panic_reset_shellcmd, CMD_TYPE_EX, "panicreset", 1, (CmdCallBackFunc)OsShellCmdSystemExcReset);
#endif
#endif

