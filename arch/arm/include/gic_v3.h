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

#ifndef _GIC_V3_H_
#define _GIC_V3_H_

#include "stdint.h"
#include "target_config.h"
#include "los_hw_cpu.h"

/**
 * @file gic_v3.h
 * @brief GICv3架构专用寄存器和宏定义头文件
 * @details 定义GICv3特有的系统寄存器、控制位和操作宏，适用于ARMv8及以上架构
 */
/**
 * @defgroup gicv3_bitops 位操作宏
 * @brief GICv3寄存器位操作辅助宏
 */
#define BIT_32(bit) (1u << bit)          ///< 32位位操作宏，生成指定位置为1的32位掩码
#define BIT_64(bit) (1ul << bit)         ///< 64位位操作宏，生成指定位置为1的64位掩码

/**
 * @defgroup gicv3_icc_el1_reg EL1级CPU接口寄存器
 * @brief GICv3架构下EL1异常级别使用的CPU接口系统寄存器
 * @note 寄存器名称格式为"S<op0>_<op1>_C<CRn>_C<CRm>_<op2>"
 */
#define ICC_CTLR_EL1    "S3_0_C12_C12_4" ///< CPU接口控制寄存器(EL1)，配置中断分发特性
#define ICC_PMR_EL1     "S3_0_C4_C6_0"  ///< 中断优先级掩码寄存器(EL1)，设置中断优先级阈值
#define ICC_IAR1_EL1    "S3_0_C12_C12_0" ///< 中断应答寄存器1(EL1)，获取组1中断ID
#define ICC_SRE_EL1     "S3_0_C12_C12_5" ///< 系统寄存器接口使能寄存器(EL1)，控制GIC系统寄存器访问
#define ICC_BPR0_EL1    "S3_0_C12_C8_3"  ///< 二进制点寄存器0(EL1)，配置组0中断优先级分组
#define ICC_BPR1_EL1    "S3_0_C12_C12_3" ///< 二进制点寄存器1(EL1)，配置组1中断优先级分组
#define ICC_IGRPEN0_EL1 "S3_0_C12_C12_6" ///< 中断组0使能寄存器(EL1)，全局控制组0中断使能
#define ICC_IGRPEN1_EL1 "S3_0_C12_C12_7" ///< 中断组1使能寄存器(EL1)，全局控制组1中断使能
#define ICC_EOIR1_EL1   "S3_0_C12_C12_1" ///< 中断结束寄存器1(EL1)，完成组1中断处理
#define ICC_SGI1R_EL1   "S3_0_C12_C11_5" ///< SGI触发寄存器1(EL1)，生成软件触发中断
#define ICC_EOIR0_EL1   "S3_0_c12_c8_1"  ///< 中断结束寄存器0(EL1)，完成组0中断处理
#define ICC_IAR0_EL1    "S3_0_C12_C8_0"  ///< 中断应答寄存器0(EL1)，获取组0中断ID

/**
 * @defgroup gicv3_icc_el3_reg EL3级CPU接口寄存器
 * @brief GICv3架构下EL3异常级别使用的CPU接口系统寄存器
 */
#define ICC_CTLR_EL3    "S3_6_C12_C12_4" ///< CPU接口控制寄存器(EL3)，配置安全状态中断分发
#define ICC_SRE_EL3     "S3_6_C12_C12_5" ///< 系统寄存器接口使能寄存器(EL3)，控制安全状态GIC访问
#define ICC_IGRPEN1_EL3 "S3_6_C12_C12_7" ///< 中断组1使能寄存器(EL3)，控制安全状态组1中断

/**
 * @defgroup gicd_ctlr_bits GICD_CTLR控制位定义
 * @brief GIC分发器控制寄存器(GICD_CTLR)的位域定义
 */
