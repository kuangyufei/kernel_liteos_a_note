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

#define OS_ALL_SWTMR_MASK 0xffffffff                                      // 表示所有软件定时器的掩码值
#define SWTMR_STRLEN  12                                                  // 软件定时器模式/状态字符串长度

/**
 * @brief 软件定时器模式字符串映射表
 * @note 与 SWTMR_MODE_E 枚举对应: Once(0), Period(1), NSD(2), OPP(3)
 */
LITE_OS_SEC_DATA_MINOR STATIC CHAR g_shellSwtmrMode[][SWTMR_STRLEN] = {
    "Once",                                                              // 单次模式：定时结束后自动删除
    "Period",                                                            // 周期模式：定时结束后重新启动
    "NSD",                                                              // 非自动删除单次模式
    "OPP",                                                              // 一次性触发模式
};

/**
 * @brief 软件定时器状态字符串映射表
 * @note 与 SWTMR_STATE_E 枚举对应: UnUsed(0), Created(1), Ticking(2)
 */
LITE_OS_SEC_DATA_MINOR STATIC CHAR g_shellSwtmrStatus[][SWTMR_STRLEN] = {
    "UnUsed",                                                            // 未使用状态
    "Created",                                                           // 已创建状态
    "Ticking",                                                           // 运行中状态
};

/**
 * @brief 打印单个软件定时器信息
 * @param[in] swtmr 软件定时器控制块指针
 * @details 格式化输出定时器ID、状态、模式、间隔、计数、参数和处理函数地址
 */
STATIC VOID OsPrintSwtmrMsg(const SWTMR_CTRL_S *swtmr)
{
    UINT32 ticks = 0;                                                     // 当前计数值
    (VOID)LOS_SwtmrTimeGet(swtmr->usTimerID, &ticks);                     // 获取定时器当前计数值

    PRINTK("%7u%10s%8s%12u%7u%#12x%#12x\n",
           swtmr->usTimerID % LOSCFG_BASE_CORE_SWTMR_LIMIT,                // 软件定时器ID（取模限制范围内）
           g_shellSwtmrStatus[swtmr->ucState],                            // 定时器状态字符串
           g_shellSwtmrMode[swtmr->ucMode],                               // 定时器模式字符串
           swtmr->uwInterval,                                             // 定时间隔（单位：Tick）
           ticks,                                                         // 当前计数值
           swtmr->uwArg,                                                  // 用户传入参数
           swtmr->pfnHandler);                                            // 定时器回调函数地址
}

/**
 * @brief 打印软件定时器信息表头
 * @details 输出表格标题行，说明各列数据含义
 */
STATIC INLINE VOID OsPrintSwtmrMsgHead(VOID)
{
    PRINTK("\r\nSwTmrID     State    Mode    Interval  Count         Arg handlerAddr\n");
}

/**
 * @brief 获取软件定时器基本信息
 * @param[in] timerID 定时器ID，OS_ALL_SWTMR_MASK表示所有定时器
 * @return 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 * @details 根据timerID查询并打印指定定时器或所有定时器的基本信息
 */
STATIC UINT32 SwtmrBaseInfoGet(UINT32 timerID)
{
    SWTMR_CTRL_S *swtmr = g_swtmrCBArray;                                 // 定时器控制块数组指针
    SWTMR_CTRL_S *swtmr1 = g_swtmrCBArray;                                // 用于计数未使用定时器的指针
    UINT16 index;                                                         // 循环索引
    UINT16 num = 0;                                                       // 未使用定时器数量

    // 统计未使用的定时器数量
    for (index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr1++) {
        if (swtmr1->ucState == 0) {                                       // 状态为0表示未使用
            num = num + 1;                                                // 未使用计数加1
        }
    }

    // 如果所有定时器都未使用，打印提示信息并返回
    if (num == LOSCFG_BASE_CORE_SWTMR_LIMIT) {
        PRINTK("\r\nThere is no swtmr was created!\n");
        return LOS_NOK;
    }

    // 如果请求所有定时器信息
    if (timerID == OS_ALL_SWTMR_MASK) {
        OsPrintSwtmrMsgHead();                                            // 打印表头
        // 遍历所有定时器控制块
        for (index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr++) {
            if (swtmr->ucState != 0) {                                    // 只处理已使用的定时器
                OsPrintSwtmrMsg(swtmr);                                   // 打印定时器详细信息
            }
        }
    } else {
        // 遍历查找指定ID的定时器
        for (index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++, swtmr++) {
            // 找到匹配ID且已使用的定时器
            if ((timerID == (size_t)(swtmr->usTimerID % LOSCFG_BASE_CORE_SWTMR_LIMIT)) && (swtmr->ucState != 0)) {
                OsPrintSwtmrMsgHead();                                    // 打印表头
                OsPrintSwtmrMsg(swtmr);                                   // 打印定时器详细信息
                return LOS_OK;
            }
        }
        PRINTK("\r\nThe SwTimerID is not exist.\n");                     // 未找到指定ID的定时器
    }
    return LOS_OK;
}

