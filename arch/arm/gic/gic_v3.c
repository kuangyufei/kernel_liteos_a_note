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

STATIC UINT32 g_curIrqNum = 0;

/**
 * @brief 将多核处理器的 MPIDR（Multiprocessor Affinity Register）值转换为中断亲和性值。
 *
 * 该函数从 MPIDR 寄存器值中提取不同级别的亲和性信息，并将其组合成一个 64 位的中断亲和性值。
 * 中断亲和性值用于指定中断应该被路由到哪些处理器核心。
 *
 * @param mpidr 64 位的 MPIDR 寄存器值，包含多核处理器的亲和性信息。
 * @return UINT64 组合后的 64 位中断亲和性值。
 */
STATIC INLINE UINT64 MpidrToAffinity(UINT64 mpidr)
{
    // 提取 MPIDR 中的 Affinity Level 3 信息，并左移 32 位
    // 3 表示 Affinity Level 的序号，32 是该信息在最终亲和性值中的位偏移
    return ((MPIDR_AFF_LEVEL(mpidr, 3) << 32) | 
            // 提取 MPIDR 中的 Affinity Level 2 信息，并左移 16 位
            // 2 表示 Affinity Level 的序号，16 是该信息在最终亲和性值中的位偏移
            (MPIDR_AFF_LEVEL(mpidr, 2) << 16) | 
            // 提取 MPIDR 中的 Affinity Level 1 信息，并左移 8 位
            // 1 表示 Affinity Level 的序号，8 是该信息在最终亲和性值中的位偏移
            (MPIDR_AFF_LEVEL(mpidr, 1) << 8)  | 
            // 提取 MPIDR 中的 Affinity Level 0 信息
            (MPIDR_AFF_LEVEL(mpidr, 0)));
}

#ifdef LOSCFG_KERNEL_SMP

STATIC UINT32 NextCpu(UINT32 cpu, UINT32 cpuMask)
{
    UINT32 next = cpu + 1;

    while (next < LOSCFG_KERNEL_CORE_NUM) {
        if (cpuMask & (1U << next)) {
            goto OUT;
        }

        next++;
    }

OUT:
    return next;
}

STATIC UINT16 GicTargetList(UINT32 *base, UINT32 cpuMask, UINT64 cluster)
{
    UINT32 nextCpu;
    UINT16 tList = 0;
    UINT32 cpu = *base;
    UINT64 mpidr = CPU_MAP_GET(cpu);
    while (cpu < LOSCFG_KERNEL_CORE_NUM) {
        tList |= 1U << (mpidr & 0xf);

        nextCpu = NextCpu(cpu, cpuMask);
        if (nextCpu >= LOSCFG_KERNEL_CORE_NUM) {
            goto out;
        }

        cpu = nextCpu;
        mpidr = CPU_MAP_GET(cpu);
        if (cluster != (mpidr & ~0xffUL)) {
            cpu--;
            goto out;
        }
    }

out:
    *base = cpu;
    return tList;
}

STATIC VOID GicSgi(UINT32 irq, UINT32 cpuMask)
{
    UINT16 tList;
    UINT32 cpu = 0;
    UINT64 val, cluster;

    while (cpuMask && (cpu < LOSCFG_KERNEL_CORE_NUM)) {
        if (cpuMask & (1U << cpu)) {
            cluster = CPU_MAP_GET(cpu) & ~0xffUL;

            tList = GicTargetList(&cpu, cpuMask, cluster);

            /* Generates a Group 1 interrupt for the current security state */
            val = ((MPIDR_AFF_LEVEL(cluster, 3) << 48) | /* 3: Serial number, 48: Register bit offset */
                   (MPIDR_AFF_LEVEL(cluster, 2) << 32) | /* 2: Serial number, 32: Register bit offset */
                   (MPIDR_AFF_LEVEL(cluster, 1) << 16) | /* 1: Serial number, 16: Register bit offset */
                   (irq << 24) | tList); /* 24: Register bit offset */

            GiccSetSgi1r(val);
        }

        cpu++;
    }
}

VOID HalIrqSendIpi(UINT32 target, UINT32 ipi)
{
    GicSgi(ipi, target);
}

