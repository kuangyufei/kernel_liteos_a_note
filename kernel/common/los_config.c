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
#include "string.h"
#include "stdio.h"
#include "los_oom.h"
#ifdef LOSCFG_COMPAT_LINUXKPI
#include "linux/workqueue.h"
#include "linux/module.h"
#endif
#include "los_sys.h"
#include "los_tick.h"
#include "los_task_pri.h"
#include "los_printf.h"
#include "los_swtmr.h"
#include "los_swtmr_pri.h"
#include "los_memory_pri.h"
#include "los_sem_pri.h"
#include "los_mux_pri.h"
#include "los_rwlock_pri.h"
#include "los_queue_pri.h"
#include "los_memstat_pri.h"
#include "los_hwi_pri.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_atomic.h"
#include "los_exc_pri.h"
#include "gic_common.h"
#include "los_vm_boot.h"

#ifdef LOSCFG_FS_VFS
#include "fs/fs.h"
#include "fs/fs_operation.h"
#endif

#if (LOSCFG_KERNEL_TRACE == YES)
#include "los_trace.h"
#endif

#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif

#ifdef LOSCFG_COMPAT_POSIX
#include "pprivate.h"
#endif

#ifdef LOSCFG_DRIVERS_HDF_PLATFORM_UART
#include "console.h"
#endif
#ifdef LOSCFG_ARCH_CORTEX_M7
#include "los_exc_pri.h"
#endif
#include "los_hw_tick_pri.h"
#include "los_hwi_pri.h"

#if defined(LOSCFG_HW_RANDOM_ENABLE) || defined (LOSCFG_DRIVERS_RANDOM)
#include "randomdev.h"
#include "yarrow.h"
#endif
#ifdef LOSCFG_SHELL_DMESG
#include "dmesg_pri.h"
#endif
#ifdef LOSCFG_SHELL_LK
#include "shell_pri.h"
#endif

#ifdef LOSCFG_KERNEL_PIPE
#include "pipe_common.h"
#endif

#include "los_process_pri.h"
#include "los_futex_pri.h"

#ifdef LOSCFG_KERNEL_VDSO
#include "los_vdso.h"
#endif

#if (LOSCFG_KERNEL_LITEIPC == YES)
#include "hm_liteipc.h"
#endif

#ifdef LOSCFG_DRIVERS_HIEVENT
#include "hievent_driver.h"
#endif

#if (LOSCFG_BASE_CORE_HILOG == YES)
#include "los_hilog.h"
#endif

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
//注册 HZ  , tick
LITE_OS_SEC_TEXT_INIT VOID osRegister(VOID)
{
    g_sysClock = OS_SYS_CLOCK; //HZ 
    g_tickPerSecond =  LOSCFG_BASE_CORE_TICK_PER_SECOND;//每秒节拍数 默认100 即一个tick = 10ms

    return;
}

//进程通讯IPC初始化 由OsMain()调用
LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsIpcInit(VOID)
{
    UINT32 ret;
#if (LOSCFG_BASE_IPC_SEM == YES)
    ret = OsSemInit();	//信号量初始化
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if (LOSCFG_BASE_IPC_QUEUE == YES)//系统已配置支持队列IPC
    ret = OsQueueInit();	//队列初始化
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return LOS_OK;
}

#ifdef LOSCFG_KERNEL_PIPE	//管道支持
LITE_OS_SEC_TEXT_INIT STATIC VOID OsDriverPipeInit(VOID)
{
    (VOID)pipe_init();//管道初始化
}
#endif

#ifdef LOSCFG_DRIVERS_HIEVENT //hievent支持
LITE_OS_SEC_TEXT_INIT STATIC VOID OsDriverHiEventInit(VOID)
{
    (VOID)HieventInit();
}
#endif

