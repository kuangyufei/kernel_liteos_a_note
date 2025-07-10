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

/**
 * @brief 静态断言检查用户中断最大数量是否合法
 * @note 确保用户定义的中断数量不超过GICv2控制器支持的最大硬件中断数(1020)
 */
STATIC_ASSERT(OS_USER_HWI_MAX <= 1020, "hwi max is too large!");

#ifdef LOSCFG_ARCH_GIC_V2

/**
 * @brief 当前活跃中断号全局变量
 * @details 用于记录正在处理的中断向量号，中断处理程序中可通过HalCurIrqGet()获取
 */
STATIC UINT32 g_curIrqNum = 0;

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief 写入软件生成中断(SGI)寄存器
 * @param vector SGI中断向量号(0-15)
 * @param cpuMask CPU亲和性掩码，指定中断发送目标CPU
 * @param filter 中断转发过滤模式:
 *        - 0b00: 转发到cpu_mask指定的CPU接口
 *        - 0b01: 转发到所有CPU接口
 *        - 0b10: 仅转发到请求中断的CPU接口
 * @note 此函数直接操作GICD_SGIR寄存器，用于多核心间IPI通信
 */
STATIC VOID GicWriteSgi(UINT32 vector, UINT32 cpuMask, UINT32 filter)
{
    UINT32 val = ((filter & 0x3) << 24) | ((cpuMask & 0xFF) << 16) | /* 24:过滤位偏移, 16:CPU掩码位偏移 */
                 (vector & 0xF);                                   /* 低4位:SGI向量号(0-15) */

    GIC_REG_32(GICD_SGIR) = val;  /* 写入GIC分发器SGI寄存器 */
}

/**
 * @brief 发送处理器间中断(IPI)
 * @param target 目标CPU掩码
 * @param ipi IPI中断类型(对应SGI向量号)
 * @note 封装GicWriteSgi函数，使用过滤模式0(按CPU掩码定向发送)
 */
VOID HalIrqSendIpi(UINT32 target, UINT32 ipi)
{
    GicWriteSgi(ipi, target, 0);
}

/**
 * @brief 设置中断的CPU亲和性
 * @param vector 中断向量号
 * @param cpuMask 目标CPU掩码(8位)
 * @note GICv2的ITARGETSR寄存器每组4个中断，每个中断占8位CPU掩码
 */
VOID HalIrqSetAffinity(UINT32 vector, UINT32 cpuMask)
{
    UINT32 offset = vector / 4; /* 4:每个ITARGETSR寄存器控制4个中断 */
    UINT32 index = vector & 0x3;/* 计算中断在寄存器内的偏移索引(0-3) */

    GIC_REG_8(GICD_ITARGETSR(offset) + index) = cpuMask; /* 设置8位CPU亲和性掩码 */
}
#endif

/**
 * @brief 获取当前处理的中断号
 * @return 当前活跃的中断向量号
 */
UINT32 HalCurIrqGet(VOID)
{
    return g_curIrqNum;
}

/**
 * @brief 屏蔽指定中断
 * @param vector 要屏蔽的中断向量号
 * @note 仅对用户定义范围内的中断(OS_USER_HWI_MIN ~ OS_USER_HWI_MAX)进行操作
 */
VOID HalIrqMask(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return; /* 忽略超出用户中断范围的请求 */
    }

    /* 写入GICD_ICENABLER寄存器，对应位写1屏蔽中断 */
    GIC_REG_32(GICD_ICENABLER(vector / 32)) = 1U << (vector % 32); /* 32:每个寄存器控制32个中断 */
}

/**
 * @brief 解除中断屏蔽
 * @param vector 要解除屏蔽的中断向量号
 * @note vector >> 5 等价于 vector / 32，32个中断共用一个使能寄存器
 */
VOID HalIrqUnmask(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return; /* 忽略超出用户中断范围的请求 */
    }

    /* 写入GICD_ISENABLER寄存器，对应位写1使能中断 */
    GIC_REG_32(GICD_ISENABLER(vector >> 5)) = 1U << (vector % 32); /* 5:2^5=32,右移5位等价除32 */
}

/**
 * @brief 挂起指定中断
 * @param vector 要挂起的中断向量号
 * @note 强制设置中断挂起状态，即使中断未实际发生
 */
VOID HalIrqPending(UINT32 vector)
{
    if ((vector > OS_USER_HWI_MAX) || (vector < OS_USER_HWI_MIN)) {
        return; /* 忽略超出用户中断范围的请求 */
    }

    /* 写入GICD_ISPENDR寄存器，对应位写1挂起中断 */
    GIC_REG_32(GICD_ISPENDR(vector >> 5)) = 1U << (vector % 32); /* 5:2^5=32,右移5位等价除32 */
}

/**
 * @brief 清除中断(发送EOI)
 * @param vector 要清除的中断向量号
 * @note 向GICC_EOIR寄存器写入中断号完成中断处理
 */
VOID HalIrqClear(UINT32 vector)
{
    GIC_REG_32(GICC_EOIR) = vector; /* 写入CPU接口EOI寄存器，结束中断处理 */
}

/**
 * @brief 每CPU核心的GIC初始化
 * @details 配置CPU接口控制寄存器和优先级屏蔽寄存器
 */
