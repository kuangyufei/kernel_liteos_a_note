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
#include "stdio.h"
#include "string.h"
#include "gic_common.h"
#include "los_atomic.h"
#include "los_exc_pri.h"
#include "los_hwi_pri.h"
#include "los_hw_tick_pri.h"
#include "los_init_pri.h"
#include "los_memory_pri.h"
#include "los_mp.h"
#include "los_mux_pri.h"
#include "los_printf.h"
#include "los_process_pri.h"
#include "los_queue_pri.h"
#include "los_sem_pri.h"
#include "los_spinlock.h"
#include "los_swtmr_pri.h"
#include "los_task_pri.h"
#include "los_sched_pri.h"
#include "los_tick.h"
#include "los_vm_boot.h"
#include "los_smp.h"

STATIC SystemRebootFunc g_rebootHook = NULL;///< 系统重启钩子函数
/// 设置系统重启钩子函数
VOID OsSetRebootHook(SystemRebootFunc func)
{
    g_rebootHook = func;
}
///获取系统重启钩子函数
SystemRebootFunc OsGetRebootHook(VOID)
{
    return g_rebootHook;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 EarliestInit(VOID)
{
    /* Must be placed at the beginning of the boot process *///必须放在启动过程的开头
    OsSetMainTask();//为每个CPU核设置临时主任务
    OsCurrTaskSet(OsGetMainTask());//设置当前任务
    OsSchedRunqueueInit();//初始化调度队列

    g_sysClock = OS_SYS_CLOCK; //设置系统时钟
    g_tickPerSecond =  LOSCFG_BASE_CORE_TICK_PER_SECOND; // 设置TICK间隔

    return LOS_OK;
}
//硬件早期初始化
LITE_OS_SEC_TEXT_INIT STATIC UINT32 ArchEarlyInit(VOID)
{
    UINT32 ret;

    /* set system counter freq | 设置系统计数器频率*/
#ifndef LOSCFG_TEE_ENABLE
    HalClockFreqWrite(OS_SYS_CLOCK); //写寄存器 MCR p15, 0, <Rt>, c14, c0, 0 ; Write Rt to CNTFRQ
#endif

#ifdef LOSCFG_PLATFORM_HWI
    OsHwiInit(); // 硬中断初始化
#endif

    OsExcInit(); // 异常初始化

    ret = OsTickInit(g_sysClock, LOSCFG_BASE_CORE_TICK_PER_SECOND);
    if (ret != LOS_OK) {
        PRINT_ERR("OsTickInit error!\n");
        return ret;
    }

    return LOS_OK;
}
//平台早期初始化
LITE_OS_SEC_TEXT_INIT STATIC UINT32 PlatformEarlyInit(VOID)
{
#if defined(LOSCFG_PLATFORM_UART_WITHOUT_VFS) && defined(LOSCFG_DRIVERS)
    uart_init(); //初始化串口
#endif /* LOSCFG_PLATFORM_UART_WITHOUT_VFS */

    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsIpcInit(VOID)
{
    UINT32 ret;

#ifdef LOSCFG_BASE_IPC_SEM
    ret = OsSemInit();
    if (ret != LOS_OK) {
        PRINT_ERR("OsSemInit error\n");
        return ret;
    }
#endif

#ifdef LOSCFG_BASE_IPC_QUEUE
    ret = OsQueueInit();
    if (ret != LOS_OK) {
        PRINT_ERR("OsQueueInit error\n");
        return ret;
    }
#endif
    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 ArchInit(VOID)
{
#ifdef LOSCFG_KERNEL_MMU
    OsArchMmuInitPerCPU();
#endif
    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 PlatformInit(VOID)
{
    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 KModInit(VOID)
{
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    OsSwtmrInit();
#endif
    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT VOID OsSystemInfo(VOID)
{
#ifdef LOSCFG_DEBUG_VERSION
    const CHAR *buildType = "debug";
#else
    const CHAR *buildType = "release";
#endif /* LOSCFG_DEBUG_VERSION */

    PRINT_RELEASE("\n******************Welcome******************\n\n"
                  "Processor   : %s"
#ifdef LOSCFG_KERNEL_SMP
                  " * %d\n"
                  "Run Mode    : SMP\n"
#else
                  "\n"
                  "Run Mode    : UP\n"
#endif
                  "GIC Rev     : %s\n"
                  "build time  : %s %s\n"
                  "Kernel      : %s %d.%d.%d.%d/%s\n"
                  "\n*******************************************\n",
                  LOS_CpuInfo(),
#ifdef LOSCFG_KERNEL_SMP
                  LOSCFG_KERNEL_SMP_CORE_NUM,
#endif
                  HalIrqVersion(), __DATE__, __TIME__,\
                  KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, buildType);
}
///由汇编调用,鸿蒙C语言层级的入口点 
LITE_OS_SEC_TEXT_INIT UINT32 OsMain(VOID)
{
    UINT32 ret;
#ifdef LOS_INIT_STATISTICS
    UINT64 startNsec, endNsec, durationUsec;
#endif

    ret = EarliestInit();//鸿蒙初开，天地混沌
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_EARLIEST);

    ret = ArchEarlyInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_ARCH_EARLY);

    ret = PlatformEarlyInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_PLATFORM_EARLY);

    /* system and chip info */
    OsSystemInfo();

    PRINT_RELEASE("\nmain core booting up...\n");

#ifdef LOS_INIT_STATISTICS
    startNsec = LOS_CurrNanosec();
#endif

    ret = OsTaskInit();//任务池初始化
    if (ret != LOS_OK) {
        return ret;
    }

    OsInitCall(LOS_INIT_LEVEL_KMOD_PREVM);

    ret = OsSysMemInit();//系统内存初始化
    if (ret != LOS_OK) {
        return ret;
    }

    OsInitCall(LOS_INIT_LEVEL_VM_COMPLETE);

    ret = OsIpcInit();//进程间通讯模块初始化
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsSystemProcessCreate();//创建系统进程 
    if (ret != LOS_OK) {
        return ret;
    }

    ret = ArchInit();	//MMU架构初始化
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_ARCH);

    ret = PlatformInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_PLATFORM);

    ret = KModInit();
    if (ret != LOS_OK) {
        return ret;
    }

    OsInitCall(LOS_INIT_LEVEL_KMOD_BASIC);

    OsInitCall(LOS_INIT_LEVEL_KMOD_EXTENDED);

#ifdef LOSCFG_KERNEL_SMP
    OsSmpInit();
#endif

    OsInitCall(LOS_INIT_LEVEL_KMOD_TASK);

#ifdef LOS_INIT_STATISTICS
    endNsec = LOS_CurrNanosec();
    durationUsec = (endNsec - startNsec) / OS_SYS_NS_PER_US;
    PRINTK("The main core takes %lluus to start.\n", durationUsec);
#endif

    return LOS_OK;
}

#ifndef LOSCFG_PLATFORM_ADAPT
STATIC VOID SystemInit(VOID)
{
    PRINTK("dummy: *** %s ***\n", __FUNCTION__);
}
#else
extern VOID SystemInit(VOID);
#endif
#ifndef LOSCFG_ENABLE_KERNEL_TEST
///创建系统初始任务并申请调度
STATIC UINT32 OsSystemInitTaskCreate(VOID)
{
    UINT32 taskID;
    TSK_INIT_PARAM_S sysTask;

    (VOID)memset_s(&sysTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    sysTask.pfnTaskEntry = (TSK_ENTRY_FUNC)SystemInit;
    sysTask.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    sysTask.pcName = "SystemInit";
    sysTask.usTaskPrio = LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO;
    sysTask.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    sysTask.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    return LOS_TaskCreate(&taskID, &sysTask);
}

//系统任务初始化
STATIC UINT32 OsSystemInit(VOID)
{
    UINT32 ret;

    ret = OsSystemInitTaskCreate();
    if (ret != LOS_OK) {
        return ret;
    }

    return 0;
}

LOS_MODULE_INIT(OsSystemInit, LOS_INIT_LEVEL_KMOD_TASK);//模块初始化
#endif
