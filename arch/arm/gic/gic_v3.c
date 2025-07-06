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

#include "gic_common.h"
#include "gic_v3.h"
#include "los_typedef.h"
#include "los_hwi.h"
#include "los_hwi_pri.h"
#include "los_mp.h"

#ifdef LOSCFG_ARCH_GIC_V3

STATIC UINT32 g_curIrqNum = 0;  // 当前中断号

/**
 * @brief 将MPIDR寄存器值转换为GICv3亲和性值
 * @param mpidr MPIDR寄存器值，包含CPU亲和性信息
 * @return 转换后的64位亲和性值，格式为[AFF3:AFF2:AFF1:AFF0]
 */
STATIC INLINE UINT64 MpidrToAffinity(UINT64 mpidr)
{
    return ((MPIDR_AFF_LEVEL(mpidr, 3) << 32) |  // 提取AFF3字段并左移32位
            (MPIDR_AFF_LEVEL(mpidr, 2) << 16) |  // 提取AFF2字段并左移16位
            (MPIDR_AFF_LEVEL(mpidr, 1) << 8)  |  // 提取AFF1字段并左移8位
            (MPIDR_AFF_LEVEL(mpidr, 0)));        // 提取AFF0字段
}

#ifdef LOSCFG_KERNEL_SMP

/**
 * @brief 查找下一个有效的CPU核心
 * @param cpu 当前CPU核心号
 * @param cpuMask CPU掩码，表示可用的CPU核心
 * @return 下一个有效的CPU核心号，若不存在则返回LOSCFG_KERNEL_CORE_NUM
 */
STATIC UINT32 NextCpu(UINT32 cpu, UINT32 cpuMask)
{
    UINT32 next = cpu + 1;  // 从当前CPU的下一个核心开始查找

    while (next < LOSCFG_KERNEL_CORE_NUM) {
        if (cpuMask & (1U << next)) {  // 检查下一个CPU核心是否在可用掩码中
            goto OUT;
        }

        next++;
    }

OUT:
    return next;
}

/**
 * @brief 生成GIC中断目标CPU列表
 * @param base [in/out] 起始CPU索引，输出时为当前处理到的CPU索引
 * @param cpuMask CPU掩码，表示目标CPU核心
 * @param cluster 集群ID
 * @return 16位目标CPU列表，每一位代表一个CPU核心
 */
STATIC UINT16 GicTargetList(UINT32 *base, UINT32 cpuMask, UINT64 cluster)
{
    UINT32 nextCpu;
    UINT16 tList = 0;  // 目标CPU列表，初始化为0
    UINT32 cpu = *base;  // 从起始CPU开始处理
    UINT64 mpidr = CPU_MAP_GET(cpu);  // 获取当前CPU的MPIDR值
    while (cpu < LOSCFG_KERNEL_CORE_NUM) {
        tList |= 1U << (mpidr & 0xf);  // 将当前CPU添加到目标列表

        nextCpu = NextCpu(cpu, cpuMask);  // 获取下一个有效的CPU
        if (nextCpu >= LOSCFG_KERNEL_CORE_NUM) {
            goto out;
        }

        cpu = nextCpu;
        mpidr = CPU_MAP_GET(cpu);  // 获取下一个CPU的MPIDR值
        if (cluster != (mpidr & ~0xffUL)) {  // 检查是否属于同一集群
            cpu--;
            goto out;
        }
    }

out:
    *base = cpu;  // 更新输出CPU索引
    return tList;
}

/**
 * @brief 发送SGI中断到指定CPU核心
 * @param irq SGI中断号(0-15)
 * @param cpuMask CPU掩码，表示目标CPU核心
 */
STATIC VOID GicSgi(UINT32 irq, UINT32 cpuMask)
{
    UINT16 tList;
    UINT32 cpu = 0;  // 从CPU 0开始处理
    UINT64 val, cluster;

    while (cpuMask && (cpu < LOSCFG_KERNEL_CORE_NUM)) {
        if (cpuMask & (1U << cpu)) {  // 检查当前CPU是否在目标掩码中
            cluster = CPU_MAP_GET(cpu) & ~0xffUL;  // 获取集群ID

            tList = GicTargetList(&cpu, cpuMask, cluster);  // 生成目标CPU列表

            /* Generates a Group 1 interrupt for the current security state */
            val = ((MPIDR_AFF_LEVEL(cluster, 3) << 48) |  // 设置AFF3字段
                   (MPIDR_AFF_LEVEL(cluster, 2) << 32) |  // 设置AFF2字段
                   (MPIDR_AFF_LEVEL(cluster, 1) << 16) |  // 设置AFF1字段
                   (irq << 24) | tList);  // 设置中断号和目标列表

            GiccSetSgi1r(val);  // 写入SGI1R寄存器发送中断
        }

        cpu++;
    }
}

