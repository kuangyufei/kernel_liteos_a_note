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

#include "los_hw_pri.h"
#include "los_tick_pri.h"
#include "los_sys_pri.h"
#include "gic_common.h"

#define STRING_COMB(x, y, z)        x ## y ## z  // 定义字符串拼接宏，使用 ## 操作符将三个参数连接成一个标识符

#ifdef LOSCFG_ARCH_SECURE_MONITOR_MODE
#define TIMER_REG(reg)              STRING_COMB(TIMER_REG_, CNTPS, reg)  // 若定义了安全监控模式，使用 CNTPS 作为寄存器前缀
#else
#define TIMER_REG(reg)              STRING_COMB(TIMER_REG_, CNTP, reg)  // 若未定义安全监控模式，使用 CNTP 作为寄存器前缀
#endif

#define TIMER_REG_CTL               TIMER_REG(_CTL)     /* 32 bits */  // 定义定时器控制寄存器，32 位，用于控制定时器的操作
#define TIMER_REG_TVAL              TIMER_REG(_TVAL)    /* 32 bits */  // 定义定时器初始值寄存器，32 位，用于设置定时器的初始计数值
#define TIMER_REG_CVAL              TIMER_REG(_CVAL)    /* 64 bits */  // 定义定时器比较值寄存器，64 位，用于设置定时器的比较值
#define TIMER_REG_CT                TIMER_REG(CT)       /* 64 bits */  // 定义定时器计数器寄存器，64 位，用于记录定时器的当前计数值

#ifdef __LP64__
#define TIMER_REG_CNTFRQ            cntfrq_el0  // 定义系统计数器频率寄存器，对应 AArch64 架构的 cntfrq_el0 寄存器

/* CNTP AArch64 registers */
#define TIMER_REG_CNTP_CTL          cntp_ctl_el0  // 定义 CNTP 定时器控制寄存器，对应 AArch64 架构的 cntp_ctl_el0 寄存器
#define TIMER_REG_CNTP_TVAL         cntp_tval_el0  // 定义 CNTP 定时器初始值寄存器，对应 AArch64 架构的 cntp_tval_el0 寄存器
#define TIMER_REG_CNTP_CVAL         cntp_cval_el0  // 定义 CNTP 定时器比较值寄存器，对应 AArch64 架构的 cntp_cval_el0 寄存器
#define TIMER_REG_CNTPCT            cntpct_el0  // 定义 CNTP 定时器计数器寄存器，对应 AArch64 架构的 cntpct_el0 寄存器

/* CNTPS AArch64 registers */
#define TIMER_REG_CNTPS_CTL         cntps_ctl_el1  // 定义 CNTPS 定时器控制寄存器，对应 AArch64 架构的 cntps_ctl_el1 寄存器
#define TIMER_REG_CNTPS_TVAL        cntps_tval_el1  // 定义 CNTPS 定时器初始值寄存器，对应 AArch64 架构的 cntps_tval_el1 寄存器
#define TIMER_REG_CNTPS_CVAL        cntps_cval_el1  // 定义 CNTPS 定时器比较值寄存器，对应 AArch64 架构的 cntps_cval_el1 寄存器
#define TIMER_REG_CNTPSCT           cntpct_el0  // 定义 CNTPS 定时器计数器寄存器，对应 AArch64 架构的 cntpct_el0 寄存器

#define READ_TIMER_REG32(reg)       AARCH64_SYSREG_READ(reg)  // 定义读取 32 位定时器寄存器的宏，调用 AARCH64_SYSREG_READ 函数
#define READ_TIMER_REG64(reg)       AARCH64_SYSREG_READ(reg)  // 定义读取 64 位定时器寄存器的宏，调用 AARCH64_SYSREG_READ 函数
#define WRITE_TIMER_REG32(reg, val) AARCH64_SYSREG_WRITE(reg, (UINT64)(val))  // 定义写入 32 位定时器寄存器的宏，将 32 位值转换为 64 位后调用 AARCH64_SYSREG_WRITE 函数
#define WRITE_TIMER_REG64(reg, val) AARCH64_SYSREG_WRITE(reg, val)  // 定义写入 64 位定时器寄存器的宏，调用 AARCH64_SYSREG_WRITE 函数

#else /* Aarch32 */
#define TIMER_REG_CNTFRQ            CP15_REG(c14, 0, c0, 0)  // 定义系统计数器频率寄存器，对应 AArch32 架构的 CP15 寄存器 c14, 0, c0, 0

