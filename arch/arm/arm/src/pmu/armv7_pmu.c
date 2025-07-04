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

#include "armv7_pmu_pri.h"; // 包含 ARMv7 性能监控单元的私有头文件
#include "perf_pmu_pri.h";  // 包含性能监控单元的私有头文件
#include "los_hw_cpu.h";    // 包含硬件 CPU 相关的头文件
#include "asm/platform.h";  // 包含平台相关的汇编头文件

OS_PMU_INTS(LOSCFG_KERNEL_CORE_NUM, g_pmuIrqNr); // 调用 OS_PMU_INTS 宏，传入内核核心数和 PMU 中断号数组
STATIC HwPmu g_armv7Pmu; // 静态定义一个 ARMv7 性能监控单元结构体实例

/**
 * @brief 读取 ARMv7 性能监控控制寄存器 (PMNC) 的值。
 *
 * 该函数使用内联汇编指令从 PMNC 寄存器读取值。
 *
 * @return UINT32 PMNC 寄存器的值。
 */
STATIC INLINE UINT32 Armv7PmncRead(VOID)
{
    UINT32 value = 0; // 初始化用于存储寄存器值的变量
    __asm__ volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(value)); // 使用内联汇编从 PMNC 寄存器读取值到 value 变量
    return value; // 返回读取到的 PMNC 寄存器的值
}

/**
 * @brief 向 ARMv7 性能监控控制寄存器 (PMNC) 写入值。
 *
 * 该函数使用内联汇编指令将指定的值写入 PMNC 寄存器，并执行指令同步屏障。
 *
 * @param value 要写入 PMNC 寄存器的值，会先与掩码进行按位与操作。
 */
STATIC INLINE VOID Armv7PmncWrite(UINT32 value)
{
    value &= ARMV7_PMNC_MASK; // 将 value 与 ARMV7_PMNC_MASK 进行按位与操作，确保值在有效范围内
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(value)); // 使用内联汇编将 value 写入 PMNC 寄存器
    ISB; // 执行指令同步屏障，确保指令按序执行
}

/**
 * @brief 检查 ARMv7 性能监控单元是否有计数器溢出。
 *
 * 该函数通过检查 PMNC 寄存器的值来判断是否有计数器溢出。
 *
 * @param pmnc PMNC 寄存器的值。
 * @return UINT32 若有计数器溢出，返回非零值；否则返回 0。
 */
STATIC INLINE UINT32 Armv7PmuOverflowed(UINT32 pmnc)
{
    return pmnc & ARMV7_OVERFLOWED_MASK; // 将 pmnc 与 ARMV7_OVERFLOWED_MASK 进行按位与操作，判断是否有溢出
}

/**
 * @brief 检查指定索引的 ARMv7 性能监控计数器是否溢出。
 *
 * 该函数通过检查 PMNC 寄存器的值来判断指定索引的计数器是否溢出。
 *
 * @param pmnc PMNC 寄存器的值。
 * @param index 计数器的索引。
 * @return UINT32 若指定索引的计数器溢出，返回非零值；否则返回 0。
 */
STATIC INLINE UINT32 Armv7PmuCntOverflowed(UINT32 pmnc, UINT32 index)
{
    return pmnc & ARMV7_CNT2BIT(ARMV7_IDX2CNT(index)); // 将 pmnc 与指定索引对应的位掩码进行按位与操作，判断该计数器是否溢出
}

/**
 * @brief 检查指定索引的 ARMv7 性能监控计数器是否有效。
 *
 * 该函数通过比较索引与最后一个有效计数器的索引，判断指定索引的计数器是否有效。
 *
 * @param index 计数器的索引。
 * @return UINT32 若索引有效，返回非零值；否则返回 0。
 */
STATIC INLINE UINT32 Armv7CntValid(UINT32 index)
{
    return index <= ARMV7_IDX_COUNTER_LAST; // 判断 index 是否小于等于最后一个有效计数器的索引
}

