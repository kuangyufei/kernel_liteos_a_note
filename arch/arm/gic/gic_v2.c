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
#include "los_hwi.h"
#include "los_hwi_pri.h"
#include "los_mp.h"

STATIC_ASSERT(OS_USER_HWI_MAX <= 1020, "hwi max is too large!");

#ifdef LOSCFG_ARCH_GIC_V2

STATIC UINT32 g_curIrqNum = 0;

#ifdef LOSCFG_KERNEL_SMP
/*
 * filter description
 *   0b00: forward to the cpu interfaces specified in cpu_mask
 *   0b01: forward to all cpu interfaces
 *   0b10: forward only to the cpu interface that request the irq
 */
STATIC VOID GicWriteSgi(UINT32 vector, UINT32 cpuMask, UINT32 filter)
{
    UINT32 val = ((filter & 0x3) << 24) | ((cpuMask & 0xFF) << 16) | /* 24, 16: Register bit offset */
                 (vector & 0xF);

    GIC_REG_32(GICD_SGIR) = val;
}

VOID HalIrqSendIpi(UINT32 target, UINT32 ipi)
{
    GicWriteSgi(ipi, target, 0);
}

VOID HalIrqSetAffinity(UINT32 vector, UINT32 cpuMask)
{
    UINT32 offset = vector / 4; /* 4: Interrupt bit width */
    UINT32 index = vector & 0x3;

    GIC_REG_8(GICD_ITARGETSR(offset) + index) = cpuMask;
}
#endif
/// 获取当前中断号
UINT32 HalCurIrqGet(VOID)
{
    return g_curIrqNum;
}
/// 屏蔽中断
VOID HalIrqMask(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return;
    }

    GIC_REG_32(GICD_ICENABLER(vector / 32)) = 1U << (vector % 32); /* 32: Interrupt bit width */
}
/// 撤销中断屏蔽
VOID HalIrqUnmask(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return;
    }

    GIC_REG_32(GICD_ISENABLER(vector >> 5)) = 1U << (vector % 32); /* 5, 32: Register bit offset */
}

VOID HalIrqPending(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return;
    }

    GIC_REG_32(GICD_ISPENDR(vector >> 5)) = 1U << (vector % 32); /* 5, 32: Register bit offset */
}

VOID HalIrqClear(UINT32 vector)
{
    GIC_REG_32(GICC_EOIR) = vector;
}
/// 中断控制器与CPU之间的关系初始化
VOID HalIrqInitPercpu(VOID)
{
    /* unmask interrupts */
    GIC_REG_32(GICC_PMR) = 0xFF;//写中断优先级屏蔽寄存器,0xFF代表开放所有中断

    /* enable gic cpu interface */
    GIC_REG_32(GICC_CTLR) = 1;//使能GICC_CTLR寄存器,意思是打通控制器和CPU之间的链路
}
/// 中断控制器本身初始化
VOID HalIrqInit(VOID)
{
    UINT32 i;

    /* set externel interrupts to be level triggered, active low. | 设置外部中断为电平触发，低电平有效。*/
    for (i = 32; i < OS_HWI_MAX_NUM; i += 16) { /* 32: Start interrupt number, 16: Interrupt bit width */
        GIC_REG_32(GICD_ICFGR(i / 16)) = 0; /* 16: Register bit offset */
    }

    /* set externel interrupts to CPU 0 | 将外部中断设置为 CPU 0*/
    for (i = 32; i < OS_HWI_MAX_NUM; i += 4) { /* 32: Start interrupt number, 4: Interrupt bit width */
        GIC_REG_32(GICD_ITARGETSR(i / 4)) = 0x01010101;
    }

    /* set priority on all interrupts | 设置所有中断的优先级*/
    for (i = 0; i < OS_HWI_MAX_NUM; i += 4) { /* 4: Interrupt bit width */
        GIC_REG_32(GICD_IPRIORITYR(i / 4)) = GICD_INT_DEF_PRI_X4;
    }

    /* disable all interrupts. | 禁止所有中断*/
    for (i = 0; i < OS_HWI_MAX_NUM; i += 32) { /* 32: Interrupt bit width */
        GIC_REG_32(GICD_ICENABLER(i / 32)) = ~0; /* 32: Interrupt bit width */
    }

    HalIrqInitPercpu();//启动和CPU之间关系

    /* enable gic distributor control */
    GIC_REG_32(GICD_CTLR) = 1;//使能中断分发寄存器

#ifdef LOSCFG_KERNEL_SMP //多核情况下会出现CPU核间的通讯
    /* register inter-processor interrupt | 注册核间中断*/
    (VOID)LOS_HwiCreate(LOS_MP_IPI_WAKEUP, 0xa0, 0, OsMpWakeHandler, 0);//由某CPU去唤醒其他CPU继续工作
    (VOID)LOS_HwiCreate(LOS_MP_IPI_SCHEDULE, 0xa0, 0, OsMpScheduleHandler, 0);//由某CPU发起对其他CPU的调度
    (VOID)LOS_HwiCreate(LOS_MP_IPI_HALT, 0xa0, 0, OsMpHaltHandler, 0);//由某CPU去暂停其他CPU的工作
#ifdef LOSCFG_KERNEL_SMP_CALL
    (VOID)LOS_HwiCreate(LOS_MP_IPI_FUNC_CALL, 0xa0, 0, OsMpFuncCallHandler, 0);//触发回调函数
#endif
#endif
}

VOID HalIrqHandler(VOID)
{
    UINT32 iar = GIC_REG_32(GICC_IAR);
    UINT32 vector = iar & 0x3FFU;

    /*
     * invalid irq number, mainly the spurious interrupts 0x3ff,
     * gicv2 valid irq ranges from 0~1019, we use OS_HWI_MAX_NUM
     * to do the checking.
     */
    if (vector >= OS_HWI_MAX_NUM) {
        return;
    }
    g_curIrqNum = vector;

    OsInterrupt(vector);

    /* use original iar to do the EOI */
    GIC_REG_32(GICC_EOIR) = iar;
}

CHAR *HalIrqVersion(VOID)
{
    UINT32 pidr = GIC_REG_32(GICD_PIDR2V2);
    CHAR *irqVerString = NULL;

    switch (pidr >> GIC_REV_OFFSET) {
        case GICV1:
            irqVerString = "GICv1";
            break;
        case GICV2:
            irqVerString = "GICv2";
            break;
        default:
            irqVerString = "unknown";
    }
    return irqVerString;
}

#endif
