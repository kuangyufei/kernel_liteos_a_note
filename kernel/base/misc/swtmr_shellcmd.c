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
#include "stdlib.h"
#include "los_swtmr_pri.h"
#include "shcmd.h"
#include "shell.h"

#define OS_ALL_SWTMR_MASK 0xffffffff
#define SWTMR_STRLEN  12

LITE_OS_SEC_DATA_MINOR STATIC CHAR g_shellSwtmrMode[][SWTMR_STRLEN] = {
    "Once",
    "Period",
    "NSD",
    "OPP",
};

LITE_OS_SEC_DATA_MINOR STATIC CHAR g_shellSwtmrStatus[][SWTMR_STRLEN] = {
    "UnUsed",
    "Created",
    "Ticking",
};

STATIC VOID OsPrintSwtmrMsg(const SWTMR_CTRL_S *swtmr)
{
    UINT32 ticks = 0;
    (VOID)LOS_SwtmrTimeGet(swtmr->usTimerID, &ticks);

    PRINTK("%7u%10s%8s%12u%7u%#12x%#12x\n",
           swtmr->usTimerID % LOSCFG_BASE_CORE_SWTMR_LIMIT,	//软件定时器ID。
           g_shellSwtmrStatus[swtmr->ucState],	//软件定时器状态,状态可能为："UnUsed", "Created", "Ticking"。
           g_shellSwtmrMode[swtmr->ucMode],		//软件定时器模式。模式可能为："Once", "Period", "NSD（单次定时器，定时结束后不会自动删除）"
           swtmr->uwInterval,	//软件定时器使用的Tick数。
           ticks,
           swtmr->uwArg,		//传入的参数。
           swtmr->pfnHandler);	//回调函数的地址。
}

STATIC INLINE VOID OsPrintSwtmrMsgHead(VOID)
{
    PRINTK("\r\nSwTmrID     State    Mode    Interval  Count         Arg handlerAddr\n");
}
///shell命令之swtmr 命令用于查询系统软件定时器相关信息。 
//参数缺省时，默认显示所有软件定时器的相关信息。
STATIC UINT32 SwtmrBaseInfoGet(UINT32 timerID)
{
    SWTMR_CTRL_S *swtmr = g_swtmrCBArray;
    SWTMR_CTRL_S *swtmr1 = g_swtmrCBArray;
    UINT16 index;
    UINT16 num = 0;

    for (index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr1++) {
        if (swtmr1->ucState == 0) {
            num = num + 1;
        }
    }

    if (num == LOSCFG_BASE_CORE_SWTMR_LIMIT) {
        PRINTK("\r\nThere is no swtmr was created!\n");
        return LOS_NOK;
    }

    if (timerID == OS_ALL_SWTMR_MASK) {
        OsPrintSwtmrMsgHead();
        for (index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr++) {
            if (swtmr->ucState != 0) {
                OsPrintSwtmrMsg(swtmr);
            }
        }
    } else {
        for (index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr++) {
            if ((timerID == (size_t)(swtmr->usTimerID % LOSCFG_BASE_CORE_SWTMR_LIMIT)) && (swtmr->ucState != 0)) {
                OsPrintSwtmrMsgHead();
                OsPrintSwtmrMsg(swtmr);
                return LOS_OK;
            }
        }
        PRINTK("\r\nThe SwTimerID is not exist.\n");
    }
    return LOS_OK;
}

#ifdef LOSCFG_SWTMR_DEBUG
STATIC VOID OsSwtmrTimeInfoShow(VOID)
{
    UINT8 mode;
    SwtmrDebugData data;

    PRINTK("SwtmrID  Cpuid   Mode    Period(us) WaitTime(us) WaitMax(us) RTime(us) RTimeMax(us) ReTime(us)"
           " ReTimeMax(us)      RunCount    LostNum     Handler\n");
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++) {
        if (!OsSwtmrDebugDataUsed(index)) {
            continue;
        }

        UINT32 ret = OsSwtmrDebugDataGet(index, &data, sizeof(SwtmrDebugData), &mode);
        if (ret != LOS_OK) {
            break;
        }

        SwtmrDebugBase *base = &data.base;
        UINT64 waitTime = ((base->waitTime / base->waitCount) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        UINT64 waitTimeMax = (base->waitTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        UINT64 runTime = ((base->runTime / base->runCount) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        UINT64 runTimeMax = (base->runTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        UINT64 readyTime = ((base->readyTime / base->runCount) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        UINT64 readyTimeMax = (base->readyTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        PRINTK("%4u%10u%7s%14u%13llu%12llu%10llu%13llu%10llu%14llu%15llu%11u%#12x\n",
               index, data.cpuid, g_shellSwtmrMode[mode], data.period * OS_US_PER_TICK, waitTime, waitTimeMax,
               runTime, runTimeMax, readyTime, readyTimeMax, base->runCount, base->times, data.handler);
    }
}
#endif

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdSwtmrInfoGet(INT32 argc, const CHAR **argv)
{
    UINT32 timerID;
    CHAR *endPtr = NULL;

    if (argc > 1) {
        goto SWTMR_HELP;
    }

    if (argc == 0) {
        timerID = OS_ALL_SWTMR_MASK;
#ifdef LOSCFG_SWTMR_DEBUG
    } else if (strcmp("-t", argv[0]) == 0) {
        OsSwtmrTimeInfoShow();
        return LOS_OK;
#endif
    } else {
        timerID = strtoul(argv[0], &endPtr, 0);
        if ((endPtr == NULL) || (*endPtr != 0) || (timerID > LOSCFG_BASE_CORE_SWTMR_LIMIT)) {
            PRINTK("\nswtmr ID can't access %s.\n", argv[0]);
            return LOS_NOK;
        }
    }

    return SwtmrBaseInfoGet(timerID);
SWTMR_HELP:
    PRINTK("Usage:\n");
    PRINTK(" swtmr      --- Information about all created software timers.\n");
    PRINTK(" swtmr ID   --- Specifies information about a software timer.\n");
    return LOS_OK;
}

SHELLCMD_ENTRY(swtmr_shellcmd, CMD_TYPE_EX, "swtmr", 1, (CmdCallBackFunc)OsShellCmdSwtmrInfoGet);//采用shell命令静态注册方式

#endif /* LOSCFG_SHELL */
