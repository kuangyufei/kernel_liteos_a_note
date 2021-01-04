/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

/**
 * @defgroup los_config System configuration items
 */

#ifndef _LOS_CONFIG_H
#define _LOS_CONFIG_H

#include "platform_config.h"
#include "los_tick.h"
#include "los_vm_zone.h"
#include "sys_config.h"
#include "hisoc/clock.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_config
 * int stack start addr
 */
extern CHAR __int_stack_start;	// 运行系统函数栈的开始地址
extern CHAR __rodata_start;		// ROM开始地址 只读
extern CHAR __rodata_end;		// ROM结束地址
extern CHAR __bss_start;		// bss开始地址 __attribute__((section(".__bss_start")));
extern CHAR __bss_end;			// bss结束地址 __attribute__((section(".__bss_end")));
extern CHAR __text_start;		// 代码区开始地址
extern CHAR __text_end;			// 代码区结束地址
extern CHAR __ram_data_start;	// RAM开始地址 可读可写
extern CHAR __ram_data_end;		// RAM结束地址
extern UINT32 __heap_start; 	// 堆区开始地址
extern UINT32 __heap_end;		// 堆区结束地址

/****************************** System clock module configuration ****************************/
/**
 * @ingroup los_config
 * System clock (unit: HZ)
 */
#ifndef OS_SYS_CLOCK	//HZ:是每秒中的周期性变动重复次数的计量
#define OS_SYS_CLOCK (get_bus_clk()) //系统主时钟频率 例如:50000000 即50微秒 
#endif
/**
 * @ingroup los_config
 * time timer clock (unit: HZ)
 */
#ifndef OS_TIME_TIMER_CLOCK
#define OS_TIME_TIMER_CLOCK OS_SYS_CLOCK //定时器频率
#endif

/**
 * @ingroup los_config
 * limit addr range when search for  'func local(frame pointer)' or 'func name'
 */
#ifndef OS_SYS_FUNC_ADDR_START
#define OS_SYS_FUNC_ADDR_START ((UINTPTR)&__int_stack_start)	//
#endif
#ifndef OS_SYS_FUNC_ADDR_END
#define OS_SYS_FUNC_ADDR_END (KERNEL_VMM_BASE + SYS_MEM_SIZE_DEFAULT) 
#endif

/**
 * @ingroup los_config
 * Number of Ticks in one second
 */
#ifndef LOSCFG_BASE_CORE_TICK_PER_SECOND
#define LOSCFG_BASE_CORE_TICK_PER_SECOND 100 //每秒Tick数
#endif

/**
 * @ingroup los_config
 * Microseconds of adjtime in one second
 */
#ifndef LOSCFG_BASE_CORE_ADJ_PER_SECOND
#define LOSCFG_BASE_CORE_ADJ_PER_SECOND 500
#endif

/**
 * @ingroup los_config
 * Sched clock interval
 */
#define SCHED_CLOCK_INTETRVAL_TICKS 100	//调度时间间隔

/**
 * @ingroup los_config
 * External configuration item for timer tailoring
 */
#ifndef LOSCFG_BASE_CORE_TICK_HW_TIME
#define LOSCFG_BASE_CORE_TICK_HW_TIME NO //定时器裁剪的外部配置项
#endif

/****************************** Hardware interrupt module configuration ******************************/
/**
 * @ingroup los_config
 * Configuration item for hardware interrupt tailoring 
 */
#ifndef LOSCFG_PLATFORM_HWI	//硬件中断裁剪配置项
#define LOSCFG_PLATFORM_HWI YES
#endif

/**
 * @ingroup los_config
 * Maximum number of used hardware interrupts, including Tick timer interrupts.
 */
#ifndef LOSCFG_PLATFORM_HWI_LIMIT
#define LOSCFG_PLATFORM_HWI_LIMIT 96 //硬件中断最大数量
#endif

/**
 * @ingroup los_config
 * The binary point value decide the maximum preemption level.
 * If preemption supported, the config value is [0, 1, 2, 3, 4, 5, 6],
 * to the corresponding preemption level value is [128, 64, 32, 16, 8, 4, 2].
 */
