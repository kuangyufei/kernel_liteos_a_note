/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include "los_task_pri.h"

/* CPU厂商支持表，定义ARM架构下支持的处理器型号 */
CpuVendor g_cpuTable[] = {
    /* armv7-a架构处理器 */
    { 0xc07, "Cortex-A7" },   // CPU ID 0xc07 (十进制3079) 对应 Cortex-A7 处理器
    { 0xc09, "Cortex-A9" },   // CPU ID 0xc09 (十进制3081) 对应 Cortex-A9 处理器
    { 0, NULL }               // 表结束标志 (ID为0表示无效)
};

/* 逻辑CPU映射表，用于多核心系统中逻辑CPU到物理CPU的映射 */
UINT64 g_cpuMap[LOSCFG_KERNEL_CORE_NUM] = {
    [0 ... LOSCFG_KERNEL_CORE_NUM - 1] = (UINT64)(-1)  // 初始化为无效值 (十进制 -1，按无符号解析为最大64位值)
};

/* FPU使能宏定义，bit[30]为FPU使能控制位 */
#define FP_EN (1U << 30)  // FPU使能标志位 (二进制1000000000000000000000000000000，十进制1073741824)

/**
 * @brief 任务退出函数
 * @details 通过软件中断(SWI)触发系统调用，实现任务的正常退出
 */
LITE_OS_SEC_TEXT_INIT VOID OsTaskExit(VOID)
{
    __asm__ __volatile__("swi  0");  // 执行软件中断指令，参数0表示任务退出系统调用
}

#ifdef LOSCFG_GDB  // 如果启用GDB调试功能 (LOSCFG_GDB为调试配置开关)
/**
 * @brief GDB调试模式下的任务入口栈帧设置
 * @param arg0 任务参数
 * @details 为GDB调试构建特殊的栈帧结构，支持断点调试和函数调用跟踪
 * @note 使用naked属性禁止编译器生成函数序言/尾声代码，手动控制栈帧
 */
STATIC VOID OsTaskEntrySetupLoopFrame(UINT32) __attribute__((noinline, naked));
VOID OsTaskEntrySetupLoopFrame(UINT32 arg0)
{
    asm volatile(
        "	sub fp, sp, #0x4\n"        // 调整栈指针，为帧指针预留4字节空间
        "	push {fp, lr}\n"           // 保存旧的帧指针和链接寄存器到栈
        "	add fp, sp, #0x4\n"        // 设置新的帧指针，指向当前栈顶
        "	push {fp, lr}\n"           // 再次压栈fp和lr，构建GDB识别的栈帧结构
        "\n"
        "	add fp, sp, #0x4\n"        // 调整帧指针位置
        "	bl OsTaskEntry\n"          // 跳转到任务入口函数OsTaskEntry
        "\n"
        "	pop {fp, lr}\n"            // 恢复fp和lr寄存器
        "	pop {fp, pc}\n"            // 恢复栈并返回
    );
}
#endif

/**
 * @brief 任务栈初始化函数
 * @param taskID 任务ID
 * @param stackSize 栈大小 (字节)
 * @param topStack 栈顶指针
 * @param initFlag 是否初始化栈内存 (TRUE:初始化 FALSE:不初始化)
 * @return 初始化后的任务上下文指针
 * @details 负责初始化任务栈结构和CPU上下文，包括通用寄存器、状态寄存器和FPU寄存器
 */
