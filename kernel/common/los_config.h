/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_tick.h"
#include "los_vm_zone.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_config
 * int stack start addr
 */
extern CHAR __int_stack_start;
extern CHAR __rodata_start;
extern CHAR __rodata_end;
extern CHAR __bss_start;
extern CHAR __bss_end;
extern CHAR __text_start;
extern CHAR __text_end;
extern CHAR __ram_data_start;
extern CHAR __ram_data_end;
extern UINT32 __heap_start;
extern UINT32 __heap_end;

/****************************** System clock module configuration ****************************/
/**
 * @ingroup los_config
 * System clock (unit: HZ)
 */
#ifndef OS_SYS_CLOCK
#define OS_SYS_CLOCK (get_bus_clk())
#endif
/**
 * @ingroup los_config
 * time timer clock (unit: HZ)
 */
#ifndef OS_TIME_TIMER_CLOCK
#define OS_TIME_TIMER_CLOCK OS_SYS_CLOCK
#endif

/**
 * @ingroup los_config
 * limit addr range when search for  'func local(frame pointer)' or 'func name'
 */
#ifndef OS_SYS_FUNC_ADDR_START
#define OS_SYS_FUNC_ADDR_START ((UINTPTR)&__int_stack_start)
#endif
#ifndef OS_SYS_FUNC_ADDR_END
#define OS_SYS_FUNC_ADDR_END (KERNEL_VMM_BASE + SYS_MEM_SIZE_DEFAULT)
#endif

/**
 * @ingroup los_config
 * Number of Ticks in one second
 */
#ifndef LOSCFG_BASE_CORE_TICK_PER_SECOND
#define LOSCFG_BASE_CORE_TICK_PER_SECOND  1000  /* 1ms per tick */
#endif

/**
 * @ingroup los_config
 * Minimum response error accuracy of tick interrupts, number of ticks in one second
 */
#ifndef LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI
#define LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI  1000UL  /* 1ms */
#endif

#if (LOSCFG_BASE_CORE_TICK_PER_SECOND > LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI)
    #error "LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI must be greater than LOSCFG_BASE_CORE_TICK_PER_SECOND"
#endif

#if (LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI > 1000UL)
    #error "LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI must be less than or equal to 1000"
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
#define SCHED_CLOCK_INTETRVAL_TICKS  LOSCFG_BASE_CORE_TICK_PER_SECOND

/**
 * @ingroup los_config
 * External configuration item for timer tailoring
 */
#if defined(LOSCFG_BASE_CORE_TICK_HW_TIME) && (LOSCFG_BASE_CORE_TICK_HW_TIME == 0)
#undef LOSCFG_BASE_CORE_TICK_HW_TIME
#endif
/****************************** Hardware interrupt module configuration ******************************/
/**
 * @ingroup los_config
 * Configuration item for hardware interrupt tailoring
 */
#ifndef LOSCFG_PLATFORM_HWI
#define LOSCFG_PLATFORM_HWI
#endif

/**
 * @ingroup los_config
 * Maximum number of used hardware interrupts, including Tick timer interrupts.
 */
#ifndef LOSCFG_PLATFORM_HWI_LIMIT
#define LOSCFG_PLATFORM_HWI_LIMIT 96
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
 * 0x800 bytes, aligned on a boundary of 8.
 * 0x800 bytes, aligned on a boundary of 4.
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
#ifndef LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO
#define LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO 10
#endif

/**
 * @ingroup los_config
 * Maximum supported number of tasks except the idle task rather than the number of usable tasks
 */
#ifndef LOSCFG_BASE_CORE_TSK_LIMIT
#define LOSCFG_BASE_CORE_TSK_LIMIT 128
#endif

/**
 * @ingroup los_config
 * Maximum supported number of process rather than the number of usable processes.
 */
#ifndef LOSCFG_BASE_CORE_PROCESS_LIMIT
#define LOSCFG_BASE_CORE_PROCESS_LIMIT 64
#endif

#if (LOSCFG_BASE_CORE_TSK_LIMIT < LOSCFG_BASE_CORE_PROCESS_LIMIT)
#error "The number of tasks must be greater than or equal to the number of processes!"
#endif

/**
 * @ingroup los_config
 * Size of the idle task stack
 */
#ifndef LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE
#define LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE SIZE(0x800)
#endif

/**
 * @ingroup los_config
 * Default task stack size
 */
#ifndef LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE
#define LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE SIZE(0x4000)
#endif

/**
 * @ingroup los_config
 * Longest execution time of tasks with the same priorities
 */
#ifndef LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT
#define LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT  20000 /* 20ms */
#endif

/**
 * @ingroup los_config
 * Configuration item for task (stack) monitoring module tailoring
 */
#ifndef LOSCFG_BASE_CORE_TSK_MONITOR
#define LOSCFG_BASE_CORE_TSK_MONITOR
#endif

/****************************** Semaphore module configuration ******************************/
/**
 * @ingroup los_config
 * Configuration item for semaphore module tailoring
 */
#ifndef LOSCFG_BASE_IPC_SEM
#define LOSCFG_BASE_IPC_SEM
#endif

/**
 * @ingroup los_config
 * Maximum supported number of semaphores
 */
#ifndef LOSCFG_BASE_IPC_SEM_LIMIT
#define LOSCFG_BASE_IPC_SEM_LIMIT 1024
#endif

/**
 * @ingroup los_config
 * Maximum number of semaphores.
 */
#ifndef OS_SEM_COUNT_MAX
#define OS_SEM_COUNT_MAX 0xFFFE
#endif

/****************************** Mutex module configuration ******************************/
/**
 * @ingroup los_config
 * Configuration item for mutex module tailoring
 */
