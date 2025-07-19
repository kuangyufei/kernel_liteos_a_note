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


/**
 * @brief 重启钩子函数指针，用于存储用户注册的重启回调函数
 * @details 静态全局变量，初始值为NULL，通过OsSetRebootHook()注册钩子函数
 * @warning 仅允许注册一个重启钩子函数，多次注册会覆盖之前的注册
 */
STATIC SystemRebootFunc g_rebootHook = NULL;  /* 重启钩子函数指针 */

/**
 * @brief 设置系统重启钩子函数
 * @param[in] func 自定义重启处理函数，为NULL时取消当前注册
 * @par 示例：
 * @code
 * VOID UserRebootHandler(UINT32 arg)
 * {
 *     // 用户自定义清理操作
 *     PRINTK("System rebooting...\n");
 * }
 * 
 * // 注册重启钩子
 * OsSetRebootHook(UserRebootHandler);
 * @endcode
 * @note 系统重启流程中会在关闭关键服务前调用该钩子函数
 */
VOID OsSetRebootHook(SystemRebootFunc func)
{
    g_rebootHook = func;  /* 更新全局钩子函数指针 */
}

/**
 * @brief 获取当前注册的重启钩子函数
 * @return 当前注册的钩子函数指针，未注册时返回NULL
 * @see OsSetRebootHook()
 */
SystemRebootFunc OsGetRebootHook(VOID)
{
    return g_rebootHook;  /* 返回当前钩子函数指针 */
}

/**
 * @brief 内核最早阶段初始化函数
 * @retval LOS_OK 初始化成功
 * @retval 其他值 初始化失败（本函数目前无失败路径）
 * @details 完成内核启动最基础的初始化工作，包括：
 *          1. 设置主任务控制块
 *          2. 初始化调度队列
 *          3. 设置系统时钟和滴答频率
 * @note 必须在系统启动流程的最开始调用，任何其他初始化函数都依赖于此函数完成的设置
 * @warning 该函数执行失败将导致系统无法启动
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 EarliestInit(VOID)
{
    /* Must be placed at the beginning of the boot process */
    OsSetMainTask();                  /* 设置主任务控制块为当前任务 */
    OsCurrTaskSet(OsGetMainTask());   /* 更新当前运行任务指针 */
    OsSchedRunqueueInit();            /* 初始化调度队列数据结构 */

    g_sysClock = OS_SYS_CLOCK;        /* 初始化系统时钟频率（Hz） */
    g_tickPerSecond = LOSCFG_BASE_CORE_TICK_PER_SECOND;  /* 设置每秒时钟滴答数 */

    return LOS_OK;  /* 返回初始化成功 */
}

/**
 * @brief 架构相关早期初始化函数
 * @retval LOS_OK 初始化成功
 * @retval LOS_ERRNO_TICK_INIT_FAILED 定时器初始化失败
 * @details 执行与硬件架构相关的早期初始化操作：
 *          1. 设置系统计数器频率
 *          2. 初始化硬件中断控制器
 *          3. 初始化异常处理机制
 *          4. 初始化系统滴答定时器
 * @note 该函数由OsMain()按固定顺序调用，不同架构（如ARM、RISC-V）有不同实现
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 ArchEarlyInit(VOID)
{
    UINT32 ret;  /* 函数返回值 */

    /* set system counter freq */
#ifndef LOSCFG_TEE_ENABLE
    HalClockFreqWrite(OS_SYS_CLOCK);  /* 向硬件写入系统时钟频率 */
#endif

#ifdef LOSCFG_PLATFORM_HWI
    OsHwiInit();  /* 初始化硬件中断控制器（如GIC） */
#endif

    OsExcInit();  /* 初始化异常处理机制，注册异常向量表 */

    /* 初始化系统滴答定时器，设置中断周期 */
    ret = OsTickInit(g_sysClock, LOSCFG_BASE_CORE_TICK_PER_SECOND);
    if (ret != LOS_OK) {
        PRINT_ERR("OsTickInit error!\n");  /* 输出错误信息到控制台 */
        return ret;  /* 返回定时器初始化错误码 */
    }

    return LOS_OK;  /* 返回初始化成功 */
}