#ifdef LOSCFG_SWTMR_DEBUG
/**
 * @brief 打印软件定时器调试时间信息
 * @details 输出定时器的周期、等待时间、运行时间等详细调试数据，仅在开启LOSCFG_SWTMR_DEBUG时有效
 */
STATIC VOID OsSwtmrTimeInfoShow(VOID)
{
    UINT8 mode;                                                           // 定时器模式
    SwtmrDebugData data;                                                  // 定时器调试数据结构

    // 打印调试信息表头
    PRINTK("SwtmrID  Cpuid   Mode    Period(us) WaitTime(us) WaitMax(us) RTime(us) RTimeMax(us) ReTime(us)"
           " ReTimeMax(us)      RunCount    LostNum     Handler\n");
    // 遍历所有定时器
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_SWTMR_LIMIT; index++) {
        if (!OsSwtmrDebugDataUsed(index)) {                               // 跳过未使用的调试数据
            continue;
        }

        // 获取定时器调试数据
        UINT32 ret = OsSwtmrDebugDataGet(index, &data, sizeof(SwtmrDebugData), &mode);
        if (ret != LOS_OK) {                                              // 获取失败则退出循环
            break;
        }

        SwtmrDebugBase *base = &data.base;                                // 基础调试数据指针
        // 计算平均等待时间(us) = (总等待周期数/次数) * 周期(ns) / 1000
        UINT64 waitTime = ((base->waitTime / base->waitCount) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        UINT64 waitTimeMax = (base->waitTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US; // 最大等待时间(us)
        UINT64 runTime = ((base->runTime / base->runCount) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US; // 平均运行时间(us)
        UINT64 runTimeMax = (base->runTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US; // 最大运行时间(us)
        UINT64 readyTime = ((base->readyTime / base->runCount) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US; // 平均就绪时间(us)
        UINT64 readyTimeMax = (base->readyTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US; // 最大就绪时间(us)
        // 打印调试信息
        PRINTK("%4u%10u%7s%14u%13llu%12llu%10llu%13llu%10llu%14llu%15llu%11u%#12x\n",
               index, data.cpuid, g_shellSwtmrMode[mode], data.period * OS_US_PER_TICK, waitTime, waitTimeMax,
               runTime, runTimeMax, readyTime, readyTimeMax, base->runCount, base->times, data.handler);
    }
}
#endif

/**
 * @brief shell命令处理函数：获取软件定时器信息
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 * @details 支持三种命令格式：swtmr(显示所有)、swtmr ID(显示指定ID)、swtmr -t(显示调试时间信息，需开启LOSCFG_SWTMR_DEBUG)
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdSwtmrInfoGet(INT32 argc, const CHAR **argv)
{
    UINT32 timerID;                                                       // 定时器ID
    CHAR *endPtr = NULL;                                                  // strtoul函数的结束指针

    if (argc > 1) {                                                       // 参数个数大于1，显示帮助信息
        goto SWTMR_HELP;
    }

    if (argc == 0) {                                                      // 无参数：显示所有定时器
        timerID = OS_ALL_SWTMR_MASK;
#ifdef LOSCFG_SWTMR_DEBUG
    } else if (strcmp("-t", argv[0]) == 0) {                             // 参数为-t：显示调试时间信息
        OsSwtmrTimeInfoShow();
        return LOS_OK;
#endif
    } else {                                                              // 参数为ID：显示指定定时器
        timerID = strtoul(argv[0], &endPtr, 0);                          // 字符串转无符号整数
        // 校验输入合法性
        if ((endPtr == NULL) || (*endPtr != 0) || (timerID > LOSCFG_BASE_CORE_SWTMR_LIMIT)) {
            PRINTK("\nswtmr ID can't access %s.\n", argv[0]);
            return LOS_NOK;
        }
    }

    return SwtmrBaseInfoGet(timerID);                                     // 获取并显示定时器信息
SWTMR_HELP:
    PRINTK("Usage:\n");                                                  // 打印帮助信息
    PRINTK(" swtmr      --- Information about all created software timers.\n");
    PRINTK(" swtmr ID   --- Specifies information about a software timer.\n");
    return LOS_OK;
}
SHELLCMD_ENTRY(swtmr_shellcmd, CMD_TYPE_EX, "swtmr", 1, (CmdCallBackFunc)OsShellCmdSwtmrInfoGet);//采用shell命令静态注册方式

#endif /* LOSCFG_SHELL */
