/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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
#include "los_hwi_pri.h"
#include "los_mp.h"
/******************************************************************************
GIC，Generic Interrupt Controller。是ARM公司提供的一个通用的中断控制器。主要作用为：
	接受硬件中断信号，并经过一定处理后，分发给对应的CPU进行处理。

SPI (Shared Peripheral Interrupt)
	公用的外部设备中断，也定义为共享中断。可以多个Cpu或者说Core处理，不限定特定的Cpu。
	比如按键触发一个中断，手机触摸屏触发的中断。
PPI (Private Peripheral Interrupt)
	私有外设中断。这是每个核心私有的中断。PPI会送达到指定的CPU上，应用场景有CPU本地时钟。
SGI (Software Generated Interrupt)
	软件触发的中断。软件可以通过写GICD_SGIR寄存器来触发一个中断事件，一般用于核间通信。
LPI (Locality-specific Peripheral Interrupt)
	LPI是GICv3中的新特性，它们在很多方面与其他类型的中断不同。LPI始终是基于消息的中断，
	它们的配置保存在表中而不是寄存器。比如PCIe的MSI/MSI-x中断。

	硬件中断号	中断类型
	0-15		SGI
	16 - 31		PPI
	32 - 1019	SPI
	1020 - 1023	用于指示特殊情况的特殊中断
	1024 - 8191	Reservd
	8192 - MAX	LPI
	
GICv3控制器由以下部分组成:
	distributor： SPI中断的管理，将中断发送给redistributor
	redistributor： PPI，SGI，LPI中断的管理，将中断发送给cpu interface
	cpu interface： 传输中断给core
	ITS： Interrupt Translation Service, 用来解析LPI中断
	其中，cpu interface是实现在core内部的，distributor，redistributor，ITS是实现在gic内部的.
	
四种中断状态
	Inactive	中断即没有Pending也没有Active
	Pending	由于外设硬件产生了中断事件（或者软件触发）该中断事件已经通过
		硬件信号通知到GIC，等待GIC分配的那个CPU进行处理
	Active	CPU已经应答（acknowledge）了该interrupt请求，并且正在处理中
	Active and Pending	当一个中断源处于Active状态的时候，同一中断源又触发了中断，
		进入pending状态

参考
	https://blog.csdn.net/yhb1047818384/article/details/86708769
******************************************************************************/

#ifdef LOSCFG_PLATFORM_BSP_GIC_V3

STATIC UINT32 g_curIrqNum = 0; //记录当前中断号

STATIC INLINE UINT64 MpidrToAffinity(UINT64 mpidr)
{
    return ((MPIDR_AFF_LEVEL(mpidr, 3) << 32) |
            (MPIDR_AFF_LEVEL(mpidr, 2) << 16) |
            (MPIDR_AFF_LEVEL(mpidr, 1) << 8)  |
            (MPIDR_AFF_LEVEL(mpidr, 0)));
}

#if (LOSCFG_KERNEL_SMP == YES)
//获取cpuMask中下一个CPU 
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
//软件触发的中断,软件可以通过写GICD_SGIR寄存器来触发一个中断事件，一般用于核间通信。
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
            val = ((MPIDR_AFF_LEVEL(cluster, 3) << 48) |
                   (MPIDR_AFF_LEVEL(cluster, 2) << 32) |
                   (MPIDR_AFF_LEVEL(cluster, 1) << 16) |
                   (irq << 24) | tList);

            GiccSetSgi1r(val);
        }

        cpu++;
    }
}
//Inter-Processor Interrupts,IPI ,向目标CPU发送核间中断
VOID HalIrqSendIpi(UINT32 target, UINT32 ipi)
{
    GicSgi(ipi, target);
}
//给指定中断号设置CPU亲和性
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
    GIC_REG_32(GICD_IGROUPR(irq / 32)) = 0;
#else
    GIC_REG_32(GICD_IGROUPR(irq / 32)) = 0xffffffff;
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
//屏蔽所有CPU的硬件中断
VOID HalIrqMask(UINT32 vector)
{
    INT32 i;
    const UINT32 mask = 1U << (vector % 32);//通过参数获取掩码

    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return;
    }

    if (vector < 32) {
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {//
            GIC_REG_32(GICR_ICENABLER0(i)) = mask;
            GicWaitForRwp(GICR_CTLR(i));
        }
    } else {
        GIC_REG_32(GICD_ICENABLER(vector >> 5)) = mask;
        GicWaitForRwp(GICD_CTLR);
    }
}
//取消屏蔽指定硬件中断向量号,vector范围[0,127]
VOID HalIrqUnmask(UINT32 vector)
{
    INT32 i;
    const UINT32 mask = 1U << (vector % 32);
	
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return;
    }

    if (vector < 32) {
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
            GIC_REG_32(GICR_ISENABLER0(i)) = mask;
            GicWaitForRwp(GICR_CTLR(i));
        }
    } else {
        GIC_REG_32(GICD_ISENABLER(vector >> 5)) = mask;
        GicWaitForRwp(GICD_CTLR);
    }
}