/**
 * @brief 平台早期初始化函数
 * @retval LOS_OK 初始化成功
 * @details 执行平台特定的早期初始化，当前主要处理UART初始化
 * @note 仅当同时定义LOSCFG_PLATFORM_UART_WITHOUT_VFS和LOSCFG_DRIVERS时才初始化UART
 * @warning UART初始化失败会导致系统无输出，但不影响内核核心功能
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 PlatformEarlyInit(VOID)
{
#if defined(LOSCFG_PLATFORM_UART_WITHOUT_VFS) && defined(LOSCFG_DRIVERS)
    uart_init();  /* 初始化UART硬件，用于系统调试输出 */
#endif /* LOSCFG_PLATFORM_UART_WITHOUT_VFS */

    return LOS_OK;  /* 返回初始化成功 */
}

/**
 * @brief 内核IPC（进程间通信）初始化函数
 * @retval LOS_OK 初始化成功
 * @retval LOS_ERRNO_SEM_INIT_FAILED 信号量初始化失败
 * @retval LOS_ERRNO_QUEUE_INIT_FAILED 消息队列初始化失败
 * @details 根据配置初始化相应的IPC机制：
 *          - 当定义LOSCFG_BASE_IPC_SEM时初始化信号量
 *          - 当定义LOSCFG_BASE_IPC_QUEUE时初始化消息队列
 * @note IPC机制是多任务通信的基础，初始化失败将导致依赖IPC的任务无法正常工作
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsIpcInit(VOID)
{
    UINT32 ret;  /* 函数返回值 */

#ifdef LOSCFG_BASE_IPC_SEM
    ret = OsSemInit();  /* 初始化信号量系统 */
    if (ret != LOS_OK) {
        PRINT_ERR("OsSemInit error\n");  /* 输出信号量初始化错误 */
        return ret;  /* 返回错误码 */
    }
#endif

#ifdef LOSCFG_BASE_IPC_QUEUE
    ret = OsQueueInit();  /* 初始化消息队列系统 */
    if (ret != LOS_OK) {
        PRINT_ERR("OsQueueInit error\n");  /* 输出消息队列初始化错误 */
        return ret;  /* 返回错误码 */
    }
#endif
    return LOS_OK;  /* 返回初始化成功 */
}

/**
 * @brief 架构初始化函数
 * @retval LOS_OK 初始化成功
 * @details 执行架构相关的高级初始化，当前主要处理MMU（内存管理单元）初始化
 * @note 仅当定义LOSCFG_KERNEL_MMU时才执行MMU初始化
 * @warning MMU初始化失败会导致内存保护机制无法工作
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 ArchInit(VOID)
{
#ifdef LOSCFG_KERNEL_MMU
    OsArchMmuInitPerCPU();  /* 初始化每个CPU的MMU设置 */
#endif
    return LOS_OK;  /* 返回初始化成功 */
}

/**
 * @brief 平台初始化函数
 * @retval LOS_OK 初始化成功
 * @details 平台特定初始化的占位函数，目前未实现具体功能
 * @note 预留接口，用于未来扩展平台相关初始化逻辑
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 PlatformInit(VOID)
{
    return LOS_OK;  /* 返回初始化成功 */
}

/**
 * @brief 内核模块初始化函数
 * @retval LOS_OK 初始化成功
 * @details 根据配置初始化内核模块，当前主要处理软件定时器
 * @note 仅当定义LOSCFG_BASE_CORE_SWTMR_ENABLE时初始化软件定时器
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 KModInit(VOID)
{
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    OsSwtmrInit();  /* 初始化软件定时器系统 */
#endif
    return LOS_OK;  /* 返回初始化成功 */
}

/**
 * @brief 输出系统信息函数
 * @details 打印处理器信息、运行模式、GIC版本、编译时间和内核版本等关键信息
 * @note 信息通过PRINT_RELEASE宏输出，在非调试版本中也会显示
 * @par 输出格式示例：
 * @code
 * ******************Welcome******************
 * 
 * Processor   : ARM Cortex-A7
 * Run Mode    : SMP
 * GIC Rev     : v2
 * build time  : Jul 15 2023 10:30:45
 * Kernel      : LiteOS-A 5.1.0.0/release
 * 
 * *******************************************
 * @endcode
 */
