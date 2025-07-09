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

#ifndef _ARMV7_PMU_PRI_H
#define _ARMV7_PMU_PRI_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/* 计数器溢出标志状态寄存器相关宏定义 */
#define ARMV7_FLAG_MASK         0xffffffff                 /* 可写位掩码，十六进制0xffffffff对应十进制4294967295 */
#define ARMV7_OVERFLOWED_MASK   ARMV7_FLAG_MASK            /* PMU溢出掩码，与可写位掩码值相同 */

/* 性能监控控制寄存器(PMNC)配置位定义 */
#define ARMV7_PMNC_E            (1U << 0)                  /* 启用所有计数器，位0置1 */
#define ARMV7_PMNC_P            (1U << 1)                  /* 重置所有计数器，位1置1 */
#define ARMV7_PMNC_C            (1U << 2)                  /* 周期计数器重置，位2置1 */
#define ARMV7_PMNC_D            (1U << 3)                  /* 周期计数器每64个CPU周期计数一次，位3置1 */
#define ARMV7_PMNC_X            (1U << 4)                  /* 导出事件到ETM跟踪，位4置1 */
#define ARMV7_PMNC_DP           (1U << 5)                  /* 非侵入式调试时禁用周期计数器，位5置1 */
#define ARMV7_PMNC_MASK         0x3f                       /* PMNC寄存器可写位掩码，十六进制0x3f对应十进制63 */

/* 性能监控事件类型寄存器(PMXEVTYPER)相关宏 */
#define ARMV7_EVTYPE_MASK       0xc80000ff                 /* 事件类型寄存器可写位掩码 */

/* ARMv7性能计数器索引定义 */
#define ARMV7_IDX_COUNTER0      1                          /* 第一个事件计数器索引 */
#define ARMV7_IDX_CYCLE_COUNTER 0                          /* 周期计数器索引 */
#define ARMV7_IDX_MAX_COUNTER   9                          /* 最大计数器索引值 */

#define ARMV7_MAX_COUNTERS      32                         /* 最大支持的计数器数量 */
#define ARMV7_IDX_COUNTER_LAST  (ARMV7_IDX_CYCLE_COUNTER + ARMV7_MAX_COUNTERS - 1)  /* 最后一个计数器索引 */
#define ARMV7_COUNTER_MASK      (ARMV7_MAX_COUNTERS - 1)   /* 计数器索引掩码，用于取模运算 */

/* ARMv7事件计数器索引映射宏 */
#define ARMV7_CNT2BIT(x)        (1UL << (x))               /* 将计数器索引转换为对应的位掩码 */
#define ARMV7_IDX2CNT(x)        (((x) - ARMV7_IDX_COUNTER0) & ARMV7_COUNTER_MASK)  /* 将外部索引转换为内部计数器编号 */

/**
 * @brief PMU事件类型枚举
 * 定义ARMv7架构支持的性能监控事件类型及对应编码值
 */
enum PmuEventType {
    ARMV7_PERF_HW_CYCLES                   = 0xFF,    /* 周期计数事件，编码0xFF(十进制255) */
    ARMV7_PERF_HW_INSTRUCTIONS             = 0x08,    /* 指令执行事件，编码0x08(十进制8) */
    ARMV7_PERF_HW_DCACHES                  = 0x04,    /* 数据缓存访问事件，编码0x04(十进制4) */
    ARMV7_PERF_HW_DCACHE_MISSES            = 0x03,    /* 数据缓存未命中事件，编码0x03(十进制3) */
    ARMV7_PERF_HW_ICACHES                  = 0x14,    /* 指令缓存访问事件，编码0x14(十进制20) */
    ARMV7_PERF_HW_ICACHE_MISSES            = 0x01,    /* 指令缓存未命中事件，编码0x01(十进制1) */
    ARMV7_PERF_HW_BRANCHES                 = 0x0C,    /* 分支指令事件(软件改变PC)，编码0x0C(十进制12) */
    ARMV7_PERF_HW_BRANCE_MISSES            = 0x10,    /* 分支预测失败事件，编码0x10(十进制16) */
    ARMV7_PERF_HW_PRED_BRANCH              = 0x12,    /* 可预测分支事件，编码0x12(十进制18) */
    ARMV7_PERF_HW_NUM_CYC_IRQ              = 0x50,    /* IRQ被中断的周期数事件，编码0x50(十进制80) */
    ARMV7_PERF_HW_EXC_TAKEN                = 0x09,    /* 异常被采纳事件，编码0x09(十进制9) */
    ARMV7_PERF_HW_DATA_READ                = 0x06,    /* 数据读取事件，编码0x06(十进制6) */
    ARMV7_PERF_HW_DATA_WRITE               = 0x07,    /* 数据写入事件，编码0x07(十进制7) */
    ARMV7_PERF_HW_STREX_PASSED             = 0x80,    /* STREX指令成功事件，编码0x80(十进制128) */
    ARMV7_PERF_HW_STREX_FAILED             = 0x81,    /* STREX指令失败事件，编码0x81(十进制129) */
    ARMV7_PERF_HW_LP_IN_TCM                = 0x82,    /* 文字池位于TCM区域事件，编码0x82(十进制130) */
    ARMV7_PERF_HW_DMB_STALL                = 0x90,    /* DMB指令导致的停顿事件，编码0x90(十进制144) */
    ARMV7_PERF_HW_ITCM_ACCESS              = 0x91,    /* ITCM访问事件，编码0x91(十进制145) */
    ARMV7_PERF_HW_DTCM_ACCESS              = 0x92,    /* DTCM访问事件，编码0x92(十进制146) */
    ARMV7_PERF_HW_DATA_EVICTION            = 0x93,    /* 数据驱逐事件，编码0x93(十进制147) */
    ARMV7_PERF_HW_SCU                      = 0x94,    /* SCU一致性操作事件，编码0x94(十进制148) */
    ARMV7_PERF_HW_INSCACHE_DEP_DW          = 0x95,    /* 指令缓存依赖停顿事件，编码0x95(十进制149) */
    ARMV7_PERF_HW_DATA_CACHE_DEP_STALL     = 0x96,    /* 数据缓存依赖停顿事件，编码0x96(十进制150) */
    ARMV7_PERF_HW_NOCACHE_NO_PER_DEP_STALL = 0x97,    /* 非缓存非外设依赖停顿事件，编码0x97(十进制151) */
    ARMV7_PERF_HW_NOCACHE_PER_DEP_STALL    = 0x98,    /* 非缓存外设依赖停顿事件，编码0x98(十进制152) */
    ARMV7_PERF_HW_DATA_CACHE_HP_DEP_STALL  = 0x99,    /* 数据缓存高优先级依赖停顿事件，编码0x99(十进制153) */
    ARMV7_PERF_HW_AXI_FAST_PERIPHERAL      = 0x9A,    /* AXI快速外设端口访问事件(读和写)，编码0x9A(十进制154) */
};

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _ARMV7_PMU_PRI_H */
