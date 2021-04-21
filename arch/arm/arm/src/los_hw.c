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
#include "los_task_pri.h"


/* support cpu vendors */
CpuVendor g_cpuTable[] = {	//支持的CPU供应商
    /* armv7-a */
    { 0xc07, "Cortex-A7" },
    { 0xc09, "Cortex-A9" },
    { 0, NULL }
};

/* logical cpu mapping */	//逻辑层cpu映射
UINT64 g_cpuMap[LOSCFG_KERNEL_CORE_NUM] = {
    [0 ... LOSCFG_KERNEL_CORE_NUM - 1] = (UINT64)(-1) //统一赋值,这个赋值方式还挺别致的.
};

/* bit[30] is enable FPU */
#define FP_EN (1U << 30)
LITE_OS_SEC_TEXT_INIT VOID OsTaskExit(VOID)
{// swi {cond} <immed_24>
    __asm__ __volatile__("swi  0");//处理器产生软中断异常，swi指令的低24位存放的是0
}//该指令可以产生SWI异常，ARM通过该指令从用户模式切到SVC模式.

#ifdef LOSCFG_GDB
STATIC VOID OsTaskEntrySetupLoopFrame(UINT32) __attribute__((noinline, naked));
VOID OsTaskEntrySetupLoopFrame(UINT32 arg0)
{
    asm volatile("\tsub fp, sp, #0x4\n"
                 "\tpush {fp, lr}\n"
                 "\tadd fp, sp, #0x4\n"
                 "\tpush {fp, lr}\n"

                 "\tadd fp, sp, #0x4\n"
                 "\tbl OsTaskEntry\n"

                 "\tpop {fp, lr}\n"
                 "\tpop {fp, pc}\n");
}
#endif
//内核态任务运行栈初始化
LITE_OS_SEC_TEXT_INIT VOID *OsTaskStackInit(UINT32 taskID, UINT32 stackSize, VOID *topStack, BOOL initFlag)
{
    UINT32 index = 1;
    TaskContext *taskContext = NULL;

    if (initFlag == TRUE) {
        OsStackInit(topStack, stackSize);
    }
    taskContext = (TaskContext *)(((UINTPTR)topStack + stackSize) - sizeof(TaskContext));//上下文存放在栈的底部

    /* initialize the task context */ //初始化任务上下文
#ifdef LOSCFG_GDB
    taskContext->PC = (UINTPTR)OsTaskEntrySetupLoopFrame;
#else
    taskContext->PC = (UINTPTR)OsTaskEntry;//内核态任务有统一的入口地址.
#endif
    taskContext->LR = (UINTPTR)OsTaskExit;  /* LR should be kept, to distinguish it's THUMB or ARM instruction */
    taskContext->resved = 0x0;
    taskContext->R[0] = taskID;             /* R0 *///因为所有内核态的任务处理函数入口都是一样的,
    //OsTaskEntry(UINT32 taskID)需要传递任务ID作为参数来检索调用哪个任务的处理函数.
    taskContext->R[index++] = 0x01010101;   /* R1, 0x01010101 : reg initialed magic word */ //0x55
    for (; index < GEN_REGS_NUM; index++) {//R2 - R12的初始化很有意思,为什么要这么做？
        taskContext->R[index] = taskContext->R[index - 1] + taskContext->R[1]; /* R2 - R12 */
    }//R[2]=R[2]<<1=0xAA

#ifdef LOSCFG_INTERWORK_THUMB // 16位模式
    taskContext->regPSR = PSR_MODE_SVC_THUMB; /* CPSR (Enable IRQ and FIQ interrupts, THUMNB-mode) */
#else //用于设置CPSR寄存器
    taskContext->regPSR = PSR_MODE_SVC_ARM;   /* CPSR (Enable IRQ and FIQ interrupts, ARM-mode) */
#endif

#if !defined(LOSCFG_ARCH_FPU_DISABLE)
    /* 0xAAA0000000000000LL : float reg initialed magic word */
    for (index = 0; index < FP_REGS_NUM; index++) {
        taskContext->D[index] = 0xAAA0000000000000LL + index; /* D0 - D31 */
    }
    taskContext->regFPSCR = 0;
    taskContext->regFPEXC = FP_EN;
#endif

    return (VOID *)taskContext;
}
//把父任务上下文克隆给子任务
LITE_OS_SEC_TEXT VOID OsUserCloneParentStack(LosTaskCB *childTaskCB, LosTaskCB *parentTaskCB)
{
    TaskContext *context = (TaskContext *)childTaskCB->stackPointer;
    VOID *cloneStack = (VOID *)(((UINTPTR)parentTaskCB->topOfStack + parentTaskCB->stackSize) - sizeof(TaskContext));
	//cloneStack指向 TaskContext
    LOS_ASSERT(parentTaskCB->taskStatus & OS_TASK_STATUS_RUNNING);//当前任务一定是正在运行的task

    (VOID)memcpy_s(childTaskCB->stackPointer, sizeof(TaskContext), cloneStack, sizeof(TaskContext));//直接把任务上下文拷贝了一份
    context->R[0] = 0;//R0寄存器为0,这个很重要, pid = fork()  pid == 0 是子进程返回.
}
/*
用户态运行栈初始化,此时上下文还在内核区

*/
LITE_OS_SEC_TEXT_INIT VOID OsUserTaskStackInit(TaskContext *context, TSK_ENTRY_FUNC taskEntry, UINTPTR stack)
{
    LOS_ASSERT(context != NULL);

#ifdef LOSCFG_INTERWORK_THUMB
    context->regPSR = PSR_MODE_USR_THUMB;
#else
    context->regPSR = PSR_MODE_USR_ARM;//工作模式:用户模式 + 工作状态:arm
#endif
    context->R[0] = stack;//将用户态栈指针保存到上下文R0处
    context->SP = TRUNCATE(stack, LOSCFG_STACK_POINT_ALIGN_SIZE);//改变 上下文的SP值,指向用户栈空间
    context->LR = 0;//保存子程序返回地址 例如 a call b ,在b中保存 a地址
    context->PC = (UINTPTR)taskEntry;//入口函数,由外部传入,由上层应用指定,固然每个都不一样.
}

DEPRECATED VOID Dmb(VOID)
{
    __asm__ __volatile__ ("dmb" : : : "memory");
}

DEPRECATED VOID Dsb(VOID)
{
    __asm__ __volatile__("dsb" : : : "memory");
}

DEPRECATED VOID Isb(VOID)
{
    __asm__ __volatile__("isb" : : : "memory");
}

VOID FlushICache(VOID)
{
    /*
     * Use ICIALLUIS instead of ICIALLU. ICIALLUIS operates on all processors in the Inner
     * shareable domain of the processor that performs the operation.
     */
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c1, 0" : : "r" (0) : "memory");
}

VOID DCacheFlushRange(UINT32 start, UINT32 end)
{
    arm_clean_cache_range(start, end);
}

VOID DCacheInvRange(UINT32 start, UINT32 end)
{
    arm_inv_cache_range(start, end);
}

