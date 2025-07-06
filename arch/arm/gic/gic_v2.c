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

STATIC_ASSERT(OS_USER_HWI_MAX <= 1020, "hwi max is too large!");  // 静态断言：确保用户中断号最大值不超过1020

#ifdef LOSCFG_ARCH_GIC_V2  // 如果配置了GICv2中断控制器

STATIC UINT32 g_curIrqNum = 0;  // 当前正在处理的中断号

#ifdef LOSCFG_KERNEL_SMP  // 如果支持SMP（对称多处理）
/*
 * 过滤器描述
 *   0b00: 转发到cpu_mask中指定的CPU接口
 *   0b01: 转发到所有CPU接口
 *   0b10: 仅转发到请求中断的CPU接口
 */
STATIC VOID GicWriteSgi(UINT32 vector, UINT32 cpuMask, UINT32 filter)
{
    UINT32 val = ((filter & 0x3) << 24) | ((cpuMask & 0xFF) << 16) | /* 24, 16: 寄存器位偏移 */  // 构造SGI控制值：过滤器(2bit) + CPU掩码(8bit) + 向量号(4bit)
                 (vector & 0xF);

    GIC_REG_32(GICD_SGIR) = val;  // 写入GICD_SGIR寄存器发送SGI中断
}

VOID HalIrqSendIpi(UINT32 target, UINT32 ipi)
{
    GicWriteSgi(ipi, target, 0);  // 调用GicWriteSgi发送IPI中断，使用过滤器0b00
}

VOID HalIrqSetAffinity(UINT32 vector, UINT32 cpuMask)
{
    UINT32 offset = vector / 4; /* 4: 中断位宽 */  // 计算ITARGETSR寄存器偏移量（每个寄存器控制4个中断）
    UINT32 index = vector & 0x3;  // 计算中断在寄存器内的索引（0-3）

    GIC_REG_8(GICD_ITARGETSR(offset) + index) = cpuMask;  // 设置中断的CPU亲和性掩码
}
#endif

UINT32 HalCurIrqGet(VOID)
{
    return g_curIrqNum;  // 返回当前正在处理的中断号
}

VOID HalIrqMask(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {  // 检查中断号是否在有效范围内
        return;  // 无效中断号，直接返回
    }

    GIC_REG_32(GICD_ICENABLER(vector / 32)) = 1U << (vector % 32); /* 32: 中断位宽 */  // 清除使能寄存器中对应中断位（禁用中断）
}

VOID HalIrqUnmask(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {  // 检查中断号是否在有效范围内
        return;  // 无效中断号，直接返回
    }

    GIC_REG_32(GICD_ISENABLER(vector >> 5)) = 1U << (vector % 32); /* 5, 32: 寄存器位偏移 */  // 设置使能寄存器中对应中断位（启用中断）
}

VOID HalIrqPending(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {  // 检查中断号是否在有效范围内
        return;  // 无效中断号，直接返回
    }

    GIC_REG_32(GICD_ISPENDR(vector >> 5)) = 1U << (vector % 32); /* 5, 32: 寄存器位偏移 */  // 设置挂起寄存器中对应中断位（触发中断）
}

VOID HalIrqClear(UINT32 vector)
{
    GIC_REG_32(GICC_EOIR) = vector;  // 写入EOIR寄存器结束中断处理
}

VOID HalIrqInitPercpu(VOID)
{
    /* 解除中断屏蔽 */
    GIC_REG_32(GICC_PMR) = 0xFF;  // 设置优先级掩码为0xFF（允许所有优先级中断）

    /* 启用GIC CPU接口 */
    GIC_REG_32(GICC_CTLR) = 1;  // 启用CPU接口控制寄存器
}