#ifdef LOSCFG_COMPAT_BSD //是否兼容BSD
extern void configure (void);
LITE_OS_SEC_TEXT_INIT STATIC INT32 OsBsdInit(VOID)
{
    /*
     * WORKAROUND: Inside configure(), nexus_init() function calls
     *             HiSi-specific, library procedure - machine_resource_init().
     *             The latter one is defined in libhi35xx_bsp.a which is only
     *             available for Hi3516 and Hi3518.
     *             Temporarily ifdef configure until this routine is implemented
     *             by other platforms.
     */
#if defined(LOSCFG_PLATFORM_HI3516DV300) || defined(LOSCFG_PLATFORM_HI3518EV300)
    configure();
#endif
    mi_startup(SI_SUB_ARCH_INIT);
    return LOS_OK;
}
#endif

// 初始化操作
LITE_OS_SEC_TEXT_INIT INT32 OsMain(VOID)
{
    UINT32 ret;

    osRegister();// 设置 g_sysClock 和 g_tickPerSecond

#ifdef LOSCFG_SHELL_DMESG
    ret = OsDmesgInit();// shell dmesg 命令用于控制内核dmesg缓存区，类似于echo ,buf 默认8K 最大80K 
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#ifdef LOSCFG_SHELL_LK
    OsLkLoggerInit(NULL);//  shell log 命令 用于修改&查询日志配置 log level [levelNum]共六个等级
#endif

#if (LOSCFG_KERNEL_TRACE == YES)
    LOS_TraceInit();
#endif

#ifdef LOSCFG_EXC_INTERACTION
#ifdef LOSCFG_ARCH_CORTEX_M7
    /* 4096: 4K space for Stack */
    ret = OsMemExcInteractionInit((UINT32)&__bss_end + 4096);
#else
    ret = OsMemExcInteractionInit((UINTPTR)&__bss_end);
#endif
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if (LOSCFG_PLATFORM_HWI == YES)
    OsHwiInit();// 硬中断初始化
#endif

    OsExcInit();// 异常接管初始化

    ret = OsTickInit(g_sysClock, LOSCFG_BASE_CORE_TICK_PER_SECOND);// tick初始化,包含注册中断事件
    if (ret != LOS_OK) {
        return ret;
    }

#ifdef LOSCFG_PLATFORM_UART_WITHOUT_VFS
#ifdef LOSCFG_DRIVERS
    uart_init();// 串口的初始化
#endif
#ifdef LOSCFG_SHELL
#endif //LOSCFG_SHELL
#endif //LOSCFG_PLATFORM_UART_WITHOUT_VFS
    ret = OsTaskInit();// 任务初始化
    if (ret != LOS_OK) {
        PRINT_ERR("OsTaskInit error\n");
        return ret;
    }

#if ((LOSCFG_BASE_IPC_QUEUE == YES) || (LOSCFG_BASE_IPC_MUX == YES) || \
     (LOSCFG_BASE_IPC_SEM == YES) || (LOSCFG_BASE_IPC_RWLOCK == YES))
    ret = OsIpcInit();// 进程间通讯(InterProcess Communication)初始化 
    if (ret != LOS_OK) {
        return ret;
    }
#endif

    ret = OsSysMemInit();// 完成内存的初始化操作,段页管理
    if (ret != LOS_OK) {
        PRINT_ERR("OsSysMemInit error\n");
        return ret;
    }

#ifdef LOSCFG_KERNEL_SYSCALL
    SyscallHandleInit();// 系统调用相关初始化
#endif

    /*
     * CPUP should be inited before first task creation which depends on the semaphore
     * when LOSCFG_KERNEL_SMP_TASK_SYNC is enabled. So don't change this init sequence
     * if not neccessary. The sequence should be like this:
     * 1. OsIpcInit
     * 2. OsCpupInit
     * 3. other inits have task creation
     */
#ifdef LOSCFG_KERNEL_CPUP //cpu统计是否支持
    ret = OsCpupInit();// shell cpup命令用于查询系统CPU的占用率 命令格式cpup [mode] [taskID]
    if (ret != LOS_OK) {
        PRINT_ERR("OsCpupInit error\n");
        return ret;
    }