#define CTLR_ENALBE_G0  BIT_32(0)         ///< 组0中断全局使能位(bit0)，1=使能组0中断分发
#define CTLR_ENABLE_G1NS BIT_32(1)        ///< 非安全组1中断使能位(bit1)，1=使能非安全组1中断
#define CTLR_ENABLE_G1S  BIT_32(2)        ///< 安全组1中断使能位(bit2)，1=使能安全组1中断
#define CTLR_RES0        BIT_32(3)        ///< 保留位(bit3)，必须保持0
#define CTLR_ARE_S       BIT_32(4)        ///< 安全状态亲和性路由使能位(bit4)，1=使能亲和性路由
#define CTLR_ARE_NS      BIT_32(5)        ///< 非安全状态亲和性路由使能位(bit5)，1=使能亲和性路由
#define CTLR_DS          BIT_32(6)        ///< 禁用安全状态位(bit6)，1=安全状态下禁用GIC功能
#define CTLR_E1NWF       BIT_32(7)        ///< 组1中断非等待完成位(bit7)，1=无需等待EOI完成
#define GICD_CTLR_RWP    BIT_32(31)       ///< 读-写权限位(bit31)，1=允许非安全状态访问安全寄存器

/**
 * @defgroup gicd_id_regs 外设标识寄存器
 * @brief GIC分发器的组件标识和外设标识寄存器，用于硬件识别和兼容性检查
 */
/* peripheral identification registers */
#define GICD_CIDR0 (GICD_OFFSET + 0xfff0)  ///< 组件标识寄存器0，包含JEDEC厂商代码
#define GICD_CIDR1 (GICD_OFFSET + 0xfff4)  ///< 组件标识寄存器1，包含产品族信息
#define GICD_CIDR2 (GICD_OFFSET + 0xfff8)  ///< 组件标识寄存器2，包含产品子类型
#define GICD_CIDR3 (GICD_OFFSET + 0xfffc)  ///< 组件标识寄存器3，包含版本信息
#define GICD_PIDR0 (GICD_OFFSET + 0xffe0)  ///< 外设标识寄存器0，包含部件号[7:0]
#define GICD_PIDR1 (GICD_OFFSET + 0xffe4)  ///< 外设标识寄存器1，包含部件号[15:8]
#define GICD_PIDR2 (GICD_OFFSET + 0xffe8)  ///< 外设标识寄存器2，包含架构版本和修订号
#define GICD_PIDR3 (GICD_OFFSET + 0xffec)  ///< 外设标识寄存器3，包含实现者代码

/**
 * @defgroup gicd_pidr_bits GICD_PIDR位域定义
 * @brief GIC分发器外设标识寄存器的位域提取宏
 */
#define GICD_PIDR2_ARCHREV_SHIFT 4         ///< 架构版本移位值(bit4-7)
#define GICD_PIDR2_ARCHREV_MASK 0xf        ///< 架构版本掩码，用于提取GIC架构版本号

/**
 * @defgroup gicr_regs 重分布器寄存器
 * @brief GICv3重分布器( Redistributor )寄存器定义，每个CPU接口对应独立的重分布器
 * @details 重分布器负责管理私有外设中断(PPI)和软件生成中断(SGI)，并与特定CPU核心关联
 */
/* redistributor registers */
#define GICR_SGI_OFFSET (GICR_OFFSET + 0x10000)  ///< SGI/PPI寄存器组偏移，距离重分布器基地址0x10000

/**
 * @defgroup gicr_core_regs 重分布器核心寄存器
 * @brief 重分布器基本控制和状态寄存器
 */