VOID HalIrqSetAffinity(UINT32 irq, UINT32 cpuMask)
{
    UINT64 affinity = MpidrToAffinity(NextCpu(0, cpuMask));

    /* When ARE is on, use router */
    GIC_REG_64(GICD_IROUTER(irq)) = affinity;
}

#endif

STATIC VOID GicWaitForRwp(UINT64 reg)
{
    INT32 count = 1000000; /* 1s */

    while (GIC_REG_32(reg) & GICD_CTLR_RWP) {
        count -= 1;
        if (!count) {
            PRINTK("gic_v3: rwp timeout 0x%x\n", GIC_REG_32(reg));
            return;
        }
    }
}

STATIC INLINE VOID GicdSetGroup(UINT32 irq)
{
    /* configure spi as group 0 on secure mode and group 1 on unsecure mode */
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    GIC_REG_32(GICD_IGROUPR(irq / 32)) = 0; /* 32: Interrupt bit width */
#else
    GIC_REG_32(GICD_IGROUPR(irq / 32)) = 0xffffffff; /* 32: Interrupt bit width */
#endif
}

STATIC INLINE VOID GicrSetWaker(UINT32 cpu)
{
    GIC_REG_32(GICR_WAKER(cpu)) &= ~GICR_WAKER_PROCESSORSLEEP;
    DSB;
    ISB;
    while ((GIC_REG_32(GICR_WAKER(cpu)) & 0x4) == GICR_WAKER_CHILDRENASLEEP);
}

STATIC INLINE VOID GicrSetGroup(UINT32 cpu)
{
    /* configure sgi/ppi as group 0 on secure mode and group 1 on unsecure mode */
#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    GIC_REG_32(GICR_IGROUPR0(cpu)) = 0;
    GIC_REG_32(GICR_IGRPMOD0(cpu)) = 0;
#else
    GIC_REG_32(GICR_IGROUPR0(cpu)) = 0xffffffff;
#endif
}

STATIC VOID GicdSetPmr(UINT32 irq, UINT8 priority)
{
    UINT32 pos = irq >> 2; /* one irq have the 8-bit interrupt priority field */
    UINT32 newPri = GIC_REG_32(GICD_IPRIORITYR(pos));

    /* Shift and mask the correct bits for the priority */
    newPri &= ~(GIC_PRIORITY_MASK << ((irq % 4) * GIC_PRIORITY_OFFSET));
    newPri |= priority << ((irq % 4) * GIC_PRIORITY_OFFSET);

    GIC_REG_32(GICD_IPRIORITYR(pos)) = newPri;
}

STATIC VOID GicrSetPmr(UINT32 irq, UINT8 priority)
{
    UINT32 cpu = ArchCurrCpuid();
    UINT32 pos = irq >> 2; /* one irq have the 8-bit interrupt priority field */
    UINT32 newPri = GIC_REG_32(GICR_IPRIORITYR0(cpu) + pos * 4);

    /* Clear priority offset bits and set new priority */
    newPri &= ~(GIC_PRIORITY_MASK << ((irq % 4) * GIC_PRIORITY_OFFSET));
    newPri |= priority << ((irq % 4) * GIC_PRIORITY_OFFSET);

    GIC_REG_32(GICR_IPRIORITYR0(cpu) + pos * 4) = newPri;
}

STATIC VOID GiccInitPercpu(VOID)
{
    /* enable system register interface */
    UINT32 sre = GiccGetSre();
    if (!(sre & 0x1)) {
        GiccSetSre(sre | 0x1);

        /*
         * Need to check that the SRE bit has actually been set. If
         * not, it means that SRE is disabled at up EL level. We're going to
         * die painfully, and there is nothing we can do about it.
         */
        sre = GiccGetSre();
        LOS_ASSERT(sre & 0x1);
    }

#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
    /* Enable group 0 and disable grp1ns grp1s interrupts */
    GiccSetIgrpen0(1);
    GiccSetIgrpen1(0);

    /*
     * For priority grouping.
     * The value of this field control show the 8-bit interrupt priority field
     * is split into a group priority field, that determines interrupt preemption,
     * and a subpriority field.
     */
    GiccSetBpr0(MAX_BINARY_POINT_VALUE);
#else
    /* enable group 1 interrupts */
    GiccSetIgrpen1(1);
#endif

    /* set priority threshold to max */
    GiccSetPmr(0xff);

    /* EOI deactivates interrupt too (mode 0) */
    GiccSetCtlr(0);
}