#endif

    ret = OsSystemProcessCreate();//创建系统进程 即:2 和 0 进程
    if (ret != LOS_OK) {
        return ret;
    }

#if (LOSCFG_BASE_CORE_SWTMR == YES)
    ret = OsSwtmrInit();//软时钟模块初始化
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupGuardCreator();
#endif

#if (LOSCFG_KERNEL_SMP == YES)
    (VOID)OsMpInit();//多处理器模块的初始化
#endif

#if defined(LOSCFG_HW_RANDOM_ENABLE) || defined (LOSCFG_DRIVERS_RANDOM)
    random_alg_context.ra_init_alg(NULL);
    run_harvester_iterate(NULL);
#endif

#ifdef LOSCFG_COMPAT_BSD
    ret = OsBsdInit();//BSD模块初始化
    if (ret != LOS_OK) {
        PRINT_ERR("init bsd failed!\n");
        return ret;
    }
#endif

#ifdef LOSCFG_KERNEL_PIPE	//打开管道宏
    OsDriverPipeInit();//管道模块初始化
#endif

    ret = OsSystemInit();//系统初始化
    if (ret != LOS_OK) {
        return ret;
    }
#if (LOSCFG_BASE_CORE_HILOG == YES)
    ret = HiLogDriverInit();
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if LOSCFG_DRIVERS_HIEVENT //打开HIEVENT宏
    OsDriverHiEventInit();//HiEvent 初始化
#endif

#if (LOSCFG_KERNEL_LITEIPC == YES)
    ret = LiteIpcInit();
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#ifdef LOSCFG_KERNEL_VDSO
    ret = OsInitVdso();
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#ifdef LOSCFG_KERNEL_VM
    ret = OsFutexInit();//linux使用了futex来进行快速通信，pthread_xxx 相关接口全部都是基于futex实现的
    if (ret != LOS_OK) {
        PRINT_ERR("Create futex failed : %d!\n", ret);
        return ret;
    }

    ret = OomTaskInit();//Out-Of-Memory 用于监控使用内存太大的task.
    if (ret != LOS_OK) {//在内存不足而某些进程太耗内存时,内核可直接把进程干掉而释放内存
        return ret;
    }
#endif

    return LOS_OK;
}
//创建系统初始化任务
STATIC UINT32 OsSystemInitTaskCreate(VOID)
{
    UINT32 taskID;
    TSK_INIT_PARAM_S sysTask;

    (VOID)memset_s(&sysTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
#ifndef LOSCFG_ENABLE_KERNEL_TEST
    sysTask.pfnTaskEntry = (TSK_ENTRY_FUNC)SystemInit;//任务的入口函数，这个任务由外部提供比如..\vendor\hi3516dv300\module_init\src\system_init.c
#else
    extern void TestSystemInit(void);
    sysTask.pfnTaskEntry = (TSK_ENTRY_FUNC)TestSystemInit;
#endif
    sysTask.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;//16K
    sysTask.pcName = "SystemInit";//任务的名称
    sysTask.usTaskPrio = LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO;// 内核默认优先级为10 
    sysTask.uwResved = LOS_TASK_STATUS_DETACHED;//任务分离模式
#if (LOSCFG_KERNEL_SMP == YES)
    sysTask.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());//cpu 亲和性设置,记录执行过任务的CPU,尽量确保由同一个CPU完成任务周期
#endif
    return LOS_TaskCreate(&taskID, &sysTask);//创建任务并加入就绪队列，并立即参与调度
}


UINT32 OsSystemInit(VOID)
{
    UINT32 ret;
#ifdef LOSCFG_FS_VFS //虚拟文件系统支持
    los_vfs_init(); //虚拟文件系统初始化
#endif
#ifdef LOSCFG_COMPAT_LINUXKPI
    g_pstSystemWq = create_workqueue("system_wq");
#endif
    ret = OsSystemInitTaskCreate();//创建 "SystemInit" 任务,用户创建用户态根进程
    if (ret != LOS_OK) {
        return ret;
    }

    return 0;
}