/**
 * @brief 选择指定索引的 ARMv7 性能监控计数器。
 *
 * 该函数使用内联汇编指令选择指定索引的计数器。
 *
 * @param index 计数器的索引。
 */
STATIC INLINE VOID Armv7PmuSelCnt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index); // 将索引转换为对应的计数器编号
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (counter)); // 使用内联汇编选择指定的计数器
    ISB; // 执行指令同步屏障，确保指令按序执行
}

/**
 * @brief 设置指定索引的 ARMv7 性能监控计数器的周期。
 *
 * 该函数根据计数器的索引，使用内联汇编指令设置计数器的周期。
 *
 * @param index 计数器的索引。
 * @param period 计数器的周期值。
 */
STATIC INLINE VOID Armv7PmuSetCntPeriod(UINT32 index, UINT32 period)
{
    if (!Armv7CntValid(index)) { // 检查索引是否有效
        PRINT_ERR("CPU writing wrong counter %u\n", index); // 若索引无效，打印错误信息
    } else if (index == ARMV7_IDX_CYCLE_COUNTER) { // 若为周期计数器
        __asm__ volatile("mcr p15, 0, %0, c9, c13, 0" : : "r" (period)); // 使用内联汇编设置周期
    } else {
        Armv7PmuSelCnt(index); // 选择指定索引的计数器
        __asm__ volatile("mcr p15, 0, %0, c9, c13, 2" : : "r" (period)); // 使用内联汇编设置计数器的周期
    }
}

/**
 * @brief 将指定事件绑定到指定索引的 ARMv7 性能监控计数器。
 *
 * 该函数将指定的事件 ID 绑定到指定索引的计数器，并打印调试信息。
 *
 * @param index 计数器的索引。
 * @param value 事件 ID。
 */
STATIC INLINE VOID Armv7BindEvt2Cnt(UINT32 index, UINT32 value)
{
    PRINT_DEBUG("bind event: %u to counter: %u\n", value, index); // 打印事件绑定的调试信息
    Armv7PmuSelCnt(index); // 选择指定索引的计数器
    value &= ARMV7_EVTYPE_MASK; // 将 value 与 ARMV7_EVTYPE_MASK 进行按位与操作，确保值在有效范围内
    __asm__ volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (value)); // 使用内联汇编将事件 ID 写入相应寄存器
}

/**
 * @brief 启用指定索引的 ARMv7 性能监控计数器。
 *
 * 该函数使用内联汇编指令启用指定索引的计数器，并打印调试信息。
 *
 * @param index 计数器的索引。
 */
STATIC INLINE VOID Armv7EnableCnt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index); // 将索引转换为对应的计数器编号
    PRINT_DEBUG("index : %u, counter: %u\n", index, counter); // 打印计数器启用的调试信息
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (ARMV7_CNT2BIT(counter))); // 使用内联汇编启用指定的计数器
}

/**
 * @brief 禁用指定索引的 ARMv7 性能监控计数器。
 *
 * 该函数使用内联汇编指令禁用指定索引的计数器，并打印调试信息。
 *
 * @param index 计数器的索引。
 */
STATIC INLINE VOID Armv7DisableCnt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index); // 将索引转换为对应的计数器编号
    PRINT_DEBUG("index : %u, counter: %u\n", index, counter); // 打印计数器禁用的调试信息
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (ARMV7_CNT2BIT(counter))); // 使用内联汇编禁用指定的计数器
}

/**
 * @brief 启用指定索引的 ARMv7 性能监控计数器的中断。
 *
 * 该函数使用内联汇编指令启用指定索引计数器的中断，并执行指令同步屏障。
 *
 * @param index 计数器的索引。
 */
STATIC INLINE VOID Armv7EnableCntInterrupt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index); // 将索引转换为对应的计数器编号
    __asm__ volatile("mcr p15, 0, %0, c9, c14, 1" : : "r" (ARMV7_CNT2BIT(counter))); // 使用内联汇编启用指定计数器的中断
    ISB; // 执行指令同步屏障，确保指令按序执行
}

