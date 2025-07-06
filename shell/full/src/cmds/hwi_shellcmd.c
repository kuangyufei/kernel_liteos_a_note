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
// 定义中断CPU占用率信息大小：CPUP_INFO_S结构体大小 × 最大中断数 × 内核核心数
#define IRQ_CPUP_INFO_SIZE     (sizeof(CPUP_INFO_S) * OS_HWI_MAX_NUM * LOSCFG_KERNEL_CORE_NUM)
// 定义所有中断CPU占用率信息总大小：3个时间段(全部/10秒/1秒) × 单个时间段信息大小
#define IRQ_CPUP_ALL_INFO_SIZE (3 * IRQ_CPUP_INFO_SIZE)
// 定义中断数据大小：OsIrqCpupCB结构体大小 × 内核核心数
#define IRQ_DATA_SZIE          (sizeof(OsIrqCpupCB) * LOSCFG_KERNEL_CORE_NUM)
// 定义CPU占用率精度乘数，用于百分比小数部分计算
#define CPUP_PRECISION_MULT    LOS_CPUP_PRECISION_MULT

/**
 * @brief  显示中断CPU占用率详细信息
 * @param  irqData [IN] 中断数据缓冲区指针
 * @param  hwiCpup1s [IN] 最近1秒中断CPU占用率信息指针
 * @param  hwiCpup10s [IN] 最近10秒中断CPU占用率信息指针
 * @param  hwiCpupAll [IN] 所有时间中断CPU占用率信息指针
 * @return VOID
 */
STATIC VOID ShellCmdHwiInfoShow(OsIrqCpupCB *irqData, CPUP_INFO_S *hwiCpup1s,
                                CPUP_INFO_S *hwiCpup10s, CPUP_INFO_S *hwiCpupAll)
{
    UINT32 intSave;                                 // 中断状态保存变量
    OsIrqCpupCB *irqDataBase = OsGetIrqCpupArrayBase();  // 获取中断CPU占用率数据基地址

    // 遍历所有中断号(从异常中断号之后开始)
    for (UINT32 i = OS_HWI_FORM_EXC_NUM; i < OS_HWI_MAX_NUM + OS_HWI_FORM_EXC_NUM; i++) {
        if (!HWI_IS_REGISTED(i)) {                   // 跳过未注册的中断
            continue;
        }

        intSave = LOS_IntLock();                     // 关中断，保证数据拷贝原子性
        // 拷贝当前中断的CPU占用率数据
        (VOID)memcpy_s(irqData, IRQ_DATA_SZIE, &irqDataBase[i * LOSCFG_KERNEL_CORE_NUM], IRQ_DATA_SZIE);
        LOS_IntRestore(intSave);                     // 恢复中断状态

        // 遍历每个CPU核心
        for (UINT32 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
            UINT64 cycles = 0;                       // 平均周期数
            UINT64 timeMax = 0;                      // 最大时间(微秒)
            OsIrqCpupCB *data = &irqData[cpu];       // 当前CPU的中断数据
            if (data->status == 0) {                 // 跳过未激活的中断数据
                continue;
            }
            UINT32 count = OsGetHwiFormCnt(cpu, i);  // 获取中断触发次数
            if (count != 0) {                        // 仅在有中断触发时计算
                if (data->count != 0) {              // 避免除零错误
                    // 计算平均耗时(微秒)：总时间×每周期纳秒数/(触发次数×每微秒纳秒数)
                    cycles = (data->allTime * OS_NS_PER_CYCLE) / (data->count * OS_SYS_NS_PER_US);
                }
                // 计算最大耗时(微秒)：最大时间×每周期纳秒数/1000
                timeMax = (data->timeMax * OS_NS_PER_CYCLE) / 1000;
            }
            CHAR *irqName = OsGetHwiFormName(i);     // 获取中断名称
            UINT32 index = (i * LOSCFG_KERNEL_CORE_NUM) + cpu;  // 计算数组索引
            // 格式化输出中断详细信息：中断号、CPU核、触发次数、平均耗时、最大耗时、CPU占用率等
            PRINTK(" %10u:%5u%11u%11llu%10llu%6u.%-2u%8u.%-2u%7u.%-2u%7s %-12s\n", i, cpu, count, cycles, timeMax,
                   hwiCpupAll[index].usage / CPUP_PRECISION_MULT, hwiCpupAll[index].usage % CPUP_PRECISION_MULT,
                   hwiCpup10s[index].usage / CPUP_PRECISION_MULT, hwiCpup10s[index].usage % CPUP_PRECISION_MULT,
                   hwiCpup1s[index].usage / CPUP_PRECISION_MULT, hwiCpup1s[index].usage % CPUP_PRECISION_MULT,
                   (g_hwiForm[index].uwParam == IRQF_SHARED) ? "shared" : "normal", (irqName != NULL) ? irqName : "");
        }
    }
}

