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

#ifndef _GIC_COMMON_H
#define _GIC_COMMON_H

#include "stdint.h"
#include "target_config.h"
#include "los_config.h"

/**
 * @file gic_common.h
 * @brief GIC(Generic Interrupt Controller)通用寄存器和宏定义头文件
 * @details 定义GICv1-v4架构通用的寄存器偏移、访问宏、优先级配置及架构版本枚举
 *          提供跨GIC版本的统一接口定义，通过条件编译支持不同GIC版本特性
 */

/* gic arch revision - GIC架构版本枚举 */
enum {
    GICV1 = 1,                          ///< GICv1架构，支持基本中断分发功能
    GICV2,                              ///< GICv2架构，增加虚拟化支持和安全扩展
    GICV3,                              ///< GICv3架构，引入ITS和系统寄存器接口
    GICV4                               ///< GICv4架构，增强虚拟化中断处理
};

#define GIC_REV_MASK                    0xF0U                               ///< GIC版本掩码，用于从IIDR寄存器提取版本信息(bit[7:4])
#define GIC_REV_OFFSET                  0x4U                                ///< GIC版本偏移位，右移4位获取版本号

#ifdef LOSCFG_ARCH_GIC_V2
/**
 * @defgroup gicv2_cpu_interface_reg GICv2 CPU接口寄存器偏移
 * @brief GICv2 CPU Interface控制和状态寄存器偏移定义
 * @{ 
 */
#define GICC_CTLR                       (GICC_OFFSET + 0x00)                ///< CPU接口控制寄存器，控制是否上报中断到处理器
#define GICC_PMR                        (GICC_OFFSET + 0x04)                ///< 中断优先级屏蔽寄存器，设置中断优先级阈值
#define GICC_BPR                        (GICC_OFFSET + 0x08)                ///< 二进制点寄存器，控制优先级分组(抢占优先级和子优先级划分)
#define GICC_IAR                        (GICC_OFFSET + 0x0c)                ///< 中断应答寄存器，读取获取当前最高优先级中断号
#define GICC_EOIR                       (GICC_OFFSET + 0x10)                ///< 中断结束寄存器，写入通知CPU接口中断处理完成
#define GICC_RPR                        (GICC_OFFSET + 0x14)                ///< 运行优先级寄存器，指示当前CPU接口的运行优先级
#define GICC_HPPIR                      (GICC_OFFSET + 0x18)                ///< 最高优先级挂起中断寄存器，显示当前挂起的最高优先级中断
/** @} */
#endif

/**
 * @defgroup gic_distributor_reg GIC分发器寄存器偏移
 * @brief GIC Distributor控制和配置寄存器偏移定义(适用于所有GIC版本)
 * @{ 
 */
#define GICD_CTLR                       (GICD_OFFSET + 0x000)               ///< 分发器控制寄存器，全局启用/禁用中断分发
#define GICD_TYPER                      (GICD_OFFSET + 0x004)               ///< 中断控制器类型寄存器，包含支持的中断数量等信息
#define GICD_IIDR                       (GICD_OFFSET + 0x008)               ///< 分发器实现标识寄存器，包含厂商ID和版本信息
#define GICD_IGROUPR(n)                 (GICD_OFFSET + 0x080 + (n) * 4)     ///< 中断组寄存器，配置中断所属组(安全/非安全)
#define GICD_ISENABLER(n)               (GICD_OFFSET + 0x100 + (n) * 4)     ///< 中断使能设置寄存器，置位对应位启用中断
#define GICD_ICENABLER(n)               (GICD_OFFSET + 0x180 + (n) * 4)     ///< 中断使能清除寄存器，置位对应位禁用中断
#define GICD_ISPENDR(n)                 (GICD_OFFSET + 0x200 + (n) * 4)     ///< 中断挂起设置寄存器，置位对应位触发中断
#define GICD_ICPENDR(n)                 (GICD_OFFSET + 0x280 + (n) * 4)     ///< 中断挂起清除寄存器，置位对应位清除挂起
#define GICD_ISACTIVER(n)               (GICD_OFFSET + 0x300 + (n) * 4)     ///< GICv2中断激活设置寄存器，标记中断为活跃状态
#define GICD_ICACTIVER(n)               (GICD_OFFSET + 0x380 + (n) * 4)     ///< 中断激活清除寄存器，清除中断活跃状态
#define GICD_IPRIORITYR(n)              (GICD_OFFSET + 0x400 + (n) * 4)     ///< 中断优先级寄存器，每个字节对应一个中断的优先级
#define GICD_ITARGETSR(n)               (GICD_OFFSET + 0x800 + (n) * 4)     ///< 中断处理器目标寄存器，配置中断发送到哪个CPU
#define GICD_ICFGR(n)                   (GICD_OFFSET + 0xc00 + (n) * 4)     ///< 中断配置寄存器，设置中断触发类型(电平/边沿)
#define GICD_SGIR                       (GICD_OFFSET + 0xf00)               ///< 软件生成中断寄存器，用于产生SGI中断
#define GICD_CPENDSGIR(n)               (GICD_OFFSET + 0xf10 + (n) * 4)     ///< SGI挂起清除寄存器，Cortex-A9不支持
#define GICD_SPENDSGIR(n)               (GICD_OFFSET + 0xf20 + (n) * 4)     ///< SGI挂起设置寄存器，Cortex-A9不支持
#define GICD_PIDR2V2                    (GICD_OFFSET + 0xfe8)               ///< GICv2外设ID寄存器2，用于识别GIC版本
#define GICD_PIDR2V3                    (GICD_OFFSET + 0xffe8)              ///< GICv3外设ID寄存器2，用于识别GIC版本
/** @} */