VOID HalIrqInitPercpu(VOID)
{
    /* 设置优先级屏蔽寄存器，0xFF表示允许所有优先级中断(8位优先级) */
    GIC_REG_32(GICC_PMR) = 0xFF;

    /* 使能CPU接口控制器 */
    GIC_REG_32(GICC_CTLR) = 1;
}

/**
 * @brief GIC中断控制器初始化入口函数
 * @details 完成GIC分发器配置、中断优先级设置、CPU接口初始化等工作
 */
VOID HalIrqInit(VOID)
{
    UINT32 i;

    /* 配置外部中断为电平触发、低电平有效 */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 16) { /* 32:起始中断号, 16:每个ICFGR寄存器控制16个中断 */
        GIC_REG_32(GICD_ICFGR(i / 16)) = 0; /* 16:寄存器索引=中断号/16, 0=电平触发 */
    }

    /* 设置外部中断默认路由到CPU 0 */
    for (i = 32; i < OS_HWI_MAX_NUM; i += 4) { /* 32:起始中断号, 4:每个ITARGETSR寄存器控制4个中断 */
        GIC_REG_32(GICD_ITARGETSR(i / 4)) = 0x01010101; /* 0x01:CPU0亲和性掩码，每个字节代表一个中断 */
    }

    /* 设置所有中断的默认优先级 */
    for (i = 0; i < OS_HWI_MAX_NUM; i += 4) { /* 4:每个IPRIORITYR寄存器控制4个中断的优先级 */
        GIC_REG_32(GICD_IPRIORITYR(i / 4)) = GICD_INT_DEF_PRI_X4; /* 宏定义:4个中断的默认优先级值 */
    }

    /* 禁用所有中断 */
    for (i = 0; i < OS_HWI_MAX_NUM; i += 32) { /* 32:每个ICENABLER寄存器控制32个中断 */
        GIC_REG_32(GICD_ICENABLER(i / 32)) = ~0; /* ~0:所有位设1，禁用该寄存器控制的所有中断 */
    }

    HalIrqInitPercpu(); /* 初始化本CPU的GIC接口 */

    /* 使能GIC分发器控制 */
    GIC_REG_32(GICD_CTLR) = 1;

#ifdef LOSCFG_KERNEL_SMP
    /* 注册处理器间中断(IPI)处理函数 */
    (VOID)LOS_HwiCreate(LOS_MP_IPI_WAKEUP, 0xa0, 0, OsMpWakeHandler, 0);        /* 唤醒IPI */
    (VOID)LOS_HwiCreate(LOS_MP_IPI_SCHEDULE, 0xa0, 0, OsMpScheduleHandler, 0);  /* 调度IPI */
    (VOID)LOS_HwiCreate(LOS_MP_IPI_HALT, 0xa0, 0, OsMpHaltHandler, 0);          /* 停机IPI */
#ifdef LOSCFG_KERNEL_SMP_CALL
    (VOID)LOS_HwiCreate(LOS_MP_IPI_FUNC_CALL, 0xa0, 0, OsMpFuncCallHandler, 0); /* 函数调用IPI */
#endif
#endif
}

/**
 * @brief GIC中断处理入口函数
 * @details 从GIC CPU接口获取中断向量，进行合法性检查后分发处理，最后发送EOI信号
 */
VOID HalIrqHandler(VOID)
{
    UINT32 iar = GIC_REG_32(GICC_IAR);       /* 读取中断确认寄存器(IAR)值 */
    UINT32 vector = iar & 0x3FFU;            /* 提取低10位中断向量号(0-1023) */

    /*
     * 无效中断号检查，主要过滤伪中断(0x3ff)
     * GICv2支持的有效中断范围是0~1019，使用OS_HWI_MAX_NUM进行边界检查
     */
    if (vector >= OS_HWI_MAX_NUM) {
        return;                              /* 忽略超出范围的中断 */
    }
    g_curIrqNum = vector;                    /* 更新当前活跃中断号 */

    OsInterrupt(vector);                     /* 调用内核中断分发函数处理具体中断 */

    /* 使用原始iar值执行EOI(End of Interrupt)操作 */
    GIC_REG_32(GICC_EOIR) = iar;             /* 写入EOIR寄存器，通知GIC中断处理完成 */
}

/**
 * @brief 获取GIC控制器版本信息
 * @return 版本字符串，可能值为"GICv1"、"GICv2"或"unknown"
 * @note 通过读取GICD_PIDR2V2寄存器的版本字段判断GIC版本
 */
CHAR *HalIrqVersion(VOID)
{
    UINT32 pidr = GIC_REG_32(GICD_PIDR2V2);  /* 读取GIC分发器外设ID寄存器2 */
    CHAR *irqVerString = NULL;               /* 版本字符串指针 */

    /* 根据GIC_REV_OFFSET偏移提取版本字段 */
    switch (pidr >> GIC_REV_OFFSET) {
        case GICV1:                          /* GICv1控制器 */
            irqVerString = "GICv1";
            break;
        case GICV2:                          /* GICv2控制器 */
            irqVerString = "GICv2";
            break;
        default:                             /* 未知版本 */
            irqVerString = "unknown";
    }
    return irqVerString;
}

#endif