/**
 * @brief 发送IPI中断到指定CPU核心
 * @param target 目标CPU掩码
 * @param ipi IPI中断号
 */
VOID HalIrqSendIpi(UINT32 target, UINT32 ipi)
{
    GicSgi(ipi, target);  // 调用GicSgi发送IPI中断
}

/**
 * @brief 设置中断的CPU亲和性
 * @param irq 中断号
 * @param cpuMask CPU掩码，表示中断可以发送到的CPU核心
 */
VOID HalIrqSetAffinity(UINT32 irq, UINT32 cpuMask)
{
    UINT64 affinity = MpidrToAffinity(NextCpu(0, cpuMask));  // 获取目标CPU的亲和性值

    /* When ARE is on, use router */
    GIC_REG_64(GICD_IROUTER(irq)) = affinity;  // 设置中断路由寄存器
}

#endif

/**
 * @brief 等待GIC寄存器写入完成
 * @param reg 要等待的寄存器地址
 */
STATIC VOID GicWaitForRwp(UINT64 reg)
{
    INT32 count = 1000000;  /* 1s超时计数 */

    while (GIC_REG_32(reg) & GICD_CTLR_RWP) {  // 检查RWP位是否清除
        count -= 1;
        if (!count) {  // 超时检查
            PRINTK("gic_v3: rwp timeout 0x%x\n", GIC_REG_32(reg));
            return;
        }
    }
}

/**
 * @brief 设置GICD中断组
 * @param irq 中断号
 */
STATIC INLINE VOID GicdSetGroup(UINT32 irq)
{
    /* configure spi as group 0 on secure mode and group 1 on unsecure mode */
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    GIC_REG_32(GICD_IGROUPR(irq / 32)) = 0;  /* 32: 中断位宽度，安全模式下设置为组0 */
#else
    GIC_REG_32(GICD_IGROUPR(irq / 32)) = 0xffffffff;  /* 32: 中断位宽度，非安全模式下设置为组1 */
#endif
}

/**
 * @brief 设置GICR唤醒状态
 * @param cpu CPU核心号
 */
STATIC INLINE VOID GicrSetWaker(UINT32 cpu)
{
    GIC_REG_32(GICR_WAKER(cpu)) &= ~GICR_WAKER_PROCESSORSLEEP;  // 清除处理器睡眠位
    DSB;  // 数据同步屏障
    ISB;  // 指令同步屏障
    while ((GIC_REG_32(GICR_WAKER(cpu)) & 0x4) == GICR_WAKER_CHILDRENASLEEP);  // 等待子控制器唤醒
}

/**
 * @brief 设置GICR中断组
 * @param cpu CPU核心号
 */
STATIC INLINE VOID GicrSetGroup(UINT32 cpu)
{
    /* configure sgi/ppi as group 0 on secure mode and group 1 on unsecure mode */
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    GIC_REG_32(GICR_IGROUPR0(cpu)) = 0;  // 安全模式下设置SGI/PPI为组0
    GIC_REG_32(GICR_IGRPMOD0(cpu)) = 0;  // 安全模式下设置中断模式
#else
    GIC_REG_32(GICR_IGROUPR0(cpu)) = 0xffffffff;  // 非安全模式下设置SGI/PPI为组1
#endif
}

/**
 * @brief 设置GICD中断优先级
 * @param irq 中断号
 * @param priority 中断优先级(0-255)
 */
STATIC VOID GicdSetPmr(UINT32 irq, UINT8 priority)
{
    UINT32 pos = irq >> 2;  /* 每个寄存器包含4个中断的优先级字段 */
    UINT32 newPri = GIC_REG_32(GICD_IPRIORITYR(pos));  // 读取当前优先级寄存器值

    /* Shift and mask the correct bits for the priority */
    newPri &= ~(GIC_PRIORITY_MASK << ((irq % 4) * GIC_PRIORITY_OFFSET));  // 清除当前中断的优先级位
    newPri |= priority << ((irq % 4) * GIC_PRIORITY_OFFSET);  // 设置新的优先级

    GIC_REG_32(GICD_IPRIORITYR(pos)) = newPri;  // 写入优先级寄存器
}