/**
 * @brief 禁用指定索引的 ARMv7 性能监控计数器的中断，并清除溢出标志。
 *
 * 该函数使用内联汇编指令禁用指定索引计数器的中断，并清除可能挂起的中断溢出标志。
 *
 * @param index 计数器的索引。
 */
STATIC INLINE VOID Armv7DisableCntInterrupt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index); // 将索引转换为对应的计数器编号
    __asm__ volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (ARMV7_CNT2BIT(counter))); // 使用内联汇编禁用指定计数器的中断
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (ARMV7_CNT2BIT(counter))); // 清除溢出标志，防止中断挂起
    ISB; // 执行指令同步屏障，确保指令按序执行
}

/**
 * @brief 获取 ARMv7 性能监控单元的溢出状态，并清除溢出标志。
 *
 * 该函数使用内联汇编指令读取溢出状态寄存器的值，清除溢出标志后返回状态值。
 *
 * @return UINT32 溢出状态寄存器的值。
 */
STATIC INLINE UINT32 Armv7PmuGetOverflowStatus(VOID)
{
    UINT32 value; // 定义用于存储溢出状态的变量
    __asm__ volatile("mrc p15, 0, %0, c9, c12, 3" : "=r" (value)); // 使用内联汇编从溢出状态寄存器读取值到 value 变量
    value &= ARMV7_FLAG_MASK; // 将 value 与 ARMV7_FLAG_MASK 进行按位与操作，获取有效标志位
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (value)); // 使用内联汇编将清除后的 value 写回溢出状态寄存器
    return value; // 返回溢出状态寄存器的值
}

/**
 * @brief 启用指定事件的 ARMv7 性能监控计数器。
 *
 * 该函数会检查计数器索引和周期的有效性，然后禁用计数器，绑定事件，启用中断和计数器。
 *
 * @param event 指向事件结构体的指针。
 */
STATIC VOID Armv7EnableEvent(Event *event)
{
    UINT32 cnt = event->counter; // 获取事件对应的计数器编号
    if (!Armv7CntValid(cnt)) { // 检查计数器索引是否有效
        PRINT_ERR("CPU enabling wrong PMNC counter IRQ enable %u\n", cnt); // 若索引无效，打印错误信息并返回
        return;
    }
    if (event->period == 0) { // 检查事件周期是否为 0
        PRINT_INFO("event period value not valid, counter: %u\n", cnt); // 若周期为 0，打印信息并返回
        return;
    }
    UINT32 intSave = LOS_IntLock(); // 锁定中断，防止中断干扰操作
    Armv7DisableCnt(cnt); // 禁用指定的计数器
    if (cnt != ARMV7_IDX_CYCLE_COUNTER) { // 若不是周期计数器
        Armv7BindEvt2Cnt(cnt, event->eventId); // 将事件绑定到指定计数器
    }
    Armv7EnableCntInterrupt(cnt); // 启用指定计数器的中断
    Armv7EnableCnt(cnt); // 启用指定的计数器
    LOS_IntRestore(intSave); // 恢复中断状态
    PRINT_DEBUG("enabled event: %u cnt: %u\n", event->eventId, cnt); // 打印事件启用的调试信息
}

/**
 * @brief 禁用指定事件的 ARMv7 性能监控计数器。
 *
 * 该函数会检查计数器索引的有效性，然后禁用计数器和其中断。
 *
 * @param event 指向事件结构体的指针。
 */
STATIC VOID Armv7DisableEvent(Event *event)
{
    UINT32 cnt = event->counter; // 获取事件对应的计数器编号
    if (!Armv7CntValid(cnt)) { // 检查计数器索引是否有效
        PRINT_ERR("CPU enabling wrong PMNC counter IRQ enable %u\n", cnt); // 若索引无效，打印错误信息并返回
        return;
    }
    UINT32 intSave = LOS_IntLock(); // 锁定中断，防止中断干扰操作
    Armv7DisableCnt(cnt); // 禁用指定的计数器
    Armv7DisableCntInterrupt(cnt); // 禁用指定计数器的中断
    LOS_IntRestore(intSave); // 恢复中断状态
}