LITE_OS_SEC_TEXT_INIT VOID OsSystemInfo(VOID)
{
#ifdef LOSCFG_DEBUG_VERSION
    const CHAR *buildType = "debug";  /* 调试版本标识 */
#else
    const CHAR *buildType = "release";  /* 发布版本标识 */
#endif /* LOSCFG_DEBUG_VERSION */

    /* 使用格式化字符串输出系统信息 */
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
                  LOS_CpuInfo(),  /* 获取CPU信息字符串 */
#ifdef LOSCFG_KERNEL_SMP
                  LOSCFG_KERNEL_SMP_CORE_NUM,  /* SMP模式下的核心数量 */
#endif
                  HalIrqVersion(), __DATE__, __TIME__, \
                  KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, buildType);
}

/**
 * @brief 内核主函数，负责整个系统的启动流程控制
 * @retval LOS_OK 系统启动成功
 * @retval 其他值 启动过程中发生错误
 * @details 内核启动的主入口点，按固定顺序执行以下操作：
 *          1. 执行最早阶段初始化（EarliestInit）
 *          2. 架构和平台早期初始化
 *          3. 输出系统信息
 *          4. 进程管理、内存管理、IPC等核心模块初始化
 *          5. SMP多核心初始化（如果启用）
 *          6. 记录并输出启动时间（如果启用统计）
 * @note 函数执行流程严格按照初始化依赖关系设计，不能随意调整顺序
 * @warning 任何阶段初始化失败都会导致系统启动中止
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsMain(VOID)
{
    UINT32 ret;  /* 函数返回值，用于检查各阶段初始化结果 */
#ifdef LOS_INIT_STATISTICS
    UINT64 startNsec, endNsec, durationUsec;  /* 启动时间统计变量 */
#endif

    /* 1. 最早阶段初始化 - 设置主任务和调度队列 */
    ret = EarliestInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_EARLIEST);  /* 调用最早阶段的初始化钩子 */

    /* 2. 架构早期初始化 - 时钟、中断和定时器 */
    ret = ArchEarlyInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_ARCH_EARLY);  /* 调用架构早期初始化钩子 */

    /* 3. 平台早期初始化 - UART等平台设备 */
    ret = PlatformEarlyInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_PLATFORM_EARLY);  /* 调用平台早期初始化钩子 */

    /* 4. 输出系统信息 - 处理器、内核版本等 */
    OsSystemInfo();

    PRINT_RELEASE("\nmain core booting up...\n");  /* 输出主核心启动提示 */

#ifdef LOS_INIT_STATISTICS
    startNsec = LOS_CurrNanosec();  /* 记录启动计时开始时间（纳秒级） */
#endif

    /* 5. 进程管理初始化 */
    ret = OsProcessInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_KMOD_PREVM);  /* 调用进程管理初始化钩子 */

    /* 6. 系统内存初始化 */
    ret = OsSysMemInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_VM_COMPLETE);  /* 调用内存管理初始化钩子 */

    /* 7. IPC机制初始化 */
    ret = OsIpcInit();
    if (ret != LOS_OK) {
        return ret;
    }
    /* 8. 创建系统进程 */
    ret = OsSystemProcessCreate();
    if (ret != LOS_OK) {
        return ret;
    }
    /* 9. 架构高级初始化（如MMU） */
    ret = ArchInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_ARCH);  /* 调用架构初始化钩子 */

    /* 10. 平台初始化 */
    ret = PlatformInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_PLATFORM);  /* 调用平台初始化钩子 */

    /* 11. 内核模块初始化（如软件定时器） */
    ret = KModInit();
    if (ret != LOS_OK) {
        return ret;
    }
    OsInitCall(LOS_INIT_LEVEL_KMOD_BASIC);  /* 调用基础模块初始化钩子 */

    /* 12. 扩展模块初始化 */
    OsInitCall(LOS_INIT_LEVEL_KMOD_EXTENDED);  /* 调用扩展模块初始化钩子 */

    /* 13. SMP多核心初始化（如果配置） */