#define GICR_CTLR(i)        (GICR_OFFSET + GICR_STRIDE * (i) + 0x0000)  ///< 重分布器控制寄存器，启用/禁用中断分发
#define GICR_IIDR(i)        (GICR_OFFSET + GICR_STRIDE * (i) + 0x0004)  ///< 重分布器实现标识寄存器，包含版本信息
#define GICR_TYPER(i, n)    (GICR_OFFSET + GICR_STRIDE * (i) + 0x0008 + (n)*4)  ///< 重分布器类型寄存器，n=0:基本类型,n=1:扩展类型
#define GICR_STATUSR(i)     (GICR_OFFSET + GICR_STRIDE * (i) + 0x0010)  ///< 重分布器状态寄存器，指示当前工作状态
#define GICR_WAKER(i)       (GICR_OFFSET + GICR_STRIDE * (i) + 0x0014)  ///< 重分布器唤醒控制寄存器，管理电源状态转换

/**
 * @defgroup gicr_irq_regs 重分布器中断控制寄存器
 * @brief 用于控制SGI/PPI中断的寄存器组
 */
#define GICR_IGROUPR0(i)    (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0080)  ///< 中断组寄存器0，配置SGI/PPI中断所属组
#define GICR_IGRPMOD0(i)    (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0d00)  ///< 中断组模式寄存器0，配置组1中断的安全属性
#define GICR_ISENABLER0(i)  (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0100)  ///< 中断使能设置寄存器0，置位使能对应中断
#define GICR_ICENABLER0(i)  (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0180)  ///< 中断使能清除寄存器0，置位禁用对应中断
#define GICR_ISPENDR0(i)    (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0200)  ///< 中断挂起设置寄存器0，置位触发中断挂起
#define GICR_ICPENDR0(i)    (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0280)  ///< 中断挂起清除寄存器0，置位清除中断挂起
#define GICR_ISACTIVER0(i)  (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0300)  ///< 中断激活设置寄存器0，置位标记中断为活跃
#define GICR_ICACTIVER0(i)  (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0380)  ///< 中断激活清除寄存器0，置位清除活跃状态
#define GICR_IPRIORITYR0(i) (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0400)  ///< 中断优先级寄存器0，每个字节对应一个中断优先级
#define GICR_ICFGR0(i)      (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0c00)  ///< 中断配置寄存器0，设置中断触发类型(边沿/电平)
#define GICR_ICFGR1(i)      (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0c04)  ///< 中断配置寄存器1，扩展中断配置
#define GICR_NSACR(i)       (GICR_SGI_OFFSET + GICR_STRIDE * (i) + 0x0e00)  ///< 非安全访问控制寄存器，控制非安全状态访问权限

/**
 * @defgroup gicr_waker_bits GICR_WAKER位域定义
 * @brief 重分布器唤醒控制寄存器的位域定义，用于电源管理
 */
#define GICR_WAKER_PROCESSORSLEEP_LEN           1U  ///< 处理器睡眠位长度(1位)
#define GICR_WAKER_PROCESSORSLEEP_OFFSET        1   ///< 处理器睡眠位偏移(bit1)
#define GICR_WAKER_CHILDRENASLEEP_LEN           1U  ///< 子设备睡眠位长度(1位)
#define GICR_WAKER_CHILDRENASLEEP_OFFSET        2   ///< 子设备睡眠位偏移(bit2)
#define GICR_WAKER_PROCESSORSLEEP               (GICR_WAKER_PROCESSORSLEEP_LEN << GICR_WAKER_PROCESSORSLEEP_OFFSET)  ///< 处理器睡眠控制位(bit1)，1=处理器进入低功耗状态
#define GICR_WAKER_CHILDRENASLEEP               (GICR_WAKER_CHILDRENASLEEP_LEN << GICR_WAKER_CHILDRENASLEEP_OFFSET)  ///< 子设备睡眠状态位(bit2)，1=子重分布器处于睡眠状态

/**
 * @brief 设置GIC CPU接口控制寄存器
 * @param val 要写入控制寄存器的值
 * @note 根据安全监控模式配置选择不同异常级别的寄存器：
 *       - 使能安全监控模式(LOSCFG_ARCH_SECURE_MONITOR_MODE)：操作EL3级ICC_CTLR_EL3
 *       - 否则：操作EL1级ICC_CTLR_EL1
 * @attention 写入后执行ISB指令确保指令同步
 */