UINT32 HalCurIrqGet(VOID)
{
    return g_curIrqNum;
}

/**
 * @brief 屏蔽指定中断向量对应的中断。
 *
 * 该函数根据传入的中断向量号，判断中断类型（SGI/PPI 或 SPI），
 * 并对相应的中断进行屏蔽操作。对于 SGI/PPI 中断，会在所有 CPU 上进行屏蔽；
 * 对于 SPI 中断，会在 GIC 分发器上进行屏蔽。
 *
 * @param vector 要屏蔽的中断向量号。
 */
VOID HalIrqMask(UINT32 vector)
{
    // 循环计数器，用于遍历所有 CPU
    INT32 i;
    // 计算用于屏蔽中断的掩码，将 1 左移 vector 对 32 取余的位数
    const UINT32 mask = 1U << (vector % 32); /* 32: Interrupt bit width */

    // 检查中断向量号是否在用户可操作的中断范围之外
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        // 若不在范围内，则不进行屏蔽操作，直接返回
        return;
    }

    // 判断中断类型，若中断向量号小于 32，则为 SGI/PPI 中断
    if (vector < 32) { /* 32: Interrupt bit width */
        // 遍历所有 CPU
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
            // 对当前 CPU 的 GICR（GIC Redistributor）的 ICENABLER0 寄存器写入掩码，屏蔽相应中断
            GIC_REG_32(GICR_ICENABLER0(i)) = mask;
            // 等待 GICR 寄存器写操作完成，确保屏蔽操作生效
            GicWaitForRwp(GICR_CTLR(i));
        }
    } else {
        // 若中断向量号大于等于 32，则为 SPI 中断
        // 对 GICD（GIC Distributor）的 ICENABLER 寄存器写入掩码，屏蔽相应中断
        // vector >> 5 等同于 vector / 32，用于计算寄存器偏移
        GIC_REG_32(GICD_ICENABLER(vector >> 5)) = mask;
        // 等待 GICD 寄存器写操作完成，确保屏蔽操作生效
        GicWaitForRwp(GICD_CTLR);
    }
}

/**
 * @brief 取消屏蔽指定中断向量对应的中断。
 *
 * 该函数根据传入的中断向量号，判断中断类型（SGI/PPI 或 SPI），
 * 并对相应的中断进行取消屏蔽操作。对于 SGI/PPI 中断，会在所有 CPU 上进行取消屏蔽；
 * 对于 SPI 中断，会在 GIC 分发器上进行取消屏蔽。
 *
 * @param vector 要取消屏蔽的中断向量号。
 */
VOID HalIrqUnmask(UINT32 vector)
{
    // 循环计数器，用于遍历所有 CPU
    INT32 i;
    // 计算用于取消屏蔽中断的掩码，将 1 左移 vector 对 32 取余的位数
    const UINT32 mask = 1U << (vector % 32); /* 32: Interrupt bit width */

    // 检查中断向量号是否在用户可操作的中断范围之外
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        // 若不在范围内，则不进行取消屏蔽操作，直接返回
        return;
    }

    // 判断中断类型，若中断向量号小于 32，则为 SGI/PPI 中断
    if (vector < 32) { /* 32: Interrupt bit width */
        // 遍历所有 CPU
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
            // 对当前 CPU 的 GICR（GIC Redistributor）的 ISENABLER0 寄存器写入掩码，取消屏蔽相应中断
            GIC_REG_32(GICR_ISENABLER0(i)) = mask;
            // 等待 GICR 寄存器写操作完成，确保取消屏蔽操作生效
            GicWaitForRwp(GICR_CTLR(i));
        }
    } else {
        // 若中断向量号大于等于 32，则为 SPI 中断
        // 对 GICD（GIC Distributor）的 ISENABLER 寄存器写入掩码，取消屏蔽相应中断
        // vector >> 5 等同于 vector / 32，用于计算寄存器偏移
        GIC_REG_32(GICD_ISENABLER(vector >> 5)) = mask; /* 5: Register bit offset */
        // 等待 GICD 寄存器写操作完成，确保取消屏蔽操作生效
        GicWaitForRwp(GICD_CTLR);
    }
}