/* CNTP AArch32 registers */
#define TIMER_REG_CNTP_CTL          CP15_REG(c14, 0, c2, 1)  // 定义 CNTP 定时器控制寄存器，对应 AArch32 架构的 CP15 寄存器 c14, 0, c2, 1
#define TIMER_REG_CNTP_TVAL         CP15_REG(c14, 0, c2, 0)  // 定义 CNTP 定时器初始值寄存器，对应 AArch32 架构的 CP15 寄存器 c14, 0, c2, 0
#define TIMER_REG_CNTP_CVAL         CP15_REG64(c14, 2)  // 定义 CNTP 定时器比较值寄存器，对应 AArch32 架构的 64 位 CP15 寄存器 c14, 2
#define TIMER_REG_CNTPCT            CP15_REG64(c14, 0)  // 定义 CNTP 定时器计数器寄存器，对应 AArch32 架构的 64 位 CP15 寄存器 c14, 0

/* CNTPS AArch32 registers are banked and accessed though CNTP */
#define CNTPS CNTP  // 在 AArch32 架构下，CNTPS 寄存器通过 CNTP 访问，因此将 CNTPS 定义为 CNTP

#define READ_TIMER_REG32(reg)       ARM_SYSREG_READ(reg)  // 定义读取 32 位定时器寄存器的宏，调用 ARM_SYSREG_READ 函数
#define READ_TIMER_REG64(reg)       ARM_SYSREG64_READ(reg)  // 定义读取 64 位定时器寄存器的宏，调用 ARM_SYSREG64_READ 函数
#define WRITE_TIMER_REG32(reg, val) ARM_SYSREG_WRITE(reg, val)  // 定义写入 32 位定时器寄存器的宏，调用 ARM_SYSREG_WRITE 函数
#define WRITE_TIMER_REG64(reg, val) ARM_SYSREG64_WRITE(reg, val)  // 定义写入 64 位定时器寄存器的宏，调用 ARM_SYSREG64_WRITE 函数

#endif
/* 
* 见于 << arm 架构参考手册>> B4.1.21 处 CNTFRQ寄存器表示系统计数器的时钟频率。
* 这个寄存器是一个通用的计时器寄存器。
* MRC p15, 0, <Rt>, c14, c0, 0 ; Read CNTFRQ into Rt
* MCR p15, 0, <Rt>, c14, c0, 0 ; Write Rt to CNTFRQ
*/
/**
 * @brief 读取系统时钟频率。
 *
 * 该函数通过调用 READ_TIMER_REG32 宏，从定时器寄存器 TIMER_REG_CNTFRQ 中读取 32 位的系统时钟频率值。
 *
 * @return UINT32 读取到的系统时钟频率值。
 */
UINT32 HalClockFreqRead(VOID)
{
    return READ_TIMER_REG32(TIMER_REG_CNTFRQ);
}

/**
 * @brief 写入系统时钟频率。
 *
 * 该函数通过调用 WRITE_TIMER_REG32 宏，将传入的频率值写入到定时器寄存器 TIMER_REG_CNTFRQ 中。
 *
 * @param freq 要写入的系统时钟频率值。
 */
VOID HalClockFreqWrite(UINT32 freq)
{
    WRITE_TIMER_REG32(TIMER_REG_CNTFRQ, freq);
}

/**
 * @brief 向定时器控制寄存器写入控制值。
 *
 * 该内联函数通过调用 WRITE_TIMER_REG32 宏，将传入的控制值写入到定时器控制寄存器 TIMER_REG_CTL 中。
 *
 * @param cntpCtl 要写入定时器控制寄存器的控制值。
 */
STATIC_INLINE VOID TimerCtlWrite(UINT32 cntpCtl)
{
    WRITE_TIMER_REG32(TIMER_REG_CTL, cntpCtl);
}

/**
 * @brief 读取定时器比较值寄存器的值。
 *
 * 该内联函数通过调用 READ_TIMER_REG64 宏，从定时器比较值寄存器 TIMER_REG_CVAL 中读取 64 位的比较值。
 *
 * @return UINT64 读取到的定时器比较值。
 */
STATIC_INLINE UINT64 TimerCvalRead(VOID)
{
    return READ_TIMER_REG64(TIMER_REG_CVAL);
}

/**
 * @brief 向定时器比较值寄存器写入比较值。
 *
 * 该内联函数通过调用 WRITE_TIMER_REG64 宏，将传入的比较值写入到定时器比较值寄存器 TIMER_REG_CVAL 中。
 *
 * @param cval 要写入定时器比较值寄存器的比较值。
 */
