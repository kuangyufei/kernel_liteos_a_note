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

#include "los_config.h"
#include "los_printf.h"
#include "los_atomic.h"
#include "los_process_pri.h"
#include "los_task_pri.h"
#include "los_swtmr_pri.h"
#include "los_sched_pri.h"
#include "los_arch_mmu.h"
#include "gic_common.h"

#if (LOSCFG_KERNEL_SMP == YES)
STATIC Atomic g_ncpu = 1;	//统计CPU的数量
#endif
//系统信息
LITE_OS_SEC_TEXT_INIT VOID OsSystemInfo(VOID)
{
#ifdef LOSCFG_DEBUG_VERSION
    const CHAR *buildType = "debug";
#else
    const CHAR *buildType = "release";
#endif /* LOSCFG_DEBUG_VERSION */

    PRINT_RELEASE("\n******************Welcome******************\n\n"
            "Processor   : %s"
#if (LOSCFG_KERNEL_SMP == YES)
            " * %d\n"
            "Run Mode    : SMP\n" 	//对称多处理（Symmetric multiprocessing，SMP）
#else
            "\n"
            "Run Mode    : UP\n"	//单核处理器( unit processing,UP) 
#endif
            "GIC Rev     : %s\n"
            "build time  : %s %s\n"
            "Kernel      : %s %d.%d.%d.%d/%s\n"
            "\n*******************************************\n",
            LOS_CpuInfo(),
#if (LOSCFG_KERNEL_SMP == YES)
            LOSCFG_KERNEL_SMP_CORE_NUM,
#endif
            HalIrqVersion(), __DATE__, __TIME__,\
            KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, buildType);
}
//次级CPU初始化,本函数执行的次数由次级CPU的个数决定. 
//例如:在四核情况下,会被执行3次, 0号通常被定义为主CPU 执行main
LITE_OS_SEC_TEXT_INIT VOID secondary_cpu_start(VOID)
{
#if (LOSCFG_KERNEL_SMP == YES)//支持多cpu核开关
    UINT32 cpuid = ArchCurrCpuid();//获取当前cpu id

#ifdef LOSCFG_KERNEL_MMU
    OsArchMmuInitPerCPU();//每个CPU都需要初始化MMU
#endif

    OsCurrTaskSet(OsGetMainTask());//设置CPU的当前任务

    /* increase cpu counter */
    LOS_AtomicInc(&g_ncpu);	//统计CPU的数量

    /* store each core's hwid */
    CPU_MAP_SET(cpuid, OsHwIDGet());//存储每个CPU的 hwid
    HalIrqInitPercpu(); //CPU硬件中断初始化

    OsCurrProcessSet(OS_PCB_FROM_PID(OsGetKernelInitProcessID())); //设置内核进程为CPU进程
    OsSwtmrInit();		//定时任务初始化,每个CPU维护自己的定时器队列
    OsIdleTaskCreate();	//创建空闲任务,每个CPU维护自己的任务队列
    OsSchedStart();
    while (1) {
        __asm volatile("wfi");//wait for Interrupt 等待中断，即下一次中断发生前都在此呆着不干活
    }//类似的还有 WFE: wait for Events 等待事件，即下一次事件发生前都在此呆着不干活
#endif
}

#if (LOSCFG_KERNEL_SMP == YES)
#ifdef LOSCFG_TEE_ENABLE //需使能,可信环境(Trusted Execute Environment)
#define TSP_CPU_ON  0xb2000011UL
STATIC INT32 raw_smc_send(UINT32 cmd)
{
    register UINT32 smc_id asm("r0") = cmd;
    do {
        asm volatile (
                "mov r0, %[a0]\n"
                "smc #0\n"
                : [a0] "+r"(smc_id)
                );
    } while (0);

    return (INT32)smc_id;
}
//触发次级CPU工作
STATIC VOID trigger_secondary_cpu(VOID)
{
    (VOID)raw_smc_send(TSP_CPU_ON);
}
//调动次级CPU干活
LITE_OS_SEC_TEXT_INIT VOID release_secondary_cores(VOID)
{
    trigger_secondary_cpu();//触发次级CPU工作
    /* wait until all APs are ready */
    while (LOS_AtomicRead(&g_ncpu) < LOSCFG_KERNEL_CORE_NUM) {
        asm volatile("wfe");//WFE: wait for Events 等待事件，即下一次事件发生前都在此hold住不干活
    }
}
#else
#define CLEAR_RESET_REG_STATUS(regval) (regval) &= ~(1U << 2)
LITE_OS_SEC_TEXT_INIT VOID release_secondary_cores(VOID)//调动次级CPU干活
{
    UINT32 regval;

    /* clear the second cpu reset status */
    READ_UINT32(regval, PERI_CRG30_BASE);
    CLEAR_RESET_REG_STATUS(regval); //清除,并重置 从cpu寄存器状态
    WRITE_UINT32(regval, PERI_CRG30_BASE);

    /* wait until all APs are ready */
    while (LOS_AtomicRead(&g_ncpu) < LOSCFG_KERNEL_CORE_NUM) {
        asm volatile("wfe");//WFE: wait for Events 等待事件，即下一次事件发生前都在此hold住不干活
    }
}
#endif /* LOSCFG_TEE_ENABLE */
#endif /* LOSCFG_KERNEL_SMP */
/******************************************************************************
内核入口函数,由汇编调用,见于reset_vector_up.S 和 reset_vector_mp.S
up指单核CPU, mp指多核CPU bl        main
******************************************************************************/
LITE_OS_SEC_TEXT_INIT INT32 main(VOID)//由主CPU执行,默认0号CPU 为主CPU 
{
    UINT32 uwRet = LOS_OK;

    OsSetMainTask();// 设置各CPU主任务
    OsCurrTaskSet(OsGetMainTask());//设置当前CPU主任务

    /* set system counter freq */
#ifndef LOSCFG_TEE_ENABLE
    HalClockFreqWrite(OS_SYS_CLOCK); //多CPU情况下设置时钟频率
#endif

    /* system and chip info */
    OsSystemInfo();//输出系统和芯片的信息

    PRINT_RELEASE("\nmain core booting up...\n"); //控制台输出 主内核启动中...

    uwRet = OsMain();// 内核各模块初始化
    if (uwRet != LOS_OK) {
        return LOS_NOK;
    }

#if (LOSCFG_KERNEL_SMP == YES)//多核支持
    PRINT_RELEASE("releasing %u secondary cores\n", LOSCFG_KERNEL_SMP_CORE_NUM - 1);
    release_secondary_cores();//让CPU其他核也开始工作,真正的并行开始了.
#endif

    CPU_MAP_SET(0, OsHwIDGet());//设置CPU映射,参数0 代表0号CPU

    OsSchedStart();

    while (1) {
        __asm volatile("wfi");//WFI: wait for Interrupt 等待中断，即下一次中断发生前都在此hold住不干活
    }
}