/**
 * @brief 将指定中断向量对应的中断置为挂起状态。
 *
 * 该函数会检查传入的中断向量号是否在用户可操作的中断范围之内，
 * 若在范围内，则将对应的中断置为挂起状态。挂起状态表示该中断已发生，
 * 等待 GIC 进行处理。
 *
 * @param vector 要置为挂起状态的中断向量号。
 */
VOID HalIrqPending(UINT32 vector)
{
    // 检查中断向量号是否在用户可操作的中断范围之外
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        // 若不在范围内，则不进行置挂起操作，直接返回
        return;
    }

    // 将指定中断向量对应的中断置为挂起状态
    // vector >> 5 等同于 vector / 32，用于计算 GICD_ISPENDR 寄存器的偏移
    // 5 表示寄存器的位偏移，32 表示每个寄存器管理 32 个中断
    // 1U << (vector % 32) 生成用于置位的掩码，将对应中断位置 1
    GIC_REG_32(GICD_ISPENDR(vector >> 5)) = 1U << (vector % 32); /* 5: Register bit offset, 32: Interrupt bit width */
}

VOID HalIrqClear(UINT32 vector)
{
    GiccSetEoir(vector);
    ISB;
}

UINT32 HalIrqSetPrio(UINT32 vector, UINT8 priority)
{
    UINT8 prio = priority;

    if (vector > OS_HWI_MAX_NUM) {
        PRINT_ERR("Invalid irq value %u, max irq is %u\n", vector, OS_HWI_MAX_NUM);
        return LOS_NOK;
    }

    prio = prio & (UINT8)GIC_INTR_PRIO_MASK;

    if (vector >= GIC_MIN_SPI_NUM) {
        GicdSetPmr(vector, prio);
    } else {
        GicrSetPmr(vector, prio);
    }

    return LOS_OK;
}

/**
 * @brief 初始化每个 CPU 的 GIC（Generic Interrupt Controller）相关设置。
 *
 * 该函数对每个 CPU 的 GIC  Redistributor（GICR）和 CPU Interface（GICC）进行初始化操作，
 * 包括设置唤醒状态、中断分组、清除和屏蔽中断、设置中断优先级，以及在 SMP 系统中取消屏蔽特定中断。
 */
VOID HalIrqInitPercpu(VOID)
{
    // 循环计数器，用于遍历中断号
    INT32 idx;
    // 获取当前 CPU 的 ID
    UINT32 cpu = ArchCurrCpuid();

    /* GICR init */
    // 设置 GICR 的唤醒状态，确保 GICR 处于唤醒状态
    GicrSetWaker(cpu);
    // 配置 GICR 的中断分组，将 SGI/PPI 中断分组
    GicrSetGroup(cpu);
    // 等待 GICR 寄存器写操作完成，确保配置生效
    GicWaitForRwp(GICR_CTLR(cpu));

    /* GICR: clear and mask sgi/ppi */
    // 屏蔽所有 SGI（Software Generated Interrupts）和 PPI（Private Peripheral Interrupts）中断
    GIC_REG_32(GICR_ICENABLER0(cpu)) = 0xffffffff;
    // 清除所有 SGI 和 PPI 中断的挂起状态
    GIC_REG_32(GICR_ICPENDR0(cpu)) = 0xffffffff;

    // 使能所有 SGI 和 PPI 中断
    GIC_REG_32(GICR_ISENABLER0(cpu)) = 0xffffffff;

    // 遍历所有 SGI 和 PPI 中断，为其设置默认优先级
    for (idx = 0; idx < GIC_MIN_SPI_NUM; idx += 1) {
        // 调用 GicrSetPmr 函数设置中断优先级为最低默认优先级
        GicrSetPmr(idx, MIN_INTERRUPT_PRIORITY);
    }

    // 等待 GICR 寄存器写操作完成，确保优先级设置生效
    GicWaitForRwp(GICR_CTLR(cpu));

    /* GICC init */
    // 初始化 GIC 的 CPU 接口
    GiccInitPercpu();

#ifdef LOSCFG_KERNEL_SMP
    /* unmask ipi interrupts */
    // 取消屏蔽处理器间唤醒中断
    HalIrqUnmask(LOS_MP_IPI_WAKEUP);
    // 取消屏蔽处理器间停止中断
    HalIrqUnmask(LOS_MP_IPI_HALT);
#endif
}