/**
 * @brief 启动所有 ARMv7 性能监控计数器。
 *
 * 该函数会读取 PMNC 寄存器的值，设置启用标志和分频标志，写入寄存器，并解除中断屏蔽。
 */
STATIC VOID Armv7StartAllCnt(VOID)
{
    PRINT_DEBUG("starting pmu...\n"); // 打印启动 PMU 的调试信息
    UINT32 reg = Armv7PmncRead() | ARMV7_PMNC_E; // 读取 PMNC 寄存器的值，并设置启用标志
    if (g_armv7Pmu.cntDivided) { // 根据 g_armv7Pmu.cntDivided 的值设置或清除分频标志
        reg |= ARMV7_PMNC_D;
    } else {
        reg &= ~ARMV7_PMNC_D;
    }
    Armv7PmncWrite(reg); // 将修改后的寄存器值写入 PMNC 寄存器
    HalIrqUnmask(g_pmuIrqNr[ArchCurrCpuid()]); // 解除当前 CPU 对应的 PMU 中断屏蔽
}

/**
 * @brief 停止所有 ARMv7 性能监控计数器。
 *
 * 该函数会读取 PMNC 寄存器的值，清除启用标志，写入寄存器，并屏蔽中断。
 */
STATIC VOID Armv7StopAllCnt(VOID)
{
    PRINT_DEBUG("stopping pmu...\n"); // 打印停止 PMU 的调试信息
    Armv7PmncWrite(Armv7PmncRead() & ~ARMV7_PMNC_E); // 读取 PMNC 寄存器的值，并清除启用标志
    HalIrqMask(g_pmuIrqNr[ArchCurrCpuid()]); // 屏蔽当前 CPU 对应的 PMU 中断
}

/**
 * @brief 重置所有 ARMv7 性能监控计数器。
 *
 * 该函数会禁用所有计数器及其中断，初始化并重置 PMNC 寄存器。
 */
STATIC VOID Armv7ResetAllCnt(VOID)
{
    UINT32 index; // 定义循环索引变量
    for (index = ARMV7_IDX_CYCLE_COUNTER; index < ARMV7_IDX_MAX_COUNTER; index++) { // 遍历所有计数器
        Armv7DisableCnt(index); // 禁用计数器
        Armv7DisableCntInterrupt(index); // 禁用计数器中断
    }
    UINT32 reg = ARMV7_PMNC_P | ARMV7_PMNC_C | (g_armv7Pmu.cntDivided ? ARMV7_PMNC_D : 0); // 初始化 PMNC 寄存器的值，设置 C、P 位和 D 位
    Armv7PmncWrite(reg); // 将初始化后的寄存器值写入 PMNC 寄存器
}

/**
 * @brief 设置指定事件的 ARMv7 性能监控计数器周期。
 *
 * 该函数会检查事件周期是否有效，若有效则设置计数器的周期。
 *
 * @param event 指向事件结构体的指针。
 */
STATIC VOID Armv7SetEventPeriod(Event *event)
{
    if (event->period != 0) { // 检查事件周期是否不为 0
        PRINT_INFO("counter: %u, period: 0x%x\n", event->counter, event->period); // 打印计数器和周期的信息
        Armv7PmuSetCntPeriod(event->counter, PERIOD_CALC(event->period)); // 设置指定计数器的周期
    }
}

/**
 * @brief 读取指定事件的 ARMv7 性能监控计数器的值。
 *
 * 该函数会检查计数器索引的有效性，读取计数器的值，并根据溢出状态进行调整。
 *
 * @param event 指向事件结构体的指针。
 * @return UINTPTR 计数器的值。
 */
