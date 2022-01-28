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
#ifdef LOSCFG_SHELL_CMD_DEBUG
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
#include "los_cpup_pri.h"
#endif
#include "los_hwi_pri.h"
#include "los_sys_pri.h"
#include "shcmd.h"


#ifdef LOSCFG_CPUP_INCLUDE_IRQ
STATIC CPUP_INFO_S hwiCpupAll[OS_HWI_MAX_NUM];
STATIC CPUP_INFO_S hwiCpup10s[OS_HWI_MAX_NUM];
STATIC CPUP_INFO_S hwiCpup1s[OS_HWI_MAX_NUM];
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdHwi(INT32 argc, const CHAR **argv)
{
    UINT32 i;
    UINT64 cycles;
    size_t size = sizeof(CPUP_INFO_S) * OS_HWI_MAX_NUM;
    OsIrqCpupCB *irqCpup = OsGetIrqCpupArrayBase();

    (VOID)argv;
    if (argc > 0) {
        PRINTK("\nUsage: hwi\n");
        return OS_ERROR;
    }

    if (irqCpup == NULL) {
        return OS_ERROR;
    }

    (VOID)LOS_GetAllIrqCpuUsage(CPUP_ALL_TIME, hwiCpupAll, size);
    (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_TEN_SECONDS, hwiCpup10s, size);
    (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_ONE_SECONDS, hwiCpup1s, size);

    PRINTK(" InterruptNo      Count  ATime(us)   CPUUSE  CPUUSE10s  CPUUSE1s   Mode Name\n");
    for (i = OS_HWI_FORM_EXC_NUM; i < OS_HWI_MAX_NUM + OS_HWI_FORM_EXC_NUM; i++) {
        UINT32 count = OsGetHwiFormCnt(i);
        if (count) {
            cycles = (((OsIrqCpupCB *)(&irqCpup[i]))->cpup.allTime * OS_NS_PER_CYCLE) / (count * OS_SYS_NS_PER_US);
        } else {
            cycles = 0;
        }
        /* Different cores has different hwi form implementation */
        if (HWI_IS_REGISTED(i)) {
            PRINTK(" %10d:%11u%11llu", i, count, cycles);
        } else {
            continue;
        }
        PRINTK("%6u.%-2u%8u.%-2u%7u.%-2u%7s %-12s\n",
               hwiCpupAll[i].usage / LOS_CPUP_PRECISION_MULT,
               hwiCpupAll[i].usage % LOS_CPUP_PRECISION_MULT,
               hwiCpup10s[i].usage / LOS_CPUP_PRECISION_MULT,
               hwiCpup10s[i].usage % LOS_CPUP_PRECISION_MULT,
               hwiCpup1s[i].usage / LOS_CPUP_PRECISION_MULT,
               hwiCpup1s[i].usage % LOS_CPUP_PRECISION_MULT,
               (g_hwiForm[i].uwParam == IRQF_SHARED) ? "shared" : "normal",
               (OsGetHwiFormName(i) != NULL) ? OsGetHwiFormName(i) : "");
    }
    return 0;
}
#else
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdHwi(INT32 argc, const CHAR **argv)
{
    UINT32 i;

    (VOID)argv;
    if (argc > 0) {
        PRINTK("\nUsage: hwi\n");
        return OS_ERROR;
    }

    PRINTK(" InterruptNo     Count     Name\n");
    for (i = OS_HWI_FORM_EXC_NUM; i < OS_HWI_MAX_NUM + OS_HWI_FORM_EXC_NUM; i++) {
        /* Different cores has different hwi form implementation */
        if (HWI_IS_REGISTED(i) && (OsGetHwiFormName(i) != NULL)) {
            PRINTK(" %8d:%10d:      %-s\n", i, OsGetHwiFormCnt(i), OsGetHwiFormName(i));
        } else if (HWI_IS_REGISTED(i)) {
            PRINTK(" %8d:%10d:\n", i, OsGetHwiFormCnt(i));
        }
    }
    return 0;
}
#endif

SHELLCMD_ENTRY(hwi_shellcmd, CMD_TYPE_EX, "hwi", 0, (CmdCallBackFunc)OsShellCmdHwi);

#endif /* LOSCFG_SHELL */