#ifdef LOSCFG_ARCH_GIC_V3
/**
 * @defgroup gicv3_distributor_reg GICv3特有分发器寄存器
 * @brief GICv3架构新增的分发器寄存器偏移定义
 * @{ 
 */
#define GICD_IGRPMODR(n)                (GICD_OFFSET + 0x0d00 + (n) * 4)    ///< 中断组模式寄存器，配置中断组1的安全状态
#define GICD_IROUTER(n)                 (GICD_OFFSET + 0x6000 + (n) * 8)    ///< 中断路由寄存器，配置中断路由目标(支持64位)
/** @} */
#endif

/**
 * @defgroup gic_reg_access GIC寄存器访问宏
 * @brief 用于访问GIC寄存器的类型转换宏
 * @{ 
 */
#define GIC_REG_8(reg)                  (*(volatile UINT8 *)((UINTPTR)(GIC_BASE_ADDR + (reg))))  ///< 8位GIC寄存器访问
#define GIC_REG_32(reg)                 (*(volatile UINT32 *)((UINTPTR)(GIC_BASE_ADDR + (reg)))) ///< 32位GIC寄存器访问
#define GIC_REG_64(reg)                 (*(volatile UINT64 *)((UINTPTR)(GIC_BASE_ADDR + (reg)))) ///< 64位GIC寄存器访问
/** @} */

/**
 * @defgroup gic_priority_def GIC优先级默认值
 * @brief GIC中断优先级默认配置
 * @{ 
 */
#define GICD_INT_DEF_PRI                0xa0U                               ///< 默认中断优先级(十进制160)，中等优先级
#define GICD_INT_DEF_PRI_X4             (((UINT32)GICD_INT_DEF_PRI << 24) | \
                                         ((UINT32)GICD_INT_DEF_PRI << 16) | \
                                         ((UINT32)GICD_INT_DEF_PRI << 8)  | \
                                         (UINT32)GICD_INT_DEF_PRI)         ///< 4个默认优先级打包值(用于一次配置4个中断)
/** @} */

#define GIC_MIN_SPI_NUM                 32U                                  ///< 最小SPI中断号，SPI中断从32开始(0-31为SGI/PPI)

/* Interrupt preemption config - 中断抢占配置 */
#define GIC_PRIORITY_MASK               0xFFU                               ///< GIC优先级掩码(8位优先级)
#define GIC_PRIORITY_OFFSET             8U                                  ///< 优先级位宽(8位)

/**
 * @brief 中断优先级移位位数计算
 * @details 根据GIC实现的优先级位数确定移位值，MAX_BINARY_POINT_VALUE为7表示不支持中断抢占
 */
#ifndef LOSCFG_ARCH_INTERRUPT_PREEMPTION
#define MAX_BINARY_POINT_VALUE              7U                                 ///< 最大二进制点值(不支持抢占)
#define PRIORITY_SHIFT                      0U                                 ///< 优先级移位值(0表示不移位)
#define GIC_MAX_INTERRUPT_PREEMPTION_LEVEL  0U                                 ///< 中断最高优先级(0表示不支持抢占)
#else
#define PRIORITY_SHIFT                      ((MAX_BINARY_POINT_VALUE + 1) % GIC_PRIORITY_OFFSET) ///< 优先级移位计算
#define GIC_MAX_INTERRUPT_PREEMPTION_LEVEL  ((UINT8)((GIC_PRIORITY_MASK + 1) >> PRIORITY_SHIFT)) ///< 计算最大抢占级别
#endif

#define GIC_INTR_PRIO_MASK                  ((UINT8)(0xFFFFFFFFU << PRIORITY_SHIFT)) ///< 中断优先级掩码(抢占部分)

/**
 * @brief 最低中断优先级计算
 * @details 抢占级别最高可达128级，对应中断优先级最大值为254[7:1]。
 *          若GIC_MAX_INTERRUPT_PREEMPTION_LEVEL为0，则最低优先级为0xff
 */
#define MIN_INTERRUPT_PRIORITY              ((UINT8)((GIC_MAX_INTERRUPT_PREEMPTION_LEVEL - 1) << PRIORITY_SHIFT)) ///< 中断最低优先级

#endif