/**
 * @brief 初始化 GIC（Generic Interrupt Controller）中断控制器。
 *
 * 该函数完成 GIC 分发器和中断的一系列初始化操作，包括禁用分发器、设置中断触发方式、
 * 配置中断分组、设置中断优先级、启用分发器以及注册处理器间中断等。
 */
VOID HalIrqInit(VOID)
{
    UINT32 i;
    UINT64 affinity;

    /* disable distributor */
    // 禁用 GIC 分发器，停止分发中断
    GIC_REG_32(GICD_CTLR) = 0;
    // 等待分发器寄存器写操作完成
    GicWaitForRwp(GICD_CTLR);
    // 确保指令同步屏障，保证之前的写操作完成后再执行后续指令
    ISB;

    /* set external interrupts to be level triggered, active low. */
    // 从第 32 个中断开始，以 16 个中断为一组，设置外部中断为低电平触发
    for (i = 32; i < OS_HWI_MAX_NUM; i += 16) { /* 32: Start interrupt number, 16: Interrupt bit width */
        // 配置中断触发方式寄存器，将对应中断设置为低电平触发
        GIC_REG_32(GICD_ICFGR(i / 16)) = 0;
    }

    /* config distributer, mask and clear all spis, set group x */
    // 从第 32 个中断开始，以 32 个中断为一组，配置分发器
    for (i = 32; i < OS_HWI_MAX_NUM; i += 32) { /* 32: Start interrupt number, 32: Interrupt bit width */
        // 屏蔽所有 SPI（Shared Peripheral Interrupts）中断
        GIC_REG_32(GICD_ICENABLER(i / 32)) = 0xffffffff; /* 32: Interrupt bit width */
        // 清除所有 SPI 中断的挂起状态
        GIC_REG_32(GICD_ICPENDR(i / 32)) = 0xffffffff; /* 32: Interrupt bit width */
        // 设置中断组模式寄存器
        GIC_REG_32(GICD_IGRPMODR(i / 32)) = 0; /* 32: Interrupt bit width */

        // 设置中断分组
        GicdSetGroup(i);
    }

    /* set spi priority as default */
    // 从第 32 个中断开始，为所有 SPI 中断设置默认优先级
    for (i = 32; i < OS_HWI_MAX_NUM; i++) { /* 32: Start interrupt number */
        // 调用 GicdSetPmr 函数设置中断优先级为最低默认优先级
        GicdSetPmr(i, MIN_INTERRUPT_PRIORITY);
    }

    // 等待分发器寄存器写操作完成
    GicWaitForRwp(GICD_CTLR);

    /* disable all interrupts. */
    // 以 32 个中断为一组，禁用所有中断
    for (i = 0; i < OS_HWI_MAX_NUM; i += 32) { /* 32: Interrupt bit width */
        // 屏蔽对应中断
        GIC_REG_32(GICD_ICENABLER(i / 32)) = 0xffffffff; /* 32: Interrupt bit width */
    }

    /* enable distributor with ARE, group 1 enabled */
    // 启用 GIC 分发器，同时启用地址路由扩展（ARE）和组 1 非安全中断
    GIC_REG_32(GICD_CTLR) = CTLR_ENALBE_G0 | CTLR_ENABLE_G1NS | CTLR_ARE_S;

    /* set spi to boot cpu only. ARE must be enabled */
    // 将 MPIDR_EL1 寄存器的值转换为中断亲和性值
    affinity = MpidrToAffinity(AARCH64_SYSREG_READ(mpidr_el1));
    // 从第 32 个中断开始，将所有 SPI 中断路由到启动 CPU
    for (i = 32; i < OS_HWI_MAX_NUM; i++) { /* 32: Start interrupt number */
        // 设置中断路由寄存器，指定中断亲和性
        GIC_REG_64(GICD_IROUTER(i)) = affinity;
    }

    // 初始化每个 CPU 的 GIC 相关设置
    HalIrqInitPercpu();

#ifdef LOSCFG_KERNEL_SMP
    /* register inter-processor interrupt */
    // 创建并注册处理器间唤醒中断
    (VOID)LOS_HwiCreate(LOS_MP_IPI_WAKEUP, 0xa0, 0, OsMpWakeHandler, 0);
    // 创建并注册处理器间调度中断
    (VOID)LOS_HwiCreate(LOS_MP_IPI_SCHEDULE, 0xa0, 0, OsMpScheduleHandler, 0);
    // 创建并注册处理器间停止中断
    (VOID)LOS_HwiCreate(LOS_MP_IPI_HALT, 0xa0, 0, OsMpScheduleHandler, 0);
#ifdef LOSCFG_KERNEL_SMP_CALL
    // 创建并注册处理器间函数调用中断
    (VOID)LOS_HwiCreate(LOS_MP_IPI_FUNC_CALL, 0xa0, 0, OsMpFuncCallHandler, 0);
#endif
#endif
}