STATIC INLINE VOID GiccSetCtlr(UINT32 val)
{
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    __asm__ volatile("msr " ICC_CTLR_EL3 ", %0" ::"r"(val)); // 写入EL3级CPU接口控制寄存器
#else
    __asm__ volatile("msr " ICC_CTLR_EL1 ", %0" ::"r"(val)); // 写入EL1级CPU接口控制寄存器
#endif
    ISB; // 指令同步屏障，确保寄存器更新生效
}

/**
 * @brief 设置GIC CPU接口优先级掩码寄存器
 * @param val 优先级掩码值(8位有效，0x00-0xFF)，仅优先级高于此值的中断会被转发
 * @note 操作EL1级ICC_PMR_EL1寄存器，所有模式共用
 * @attention 写入后执行ISB和DSB指令确保系统可见性
 */
STATIC INLINE VOID GiccSetPmr(UINT32 val)
{
    __asm__ volatile("msr " ICC_PMR_EL1 ", %0" ::"r"(val)); // 设置中断优先级阈值
    ISB; // 指令同步屏障
    DSB; // 数据同步屏障
}

/**
 * @brief 设置GIC CPU接口中断组0使能寄存器
 * @param val 组0使能值(0=禁用，1=使能)
 * @note 操作EL1级ICC_IGRPEN0_EL1寄存器，控制Group 0中断的转发
 * @attention 写入后执行ISB指令确保状态更新
 */
STATIC INLINE VOID GiccSetIgrpen0(UINT32 val)
{
    __asm__ volatile("msr " ICC_IGRPEN0_EL1 ", %0" ::"r"(val)); // 使能/禁用Group 0中断
    ISB; // 指令同步屏障
}

/**
 * @brief 设置GIC CPU接口中断组1使能寄存器
 * @param val 组1使能值(0=禁用，1=使能)
 * @note 根据安全监控模式配置选择不同异常级别的寄存器：
 *       - 使能安全监控模式：操作EL3级ICC_IGRPEN1_EL3
 *       - 否则：操作EL1级ICC_IGRPEN1_EL1
 * @attention 控制Group 1中断的转发，写入后执行ISB指令
 */
STATIC INLINE VOID GiccSetIgrpen1(UINT32 val)
{
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    __asm__ volatile("msr " ICC_IGRPEN1_EL3 ", %0" ::"r"(val)); // EL3级Group 1中断控制
#else
    __asm__ volatile("msr " ICC_IGRPEN1_EL1 ", %0" ::"r"(val)); // EL1级Group 1中断控制
#endif
    ISB; // 指令同步屏障
}

/**
 * @brief 获取GIC CPU接口安全状态扩展寄存器值
 * @return 寄存器当前值，包含安全状态配置信息
 * @note 根据安全监控模式配置选择不同异常级别的寄存器：
 *       - 使能安全监控模式：读取EL3级ICC_SRE_EL3
 *       - 否则：读取EL1级ICC_SRE_EL1
 * @attention 用于检查系统寄存器接口(SRE)的使能状态
 */
STATIC INLINE UINT32 GiccGetSre(VOID)
{
    UINT32 temp;
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    __asm__ volatile("mrs %0, " ICC_SRE_EL3 : "=r"(temp)); // 读取EL3级安全状态扩展寄存器
#else
    __asm__ volatile("mrs %0, " ICC_SRE_EL1 : "=r"(temp)); // 读取EL1级安全状态扩展寄存器
#endif
    return temp;
}

/**
 * @brief 设置GIC CPU接口安全状态扩展寄存器
 * @param val 安全状态扩展配置值，典型值为0x7(使能所有SRE功能)
 * @note 根据安全监控模式配置选择不同异常级别的寄存器：
 *       - 使能安全监控模式：写入EL3级ICC_SRE_EL3
 *       - 否则：写入EL1级ICC_SRE_EL1
 * @attention 配置安全相关功能(如系统寄存器访问、IRQ/FIQ路由)，需ISB同步
 */