#ifdef LOSCFG_ARCH_INTERRUPT_PREEMPTION
#ifndef MAX_BINARY_POINT_VALUE
#define MAX_BINARY_POINT_VALUE  4
#endif
#endif

/****************************** Task module configuration ********************************/
/**
 * @ingroup los_config
 * Minimum stack size.
 *
 * 0x600 bytes, aligned on a boundary of 8.
 * 0x600 bytes, aligned on a boundary of 4.
 */
#ifndef LOS_TASK_MIN_STACK_SIZE
#ifdef __LP64__
#define LOS_TASK_MIN_STACK_SIZE (ALIGN(0x800, 8))
#else
#define LOS_TASK_MIN_STACK_SIZE (ALIGN(0x800, 4))
#endif
#endif

/**
 * @ingroup los_config
 * Default task priority
 */
#ifndef LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO	//内核任务默认优先级
#define LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO 10
#endif

/**
 * @ingroup los_config
 * Maximum supported number of tasks except the idle task rather than the number of usable tasks
 */
#ifndef LOSCFG_BASE_CORE_TSK_LIMIT	//支持的最大任务数（空闲任务除外，而不是可用任务数）
#define LOSCFG_BASE_CORE_TSK_LIMIT 128
#endif

/**
 * @ingroup los_config
 * Maximum supported number of process rather than the number of usable processes.
 */
#ifndef LOSCFG_BASE_CORE_PROCESS_LIMIT	//支持的最大进程数，而不是可用进程数
#define LOSCFG_BASE_CORE_PROCESS_LIMIT 64
#endif

#if (LOSCFG_BASE_CORE_TSK_LIMIT < LOSCFG_BASE_CORE_PROCESS_LIMIT)
#error "The number of tasks must be greater than or equal to the number of processes!"
#endif

/**
 * @ingroup los_config
 * Size of the idle task stack
 */
#ifndef LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE	//空闲任务栈大小
#define LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE SIZE(0x800)// 2K
#endif

/**
 * @ingroup los_config
 * Default task stack size
 */
#ifndef LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE	//内核默认任务栈大小
#define LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE SIZE(0x4000) //16K 
#endif

/**
 * @ingroup los_config
 * Longest execution time of tasks with the same priorities
 */
#ifndef LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT	//相同优先级任务的最长执行时间，时间片
#define LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT 2
#endif

/**
 * @ingroup los_config
 * Configuration item for task (stack) monitoring module tailoring
 */
#ifndef LOSCFG_BASE_CORE_TSK_MONITOR //任务（栈）监控模块裁剪配置项
#define LOSCFG_BASE_CORE_TSK_MONITOR YES
#endif

/**
 * @ingroup los_config
 * Configuration item for task perf task filter hook
 */
#ifndef OS_PERF_TSK_FILTER
#define OS_PERF_TSK_FILTER NO
#endif

/****************************** Semaphore module configuration ******************************/
/**
 * @ingroup los_config
 * Configuration item for semaphore module tailoring
 */
#ifndef LOSCFG_BASE_IPC_SEM
#define LOSCFG_BASE_IPC_SEM YES	//信号量支持
#endif

/**
 * @ingroup los_config
 * Maximum supported number of semaphores
 */
#ifndef LOSCFG_BASE_IPC_SEM_LIMIT
#define LOSCFG_BASE_IPC_SEM_LIMIT 1024 //信号量的最大个数
#endif

/****************************** mutex module configuration ******************************/
/**
 * @ingroup los_config
 * Configuration item for mutex module tailoring
 */
#ifndef LOSCFG_BASE_IPC_MUX
#define LOSCFG_BASE_IPC_MUX YES	//互斥量的支持
#endif

/****************************** Queue module configuration ********************************/
/**
 * @ingroup los_config
 * Configuration item for queue module tailoring
 */
#ifndef LOSCFG_BASE_IPC_QUEUE
#define LOSCFG_BASE_IPC_QUEUE YES //是否支持IPC队列?
#endif

/**
 * @ingroup los_config
 * Maximum supported number of queues rather than the number of usable queues
 */
