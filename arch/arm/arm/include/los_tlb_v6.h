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

/**
 * @defgroup los_tlb_v6 MMU TLB v6
 * @ingroup kernel
 */
#ifndef __LOS_TLB_V6_H__
#define __LOS_TLB_V6_H__

#include "los_typedef.h"
#include "arm.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 使TLB失效并添加屏障指令
 * @note 该函数确保TLB失效操作对所有核心可见，且后续指令在失效完成后执行
 */
STATIC INLINE VOID OsArmInvalidateTlbBarrier(VOID)
{
#ifdef LOSCFG_KERNEL_SMP
    OsArmWriteBpiallis(0);               // SMP模式下：使所有分支预测器和TLB条目失效
#else
    OsArmWriteBpiall(0);                 // UP模式下：使所有分支预测器和TLB条目失效
#endif
    DSB;                                  // 数据同步屏障：确保之前的TLB操作完成
    ISB;                                  // 指令同步屏障：刷新流水线并获取新指令
}

/**
 * @brief 使特定虚拟地址的TLB条目失效（不带屏障）
 * @param va 要失效的虚拟地址（必须页对齐）
 * @note 1. 本函数不含屏障指令，必要时需在外部添加
 *       2. 地址与0xfffff000进行掩码操作，确保按4KB页边界对齐
 */
STATIC INLINE VOID OsArmInvalidateTlbMvaNoBarrier(VADDR_T va)
{
#ifdef LOSCFG_KERNEL_SMP
    OsArmWriteTlbimvaais(va & 0xfffff000); // SMP模式下：使指定虚拟地址的TLB条目失效
#else
    OsArmWriteTlbimvaa(va & 0xfffff000);   // UP模式下：使指定虚拟地址的TLB条目失效
#endif
}

/**
 * @brief 使连续范围虚拟地址的TLB条目失效（不带屏障）
 * @param start 起始虚拟地址（必须页对齐）
 * @param count 要失效的页数
 * @note 循环调用OsArmInvalidateTlbMvaNoBarrier处理连续页面
 */
STATIC INLINE VOID OsArmInvalidateTlbMvaRangeNoBarrier(VADDR_T start, UINT32 count)
{
    UINT32 index = 0;                     // 循环索引

    while (count > 0) {                   // 遍历所有需要失效的页面
        // 计算当前虚拟地址：start + index * 页面大小（页面大小 = 1 << MMU_DESCRIPTOR_L2_SMALL_SHIFT）
        OsArmInvalidateTlbMvaNoBarrier(start + (index << MMU_DESCRIPTOR_L2_SMALL_SHIFT));
        index++;                          // 移动到下一页
        count--;                          // 递减剩余计数
    }
}

/**
 * @brief 清洁并使整个TLB失效
 * @note 使用ARM协处理器指令实现：MCR p15, 0, Rd, c8, c7, 0
 *       该指令使指令TLB和数据TLB中的所有条目失效
 */
STATIC INLINE VOID OsCleanTLB(VOID)
{
    UINT32 val = 0;                       //  dummy值，指令未使用
    __asm volatile("mcr    p15, 0, %0, c8, c7, 0" : : "r"(val)); // ARM CP15指令：TLB清洁和失效
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_TLB_V6_H__ */