STATIC INLINE VOID GiccSetSre(UINT32 val)
{
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    __asm__ volatile("msr " ICC_SRE_EL3 ", %0" ::"r"(val)); // 配置EL3级安全状态扩展
#else
    __asm__ volatile("msr " ICC_SRE_EL1 ", %0" ::"r"(val)); // 配置EL1级安全状态扩展
#endif
    ISB; // 指令同步屏障
}

/**
 * @brief 发送GIC CPU接口中断结束信号(EOI)
 * @param val 中断ID和状态信息(从ICC_IAR_ELx获取的值)
 * @note 根据安全监控模式配置选择不同异常级别的寄存器：
 *       - 使能安全监控模式：写入EL1级ICC_EOIR0_EL1(Group 0中断)
 *       - 否则：写入EL1级ICC_EOIR1_EL1(Group 1中断)
 * @attention 必须在中断处理完成后调用，通知GIC中断已处理
 */
STATIC INLINE VOID GiccSetEoir(UINT32 val)
{
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    __asm__ volatile("msr " ICC_EOIR0_EL1 ", %0" ::"r"(val)); // Group 0中断结束确认
#else
    __asm__ volatile("msr " ICC_EOIR1_EL1 ", %0" ::"r"(val)); // Group 1中断结束确认
#endif
    ISB; // 指令同步屏障
}

/**
 * @brief 获取GIC CPU接口中断识别寄存器值
 * @return 包含当前最高优先级中断ID的寄存器值
 * @note 根据安全监控模式配置选择不同异常级别的寄存器：
 *       - 使能安全监控模式：读取EL1级ICC_IAR0_EL1(Group 0中断)
 *       - 否则：读取EL1级ICC_IAR1_EL1(Group 1中断)
 * @attention 调用后需执行DSB确保中断状态同步，返回值0x3FF表示无待处理中断
 */
STATIC INLINE UINT32 GiccGetIar(VOID)
{
    UINT32 temp;

#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    __asm__ volatile("mrs %0, " ICC_IAR0_EL1 : "=r"(temp)); // 获取Group 0中断ID
#else
    __asm__ volatile("mrs %0, " ICC_IAR1_EL1 : "=r"(temp)); // 获取Group 1中断ID
#endif
    DSB; // 数据同步屏障，确保中断状态更新

    return temp;
}

/**
 * @brief 发送软件生成中断(SGI)到指定CPU
 * @param val 64位SGI配置值，格式为[31:24]CPU掩码, [23:16]目标列表过滤器, [15:12]保留, [11:8]SGI ID, [7:0]保留
 * @note 操作EL1级ICC_SGI1R_EL1寄存器，用于多核间中断通信
 * @attention 必须执行ISB和DSB确保SGI可靠发送，典型用于核间同步
 */
STATIC INLINE VOID GiccSetSgi1r(UINT64 val)
{
    __asm__ volatile("msr " ICC_SGI1R_EL1 ", %0" ::"r"(val)); // 配置并发送SGI
    ISB; // 指令同步屏障
    DSB; // 数据同步屏障
}

/**
 * @brief 设置GIC CPU接口二进制优先级屏蔽寄存器0
 * @param val 优先级屏蔽值，控制中断抢占行为
 * @note 操作EL1级ICC_BPR0_EL1寄存器，设置Group 0中断的抢占阈值
 * @attention 写入后执行ISB和DSB确保优先级设置生效
 */
STATIC INLINE VOID GiccSetBpr0(UINT32 val)
{
    __asm__ volatile("msr " ICC_BPR0_EL1 ", %0" ::"r"(val)); // 设置Group 0优先级屏蔽
    ISB; // 指令同步屏障
    DSB; // 数据同步屏障
}
#endif