#ifndef LOSCFG_BASE_IPC_QUEUE_LIMIT
#define LOSCFG_BASE_IPC_QUEUE_LIMIT 1024 //队列个数
#endif
/****************************** Software timer module configuration **************************/
#if (LOSCFG_BASE_IPC_QUEUE == YES) //软时钟模块需打开IPC队列

/**
 * @ingroup los_config
 * Configuration item for software timer module tailoring
 */
#ifndef LOSCFG_BASE_CORE_SWTMR
#define LOSCFG_BASE_CORE_SWTMR YES //是否支持软时钟?
#endif

/**
 * @ingroup los_config
 * Maximum supported number of software timers rather than the number of usable software timers
 */
#ifndef LOSCFG_BASE_CORE_SWTMR_LIMIT
#define LOSCFG_BASE_CORE_SWTMR_LIMIT 1024 // 最大支持的软件定时器数
#endif
/**
 * @ingroup los_config
 * Max number of software timers ID
 *
 * 0xFFFF: max number of all software timers
 */
#ifndef OS_SWTMR_MAX_TIMERID
#define OS_SWTMR_MAX_TIMERID ((0xFFFF / LOSCFG_BASE_CORE_SWTMR_LIMIT) * LOSCFG_BASE_CORE_SWTMR_LIMIT)//65535
#endif
/**
 * @ingroup los_config
 * Maximum size of a software timer queue
 */
#ifndef OS_SWTMR_HANDLE_QUEUE_SIZE
#define OS_SWTMR_HANDLE_QUEUE_SIZE LOSCFG_BASE_CORE_SWTMR_LIMIT //软时钟队列的大小
#endif
#endif


/****************************** Memory module configuration **************************/
#ifndef OS_EXC_INTERACTMEM_SIZE
#define OS_EXC_INTERACTMEM_SIZE (EXC_INTERACT_MEM_SIZE) //0x100000 //1M
#endif
/**
 * @ingroup los_config
 * Starting address of the system memory
 */
#ifndef OS_SYS_MEM_ADDR
#define OS_SYS_MEM_ADDR                        (&m_aucSysMem1[0])//系统动态内存起始地址
#endif

/**
 * @ingroup los_config
 * Memory size
 */
#ifndef OS_SYS_MEM_SIZE //系统动态内存池的大小（DDR自适应配置），以byte为单位,从bss段末尾至系统DDR末尾
#define OS_SYS_MEM_SIZE \
    ((OS_SYS_FUNC_ADDR_END) - ((OS_EXC_INTERACTMEM_SIZE + ((UINTPTR)&__bss_end) + (64 - 1)) & ~(64 - 1)))
#endif

/****************************** SMP module configuration **************************/
#ifndef LOSCFG_KERNEL_SMP
#define LOSCFG_KERNEL_SMP                               NO //SMP指对称多处理（Symmetrical Multi-Processing）,多核时要设为 YES
#endif

#ifndef LOSCFG_KERNEL_SMP_LOCKDEP
#define LOSCFG_KERNEL_SMP_LOCKDEP                       NO //死锁检测模块 Lockdep
#endif

#ifndef LOSCFG_KERNEL_SMP_TASK_SYNC
#define LOSCFG_KERNEL_SMP_TASK_SYNC                     NO //任务同步
#endif

#ifndef LOSCFG_KERNEL_SCHED_STATISTICS
#define LOSCFG_KERNEL_SCHED_STATISTICS                  NO //对任务调度的统计
#endif

#if (LOSCFG_KERNEL_SMP == YES)
#define LOSCFG_KERNEL_CORE_NUM                          LOSCFG_KERNEL_SMP_CORE_NUM //多核情况下支持的CPU核数
#else
#define LOSCFG_KERNEL_CORE_NUM                          1	//单核配置
#endif

#define LOSCFG_KERNEL_CPU_MASK                          ((1 << LOSCFG_KERNEL_CORE_NUM) - 1) //CPU掩码,每一个核占用一个位,用于计算和定位具体CPU核

/****************************** trace module configuration **************************/
#ifndef LOSCFG_KERNEL_TRACE
#define LOSCFG_KERNEL_TRACE                             NO  //内核追踪开关
#endif

