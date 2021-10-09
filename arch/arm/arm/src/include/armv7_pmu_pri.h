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

/* counters overflow flag status reg */
#define ARMV7_FLAG_MASK         0xffffffff                 /* Mask for writable bits */
#define ARMV7_OVERFLOWED_MASK   ARMV7_FLAG_MASK            /* Mask for pmu overflowed */

/* pmnc config reg */
#define ARMV7_PMNC_E            (1U << 0)                  /* Enable all counters */
#define ARMV7_PMNC_P            (1U << 1)                  /* Reset all counters */
#define ARMV7_PMNC_C            (1U << 2)                  /* Cycle counter reset */
#define ARMV7_PMNC_D            (1U << 3)                  /* CCNT counts every 64th cpu cycle */
#define ARMV7_PMNC_X            (1U << 4)                  /* Export to ETM */
#define ARMV7_PMNC_DP           (1U << 5)                  /* Disable CCNT if non-invasive debug */
#define ARMV7_PMNC_MASK         0x3f                       /* Mask for writable bits */

/* pmxevtyper event selection reg */
#define ARMV7_EVTYPE_MASK       0xc80000ff                 /* Mask for writable bits */

/* armv7 counters index */
#define ARMV7_IDX_COUNTER0      1
#define ARMV7_IDX_CYCLE_COUNTER 0
#define ARMV7_IDX_MAX_COUNTER   9

#define ARMV7_MAX_COUNTERS      32
#define ARMV7_IDX_COUNTER_LAST  (ARMV7_IDX_CYCLE_COUNTER + ARMV7_MAX_COUNTERS - 1)
#define ARMV7_COUNTER_MASK      (ARMV7_MAX_COUNTERS - 1)

/* armv7 event counter index mapping */
#define ARMV7_CNT2BIT(x)        (1UL << (x))
#define ARMV7_IDX2CNT(x)        (((x) - ARMV7_IDX_COUNTER0) & ARMV7_COUNTER_MASK)

enum PmuEventType {
    ARMV7_PERF_HW_CYCLES                   = 0xFF,    /* cycles */
    ARMV7_PERF_HW_INSTRUCTIONS             = 0x08,    /* instructions */
    ARMV7_PERF_HW_DCACHES                  = 0x04,    /* dcache */
    ARMV7_PERF_HW_DCACHE_MISSES            = 0x03,    /* dcache-misses */
    ARMV7_PERF_HW_ICACHES                  = 0x14,    /* icache */
    ARMV7_PERF_HW_ICACHE_MISSES            = 0x01,    /* icache-misses */
    ARMV7_PERF_HW_BRANCHES                 = 0x0C,    /* software change of pc */
    ARMV7_PERF_HW_BRANCE_MISSES            = 0x10,    /* branch-misses */
    ARMV7_PERF_HW_PRED_BRANCH              = 0x12,    /* predictable branches */
    ARMV7_PERF_HW_NUM_CYC_IRQ              = 0x50,    /* number of cycles Irqs are interrupted */
    ARMV7_PERF_HW_EXC_TAKEN                = 0x09,    /* exception_taken */
    ARMV7_PERF_HW_DATA_READ                = 0x06,    /* data read */
    ARMV7_PERF_HW_DATA_WRITE               = 0x07,    /* data write */
    ARMV7_PERF_HW_STREX_PASSED             = 0x80,    /* strex passed */
    ARMV7_PERF_HW_STREX_FAILED             = 0x81,    /* strex failed */
    ARMV7_PERF_HW_LP_IN_TCM                = 0x82,    /* literal pool in TCM region */
    ARMV7_PERF_HW_DMB_STALL                = 0x90,    /* DMB stall */
    ARMV7_PERF_HW_ITCM_ACCESS              = 0x91,    /* ITCM access */
    ARMV7_PERF_HW_DTCM_ACCESS              = 0x92,    /* DTCM access */
    ARMV7_PERF_HW_DATA_EVICTION            = 0x93,    /* data eviction */
    ARMV7_PERF_HW_SCU                      = 0x94,    /* SCU coherency operation */
    ARMV7_PERF_HW_INSCACHE_DEP_DW          = 0x95,    /* instruction cache dependent stall */
    ARMV7_PERF_HW_DATA_CACHE_DEP_STALL     = 0x96,    /* data cache dependent stall */
    ARMV7_PERF_HW_NOCACHE_NO_PER_DEP_STALL = 0x97,    /* non-cacheable no peripheral dependent stall */
    ARMV7_PERF_HW_NOCACHE_PER_DEP_STALL    = 0x98,    /* non-Cacheable peripheral dependent stall */
    ARMV7_PERF_HW_DATA_CACHE_HP_DEP_STALL  = 0x99,    /* data cache high priority dependent stall */
    ARMV7_PERF_HW_AXI_FAST_PERIPHERAL      = 0x9A,    /* Accesses_to_AXI_fast_peripheral_port(reads_and_writes) */
};

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _ARMV7_PMU_PRI_H */