/**
 * @brief  shell中断命令实现(LOSCFG_CPUP_INCLUDE_IRQ已定义)，显示中断CPU占用率详细信息
 * @param  argc [IN] 命令参数个数
 * @param  argv [IN] 命令参数列表
 * @return UINT32 成功返回0，失败返回OS_ERROR
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdHwi(INT32 argc, const CHAR **argv)
{
    UINT32 size;                                    // 内存分配大小

    (VOID)argv;                                      // 未使用参数，避免编译警告
    if (argc > 0) {                                  // 检查是否有多余参数
        PRINTK("\nUsage: hwi\n");                   // 输出命令使用方法
        return OS_ERROR;                             // 参数错误，返回错误码
    }

    size = IRQ_CPUP_ALL_INFO_SIZE + IRQ_DATA_SZIE;   // 计算总内存需求
    // 分配内存用于存储中断CPU占用率信息
    CHAR *irqCpup = LOS_MemAlloc(m_aucSysMem0, size);
    if (irqCpup == NULL) {                           // 内存分配失败检查
        return OS_ERROR;
    }

    // 划分内存区域：所有时间/10秒/1秒的CPU占用率信息和中断数据
    CPUP_INFO_S *hwiCpupAll = (CPUP_INFO_S *)irqCpup;
    CPUP_INFO_S *hwiCpup10s = (CPUP_INFO_S *)(irqCpup + IRQ_CPUP_INFO_SIZE);
    CPUP_INFO_S *hwiCpup1s = (CPUP_INFO_S *)(irqCpup + 2 * IRQ_CPUP_INFO_SIZE); /* 2: 偏移乘数 */
    OsIrqCpupCB *irqData = (OsIrqCpupCB *)(irqCpup + IRQ_CPUP_ALL_INFO_SIZE);

    // 获取不同时间段的中断CPU占用率信息
    (VOID)LOS_GetAllIrqCpuUsage(CPUP_ALL_TIME, hwiCpupAll, IRQ_CPUP_INFO_SIZE);
    (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_TEN_SECONDS, hwiCpup10s, IRQ_CPUP_INFO_SIZE);
    (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_ONE_SECONDS, hwiCpup1s, IRQ_CPUP_INFO_SIZE);

    // 打印表头
    PRINTK(" InterruptNo  cpu      Count  ATime(us) MTime(us)   CPUUSE  CPUUSE10s  CPUUSE1s   Mode Name\n");
    ShellCmdHwiInfoShow(irqData, hwiCpup1s, hwiCpup10s, hwiCpupAll);  // 显示中断详细信息
    (VOID)LOS_MemFree(m_aucSysMem0, irqCpup);      // 释放内存
    return 0;                                       // 成功返回
}
#else
/**
 * @brief  shell中断命令实现(LOSCFG_CPUP_INCLUDE_IRQ未定义)，显示简化的中断信息
 * @param  argc [IN] 命令参数个数
 * @param  argv [IN] 命令参数列表
 * @return UINT32 成功返回0，失败返回OS_ERROR
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdHwi(INT32 argc, const CHAR **argv)
{
    UINT32 i;                                       // 循环计数器

    (VOID)argv;                                      // 未使用参数，避免编译警告
    if (argc > 0) {                                  // 检查是否有多余参数
        PRINTK("\nUsage: hwi\n");                   // 输出命令使用方法
        return OS_ERROR;                             // 参数错误，返回错误码
    }

    PRINTK(" InterruptNo     Count     Name\n");     // 打印表头
    // 遍历所有中断号(从异常中断号之后开始)
    for (i = OS_HWI_FORM_EXC_NUM; i < OS_HWI_MAX_NUM + OS_HWI_FORM_EXC_NUM; i++) {
        /* 不同核心有不同的中断表单实现 */
        // 如果中断已注册且有名称，显示中断号、触发次数和名称
        if (HWI_IS_REGISTED(i) && (OsGetHwiFormName(i) != NULL)) {
            PRINTK(" %8d:%10d:      %-s\n", i, OsGetHwiFormCnt(i), OsGetHwiFormName(i));
        } else if (HWI_IS_REGISTED(i)) {             // 如果中断已注册但无名称，仅显示中断号和触发次数
            PRINTK(" %8d:%10d:\n", i, OsGetHwiFormCnt(i));
        }
    }
    return 0;                                       // 成功返回
}
#endif
#endif

SHELLCMD_ENTRY(hwi_shellcmd, CMD_TYPE_EX, "hwi", 0, (CmdCallBackFunc)OsShellCmdHwi);

#endif /* LOSCFG_SHELL */