#ifdef LOSCFG_KERNEL_SMP
    OsSmpInit();  /* 初始化对称多处理，启动从核心 */
#endif

    /* 14. 任务模块初始化 */
    OsInitCall(LOS_INIT_LEVEL_KMOD_TASK);  /* 调用任务模块初始化钩子 */

#ifdef LOS_INIT_STATISTICS
    endNsec = LOS_CurrNanosec();  /* 记录启动计时结束时间 */
    durationUsec = (endNsec - startNsec) / OS_SYS_NS_PER_US;  /* 计算启动耗时（转换为微秒） */
    PRINTK("The main core takes %lluus to start.\n", durationUsec);  /* 输出启动耗时 */
#endif

    return LOS_OK;  /* 所有初始化完成，返回成功 */
}
// 条件编译：根据平台适配配置决定SystemInit函数的实现方式
// 当未启用平台适配时，定义本地SystemInit函数（调试用）
#ifndef LOSCFG_PLATFORM_ADAPT
/**
 * @brief 系统初始化函数（默认调试实现）
 * @details 当未配置平台适配时，提供一个默认的SystemInit实现，仅打印调试信息
 * @note 实际产品中应通过LOSCFG_PLATFORM_ADAPT启用平台专用实现
 */
STATIC VOID SystemInit(VOID)
{
    PRINTK("dummy: *** %s ***\n", __FUNCTION__);  // 打印函数名，标识调试版本的初始化入口
}
#else
// 当启用平台适配时，声明外部SystemInit函数（由平台代码实现）
extern VOID SystemInit(VOID);
#endif

// 条件编译：内核测试模式下可能会重定义系统初始化流程
#ifndef LOSCFG_ENABLE_KERNEL_TEST
/**
 * @brief 创建系统初始化任务
 * @details 构建系统初始化任务的参数并创建任务，该任务负责执行SystemInit函数
 * @return UINT32 任务创建结果
 *         - LOS_OK: 创建成功
 *         - 其他值: 创建失败（具体错误码见错误码定义）
 */
STATIC UINT32 OsSystemInitTaskCreate(VOID)
{
    UINT32 taskID;                          // 任务ID变量，用于接收创建的任务ID
    TSK_INIT_PARAM_S sysTask;               // 任务初始化参数结构体

    // 初始化任务参数结构体，清零所有成员
    (VOID)memset_s(&sysTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    sysTask.pfnTaskEntry = (TSK_ENTRY_FUNC)SystemInit;  // 设置任务入口函数为SystemInit
    sysTask.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;  // 使用默认堆栈大小
    sysTask.pcName = "SystemInit";          // 任务名称，用于调试和任务管理
    sysTask.usTaskPrio = LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO;  // 使用默认任务优先级
    sysTask.uwResved = LOS_TASK_STATUS_DETACHED;  // 设置任务为分离状态，结束后自动回收资源
#ifdef LOSCFG_KERNEL_SMP
    // SMP配置下，将初始化任务绑定到当前CPU核心
    sysTask.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    // 创建系统初始化任务，返回创建结果
    return LOS_TaskCreate(&taskID, &sysTask);
}

/**
 * @brief 系统初始化入口函数
 * @details 负责协调系统初始化过程，通过创建初始化任务启动系统配置
 * @return UINT32 初始化结果
 *         - LOS_OK: 初始化成功
 *         - 其他值: 任务创建失败
 */
STATIC UINT32 OsSystemInit(VOID)
{
    UINT32 ret;                             // 函数返回值变量

    // 创建系统初始化任务
    ret = OsSystemInitTaskCreate();
    if (ret != LOS_OK) {                    // 检查任务创建是否成功
        return ret;                         // 创建失败时返回错误码
    }

    return 0;                               // 初始化流程启动成功
}

// 模块初始化注册：将OsSystemInit函数注册为内核模块任务级初始化入口
// 初始化级别为LOS_INIT_LEVEL_KMOD_TASK，表示在内核模块任务阶段执行
LOS_MODULE_INIT(OsSystemInit, LOS_INIT_LEVEL_KMOD_TASK);
#endif
