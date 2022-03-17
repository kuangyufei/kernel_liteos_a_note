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
#define IRQ_CPUP_INFO_SIZE     (sizeof(CPUP_INFO_S) * OS_HWI_MAX_NUM * LOSCFG_KERNEL_CORE_NUM)
#define IRQ_CPUP_ALL_INFO_SIZE (3 * IRQ_CPUP_INFO_SIZE)
#define IRQ_DATA_SZIE          (sizeof(OsIrqCpupCB) * LOSCFG_KERNEL_CORE_NUM)
#define CPUP_PRECISION_MULT    LOS_CPUP_PRECISION_MULT

STATIC VOID ShellCmdHwiInfoShow(OsIrqCpupCB *irqData, CPUP_INFO_S *hwiCpup1s,
                                CPUP_INFO_S *hwiCpup10s, CPUP_INFO_S *hwiCpupAll)
{
    UINT32 intSave;
    OsIrqCpupCB *irqDataBase = OsGetIrqCpupArrayBase();

    for (UINT32 i = OS_HWI_FORM_EXC_NUM; i < OS_HWI_MAX_NUM + OS_HWI_FORM_EXC_NUM; i++) {
        if (!HWI_IS_REGISTED(i)) {
            continue;
        }

        intSave = LOS_IntLock();
        (VOID)memcpy_s(irqData, IRQ_DATA_SZIE, &irqDataBase[i * LOSCFG_KERNEL_CORE_NUM], IRQ_DATA_SZIE);
        LOS_IntRestore(intSave);

        for (UINT32 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
            UINT64 cycles = 0;
            UINT64 timeMax = 0;
            OsIrqCpupCB *data = &irqData[cpu];
            if (data->status == 0) {
                continue;
            }
            UINT32 count = OsGetHwiFormCnt(cpu, i);
            if (count != 0) {
                if (data->count != 0) {
                    cycles = (data->allTime * OS_NS_PER_CYCLE) / (data->count * OS_SYS_NS_PER_US);
                }
                timeMax = (data->timeMax * OS_NS_PER_CYCLE) / 1000;
            }
            CHAR *irqName = OsGetHwiFormName(i);
            UINT32 index = (i * LOSCFG_KERNEL_CORE_NUM) + cpu;
            PRINTK(" %10u:%5u%11u%11llu%10llu%6u.%-2u%8u.%-2u%7u.%-2u%7s %-12s\n", i, cpu, count, cycles, timeMax,
                   hwiCpupAll[index].usage / CPUP_PRECISION_MULT, hwiCpupAll[index].usage % CPUP_PRECISION_MULT,
                   hwiCpup10s[index].usage / CPUP_PRECISION_MULT, hwiCpup10s[index].usage % CPUP_PRECISION_MULT,
                   hwiCpup1s[index].usage / CPUP_PRECISION_MULT, hwiCpup1s[index].usage % CPUP_PRECISION_MULT,
                   (g_hwiForm[index].uwParam == IRQF_SHARED) ? "shared" : "normal", (irqName != NULL) ? irqName : "");
        }
    }
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdHwi(INT32 argc, const CHAR **argv)
{
    UINT32 size;

    (VOID)argv;
    if (argc > 0) {
        PRINTK("\nUsage: hwi\n");
        return OS_ERROR;
    }

    size = IRQ_CPUP_ALL_INFO_SIZE + IRQ_DATA_SZIE;
    CHAR *irqCpup = LOS_MemAlloc(m_aucSysMem0, size);
    if (irqCpup == NULL) {
        return OS_ERROR;
    }

    CPUP_INFO_S *hwiCpupAll = (CPUP_INFO_S *)irqCpup;
    CPUP_INFO_S *hwiCpup10s = (CPUP_INFO_S *)(irqCpup + IRQ_CPUP_INFO_SIZE);
    CPUP_INFO_S *hwiCpup1s = (CPUP_INFO_S *)(irqCpup + 2 * IRQ_CPUP_INFO_SIZE); /* 2: offset */
    OsIrqCpupCB *irqData = (OsIrqCpupCB *)(irqCpup + IRQ_CPUP_ALL_INFO_SIZE);

    (VOID)LOS_GetAllIrqCpuUsage(CPUP_ALL_TIME, hwiCpupAll, IRQ_CPUP_INFO_SIZE);
    (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_TEN_SECONDS, hwiCpup10s, IRQ_CPUP_INFO_SIZE);
    (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_ONE_SECONDS, hwiCpup1s, IRQ_CPUP_INFO_SIZE);

    PRINTK(" InterruptNo  cpu      Count  ATime(us) MTime(us)   CPUUSE  CPUUSE10s  CPUUSE1s   Mode Name\n");
    ShellCmdHwiInfoShow(irqData, hwiCpup1s, hwiCpup10s, hwiCpupAll);
    (VOID)LOS_MemFree(m_aucSysMem0, irqCpup);
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
