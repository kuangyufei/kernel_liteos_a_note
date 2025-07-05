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
#include "los_task_pri.h"
#ifdef LOSCFG_IPC_CONTAINER
#include "los_ipc_container_pri.h"
#endif

#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif


/**
 * @brief  将布尔值转换为启用状态字符串
 * @param[in]  x - 布尔值（TRUE/FALSE）
 * @return const CHAR* - 状态字符串（"YES"/"NO"）
 */
#define SYSINFO_ENABLED(x) (((x) == TRUE) ? "YES" : "NO")

/**
 * @brief  获取当前系统中已使用的任务数量
 * @return UINT32 - 已使用任务数量
 */
UINT32 OsShellCmdTaskCntGet(VOID)
{
    UINT32 loop;                          // 循环计数器
    UINT32 taskCnt = 0;                   // 任务计数变量
    UINT32 intSave;                       // 中断状态保存变量
    LosTaskCB *taskCB = NULL;             // 任务控制块指针

    intSave = LOS_IntLock();              // 关闭中断，保证操作原子性
    // 遍历所有任务控制块
    for (loop = 0; loop < g_taskMaxNum; loop++) {
        taskCB = (LosTaskCB *)g_taskCBArray + loop;  // 获取当前任务控制块
        if (OsTaskIsUnused(taskCB)) {     // 如果任务未使用
            continue;                     // 跳过该任务
        }
        taskCnt++;                        // 增加任务计数
    }
    LOS_IntRestore(intSave);              // 恢复中断状态
    return taskCnt;                       // 返回任务数量
}

/**
 * @brief  获取当前系统中已使用的信号量数量
 * @return UINT32 - 已使用信号量数量
 */
UINT32 OsShellCmdSemCntGet(VOID)
{
    UINT32 loop;                          // 循环计数器
    UINT32 semCnt = 0;                    // 信号量计数变量
    UINT32 intSave;                       // 中断状态保存变量
    LosSemCB *semNode = NULL;             // 信号量控制块指针

    intSave = LOS_IntLock();              // 关闭中断，保证操作原子性
    // 遍历所有信号量
    for (loop = 0; loop < LOSCFG_BASE_IPC_SEM_LIMIT; loop++) {
        semNode = GET_SEM(loop);          // 获取当前信号量
        if (semNode->semStat == OS_SEM_USED) {  // 如果信号量已使用
            semCnt++;                     // 增加信号量计数
        }
    }
    LOS_IntRestore(intSave);              // 恢复中断状态
    return semCnt;                        // 返回信号量数量
}

/**
 * @brief  获取当前系统中已使用的队列数量
 * @return UINT32 - 已使用队列数量
 */
UINT32 OsShellCmdQueueCntGet(VOID)
{
    UINT32 loop;                          // 循环计数器
    UINT32 queueCnt = 0;                  // 队列计数变量
    UINT32 intSave;                       // 中断状态保存变量
    LosQueueCB *queueCB = NULL;           // 队列控制块指针

    intSave = LOS_IntLock();              // 关闭中断，保证操作原子性
    queueCB = IPC_ALL_QUEUE;              // 获取队列数组起始地址
    // 遍历所有队列
    for (loop = 0; loop < LOSCFG_BASE_IPC_QUEUE_LIMIT; loop++, queueCB++) {
        if (queueCB->queueState == OS_QUEUE_INUSED) {  // 如果队列已使用
            queueCnt++;                   // 增加队列计数
        }
    }
    LOS_IntRestore(intSave);              // 恢复中断状态
    return queueCnt;                      // 返回队列数量
}

/**
 * @brief  获取当前系统中已使用的软件定时器数量
 * @return UINT32 - 已使用软件定时器数量
 */
UINT32 OsShellCmdSwtmrCntGet(VOID)
{
    UINT32 loop;                          // 循环计数器
    UINT32 swtmrCnt = 0;                  // 软件定时器计数变量
    UINT32 intSave;                       // 中断状态保存变量
    SWTMR_CTRL_S *swtmrCB = NULL;         // 软件定时器控制块指针

    intSave = LOS_IntLock();              // 关闭中断，保证操作原子性
    swtmrCB = g_swtmrCBArray;             // 获取软件定时器数组起始地址
    // 遍历所有软件定时器
    for (loop = 0; loop < LOSCFG_BASE_CORE_SWTMR_LIMIT; loop++, swtmrCB++) {
        if (swtmrCB->ucState != OS_SWTMR_STATUS_UNUSED) {  // 如果定时器未被标记为未使用
            swtmrCnt++;                   // 增加软件定时器计数
        }
    }
    LOS_IntRestore(intSave);              // 恢复中断状态
    return swtmrCnt;                      // 返回软件定时器数量
}