LITE_OS_SEC_TEXT_INIT VOID *OsTaskStackInit(UINT32 taskID, UINT32 stackSize, VOID *topStack, BOOL initFlag)
{
    if (initFlag == TRUE) {
        OsStackInit(topStack, stackSize);  // 初始化栈内存 (填充栈保护值和对齐处理)
    }
    // 计算任务上下文位置：栈顶 + 栈大小 - 上下文结构体大小 (向下生长栈)
    TaskContext *taskContext = (TaskContext *)(((UINTPTR)topStack + stackSize) - sizeof(TaskContext));

    /* 初始化任务上下文寄存器 */
#ifdef LOSCFG_GDB
    taskContext->PC = (UINTPTR)OsTaskEntrySetupLoopFrame;  // GDB模式下使用调试入口
#else
    taskContext->PC = (UINTPTR)OsTaskEntry;  // 正常模式下直接指向任务入口函数
#endif
    taskContext->LR = (UINTPTR)OsTaskExit;  /* LR寄存器初始化为任务退出函数，用于区分THUMB/ARM指令集 */
    taskContext->R0 = taskID;               /* R0寄存器存储任务ID作为入口参数 */

#ifdef LOSCFG_THUMB
    // THUMB模式：启用IRQ和FIQ中断 (PSR_I=0, PSR_F=0)，工作在特权模式 (PSR_M=0x13)
    taskContext->regCPSR = PSR_MODE_SVC_THUMB;  // CPSR=0x60000013
#else
    // ARM模式：启用IRQ和FIQ中断 (PSR_I=0, PSR_F=0)，工作在特权模式 (PSR_M=0x13)
    taskContext->regCPSR = PSR_MODE_SVC_ARM;    // CPSR=0x00000013
#endif

#if !defined(LOSCFG_ARCH_FPU_DISABLE)  // 如果未禁用FPU功能
    /* 0xAAA0000000000000LL : 浮点寄存器初始化魔术字，用于调试识别 */
    for (UINT32 index = 0; index < FP_REGS_NUM; index++) {
        taskContext->D[index] = 0xAAA0000000000000LL + index;  /* 初始化D0-D31浮点寄存器 */
    }
    taskContext->regFPSCR = 0;  // 浮点状态控制寄存器初始化为0
    taskContext->regFPEXC = FP_EN;  // 启用FPU (设置FPEXC寄存器的EN位)
#endif

    return (VOID *)taskContext;  // 返回初始化好的任务上下文指针
}

/**
 * @brief 用户任务克隆父进程栈
 * @param childStack 子进程栈指针
 * @param sp 用户栈指针 (0表示使用父进程栈顶)
 * @param parentTopOfStack 父进程栈顶
 * @param parentStackSize 父进程栈大小
 * @details 复制父进程的任务上下文到子进程栈，用于实现fork系统调用
 */
VOID OsUserCloneParentStack(VOID *childStack, UINTPTR sp, UINTPTR parentTopOfStack, UINT32 parentStackSize)
{
    LosTaskCB *task = OsCurrTaskGet();  // 获取当前任务控制块
    sig_cb *sigcb = &task->sig;         // 获取信号控制块
    VOID *cloneStack = NULL;            // 克隆的栈指针

    if (sigcb->sigContext != NULL) {  // 如果存在信号上下文 (任务被信号中断)
        // 信号上下文地址 - 任务上下文大小 = 实际任务上下文起始地址
        cloneStack = (VOID *)((UINTPTR)sigcb->sigContext - sizeof(TaskContext));
    } else {
        // 计算父进程任务上下文地址：栈顶 + 栈大小 - 上下文结构体大小
        cloneStack = (VOID *)(((UINTPTR)parentTopOfStack + parentStackSize) - sizeof(TaskContext));
    }

    // 复制父进程上下文到子进程栈 (大小为TaskContext结构体大小)
    (VOID)memcpy_s(childStack, sizeof(TaskContext), cloneStack, sizeof(TaskContext));
    ((TaskContext *)childStack)->R0 = 0;  // 子进程fork返回值为0
    if (sp != 0) {  // 如果指定了用户栈指针
        // 设置用户栈指针并按栈对齐要求截断 (通常为8字节或16字节对齐)
        ((TaskContext *)childStack)->USP = TRUNCATE(sp, LOSCFG_STACK_POINT_ALIGN_SIZE);
        ((TaskContext *)childStack)->ULR = 0;  // 用户模式链接寄存器清零
    }
}

/**
 * @brief 用户模式任务栈初始化
 * @param context 任务上下文指针
 * @param taskEntry 用户任务入口地址
 * @param stack 用户栈指针
 * @details 初始化用户模式任务的上下文，设置用户模式寄存器和栈指针
 */