VOID HalIrqPending(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return;
    }

    GIC_REG_32(GICD_ISPENDR(vector >> 5)) = 1U << (vector % 32);
}
//清除中断号对应的中断寄存器的状态位，此接口依赖中断控制器版本，非必需
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
//当前CPU初始化硬中断
VOID HalIrqInitPercpu(VOID)
{
    INT32 idx;
    UINT32 cpu = ArchCurrCpuid();//获取当前CPU

    /* GICR init */
    GicrSetWaker(cpu);//设置CPU为唤醒状态
    GicrSetGroup(cpu);
    GicWaitForRwp(GICR_CTLR(cpu));

    /* GICR: clear and mask sgi/ppi */
    GIC_REG_32(GICR_ICENABLER0(cpu)) = 0xffffffff;
    GIC_REG_32(GICR_ICPENDR0(cpu)) = 0xffffffff;

    GIC_REG_32(GICR_ISENABLER0(cpu)) = 0xffffffff;

    for (idx = 0; idx < GIC_MIN_SPI_NUM; idx += 1) {
        GicrSetPmr(idx, MIN_INTERRUPT_PRIORITY);
    }

    GicWaitForRwp(GICR_CTLR(cpu));

    /* GICC init */
    GiccInitPercpu();

#ifdef LOSCFG_KERNEL_SMP
    /* unmask ipi interrupts */
    HalIrqUnmask(LOS_MP_IPI_WAKEUP);//恢复CPU唤醒信号
    HalIrqUnmask(LOS_MP_IPI_HALT);//恢复CPU停止信号
#endif
}
//硬中断初始化
VOID HalIrqInit(VOID)
{
    UINT32 i;
    UINT64 affinity;

    /* disable distributor */ 
    GIC_REG_32(GICD_CTLR) = 0;	//禁止仲裁寄存器工作
    GicWaitForRwp(GICD_CTLR);
    ISB;

    /* set externel interrupts to be level triggered, active low. */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 16) {//将外部中断设置为电平触发，低电平有效
        GIC_REG_32(GICD_ICFGR(i / 16)) = 0;//设置16个私有外设中断
    }
	//SPI是串行外设接口(Serial Peripheral Interface)的缩写
    /* config distributer, mask and clear all spis, set group x */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 32) {//配置分配器，屏蔽并清除所有SPI,设置组
        GIC_REG_32(GICD_ICENABLER(i / 32)) = 0xffffffff;//屏蔽中断使能寄存器
        GIC_REG_32(GICD_ICPENDR(i / 32)) = 0xffffffff;////屏蔽中断挂起寄存器
        GIC_REG_32(GICD_IGRPMODR(i / 32)) = 0;//清除中断组模式寄存器

        GicdSetGroup(i);//设置中断组
    }

    /* set spi priority as default */
    for (i = 32; i < OS_HWI_MAX_NUM; i++) {
        GicdSetPmr(i, MIN_INTERRUPT_PRIORITY);
    }

    GicWaitForRwp(GICD_CTLR);

    /* disable all interrupts. */
    for (i = 0; i < OS_HWI_MAX_NUM; i += 32) {//让所有中断失效
        GIC_REG_32(GICD_ICENABLER(i / 32)) = 0xffffffff;
    }

    /* enable distributor with ARE, group 1 enabled */
    GIC_REG_32(GICD_CTLR) = CTLR_ENALBE_G0 | CTLR_ENABLE_G1NS | CTLR_ARE_S;

    /* set spi to boot cpu only. ARE must be enabled */
    affinity = MpidrToAffinity(AARCH64_SYSREG_READ(mpidr_el1));
    for (i = 32; i < OS_HWI_MAX_NUM; i++) {
        GIC_REG_64(GICD_IROUTER(i)) = affinity;
    }

    HalIrqInitPercpu();//每个CPU初始化硬中断

#if (LOSCFG_KERNEL_SMP == YES)
    /* register inter-processor interrupt *///注册寄存器处理器间中断处理函数,啥意思?就是当前CPU核向其他CPU核发送中断信号
    //处理器间中断允许一个CPU向系统其他的CPU发送中断信号，处理器间中断（IPI）不是通过IRQ线传输的，而是作为信号直接放在连接所有CPU本地APIC的总线上。
    LOS_HwiCreate(LOS_MP_IPI_WAKEUP, 0xa0, 0, OsMpWakeHandler, 0);		//创建硬中断,用于唤醒处理函数
    LOS_HwiCreate(LOS_MP_IPI_SCHEDULE, 0xa0, 0, OsMpScheduleHandler, 0);//创建硬中断,用于调度处理函数
    LOS_HwiCreate(LOS_MP_IPI_HALT, 0xa0, 0, OsMpScheduleHandler, 0);	//创建硬中断,用于暂停处理函数
#endif
}
//硬中断处理函数，这里由硬件触发,调用见于 ..\arch\arm\arm\src\los_dispatch.S
VOID HalIrqHandler(VOID)
{
    UINT32 iar = GiccGetIar();//获取中断号
    UINT32 vector = iar & 0x3FFU;

    /*
     * invalid irq number, mainly the spurious interrupts 0x3ff,
     * valid irq ranges from 0~1019, we use OS_HWI_MAX_NUM to do
     * the checking.
     */ //无效的中断号，主要是伪中断0x3ff，有效的中断号在0~1019之间,使用OS_HWI_MAX_NUM来检查
    if (vector >= OS_HWI_MAX_NUM) {//[0,127]
        return;
    }
    g_curIrqNum = vector;//记录当前中断号

    OsInterrupt(vector);//处理中断
    GiccSetEoir(vector);
}
//获取中断控制器版本
CHAR *HalIrqVersion(VOID)
{
    UINT32 pidr = GIC_REG_32(GICD_PIDR2V3);
    CHAR *irqVerString = NULL;

    switch (pidr >> GIC_REV_OFFSET) {
        case GICV3:
            irqVerString = "GICv3";
            break;
        case GICV4:
            irqVerString = "GICv4";
            break;
        default:
            irqVerString = "unknown";
    }
    return irqVerString;
}

#endif