STATIC UINTPTR Armv7ReadEventCnt(Event *event)
{
    UINT32 value = 0; // 初始化用于存储计数器值的变量
    UINT32 index = event->counter; // 获取事件对应的计数器索引
    if (!Armv7CntValid(index)) { // 检查计数器索引是否有效
        PRINT_ERR("CPU reading wrong counter %u\n", index); // 若索引无效，打印错误信息
    } else if (index == ARMV7_IDX_CYCLE_COUNTER) { // 若为周期计数器
        __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (value)); // 使用内联汇编读取计数器的值
    } else {
        Armv7PmuSelCnt(index); // 选择指定索引的计数器
        __asm__ volatile("mrc p15, 0, %0, c9, c13, 2" : "=r" (value)); // 使用内联汇编读取计数器的值
    }
    if (value < PERIOD_CALC(event->period)) { // 判断读取的值是否小于周期计算值
        if (Armv7PmuCntOverflowed(Armv7PmuGetOverflowStatus(), event->counter)) { // 若计数器溢出
            value += event->period; // 将周期值加到读取的值上
        }
    } else {
        value -= PERIOD_CALC(event->period); // 若读取的值大于等于周期计算值，减去周期计算值
    }
    return value; // 返回调整后的计数器值
}

// 定义 ARMv7 性能监控事件映射表
STATIC const UINT32 g_armv7Map[] = {
    [PERF_COUNT_HW_CPU_CYCLES]          = ARMV7_PERF_HW_CYCLES,
    [PERF_COUNT_HW_INSTRUCTIONS]        = ARMV7_PERF_HW_INSTRUCTIONS,
    [PERF_COUNT_HW_DCACHE_REFERENCES]   = ARMV7_PERF_HW_DCACHES,
    [PERF_COUNT_HW_DCACHE_MISSES]       = ARMV7_PERF_HW_DCACHE_MISSES,
    [PERF_COUNT_HW_ICACHE_REFERENCES]   = ARMV7_PERF_HW_ICACHES,
    [PERF_COUNT_HW_ICACHE_MISSES]       = ARMV7_PERF_HW_ICACHE_MISSES,
    [PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = ARMV7_PERF_HW_BRANCHES,
    [PERF_COUNT_HW_BRANCH_MISSES]       = ARMV7_PERF_HW_BRANCE_MISSES,
};

/**
 * @brief 映射 ARMv7 性能监控事件类型。
 *
 * 该函数可以将通用事件类型映射为 ARMv7 实际事件类型，或反之。
 *
 * @param eventType 事件类型。
 * @param reverse 是否反向映射，TRUE 表示反向映射，FALSE 表示正向映射。
 * @return UINT32 映射后的事件类型。
 */
UINT32 Armv7PmuMapEvent(UINT32 eventType, BOOL reverse)
{
    if (!reverse) {  // Common event to armv7 real event
        if (eventType < ARRAY_SIZE(g_armv7Map)) { // 正向映射，若事件类型在映射表范围内
            return g_armv7Map[eventType]; // 返回映射后的事件类型
        }
        return eventType; // 若不在范围内，返回原事件类型
    } else {        // Armv7 real event to common event
        UINT32 i; // 定义循环索引变量
        for (i = 0; i < ARRAY_SIZE(g_armv7Map); i++) { // 反向映射，遍历映射表查找匹配的事件类型
            if (g_armv7Map[i] == eventType) {
                return i; // 若找到匹配项，返回对应的索引
            }
        }
        return PERF_HW_INVALID_EVENT_TYPE; // 若未找到匹配项，返回无效事件类型
    }
}

/**
 * @brief ARMv7 性能监控单元中断处理函数。
 *
 * 该函数会处理性能监控计数器的溢出中断，更新事件计数并处理溢出情况。
 */
STATIC VOID Armv7PmuIrqHandler(VOID)
{
    UINT32 index; // 定义循环索引变量
    PerfRegs regs; // 定义 PerfRegs 结构体变量
    PerfEvent *events = &(g_armv7Pmu.pmu.events); // 获取性能监控事件结构体指针
    UINT32 eventNum = events->nr; // 获取事件数量
    UINT32 pmnc = Armv7PmuGetOverflowStatus(); // 获取并重置中断标志
    if (!Armv7PmuOverflowed(pmnc)) { // 若没有计数器溢出
        return; // 直接返回
    }
    (VOID)memset_s(&regs, sizeof(PerfRegs), 0, sizeof(PerfRegs)); // 初始化 PerfRegs 结构体为 0
    OsPerfFetchIrqRegs(&regs); // 获取中断寄存器的值
    Armv7StopAllCnt(); // 停止所有计数器
    for (index = 0; index < eventNum; index++) { // 遍历所有事件
        Event *event = &(events->per[index]); // 获取当前事件的指针
        if (!Armv7PmuCntOverflowed(pmnc, event->counter) || (event->period == 0)) { // 检查当前计数器是否溢出且周期不为 0
            continue; // 若不满足条件，跳过当前循环
        }
        Armv7PmuSetCntPeriod(event->counter, PERIOD_CALC(event->period)); // 设置当前计数器的周期
        OsPerfUpdateEventCount(event, event->period); // 更新事件计数
        OsPerfHandleOverFlow(event, &regs); // 处理计数器溢出情况
    }
    Armv7StartAllCnt(); // 启动所有计数器
}

/**
 * @brief 获取 ARMv7 性能监控单元的最大计数器索引。
 *
 * @return UINT32 最大计数器索引。
 */
UINT32 OsGetPmuMaxCounter(VOID)
{
    return ARMV7_IDX_MAX_COUNTER; // 返回最大计数器索引
}

/**
 * @brief 获取 ARMv7 性能监控单元的周期计数器索引。
 *
 * @return UINT32 周期计数器索引。
 */
UINT32 OsGetPmuCycleCounter(VOID)
{
    return ARMV7_IDX_CYCLE_COUNTER; // 返回周期计数器索引
}

/**
 * @brief 获取 ARMv7 性能监控单元的计数器 0 索引。
 *
 * @return UINT32 计数器 0 索引。
 */
UINT32 OsGetPmuCounter0(VOID)
{
    return ARMV7_IDX_COUNTER0; // 返回计数器 0 索引
}

// 静态定义 ARMv7 性能监控单元结构体实例，并初始化其成员函数
STATIC HwPmu g_armv7Pmu = {
    .canDivided = TRUE,
    .enable     = Armv7EnableEvent,
    .disable    = Armv7DisableEvent,
    .start      = Armv7StartAllCnt,
    .stop       = Armv7StopAllCnt,
    .clear      = Armv7ResetAllCnt,
    .setPeriod  = Armv7SetEventPeriod,
    .readCnt    = Armv7ReadEventCnt,
    .mapEvent   = Armv7PmuMapEvent,
};

/**
 * @brief 初始化 ARMv7 硬件性能监控单元。
 *
 * 该函数会为每个 CPU 核心创建 PMU 中断处理函数，并调用性能监控硬件初始化函数。
 *
 * @return UINT32 初始化结果，LOS_OK 表示成功，其他值表示失败。
 */
UINT32 OsHwPmuInit(VOID)
{
    UINT32 ret; // 定义用于存储返回值的变量
    UINT32 index; // 定义循环索引变量
    for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) { // 遍历所有 CPU 核心
        ret = LOS_HwiCreate(g_pmuIrqNr[index], 0, 0, Armv7PmuIrqHandler, 0); // 为每个 CPU 核心创建 PMU 中断处理函数
        if (ret != LOS_OK) { // 若创建失败
            PRINT_ERR("pmu %u irq handler register failed\n", g_pmuIrqNr[index]); // 打印错误信息并返回错误码
            return ret;
        }
#ifdef LOSCFG_KERNEL_SMP
        HalIrqSetAffinity(g_pmuIrqNr[index], CPUID_TO_AFFI_MASK(index)); // 若为多核系统，设置中断亲和性
#endif
    }
    ret = OsPerfHwInit(&g_armv7Pmu); // 调用性能监控硬件初始化函数
    return ret; // 返回初始化结果
}