LITE_OS_SEC_TEXT_INIT VOID OsUserTaskStackInit(TaskContext *context, UINTPTR taskEntry, UINTPTR stack)
{
    LOS_ASSERT(context != NULL);  // 断言：上下文指针不能为空

#ifdef LOSCFG_THUMB
    // 用户模式THUMB模式：CPSR=0x60000010 (PSR_I=0, PSR_F=0, PSR_M=0x10)
    context->regCPSR = PSR_MODE_USR_THUMB;
#else
    // 用户模式ARM模式：CPSR=0x00000010 (PSR_I=0, PSR_F=0, PSR_M=0x10)
    context->regCPSR = PSR_MODE_USR_ARM;
#endif
    context->R0 = stack;  // R0寄存器存储用户栈指针作为入口参数
    // 设置用户栈指针并按栈对齐要求截断 (通常为8字节或16字节对齐)
    context->USP = TRUNCATE(stack, LOSCFG_STACK_POINT_ALIGN_SIZE);
    context->ULR = 0;     // 用户模式链接寄存器初始化为0
    context->PC = (UINTPTR)taskEntry;  // PC寄存器设置为用户任务入口地址
}

/**
 * @brief 初始化信号处理上下文
 * @param sp 原始栈指针
 * @param signalContext 信号上下文指针
 * @param sigHandler 信号处理函数地址
 * @param signo 信号编号
 * @param param 信号参数
 * @details 复制原始上下文并修改PC为信号处理函数，设置信号编号和参数
 */
VOID OsInitSignalContext(const VOID *sp, VOID *signalContext, UINTPTR sigHandler, UINT32 signo, UINT32 param)
{
    IrqContext *newSp = (IrqContext *)signalContext;  // 转换为中断上下文结构体
    // 复制原始栈上下文到信号上下文 (保存被中断时的寄存器状态)
    (VOID)memcpy_s(signalContext, sizeof(IrqContext), sp, sizeof(IrqContext));
    newSp->PC = sigHandler;  // 设置PC为信号处理函数地址
    newSp->R0 = signo;       // R0存储信号编号
    newSp->R1 = param;       // R1存储信号参数
}

/**
 * @brief 数据内存屏障 (已弃用)
 * @details 确保内存操作的顺序性，已被新的内存屏障API取代
 * @deprecated 使用ArchDataMemoryBarrier()替代
 */
DEPRECATED VOID Dmb(VOID)
{
    __asm__ __volatile__ ("dmb" : : : "memory");  // 执行dmb指令，确保数据内存访问顺序
}

/**
 * @brief 数据同步屏障 (已弃用)
 * @details 确保所有内存操作完成后再执行后续指令，已被新的内存屏障API取代
 * @deprecated 使用ArchDataSyncBarrier()替代
 */
DEPRECATED VOID Dsb(VOID)
{
    __asm__ __volatile__("dsb" : : : "memory");  // 执行dsb指令，等待所有数据访问完成
}

/**
 * @brief 指令同步屏障 (已弃用)
 * @details 刷新指令流水线，确保指令按顺序执行，已被新的内存屏障API取代
 * @deprecated 使用ArchInstructionSyncBarrier()替代
 */
DEPRECATED VOID Isb(VOID)
{
    __asm__ __volatile__("isb" : : : "memory");  // 执行isb指令，刷新指令缓存和流水线
}

/**
 * @brief 刷新指令缓存
 * @details  invalidate整个指令缓存，确保CPU获取最新指令
 * @note 使用ICIALLUIS操作代替ICIALLU，支持多核系统中的内层共享域
 */
VOID FlushICache(VOID)
{
    /*
     * 使用ICIALLUIS代替ICIALLU指令。ICIALLUIS操作作用于执行该操作的处理器的内层共享域中的所有处理器
     * MCR指令：将0写入CP15协处理器的c7寄存器，c1子寄存器，操作码0
     */
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c1, 0" : : "r" (0) : "memory");
}

/**
 * @brief 刷新数据缓存指定范围
 * @param start 起始地址
 * @param end 结束地址
 * @details 将指定地址范围内的数据缓存写入内存并使其失效
 */
VOID DCacheFlushRange(UINT32 start, UINT32 end)
{
    arm_clean_cache_range(start, end);  // 调用ARM架构缓存清理函数
}

/**
 * @brief 使数据缓存指定范围失效
 * @param start 起始地址
 * @param end 结束地址
 * @details 使指定地址范围内的数据缓存条目失效，强制从内存重新加载数据
 */
VOID DCacheInvRange(UINT32 start, UINT32 end)
{
    arm_inv_cache_range(start, end);  // 调用ARM架构缓存失效函数
}
