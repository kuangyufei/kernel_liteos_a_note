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
#include "los_tick.h"
#include "los_vm_boot.h"

STATIC SystemRebootFunc g_rebootHook = NULL;

VOID OsSetRebootHook(SystemRebootFunc func)
{
    g_rebootHook = func;
}

SystemRebootFunc OsGetRebootHook(VOID)
{
    return g_rebootHook;
}

extern UINT32 OsSystemInit(VOID);
extern VOID SystemInit(VOID);
#if (LOSCFG_KERNEL_SMP == 1)
extern VOID release_secondary_cores(VOID);
#endif

LITE_OS_SEC_TEXT_INIT STATIC UINT32 EarliestInit(VOID)
{
    /* Must be placed at the beginning of the boot process */
    OsSetMainTask();
    OsCurrTaskSet(OsGetMainTask());

    g_sysClock = OS_SYS_CLOCK;
    g_tickPerSecond =  LOSCFG_BASE_CORE_TICK_PER_SECOND;

    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 ArchEarlyInit(VOID)
{
    UINT32 ret = LOS_OK;

    /* set system counter freq */
#ifndef LOSCFG_TEE_ENABLE
    HalClockFreqWrite(OS_SYS_CLOCK);
#endif

#if (LOSCFG_PLATFORM_HWI == 1)
    OsHwiInit();
#endif

    OsExcInit();

    ret = OsTickInit(g_sysClock, LOSCFG_BASE_CORE_TICK_PER_SECOND);
    if (ret != LOS_OK) {
        PRINT_ERR("OsTickInit error!\n");
        return ret;
    }

    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 PlatformEarlyInit(VOID)
{
#if defined(LOSCFG_PLATFORM_UART_WITHOUT_VFS) && defined(LOSCFG_DRIVERS)
    uart_init();
#endif /* LOSCFG_PLATFORM_UART_WITHOUT_VFS */

    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsIpcInit(VOID)
{
    UINT32 ret;

#if (LOSCFG_BASE_IPC_SEM == 1)
    ret = OsSemInit();
    if (ret != LOS_OK) {
        PRINT_ERR("OsSemInit error\n");
        return ret;
    }
#endif

#if (LOSCFG_BASE_IPC_QUEUE == 1)
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
#if (LOSCFG_BASE_CORE_SWTMR == 1)
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
#if (LOSCFG_KERNEL_SMP == 1)
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
#if (LOSCFG_KERNEL_SMP == 1)
                  LOSCFG_KERNEL_SMP_CORE_NUM,
#endif
                  HalIrqVersion(), __DATE__, __TIME__,\
                  KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, buildType);
}

LITE_OS_SEC_TEXT_INIT INT32 OsMain(VOID)
{
    UINT32 ret;
#ifdef LOS_INIT_STATISTICS
    UINT64 startNsec, endNsec, durationUsec;
#endif

    ret = EarliestInit();
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

    ret = OsTaskInit();
    if (ret != LOS_OK) {
        return ret;
    }

    OsInitCall(LOS_INIT_LEVEL_KMOD_PREVM);

    ret = OsSysMemInit();
    if (ret != LOS_OK) {
        return ret;
    }
#if (LOSCFG_KERNEL_SMP == 1)
    release_secondary_cores();
#endif
    OsInitCall(LOS_INIT_LEVEL_VM_COMPLETE);

    ret = OsIpcInit();
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsSystemProcessCreate();
    if (ret != LOS_OK) {
        return ret;
    }

    ret = ArchInit();
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

    OsInitCall(LOS_INIT_LEVEL_KMOD_TASK);

#ifdef LOS_INIT_STATISTICS
    endNsec = LOS_CurrNanosec();
    durationUsec = (endNsec - startNsec) / OS_SYS_NS_PER_US;
    PRINTK("The main core takes %lluus to start.\n", durationUsec);
#endif

    return LOS_OK;
}

STATIC UINT32 OsSystemInitTaskCreate(VOID)
{
    UINT32 taskID;
    TSK_INIT_PARAM_S sysTask;

    (VOID)memset_s(&sysTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
#ifndef LOSCFG_ENABLE_KERNEL_TEST
    sysTask.pfnTaskEntry = (TSK_ENTRY_FUNC)SystemInit;
#else
    extern void TestSystemInit(void);
    sysTask.pfnTaskEntry = (TSK_ENTRY_FUNC)TestSystemInit;
#endif
    sysTask.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    sysTask.pcName = "SystemInit";
    sysTask.usTaskPrio = LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO;
    sysTask.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == 1)
    sysTask.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    return LOS_TaskCreate(&taskID, &sysTask);
}


UINT32 OsSystemInit(VOID)
{
    UINT32 ret;

    ret = OsSystemInitTaskCreate();
    if (ret != LOS_OK) {
        return ret;
    }

    return 0;
}

LOS_MODULE_INIT(OsSystemInit, LOS_INIT_LEVEL_KMOD_TASK);
