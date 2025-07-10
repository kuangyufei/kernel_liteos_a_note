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

/**
 * @defgroup los_hw Hardware
 * @ingroup kernel
 */
#ifndef _LOS_HW_H
#define _LOS_HW_H

#include "los_typedef.h"
#include "los_hw_cpu.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_hw
 * @defgroup hw_sched_macros 调度模式定义
 * @brief 定义系统调度发生的上下文类型
 * @{ 
 */
#define OS_SCHEDULE_IN_IRQ                                  0x0  /**< 调度发生在中断上下文，值为0 */
#define OS_SCHEDULE_IN_TASK                                 0x1  /**< 调度发生在任务上下文，值为1 */
/** @} */

/**
 * @ingroup los_hw
 * @defgroup hw_psr_macros 程序状态寄存器(PSR)位定义
 * @brief ARM架构下PSR寄存器的关键位和模式组合宏
 * @{ 
 */
#define PSR_T_ARM                                           0x00000000u /**< T位：0表示ARM指令集模式 */
#define PSR_T_THUMB                                         0x00000020u /**< T位：1表示Thumb指令集模式 (十进制32) */
#define PSR_MODE_SVC                                        0x00000013u /**< 模式位：SVC(管理)模式 (十进制19) */
#define PSR_MODE_SYS                                        0x0000001Fu /**< 模式位：SYS(系统)模式 (十进制31) */
#define PSR_FIQ_DIS                                         0x00000040u /**< F位：禁用FIQ中断 (十进制64) */
#define PSR_IRQ_DIS                                         0x00000080u /**< I位：禁用IRQ中断 (十进制128) */
#define PSR_MODE_USR                                        0x00000010u /**< 模式位：USR(用户)模式 (十进制16) */

/**
 * @brief SVC模式Thumb指令集配置
 * @note 组合值：0x13 | 0x20 | 0x40 | 0x80 = 0xF3 (十进制243)
 *       包含：SVC模式 + Thumb指令集 + FIQ禁用 + IRQ禁用
 */
#define PSR_MODE_SVC_THUMB                                  (PSR_MODE_SVC | PSR_T_THUMB | PSR_FIQ_DIS | PSR_IRQ_DIS)
/**
 * @brief SVC模式ARM指令集配置
 * @note 组合值：0x13 | 0x00 | 0x40 | 0x80 = 0xD3 (十进制211)
 *       包含：SVC模式 + ARM指令集 + FIQ禁用 + IRQ禁用
 */
#define PSR_MODE_SVC_ARM                                    (PSR_MODE_SVC | PSR_T_ARM   | PSR_FIQ_DIS | PSR_IRQ_DIS)

#define PSR_MODE_SYS_THUMB                                  (PSR_MODE_SYS | PSR_T_THUMB) /**< SYS模式Thumb指令集配置 (0x1F | 0x20 = 0x3F) */
#define PSR_MODE_SYS_ARM                                    (PSR_MODE_SYS | PSR_T_ARM)   /**< SYS模式ARM指令集配置 (0x1F | 0x00 = 0x1F) */

#define PSR_MODE_USR_THUMB                                  (PSR_MODE_USR | PSR_T_THUMB) /**< USR模式Thumb指令集配置 (0x10 | 0x20 = 0x30) */
#define PSR_MODE_USR_ARM                                    (PSR_MODE_USR | PSR_T_ARM)   /**< USR模式ARM指令集配置 (0x10 | 0x00 = 0x10) */
/** @} */

/**
 * @ingroup los_hw
 * @brief 调度检查条件宏
 * @return 布尔值：true表示可以调度，false表示不可调度
 * @note 调度条件：当前非中断活跃状态且系统可抢占
 * @see OsPreemptable() 获取系统抢占状态函数
 */
#define LOS_CHECK_SCHEDULE                                  ((!OS_INT_ACTIVE) && OsPreemptable())

/**
 * @ingroup los_hw
 * @brief CPU厂商信息结构
 * @details 存储CPU型号编号与厂商名称的映射关系
 */
typedef struct {
    const UINT32 partNo;       /**< CPU型号编号，由厂商定义的唯一标识符 */
    const CHAR *cpuName;       /**< CPU厂商名称字符串，如"ARM Cortex-A7" */
} CpuVendor;

extern CpuVendor g_cpuTable[];  /**< 全局CPU厂商信息表，存储支持的CPU型号列表 */
extern UINT64 g_cpuMap[];       /**< 全局CPU映射表，记录逻辑CPU与物理HWID的对应关系 */

/**
 * @ingroup los_hw
 * @brief 获取指定逻辑CPU的物理HWID
 * @param cpuid 逻辑CPU编号 (从0开始)
 * @return 对应的物理HWID值
 */
#define CPU_MAP_GET(cpuid)          g_cpuMap[(cpuid)]
/**
 * @ingroup los_hw
 * @brief 设置逻辑CPU与物理HWID的映射关系
 * @param cpuid 逻辑CPU编号 (从0开始)
 * @param hwid 物理CPU的硬件ID
 */
#define CPU_MAP_SET(cpuid, hwid)    g_cpuMap[(cpuid)] = (hwid)
/**
 * @ingroup los_hw
 * @brief Invalidate instruction cache.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to invalidate the instruction cache.</li>
 * </ul>
 * @attention None.
 *
 * @param None.
 *
 * @retval #None.
 *
 * @par Dependency:
 * los_hw.h: the header file that contains the API declaration.
 * @see None.
 */
extern VOID FlushICache(VOID);

/**
 * @ingroup los_hw
 * @brief Flush data cache.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to flush the data cache to the memory.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The input end address must be greater than the input start address.</li>
 * </ul>
 *
 * @param  start [IN] Type #int Flush start address.
 * @param  end [IN] Type #int Flush end address.
 *
 * @retval #None.
 *
 * @par Dependency:
 * los_hw.h: the header file that contains the API declaration.
 * @see None.
 */
extern VOID DCacheFlushRange(UINTPTR start, UINTPTR end);

/**
 * @ingroup los_hw
 * @brief Invalidate data cache.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to Invalidate the data in cache.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>The input end address must be greater than the input start address.</li>
 * </ul>
 *
 * @param  start [IN] Type #int Invalidate start address.
 * @param  end [IN] Type #int Invalidate end address .
 *
 * @retval #None.
 *
 * @par Dependency:
 * los_hw.h: the header file that contains the API declaration.
 * @see None.
 */
extern VOID DCacheInvRange(UINTPTR start, UINTPTR end);

/**
 * @ingroup los_hw
 * @brief 获取CPU核心名称
 *
 * @par 描述
 * 该API通过读取CPU主ID寄存器(MIDR)提取型号编号，并从CPU厂商信息表中查询对应的CPU核心名称字符串。
 * 主要用于系统诊断、日志输出及硬件适配层识别CPU类型。
 *
 * @attention
 * <ul>
 * <li>依赖全局CPU厂商信息表g_cpuTable的正确初始化，表项以partNo=0作为结束标志</li>
 * <li>仅支持预定义在g_cpuTable中的CPU型号识别，未定义型号返回"unknown"</li>
 * </ul>
 *
 * @param 无
 *
 * @retval const CHAR* CPU核心名称字符串，如"ARM Cortex-A7"；未知型号返回"unknown"
 *
 * @par 依赖
 * <ul>
 * <li>los_hw.h：包含该API的声明</li>
 * <li>OsMainIDGet()：读取CPU主ID寄存器的底层函数</li>
 * <li>g_cpuTable：CPU型号与名称映射表</li>
 * </ul>
 * @see CpuVendor
 */
STATIC INLINE const CHAR *LOS_CpuInfo(VOID)
{
    INT32 i;                                  /**< 循环索引变量 */
    UINT32 midr = OsMainIDGet();              /**< 读取CPU主ID寄存器(MIDR)值
                                              *   MIDR格式：[31:24]实现者代码, [23:16]变体号, [15:4]主型号, [3:0]修订版本 */
    /* [15:4] is the primary part number */
    UINT32 partNo = (midr & 0xFFF0) >> 0x4;   /**< 提取主型号编号：MIDR[15:4]位
                                              *   0xFFF0掩码筛选12位主型号域，右移4位对齐到最低位 */

    /* 遍历CPU厂商信息表查找匹配型号 */
    for (i = 0; g_cpuTable[i].partNo != 0; i++) {
        if (partNo == g_cpuTable[i].partNo) {  /**< 比较提取的型号编号与表中预定义值 */
            return g_cpuTable[i].cpuName;      /**< 返回匹配的CPU名称字符串 */
        }
    }

    return "unknown";                         /**< 未找到匹配项，返回未知型号标识 */
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HW_H */