#ifndef LOSCFG_BASE_IPC_MUX
#define LOSCFG_BASE_IPC_MUX
#endif

/****************************** rwlock module configuration ******************************/
/**
 * @ingroup los_config
 * Configuration item for rwlock module tailoring
 */
#ifndef LOSCFG_BASE_IPC_RWLOCK
#define LOSCFG_BASE_IPC_RWLOCK
#endif

/****************************** Queue module configuration ********************************/
/**
 * @ingroup los_config
 * Configuration item for queue module tailoring
 */
#ifndef LOSCFG_BASE_IPC_QUEUE
#define LOSCFG_BASE_IPC_QUEUE
#endif

/**
 * @ingroup los_config
 * Maximum supported number of queues rather than the number of usable queues
 */
#ifndef LOSCFG_BASE_IPC_QUEUE_LIMIT
#define LOSCFG_BASE_IPC_QUEUE_LIMIT 1024
#endif
/****************************** Software timer module configuration **************************/
#ifdef LOSCFG_BASE_IPC_QUEUE

/**
 * @ingroup los_config
 * Configuration item for software timer module tailoring
 */
#ifndef LOSCFG_BASE_CORE_SWTMR_ENABLE
#define LOSCFG_BASE_CORE_SWTMR_ENABLE
#endif

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
#define LOSCFG_BASE_CORE_SWTMR 1
#else
#define LOSCFG_BASE_CORE_SWTMR 0
#endif

/**
 * @ingroup los_config
 * Maximum supported number of software timers rather than the number of usable software timers
 */
#ifndef LOSCFG_BASE_CORE_SWTMR_LIMIT
#define LOSCFG_BASE_CORE_SWTMR_LIMIT 1024
#endif
/**
 * @ingroup los_config
 * Max number of software timers ID
 *
 * 0xFFFF: max number of all software timers
 */
#ifndef OS_SWTMR_MAX_TIMERID
#define OS_SWTMR_MAX_TIMERID ((0xFFFF / LOSCFG_BASE_CORE_SWTMR_LIMIT) * LOSCFG_BASE_CORE_SWTMR_LIMIT)
#endif
/**
 * @ingroup los_config
 * Maximum size of a software timer queue
 */
#ifndef OS_SWTMR_HANDLE_QUEUE_SIZE
#define OS_SWTMR_HANDLE_QUEUE_SIZE LOSCFG_BASE_CORE_SWTMR_LIMIT
#endif
#endif


/****************************** Memory module configuration **************************/
/**
 * @ingroup los_config
 * Starting address of the system memory
 */
#ifndef OS_SYS_MEM_ADDR
#define OS_SYS_MEM_ADDR                        (&m_aucSysMem1[0])
#endif

/**
 * @ingroup los_config
 * Memory size
 */
#ifndef OS_SYS_MEM_SIZE
#define OS_SYS_MEM_SIZE \
    ((OS_SYS_FUNC_ADDR_END) - (((UINTPTR)&__bss_end + (64 - 1)) & ~(64 - 1)))
#endif

/****************************** SMP module configuration **************************/
#ifdef LOSCFG_KERNEL_SMP
#define LOSCFG_KERNEL_CORE_NUM                          LOSCFG_KERNEL_SMP_CORE_NUM
#else
#define LOSCFG_KERNEL_CORE_NUM                          1
#endif

#define LOSCFG_KERNEL_CPU_MASK                          ((1 << LOSCFG_KERNEL_CORE_NUM) - 1)

/**
 * @ingroup los_config
 * Version number
 */
#define _T(x)                                   x
#define KERNEL_NAME                            "Huawei LiteOS"
#define KERNEL_NODE_NAME                       "hisilicon"
#define KERNEL_SEP                             " "
#define _V(v)                                  _T(KERNEL_NAME)_T(KERNEL_SEP)_T(v)

/**
 * @ingroup los_config
 * The Version number of Public
 */
#define KERNEL_MAJOR                     2
#define KERNEL_MINOR                     0
#define KERNEL_PATCH                     0
#define KERNEL_ITRE                      37

#define VERSION_NUM(a, b, c, d) (((a) << 24) | ((b) << 16) | (c) << 8 | (d))
#define KERNEL_OPEN_VERSION_NUM VERSION_NUM(KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE)

/**
 * @ingroup los_config
 * The container limit
 */
#ifndef LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT
#define LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT  10
#endif

/****************************** Exception information configuration ******************************/
#ifdef LOSCFG_SAVE_EXCINFO
/**
 * @ingroup los_config
 * the size of space for recording exception information
 */
#define EXCINFO_RECORD_BUF_SIZE (16 * 1024)

/**
 * @ingroup los_config
 * the address of space for recording exception information
 * @attention
 * <ul>
 * <li> if uses, the address must be valid in flash, and it should not overlap with other addresses
 * used to store valid information. </li>
 * </ul>
 *
 */
#define EXCINFO_RECORD_ADDR (0xffffffff)

/**
 * @ingroup los_config
 * @brief  define the type of functions for reading or writing exception information.
 *
 * @par Description:
 * <ul>
 * <li>This definition is used to declare the type of functions for reading or writing exception information</li>
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
 * <ul><li>los_config.h: the header file that contains the type definition.</li></ul>
 * @see
 */
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
VOID LOS_ExcInfoRegHook(UINT32 startAddr, UINT32 space, CHAR *buf, log_read_write_fn hook);
#endif

extern UINT32 OsMain(VOID);

typedef VOID (*SystemRebootFunc)(VOID);
VOID OsSetRebootHook(SystemRebootFunc func);
SystemRebootFunc OsGetRebootHook(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_CONFIG_H */