VOID HalIrqInit(VOID)
{
    UINT32 i;

    /* 将外部中断设置为电平触发，低电平有效 */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 16) { /* 32: 起始中断号，16: 中断位宽 */  // 遍历外部中断配置寄存器
        GIC_REG_32(GICD_ICFGR(i / 16)) = 0; /* 16: 寄存器位偏移 */  // 设置为电平触发模式
    }

    /* 将外部中断路由到CPU 0 */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 4) { /* 32: 起始中断号，4: 中断位宽 */  // 遍历目标CPU寄存器
        GIC_REG_32(GICD_ITARGETSR(i / 4)) = 0x01010101;  // 设置所有4个中断的目标CPU为CPU 0
    }

    /* 设置所有中断的优先级 */
    for (i = 0; i < OS_HWI_MAX_NUM; i += 4) { /* 4: 中断位宽 */  // 遍历优先级寄存器
        GIC_REG_32(GICD_IPRIORITYR(i / 4)) = GICD_INT_DEF_PRI_X4;  // 设置4个中断的默认优先级
    }

    /* 禁用所有中断 */
    for (i = 0; i < OS_HWI_MAX_NUM; i += 32) { /* 32: 中断位宽 */  // 遍历禁用寄存器
        GIC_REG_32(GICD_ICENABLER(i / 32)) = ~0; /* 32: 中断位宽 */  // 清除所有中断使能位
    }

    HalIrqInitPercpu();  // 初始化每CPU中断设置

    /* 启用GIC分发器控制 */
    GIC_REG_32(GICD_CTLR) = 1;  // 启用分发器控制寄存器

#ifdef LOSCFG_KERNEL_SMP  // 如果支持SMP
    /* 注册处理器间中断 */
    (VOID)LOS_HwiCreate(LOS_MP_IPI_WAKEUP, 0xa0, 0, OsMpWakeHandler, 0);  // 创建唤醒IPI中断处理
    (VOID)LOS_HwiCreate(LOS_MP_IPI_SCHEDULE, 0xa0, 0, OsMpScheduleHandler, 0);  // 创建调度IPI中断处理
    (VOID)LOS_HwiCreate(LOS_MP_IPI_HALT, 0xa0, 0, OsMpHaltHandler, 0);  // 创建停机IPI中断处理
#ifdef LOSCFG_KERNEL_SMP_CALL  // 如果支持SMP函数调用
    (VOID)LOS_HwiCreate(LOS_MP_IPI_FUNC_CALL, 0xa0, 0, OsMpFuncCallHandler, 0);  // 创建函数调用IPI中断处理
#endif
#endif
}

VOID HalIrqHandler(VOID)
{
    UINT32 iar = GIC_REG_32(GICC_IAR);  // 读取中断确认寄存器获取中断号
    UINT32 vector = iar & 0x3FFU;  // 提取低10位作为中断向量号

    /*
     * 无效中断号，主要是伪中断0x3ff
     * gicv2有效中断范围是0~1019，我们使用OS_HWI_MAX_NUM进行检查
     */
    if (vector >= OS_HWI_MAX_NUM) {  // 检查中断号是否有效
        return;  // 无效中断，直接返回
    }
    g_curIrqNum = vector;  // 更新当前中断号

    OsInterrupt(vector);  // 调用内核中断处理函数

    /* 使用原始iar值执行EOI */
    GIC_REG_32(GICC_EOIR) = iar;  // 写入EOIR寄存器结束中断处理
}

CHAR *HalIrqVersion(VOID)
{
    UINT32 pidr = GIC_REG_32(GICD_PIDR2V2);  // 读取PIDR2寄存器获取版本信息
    CHAR *irqVerString = NULL;  // 版本字符串指针

    switch (pidr >> GIC_REV_OFFSET) {  // 提取版本字段
        case GICV1:
            irqVerString = "GICv1";  // GICv1版本
            break;
        case GICV2:
            irqVerString = "GICv2";  // GICv2版本
            break;
        default:
            irqVerString = "unknown";  // 未知版本
    }
    return irqVerString;  // 返回版本字符串
}

#endif