/**
 * @brief  收集并打印系统资源使用情况
 * @details 包括任务、信号量、队列和软件定时器的使用数量、最大容量和启用状态
 */
LITE_OS_SEC_TEXT_MINOR VOID OsShellCmdSystemInfoGet(VOID)
{
    UINT8 isTaskEnable  = TRUE;           // 任务模块启用状态
#ifdef LOSCFG_BASE_IPC_SEM
    UINT8 isSemEnable   = TRUE;           // 信号量模块启用状态（已配置）
#else
    UINT8 isSemEnable   = FALSE;          // 信号量模块启用状态（未配置）
#endif
#ifdef LOSCFG_BASE_IPC_QUEUE
    UINT8 isQueueEnable = TRUE;           // 队列模块启用状态（已配置）
#else
    UINT8 isQueueEnable = FALSE;          // 队列模块启用状态（未配置）
#endif
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    UINT8 isSwtmrEnable = TRUE;           // 软件定时器模块启用状态（已配置）
#else
    UINT8 isSwtmrEnable = FALSE;          // 软件定时器模块启用状态（未配置）
#endif

    // 打印系统信息表头
    PRINTK("\n   Module    Used      Total     Enabled\n");
    PRINTK("--------------------------------------------\n");
    // 打印任务信息：当前使用数、最大支持数、启用状态
    PRINTK("   Task      %-10u%-10d%s\n",
           OsShellCmdTaskCntGet(),        // 当前使用的任务数量
           LOSCFG_BASE_CORE_TSK_LIMIT,    // 系统最大支持任务数量
           SYSINFO_ENABLED(isTaskEnable));// 任务模块启用状态
    // 打印信号量信息：当前使用数、最大支持数、启用状态
    PRINTK("   Sem       %-10u%-10d%s\n",
           OsShellCmdSemCntGet(),         // 当前使用的信号量数量
           LOSCFG_BASE_IPC_SEM_LIMIT,     // 系统最大支持信号量数量
           SYSINFO_ENABLED(isSemEnable)); // 信号量模块启用状态
    // 打印队列信息：当前使用数、最大支持数、启用状态
    PRINTK("   Queue     %-10u%-10d%s\n",
           OsShellCmdQueueCntGet(),       // 当前使用的队列数量
           LOSCFG_BASE_IPC_QUEUE_LIMIT,   // 系统最大支持队列数量
           SYSINFO_ENABLED(isQueueEnable));// 队列模块启用状态
    // 打印软件定时器信息：当前使用数、最大支持数、启用状态
    PRINTK("   SwTmr     %-10u%-10d%s\n",
           OsShellCmdSwtmrCntGet(),       // 当前使用的软件定时器数量
           LOSCFG_BASE_CORE_SWTMR_LIMIT,  // 系统最大支持软件定时器数量
           SYSINFO_ENABLED(isSwtmrEnable));// 软件定时器模块启用状态
}

/**
 * @brief  systeminfo shell命令处理函数
 * @details 显示当前操作系统内资源使用情况，包括任务、信号量、队列、定时器等
 * @param[in]  argc - 命令参数个数
 * @param[in]  argv - 命令参数列表
 * @return INT32 - 执行结果（0表示成功，-1表示失败）
 */
INT32 OsShellCmdSystemInfo(INT32 argc, const CHAR **argv)
{
    if (argc == 0) {                      // 如果没有参数
        OsShellCmdSystemInfoGet();        // 获取并显示系统信息
        return 0;                         // 返回成功
    }
    // 参数错误处理：输出错误信息和命令用法
    PRINTK("systeminfo: invalid option %s\n"
           "Systeminfo has NO ARGS.\n",
           argv[0]);
    return -1;                            // 返回失败
}

#ifdef LOSCFG_SHELL
SHELLCMD_ENTRY(systeminfo_shellcmd, CMD_TYPE_EX, "systeminfo", 1, (CmdCallBackFunc)OsShellCmdSystemInfo);
#endif /* LOSCFG_SHELL */