/**
 * @brief 设置GICR中断优先级
 * @param irq 中断号
 * @param priority 中断优先级(0-255)
 */
STATIC VOID GicrSetPmr(UINT32 irq, UINT8 priority)
{
    UINT32 cpu = ArchCurrCpuid();  // 获取当前CPU核心号
    UINT32 pos = irq >> 2;  /* 每个寄存器包含4个中断的优先级字段 */
    UINT32 newPri = GIC_REG_32(GICR_IPRIORITYR0(cpu) + pos * 4);  // 读取当前优先级寄存器值

    /* Clear priority offset bits and set new priority */
    newPri &= ~(GIC_PRIORITY_MASK << ((irq % 4) * GIC_PRIORITY_OFFSET));  // 清除当前中断的优先级位
    newPri |= priority << ((irq % 4) * GIC_PRIORITY_OFFSET);  // 设置新的优先级

    GIC_REG_32(GICR_IPRIORITYR0(cpu) + pos * 4) = newPri;  // 写入优先级寄存器
}

/**
 * @brief 每CPU初始化GICC
 */
STATIC VOID GiccInitPercpu(VOID)
{
    /* enable system register interface */
    UINT32 sre = GiccGetSre();  // 读取系统寄存器使能状态
    if (!(sre & 0x1)) {  // 检查系统寄存器接口是否已使能
        GiccSetSre(sre | 0x1);  // 使能系统寄存器接口

        /*
         * Need to check that the SRE bit has actually been set. If
         * not, it means that SRE is disabled at up EL level. We're going to
         * die painfully, and there is nothing we can do about it.
         */
        sre = GiccGetSre();  // 重新读取SRE状态
        LOS_ASSERT(sre & 0x1);  // 断言检查SRE是否已成功使能
    }

#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    /* Enable group 0 and disable grp1ns grp1s interrupts */
    GiccSetIgrpen0(1);  // 使能组0中断
    GiccSetIgrpen1(0);  // 禁用组1中断

    /*
     * For priority grouping.
     * The value of this field control show the 8-bit interrupt priority field
     * is split into a group priority field, that determines interrupt preemption,
     * and a subpriority field.
     */
    GiccSetBpr0(MAX_BINARY_POINT_VALUE);  // 设置优先级分组
#else
    /* enable group 1 interrupts */
    GiccSetIgrpen1(1);  // 使能组1中断
#endif

    /* set priority threshold to max */
    GiccSetPmr(0xff);  // 设置优先级阈值为最低(所有中断都可抢占)

    /* EOI deactivates interrupt too (mode 0) */
    GiccSetCtlr(0);  // 设置GICC控制寄存器
}

/**
 * @brief 获取当前中断号
 * @return 当前处理的中断号
 */
UINT32 HalCurIrqGet(VOID)
{
    return g_curIrqNum;  // 返回当前中断号
}

/**
 * @brief 屏蔽指定中断
 * @param vector 中断号
 */
VOID HalIrqMask(UINT32 vector)
{
    INT32 i;
    const UINT32 mask = 1U << (vector % 32);  /* 32: 中断位宽度，计算中断掩码 */

    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {  // 检查中断号是否有效
        return;
    }

    if (vector < 32) {  /* 32: 中断位宽度，判断是否为SGI/PPI中断 */
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {  // 遍历所有CPU核心
            GIC_REG_32(GICR_ICENABLER0(i)) = mask;  // 禁用该CPU上的中断
            GicWaitForRwp(GICR_CTLR(i));  // 等待写入完成
        }
    } else {
        GIC_REG_32(GICD_ICENABLER(vector >> 5)) = mask;  // 禁用SPI中断
        GicWaitForRwp(GICD_CTLR);  // 等待写入完成
    }
}

/**
 * @brief 解除屏蔽指定中断
 * @param vector 中断号
 */
