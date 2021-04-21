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
#include "los_swtmr.h"
#include "los_sem_pri.h"
#include "los_queue_pri.h"
#include "los_swtmr_pri.h"

#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif


#define SYSINFO_ENABLED(x) (((x) == YES) ? "YES" : "NO")
UINT32 OsShellCmdTaskCntGet(VOID)
{
    UINT32 loop;
    UINT32 taskCnt = 0;
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;

    intSave = LOS_IntLock();
    for (loop = 0; loop < g_taskMaxNum; loop++) {
        taskCB = (LosTaskCB *)g_taskCBArray + loop;
        if (OsTaskIsUnused(taskCB)) {
            continue;
        }
        taskCnt++;
    }
    LOS_IntRestore(intSave);
    return taskCnt;
}

UINT32 OsShellCmdSemCntGet(VOID)
{
    UINT32 loop;
    UINT32 semCnt = 0;
    UINT32 intSave;
    LosSemCB *semNode = NULL;

    intSave = LOS_IntLock();
    for (loop = 0; loop < LOSCFG_BASE_IPC_SEM_LIMIT; loop++) {
        semNode = GET_SEM(loop);
        if (semNode->semStat == OS_SEM_USED) {
            semCnt++;
        }
    }
    LOS_IntRestore(intSave);
    return semCnt;
}

UINT32 OsShellCmdQueueCntGet(VOID)
{
    UINT32 loop;
    UINT32 queueCnt = 0;
    UINT32 intSave;
    LosQueueCB *queueCB = NULL;

    intSave = LOS_IntLock();
    queueCB = g_allQueue;
    for (loop = 0; loop < LOSCFG_BASE_IPC_QUEUE_LIMIT; loop++, queueCB++) {
        if (queueCB->queueState == OS_QUEUE_INUSED) {
            queueCnt++;
        }
    }
    LOS_IntRestore(intSave);
    return queueCnt;
}

UINT32 OsShellCmdSwtmrCntGet(VOID)
{
    UINT32 loop;
    UINT32 swtmrCnt = 0;
    UINT32 intSave;
    SWTMR_CTRL_S *swtmrCB = NULL;

    intSave = LOS_IntLock();
    swtmrCB = g_swtmrCBArray;
    for (loop = 0; loop < LOSCFG_BASE_CORE_SWTMR_LIMIT; loop++, swtmrCB++) {
        if (swtmrCB->ucState != OS_SWTMR_STATUS_UNUSED) {
            swtmrCnt++;
        }
    }
    LOS_IntRestore(intSave);
    return swtmrCnt;
}
//查看系统资源使用情况
LITE_OS_SEC_TEXT_MINOR VOID OsShellCmdSystemInfoGet(VOID)
{
    UINT8 isTaskEnable  = YES;
    UINT8 isSemEnable   = LOSCFG_BASE_IPC_SEM;
    UINT8 isQueueEnable = LOSCFG_BASE_IPC_QUEUE;
    UINT8 isSwtmrEnable = LOSCFG_BASE_CORE_SWTMR;
//模块名称	当前使用量 最大可用量              	   模块是否开启 
    PRINTK("\n   Module    Used      Total     Enabled\n");
    PRINTK("--------------------------------------------\n");
    PRINTK("   Task      %-10u%-10d%s\n",
           OsShellCmdTaskCntGet(),		//有效任务数
           LOSCFG_BASE_CORE_TSK_LIMIT,	//任务最大数 128
           SYSINFO_ENABLED(isTaskEnable));//任务是否失效 YES or NO
    PRINTK("   Sem       %-10u%-10d%s\n",
           OsShellCmdSemCntGet(),		//信号量的数量
           LOSCFG_BASE_IPC_SEM_LIMIT,	//信号量最大数 1024
           SYSINFO_ENABLED(isSemEnable));//信号量是否失效 YES or NO
    PRINTK("   Queue     %-10u%-10d%s\n",
           OsShellCmdQueueCntGet(),		//队列的数量
           LOSCFG_BASE_IPC_QUEUE_LIMIT,	//队列的最大数 1024
           SYSINFO_ENABLED(isQueueEnable));//队列是否失效 YES or NO
    PRINTK("   SwTmr     %-10u%-10d%s\n",
           OsShellCmdSwtmrCntGet(),		//定时器的数量
           LOSCFG_BASE_CORE_SWTMR_LIMIT,	//定时器的总数 1024
           SYSINFO_ENABLED(isSwtmrEnable));	//定时器是否失效 YES or NO
}
//systeminfo命令用于显示当前操作系统内资源使用情况，包括任务、信号量、互斥量、队列、定时器等。
INT32 OsShellCmdSystemInfo(INT32 argc, const CHAR **argv)
{
    if (argc == 0) {
        OsShellCmdSystemInfoGet();
        return 0;
    }
    PRINTK("systeminfo: invalid option %s\n"
           "Systeminfo has NO ARGS.\n",
           argv[0]);
    return -1;
}

#ifdef LOSCFG_SHELL
SHELLCMD_ENTRY(systeminfo_shellcmd, CMD_TYPE_EX, "systeminfo", 1, (CmdCallBackFunc)OsShellCmdSystemInfo);
#endif /* LOSCFG_SHELL */