/**
 * @ingroup los_trace
 * It's the total size of trace buffer. It's in the unit of char
 */
#if (LOSCFG_KERNEL_TRACE == YES)
#define LOS_TRACE_BUFFER_SIZE                           2048 //内核追踪缓存大小 2K
#endif

/**
 * @ingroup los_config
 * Version number
 */
#define _T(x)                                   x
#define KERNEL_NAME                            "Huawei LiteOS"
#define KERNEL_SEP                             " "
#define _V(v)                                  _T(KERNEL_NAME)_T(KERNEL_SEP)_T(v)

/**
 * @ingroup los_config
 * The Version number of Public 	//Public的版本号 ,每个占8位,刚好一个字节
 */
#define KERNEL_MAJOR                     2	//主版本号
#define KERNEL_MINOR                     0	//小版本号
#define KERNEL_PATCH                     0	//补丁版本号
#define KERNEL_ITRE                      35	//内部定义版本号

#define VERSION_NUM(a, b, c, d) (((a) << 24) | ((b) << 16) | (c) << 8 | (d))
#define KERNEL_OPEN_VERSION_NUM VERSION_NUM(KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE)

/****************************** Dynamic loading module configuration **************************/
#ifndef OS_AUTOINIT_DYNLOADER
#define OS_AUTOINIT_DYNLOADER YES	//是否支持动态加载ELF运行
#endif

/****************************** exception information  configuration ******************************/
#ifdef LOSCFG_SHELL_EXCINFO	//异常信息开关
/**
 * @ingroup los_config
 * the size of space for recording exception information
 */
#define EXCINFO_RECORD_BUF_SIZE (16 * 1024)	//记录异常信息缓存大小 16K

/**
 * @ingroup los_config
 * the address of space for recording exception information
 * @attention
 * <ul>
 * <li> if uses, the address must be valid in flash, and it should not overlap with other addresses
 * used to store valid information.  </li>
 * </ul>
 *
 */
#define EXCINFO_RECORD_ADDR (0xffffffff) //记录异常信息的空间地址

/**
 * @ingroup los_config
 * @brief  define the type of functions for reading or writing exception information.
 *
 * @par Description:
 * <ul>
 * <li>This defination is used to declare the type of functions for reading or writing exception information</li>
 * </ul>
 * @attention
 * <ul>
 * <li> "startAddr" must be left to save the exception address space, the size of "buf" is "space"  </li>
 * </ul>
 *
 * @param startAddr    [IN] Address of storage ,its must be left to save the exception address space
 * @param space        [IN] size of storage.its is also the size of "buf"
 * @param rwFlag       [IN] writer-read flag, 0 for writing,1 for reading, other number is to do nothing.
 * @param buf          [IN] the buffer of storing data.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_config.h: the header file that contains the type defination.</li></ul>
 * @see
 */ //定义用于读取或写入异常信息的指针函数类型
typedef VOID (*log_read_write_fn)(UINT32 startAddr, UINT32 space, UINT32 rwFlag, CHAR *buf);

/**
 * @ingroup los_config
 * @brief Register recording exception information function.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to Register recording exception information function,
 * and specify location and space and size</li>
 * </ul>
 * @attention
 * <ul>
 * <li> "startAddr" must be left to save the exception address space, the size of "buf" is "space",
 * the space of "buf" is malloc or free in user's code  </li>
 * </ul>
 *
 * @param startAddr    [IN] Address of storage, it must be left to save the exception address space
 * @param space        [IN] size of storage space, it is also the size of "buf"
 * @param buf          [IN] the buffer of storing exception information, the space of "buf" is malloc or free
                            in user's code
 * @param hook         [IN] the function for reading or writing exception information.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_config.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
/**
*	此API用于注册记录异常信息函数，并指定位置、空间和大小
*	startAddr:	保存发送异常的地址信息的启始地址
*	space:		buf的大小
*	buf:		缓存区
*	hook:		读写异常信息的函数
*/
VOID LOS_ExcInfoRegHook(UINT32 startAddr, UINT32 space, CHAR *buf, log_read_write_fn hook);
#endif

extern VOID OsStart(VOID);
extern INT32 OsMain(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_CONFIG_H */