VOID HalIrqUnmask(UINT32 vector)
{
    INT32 i;
    const UINT32 mask = 1U << (vector % 32);  /* 32: 中断位宽度，计算中断掩码 */

    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {  // 检查中断号是否有效
        return;
    }

    if (vector < 32) {  /* 32: 中断位宽度，判断是否为SGI/PPI中断 */
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {  // 遍历所有CPU核心
            GIC_REG_32(GICR_ISENABLER0(i)) = mask;  // 使能该CPU上的中断
            GicWaitForRwp(GICR_CTLR(i));  // 等待写入完成
        }
    } else {
        GIC_REG_32(GICD_ISENABLER(vector >> 5)) = mask;  /* 5: 右移5位计算寄存器索引，使能SPI中断 */
        GicWaitForRwp(GICD_CTLR);  // 等待写入完成
    }
}

/**
 * @brief 挂起指定中断
 * @param vector 中断号
 */
VOID HalIrqPending(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {  // 检查中断号是否有效
        return;
    }

    GIC_REG_32(GICD_ISPENDR(vector >> 5)) = 1U << (vector % 32);  /* 5: 寄存器索引位移，32: 中断位宽度，设置中断挂起位 */
}

/**
 * @brief 清除指定中断
 * @param vector 中断号
 */
VOID HalIrqClear(UINT32 vector)
{
    GiccSetEoir(vector);  // 写入EOIR寄存器，结束中断
    ISB;  // 指令同步屏障
}

/**
 * @brief 设置中断优先级
 * @param vector 中断号
 * @param priority 中断优先级(0-255)
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
UINT32 HalIrqSetPrio(UINT32 vector, UINT8 priority)
{
    UINT8 prio = priority;

    if (vector > OS_HWI_MAX_NUM) {  // 检查中断号是否超出最大范围
        PRINT_ERR("Invalid irq value %u, max irq is %u\n", vector, OS_HWI_MAX_NUM);
        return LOS_NOK;
    }

    prio = prio & (UINT8)GIC_INTR_PRIO_MASK;  // 确保优先级在有效范围内

    if (vector >= GIC_MIN_SPI_NUM) {  // 判断是否为SPI中断
        GicdSetPmr(vector, prio);  // 设置SPI中断优先级
    } else {
        GicrSetPmr(vector, prio);  // 设置SGI/PPI中断优先级
    }

    return LOS_OK;
}

/**
 * @brief 每CPU初始化GIC
 */
VOID HalIrqInitPercpu(VOID)
{
    INT32 idx;
    UINT32 cpu = ArchCurrCpuid();  // 获取当前CPU核心号

    /* GICR init */
    GicrSetWaker(cpu);  // 设置GICR唤醒状态
    GicrSetGroup(cpu);  // 设置GICR中断组
    GicWaitForRwp(GICR_CTLR(cpu));  // 等待GICR配置完成

    /* GICR: clear and mask sgi/ppi */
    GIC_REG_32(GICR_ICENABLER0(cpu)) = 0xffffffff;  // 禁用所有SGI/PPI中断
    GIC_REG_32(GICR_ICPENDR0(cpu)) = 0xffffffff;  // 清除所有SGI/PPI中断挂起位

    GIC_REG_32(GICR_ISENABLER0(cpu)) = 0xffffffff;  // 使能所有SGI/PPI中断

    for (idx = 0; idx < GIC_MIN_SPI_NUM; idx += 1) {  // 遍历所有SGI/PPI中断
        GicrSetPmr(idx, MIN_INTERRUPT_PRIORITY);  // 设置默认优先级
    }

    GicWaitForRwp(GICR_CTLR(cpu));  // 等待GICR配置完成

    /* GICC init */
    GiccInitPercpu();  // 初始化GICC

#ifdef LOSCFG_KERNEL_SMP
    /* unmask ipi interrupts */
    HalIrqUnmask(LOS_MP_IPI_WAKEUP);  // 解除IPI唤醒中断屏蔽
    HalIrqUnmask(LOS_MP_IPI_HALT);  // 解除IPI停止中断屏蔽
#endif
}

/**
 * @brief 初始化GIC中断控制器
 */