/**
 * @brief 处理 GIC 中断的主函数。
 *
 * 该函数从 GIC CPU 接口寄存器中读取中断确认寄存器（IAR）的值，
 * 提取出中断向量号，然后对该中断进行有效性检查。
 * 如果中断向量号有效，则调用操作系统的中断处理函数，
 * 最后向 GIC 发送中断结束信号（EOI）。
 */
VOID HalIrqHandler(VOID)
{
    // 从 GIC CPU 接口寄存器中读取中断确认寄存器（IAR）的值
    // 该寄存器包含了当前正在处理的中断的相关信息
    UINT32 iar = GiccGetIar();
    // 从 IAR 中提取出中断向量号，通过与 0x3FFU 进行按位与操作
    // 0x3FFU 对应的二进制为 1111111111，可提取出低 10 位的中断向量号
    UINT32 vector = iar & 0x3FFU;

    /*
     * 检查中断向量号是否无效，主要是处理伪中断（spurious interrupts）0x3FF
     * 有效的中断向量号范围是 0 到 1019，这里使用 OS_HWI_MAX_NUM 进行检查
     * 如果中断向量号超出有效范围，则不处理该中断，直接返回
     */
    if (vector >= OS_HWI_MAX_NUM) {
        return;
    }
    // 将当前有效的中断向量号保存到全局变量 g_curIrqNum 中
    g_curIrqNum = vector;

    // 调用操作系统的中断处理函数，传入有效的中断向量号
    // 该函数会根据中断向量号执行相应的中断处理逻辑
    OsInterrupt(vector);
    // 向 GIC 发送中断结束信号（EOI），通知 GIC 当前中断处理完毕
    // 以便 GIC 可以继续处理其他中断
    GiccSetEoir(vector);
}

/**
 * @brief 获取 GIC（Generic Interrupt Controller）的版本信息。
 *
 * 该函数通过读取 GIC 分发器的 PIDR2V3 寄存器值，根据寄存器中的版本信息，
 * 返回对应的 GIC 版本字符串。如果版本信息未知，则返回 "unknown"。
 *
 * @return CHAR* 指向表示 GIC 版本信息的字符串指针。
 */
CHAR *HalIrqVersion(VOID)
{
    // 读取 GIC 分发器的 PIDR2V3 寄存器值
    UINT32 pidr = GIC_REG_32(GICD_PIDR2V3);
    // 用于存储表示 GIC 版本信息的字符串指针，初始化为 NULL
    CHAR *irqVerString = NULL;

    // 根据 PIDR2V3 寄存器值右移 GIC_REV_OFFSET 位后的结果判断 GIC 版本
    switch (pidr >> GIC_REV_OFFSET) {
        // 如果版本为 GICv3
        case GICV3:
            // 将版本字符串设置为 "GICv3"
            irqVerString = "GICv3";
            break;
        // 如果版本为 GICv4
        case GICV4:
            // 将版本字符串设置为 "GICv4"
            irqVerString = "GICv4";
            break;
        // 如果版本信息未知
        default:
            // 将版本字符串设置为 "unknown"
            irqVerString = "unknown";
    }
    // 返回表示 GIC 版本信息的字符串指针
    return irqVerString;
}

#endif