STATIC_INLINE VOID TimerCvalWrite(UINT64 cval)
{
    WRITE_TIMER_REG64(TIMER_REG_CVAL, cval);
}

/**
 * @brief 向定时器初始值寄存器写入初始值。
 *
 * 该内联函数通过调用 WRITE_TIMER_REG32 宏，将传入的初始值写入到定时器初始值寄存器 TIMER_REG_TVAL 中。
 *
 * @param tval 要写入定时器初始值寄存器的初始值。
 */
STATIC_INLINE VOID TimerTvalWrite(UINT32 tval)
{
    WRITE_TIMER_REG32(TIMER_REG_TVAL, tval);
}

/**
 * @brief 获取当前定时器的计数值。
 *
 * 该函数通过调用 READ_TIMER_REG64 宏，从定时器计数器寄存器 TIMER_REG_CT 中读取 64 位的计数值。
 *
 * @return UINT64 当前定时器的计数值。
 */
UINT64 HalClockGetCycles(VOID)
{
    UINT64 cntpct;

    cntpct = READ_TIMER_REG64(TIMER_REG_CT);
    return cntpct;
}
//硬时钟初始化
LITE_OS_SEC_TEXT_INIT VOID HalClockInit(VOID)
{
    UINT32 ret;

    g_sysClock = HalClockFreqRead();//读取CPU的时钟频率
    ret = LOS_HwiCreate(OS_TICK_INT_NUM, MIN_INTERRUPT_PRIORITY, 0, OsTickHandler, 0);//创建硬中断定时器
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d create tick irq failed, ret:0x%x\n", __FUNCTION__, __LINE__, ret);
    }
}

// 启动硬件时钟，解除定时器中断屏蔽，初始化并启动定时器
LITE_OS_SEC_TEXT_INIT VOID HalClockStart(VOID)
{
    HalIrqUnmask(OS_TICK_INT_NUM);  // 解除对定时器中断的屏蔽，允许定时器中断触发

    /* 触发第一个时钟滴答 */
    TimerCtlWrite(0);  // 将定时器控制寄存器的值设置为 0，停止定时器
    TimerTvalWrite(OS_CYCLE_PER_TICK);  // 将定时器初始值寄存器设置为每个时钟滴答对应的时钟周期数
    TimerCtlWrite(1);  // 将定时器控制寄存器的值设置为 1，启动定时器
}

// 实现微秒级延时，根据系统时钟计算周期数并循环等待
VOID HalDelayUs(UINT32 usecs)
{
    UINT64 cycles = (UINT64)usecs * g_sysClock / OS_SYS_US_PER_SECOND;  // 计算指定微秒数对应的时钟周期数
    UINT64 deadline = HalClockGetCycles() + cycles;  // 计算延时结束时的目标时钟周期数

    while (HalClockGetCycles() < deadline) {  // 循环等待，直到当前时钟周期数达到目标周期数
        __asm__ volatile ("nop");  // 执行空操作，避免 CPU 空转消耗过多资源
    }
}

// 获取调度时钟的时间，该函数已弃用
DEPRECATED UINT64 hi_sched_clock(VOID)
{
    return LOS_CurrNanosec();  // 返回当前的纳秒数
}

// 获取定时器滴答剩余的时钟周期数
UINT32 HalClockGetTickTimerCycles(VOID)
{
    UINT64 cval = TimerCvalRead();  // 读取定时器比较值寄存器的值
    UINT64 cycles = HalClockGetCycles();  // 获取当前定时器的计数值

    return (UINT32)((cval > cycles) ? (cval - cycles) : 0);  // 计算并返回剩余周期数，若比较值小于当前计数值则返回 0
}

// 重新加载定时器滴答的时钟周期数
UINT64 HalClockTickTimerReload(UINT64 cycles)
{
    HalIrqMask(OS_TICK_INT_NUM);  // 屏蔽定时器中断
    HalIrqClear(OS_TICK_INT_NUM);  // 清除定时器中断标志

    TimerCtlWrite(0);  // 将定时器控制寄存器的值设置为 0，停止定时器
    TimerCvalWrite(HalClockGetCycles() + cycles);  // 将定时器比较值寄存器设置为当前计数值加上新的周期数
    TimerCtlWrite(1);  // 将定时器控制寄存器的值设置为 1，启动定时器

    HalIrqUnmask(OS_TICK_INT_NUM);  // 解除对定时器中断的屏蔽
    return cycles;  // 返回重新加载的周期数
}