VOID HalIrqInit(VOID)
{
    UINT32 i;
    UINT64 affinity;

    /* disable distributor */
    GIC_REG_32(GICD_CTLR) = 0;  // 禁用GICD
    GicWaitForRwp(GICD_CTLR);  // 等待GICD配置完成
    ISB;  // 指令同步屏障

    /* set external interrupts to be level triggered, active low. */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 16) { /* 32: 起始中断号，16: 中断位宽度，配置中断触发方式 */
        GIC_REG_32(GICD_ICFGR(i / 16)) = 0;  // 设置为电平触发
    }

    /* config distributer, mask and clear all spis, set group x */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 32) { /* 32: 起始中断号，32: 中断位宽度，配置SPI中断 */
        GIC_REG_32(GICD_ICENABLER(i / 32)) = 0xffffffff; /* 32: 中断位宽度，禁用SPI中断 */
        GIC_REG_32(GICD_ICPENDR(i / 32)) = 0xffffffff; /* 32: 中断位宽度，清除SPI中断挂起位 */
        GIC_REG_32(GICD_IGRPMODR(i / 32)) = 0; /* 32: 中断位宽度，设置中断模式 */

        GicdSetGroup(i);  // 设置SPI中断组
    }

    /* set spi priority as default */
    for (i = 32; i < OS_HWI_MAX_NUM; i++) { /* 32: 起始中断号，设置SPI中断优先级 */
        GicdSetPmr(i, MIN_INTERRUPT_PRIORITY);  // 设置默认优先级
    }

    GicWaitForRwp(GICD_CTLR);  // 等待GICD配置完成

    /* disable all interrupts. */
    for (i = 0; i < OS_HWI_MAX_NUM; i += 32) { /* 32: 中断位宽度，禁用所有中断 */
        GIC_REG_32(GICD_ICENABLER(i / 32)) = 0xffffffff; /* 32: 中断位宽度 */
    }

    /* enable distributor with ARE, group 1 enabled */
    GIC_REG_32(GICD_CTLR) = CTLR_ENALBE_G0 | CTLR_ENABLE_G1NS | CTLR_ARE_S;  // 使能GICD并配置ARE

    /* set spi to boot cpu only. ARE must be enabled */
    affinity = MpidrToAffinity(AARCH64_SYSREG_READ(mpidr_el1));  // 获取启动CPU的亲和性值
    for (i = 32; i < OS_HWI_MAX_NUM; i++) { /* 32: 起始中断号，设置SPI中断路由 */
        GIC_REG_64(GICD_IROUTER(i)) = affinity;  // 设置中断路由到启动CPU
    }

    HalIrqInitPercpu();  // 初始化每CPU GIC配置

#ifdef LOSCFG_KERNEL_SMP
    /* register inter-processor interrupt */
    (VOID)LOS_HwiCreate(LOS_MP_IPI_WAKEUP, 0xa0, 0, OsMpWakeHandler, 0);  // 创建IPI唤醒中断
    (VOID)LOS_HwiCreate(LOS_MP_IPI_SCHEDULE, 0xa0, 0, OsMpScheduleHandler, 0);  // 创建IPI调度中断
    (VOID)LOS_HwiCreate(LOS_MP_IPI_HALT, 0xa0, 0, OsMpScheduleHandler, 0);  // 创建IPI停止中断
#ifdef LOSCFG_KERNEL_SMP_CALL
    (VOID)LOS_HwiCreate(LOS_MP_IPI_FUNC_CALL, 0xa0, 0, OsMpFuncCallHandler, 0);  // 创建IPI函数调用中断
#endif
#endif
}

/**
 * @brief GIC中断处理函数
 */
VOID HalIrqHandler(VOID)
{
    UINT32 iar = GiccGetIar();  // 读取中断确认寄存器
    UINT32 vector = iar & 0x3FFU;  // 提取中断号

    /*
     * invalid irq number, mainly the spurious interrupts 0x3ff,
     * valid irq ranges from 0~1019, we use OS_HWI_MAX_NUM to do
     * the checking.
     */
    if (vector >= OS_HWI_MAX_NUM) {  // 检查中断号是否有效
        return;
    }
    g_curIrqNum = vector;  // 更新当前中断号

    OsInterrupt(vector);  // 调用内核中断处理函数
    GiccSetEoir(vector);  // 写入EOIR寄存器，结束中断
}

/**
 * @brief 获取GIC版本信息
 * @return GIC版本字符串
 */
CHAR *HalIrqVersion(VOID)
{
    UINT32 pidr = GIC_REG_32(GICD_PIDR2V3);  // 读取GICD版本寄存器
    CHAR *irqVerString = NULL;

    switch (pidr >> GIC_REV_OFFSET) {  // 提取版本字段
        case GICV3:
            irqVerString = "GICv3";  // GICv3版本
            break;
        case GICV4:
            irqVerString = "GICv4";  // GICv4版本
            break;
        default:
            irqVerString = "unknown";  // 未知版本
    }
    return irqVerString;
}

#endif