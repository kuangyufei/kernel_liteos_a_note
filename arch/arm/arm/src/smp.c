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

#include "smp.h" // 包含对称多处理相关的头文件
#include "arch_config.h" // 包含架构配置相关的头文件
#include "los_base.h" // 包含操作系统基础功能的头文件
#include "los_hw.h" // 包含硬件相关的头文件
#include "los_atomic.h" // 包含原子操作相关的头文件
#include "los_arch_mmu.h" // 包含架构相关的内存管理单元头文件
#include "gic_common.h" // 包含通用中断控制器相关的头文件
#include "los_task_pri.h" // 包含任务优先级相关的头文件

#ifdef LOSCFG_KERNEL_SMP // 如果定义了内核对称多处理配置宏

extern VOID reset_vector(VOID); // 声明外部函数 reset_vector，该函数在其他文件中定义

// 定义 CPU 初始化结构体，用于存储从核启动相关信息
struct OsCpuInit {
    ArchCpuStartFunc cpuStart; // 从核启动函数指针
    VOID *arg; // 传递给从核启动函数的参数
    Atomic initFlag; // 原子类型的初始化标志，用于指示从核是否初始化完成
};

// 静态数组，存储每个从核的初始化信息，数组大小为核心数减 1（排除主核）
STATIC struct OsCpuInit g_cpuInit[CORE_NUM - 1] = {0};

/**
 * @brief 启动指定编号的从核
 * 
 * 该函数用于初始化从核的启动信息，并调用特定的 SMP 操作启动从核。
 * 等待从核初始化完成后返回。
 * 
 * @param cpuNum 要启动的从核编号
 * @param func 从核启动后要执行的函数
 * @param ops 包含 SMP 操作函数的结构体指针
 * @param arg 传递给从核启动函数的参数
 */
VOID HalArchCpuOn(UINT32 cpuNum, ArchCpuStartFunc func, struct SmpOps *ops, VOID *arg)
{
    struct OsCpuInit *cpuInit = &g_cpuInit[cpuNum - 1]; // 获取对应从核的初始化信息结构体指针
    // 计算从核启动入口地址，通过重置向量地址进行偏移计算
    UINTPTR startEntry = (UINTPTR)&reset_vector - KERNEL_VMM_BASE + SYS_MEM_BASE;
    INT32 ret; // 存储启动操作的返回值

    cpuInit->cpuStart = func; // 设置从核启动函数
    cpuInit->arg = arg; // 设置传递给从核启动函数的参数
    cpuInit->initFlag = 0; // 初始化标志置为 0，表示从核未初始化完成

    // 刷新数据缓存，确保从核能够获取到最新的初始化信息
    DCacheFlushRange((UINTPTR)cpuInit, (UINTPTR)cpuInit + sizeof(struct OsCpuInit));

    // 断言操作结构体指针不为空，若为空则触发断言错误
    LOS_ASSERT(ops != NULL);

    // 调用 SMP 操作的 CpuOn 函数启动从核
    ret = ops->SmpCpuOn(cpuNum, startEntry);
    if (ret < 0) { // 如果启动失败
        // 打印错误信息，包含从核编号和错误返回值
        PRINT_ERR("cpu start failed, cpu num: %u, ret: %d\n", cpuNum, ret);
        return; // 直接返回
    }

    // 循环等待从核初始化完成，使用 WFE 指令进入等待状态
    while (!LOS_AtomicRead(&cpuInit->initFlag)) {
        WFE;
    }
}

/**
 * @brief 从核启动后的入口函数
 * 
 * 该函数是从核启动后的初始执行函数，负责完成从核的初始化工作，
 * 包括设置当前任务、标记初始化完成、初始化 MMU、设置硬件 ID 映射、
 * 初始化中断控制器等，最后调用从核启动函数。
 */
VOID HalSecondaryCpuStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid(); // 获取当前从核的编号
    struct OsCpuInit *cpuInit = &g_cpuInit[cpuid - 1]; // 获取对应从核的初始化信息结构体指针

    
    OsCurrTaskSet(OsGetMainTask());// 设置当前任务为主任务

    
    LOS_AtomicSet(&cpuInit->initFlag, 1);// 将初始化标志置为 1，表示从核初始化完成
    SEV; // 发送事件信号，唤醒等待的主核

#ifdef LOSCFG_KERNEL_MMU // 如果定义了内核内存管理单元配置宏
    OsArchMmuInitPerCPU();// 初始化当前从核的内存管理单元
#endif

    
    CPU_MAP_SET(cpuid, OsHwIDGet());// 存储每个核心的硬件 ID 到 CPU 映射表中
    
    HalIrqInitPercpu();// 初始化当前从核的中断控制器

    
    cpuInit->cpuStart(cpuInit->arg);

    // 进入无限循环，使用 WFI 指令进入等待状态
    while (1) {
        WFI;
    }
}
#endif

