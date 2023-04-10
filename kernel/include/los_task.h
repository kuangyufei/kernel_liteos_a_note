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

/**
 * @defgroup los_task Task
 * @ingroup kernel
 */

#ifndef _LOS_TASK_H
#define _LOS_TASK_H

#include "los_base.h"
#include "los_list.h"
#include "los_sys.h"
#include "los_tick.h"
#include "los_event.h"
#include "los_memory.h"
#include "los_err.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//cpuid转换 2 -> 0100
#define CPUID_TO_AFFI_MASK(cpuid)               (0x1u << (cpuid))

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is automatically deleted.
 */
#define LOS_TASK_STATUS_DETACHED                0x0U //独立模式,自动删除

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is joinable.
 */
#define LOS_TASK_ATTR_JOINABLE                  0x80000000 //联合模式,可由其他任务删除

/**
 * @ingroup los_task
 * Task error code: Insufficient memory for task creation.
 *
 * Value: 0x03000200
 *
 * Solution: Allocate bigger memory partition to task creation.
 */
#define LOS_ERRNO_TSK_NO_MEMORY                 LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x00)

/**
 * @ingroup los_task
 * Task error code: Null parameter.
 *
 * Value: 0x02000201
 *
 * Solution: Check the parameter.
 */
#define LOS_ERRNO_TSK_PTR_NULL                  LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x01)

/**
 * @ingroup los_task
 * Task error code: The task stack is not aligned.
 *
 * Value: 0x02000202
 *
 * Solution: Align the task stack.
 */
#define LOS_ERRNO_TSK_STKSZ_NOT_ALIGN           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x02)

/**
 * @ingroup los_task
 * Task error code: Incorrect task priority.
 *
 * Value: 0x02000203
 *
 * Solution: Re-configure the task priority by referring to the priority range.
 */
#define LOS_ERRNO_TSK_PRIOR_ERROR               LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x03)

/**
 * @ingroup los_task
 * Task error code: The task entrance is NULL.
 *
 * Value: 0x02000204
 *
 * Solution: Define the task entrance function.
 */
#define LOS_ERRNO_TSK_ENTRY_NULL                LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x04)

/**
 * @ingroup los_task
 * Task error code: The task name is NULL.
 *
 * Value: 0x02000205
 *
 * Solution: Set the task name.
 */
#define LOS_ERRNO_TSK_NAME_EMPTY                LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x05)

/**
 * @ingroup los_task
 * Task error code: The task stack size is too small.
 *
 * Value: 0x02000206
 *
 * Solution: Expand the task stack.
 */
#define LOS_ERRNO_TSK_STKSZ_TOO_SMALL           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x06)

/**
 * @ingroup los_task
 * Task error code: Invalid task ID.
 *
 * Value: 0x02000207
 *
 * Solution: Check the task ID.
 */
#define LOS_ERRNO_TSK_ID_INVALID                LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x07)

/**
 * @ingroup los_task
 * Task error code: The task is already suspended.
 *
 * Value: 0x02000208
 *
 * Solution: Suspend the task after it is resumed.
 */
#define LOS_ERRNO_TSK_ALREADY_SUSPENDED         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x08)

/**
 * @ingroup los_task
 * Task error code: The task is not suspended.
 *
 * Value: 0x02000209
 *
 * Solution: Suspend the task.
 */
#define LOS_ERRNO_TSK_NOT_SUSPENDED             LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x09)

/**
 * @ingroup los_task
 * Task error code: The task is not created.
 *
 * Value: 0x0200020a
 *
 * Solution: Create the task.
 */
#define LOS_ERRNO_TSK_NOT_CREATED               LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0a)

/**
 * @ingroup los_task
 * Task error code: The task is locked when it is being deleted.
 *
 * Value: 0x0300020b
 *
 * Solution: Unlock the task.
 */
#define LOS_ERRNO_TSK_DELETE_LOCKED             LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x0b)

/**
 * @ingroup los_task
 * Task error code: The task message is nonzero.
 *
 * Value: 0x0200020c
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_MSG_NONZERO               LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0c)

/**
 * @ingroup los_task
 * Task error code: The task delay occurs during an interrupt.
 *
 * Value: 0x0300020d
 *
 * Solution: Perform this operation after exiting from the interrupt.
 */
#define LOS_ERRNO_TSK_DELAY_IN_INT              LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x0d)

/**
 * @ingroup los_task
 * Task error code: The task delay occurs when the task is locked.
 *
 * Value: 0x0200020e
 *
 * Solution: Perform this operation after unlocking the task.
 */
#define LOS_ERRNO_TSK_DELAY_IN_LOCK             LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0e)

/**
 * @ingroup los_task
 * Task error code: The task yeild occurs when the task is locked.
 * Value: 0x0200020f
 *
 * Solution: Check the task.
 */
#define LOS_ERRNO_TSK_YIELD_IN_LOCK             LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0f)

/**
 * @ingroup los_task
 * Task error code: Only one task or no task is available for scheduling.
 *
 * Value: 0x02000210
 *
 * Solution: Increase the number of tasks.
 */
#define LOS_ERRNO_TSK_YIELD_NOT_ENOUGH_TASK     LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x10)

/**
 * @ingroup los_task
 * Task error code: No free task control block is available.
 *
 * Value: 0x02000211
 *
 * Solution: Increase the number of task control blocks.
 */
#define LOS_ERRNO_TSK_TCB_UNAVAILABLE           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x11)

/**
 * @ingroup los_task
 * Task error code: The task hook function is not matchable.
 *
 * Value: 0x02000212
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_HOOK_NOT_MATCH            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x12)

/**
 * @ingroup los_task
 * Task error code: The number of task hook functions exceeds the permitted upper limit.
 *
 * Value: 0x02000213
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_HOOK_IS_FULL              LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x13)

/**
 * @ingroup los_task
 * Task error code: The operation is performed on the system-level task.
 *       old usage: The operation is performed on the idle task (LOS_ERRNO_TSK_OPERATE_IDLE)
 *
 * Value: 0x02000214
 *
 * Solution: Check the task ID and do not operate on the system-level task.
 */
#define LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK       LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x14)

/**
 * @ingroup los_task
 * Task error code: The task that is being suspended is locked.
 *
 * Value: 0x03000215
 *
 * Solution: Suspend the task after unlocking the task.
 */
#define LOS_ERRNO_TSK_SUSPEND_LOCKED            LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x15)

/**
 * @ingroup los_task
 * Task error code: The task stack fails to be freed.
 *
 * Value: 0x02000217
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_FREE_STACK_FAILED         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x17)

/**
 * @ingroup los_task
 * Task error code: The task stack area is too small.
 *
 * Value: 0x02000218
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_STKAREA_TOO_SMALL         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x18)

/**
 * @ingroup los_task
 * Task error code: The task fails to be activated.
 *
 * Value: 0x03000219
 *
 * Solution: Perform task switching after creating an idle task.
 */
#define LOS_ERRNO_TSK_ACTIVE_FAILED             LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x19)

/**
 * @ingroup los_task
 * Task error code: Too many task configuration items.
 *
 * Value: 0x0200021a
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_CONFIG_TOO_MANY           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1a)

/**
 * @ingroup los_task
 * Task error code:
 *
 * Value: 0x0200021b
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_CP_SAVE_AREA_NOT_ALIGN    LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1b)

/**
 * @ingroup los_task
 * Task error code:
 *
 * Value: 0x0200021d
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_MSG_Q_TOO_MANY            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1d)

/**
 * @ingroup los_task
 * Task error code:
 *
 * Value: 0x0200021e
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_CP_SAVE_AREA_NULL         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1e)

/**
 * @ingroup los_task
 * Task error code:
 *
 * Value: 0x0200021f
 *
 * Solution: This error code is not in use temporarily.
 */
#define LOS_ERRNO_TSK_SELF_DELETE_ERR           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1f)

/**
 * @ingroup los_task
 * Task error code: The task stack size is too large.
 *
 * Value: 0x02000220
 *
 * Solution: shrink the task stack size parameter.
 */
#define LOS_ERRNO_TSK_STKSZ_TOO_LARGE           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x20)

/**
 * @ingroup los_task
 * Task error code: Suspending software timer task is not allowed.
 *
 * Value: 0x02000221
 *
 * Solution: Check the task ID and do not suspend software timer task.
 */
#define LOS_ERRNO_TSK_SUSPEND_SWTMR_NOT_ALLOWED LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x21)

/**
 * @ingroup los_task
 * Task error code: The task delay occurs in software timer task.
 *
 * Value: 0x03000222
 *
 * Solution: Don't call Los_TaskDelay in software timer callback.
 */
#define LOS_ERRNO_TSK_DELAY_IN_SWTMR_TSK        LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x22)

/**
 * @ingroup los_task
 * Task error code: The cpu affinity mask is incorrect.
 *
 * Value: 0x03000223
 *
 * Solution: Please set the correct cpu affinity mask.
 */
#define LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR     LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x23)

/**
 * @ingroup los_task
 * Task error code: Task yeild in int is not permited, which will result in unexpected result.
 *
 * Value: 0x02000224
 *
 * Solution: Don't call LOS_TaskYield in Interrupt.
 */
#define LOS_ERRNO_TSK_YIELD_IN_INT              LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x24)

/**
* @ingroup los_task
* Task error code: Task sync resource (semaphore) allocated failed.
*
* Value: 0x02000225
*
* Solution: Expand LOSCFG_BASE_IPC_SEM_LIMIT.
*/
#define LOS_ERRNO_TSK_MP_SYNC_RESOURCE          LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x25)

/**
* @ingroup los_task
* Task error code: Task sync failed on operating running task across cores.
* 
* Value: 0x02000226
*
* Solution: Check task delete can be handled in user's scenario.
*/ //跨核心运行任务时任务同步失败,检查任务删除可以在用户场景中处理
#define LOS_ERRNO_TSK_MP_SYNC_FAILED            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x26)

/**
* @ingroup los_task
* Task error code: Task wait and timeout.
*
* Value: 0x02000227
*
* Solution: Returns the timeout.
*/
#define LOS_ERRNO_TSK_TIMEOUT                   LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x27)

/**
* @ingroup los_task
* Task error code: A task that is not in the current process.
*
* Value: 0x02000228
*
* Solution: tasks that are not part of the current process are not allowed to operate.
*/
#define LOS_ERRNO_TSK_NOT_SELF_PROCESS          LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x28)

/**
* @ingroup  los_task
* @brief Define the type of a task entrance function.
*
* @par Description:
* This API is used to define the type of a task entrance function and call it after a task is created and triggered.
* @attention None.
*
* @param  param1 [IN] Type #UINTPTR The first parameter passed to the task handling function.
* @param  param2 [IN] Type #UINTPTR The second parameter passed to the task handling function.
* @param  param3 [IN] Type #UINTPTR The third parameter passed to the task handling function.
* @param  param4 [IN] Type #UINTPTR The fourth parameter passed to the task handling function.
*
* @retval None.
* @par Dependency:
* <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
* @see
*/
typedef VOID *(*TSK_ENTRY_FUNC)(UINTPTR param1,
                                UINTPTR param2,
                                UINTPTR param3,
                                UINTPTR param4);

/**
 * @ingroup los_task
 * You are not allowed to add any fields and adjust fields to the structure 
 	| 不允许在结构中添加任何字段和调整字段 @note_thinking 为什么呢 ?
 */
typedef struct {//用户态栈信息,(按递减满栈方式注解)
    UINTPTR         userArea;	///< 用户空间的堆区开始位置
    UINTPTR         userSP;		///< 用户空间当前栈指针位置
    UINTPTR         userMapBase;///< 用户空间的栈顶位置.
    UINT32          userMapSize;///< 用户空间的栈大小,栈底 = userMapBase + userMapSize
} UserTaskParam;

/**
 * @ingroup los_task
 * Define the structure of the parameters used for task creation.
 *
 * Information of specified parameters passed in during task creation.
 */
typedef struct tagTskInitParam {//Task的初始化参数
    TSK_ENTRY_FUNC  pfnTaskEntry;  /**< Task entrance function | 任务的入口函数*/
    UINT16          usTaskPrio;    /**< Task priority | 任务优先级*/
    UINT16          policy;        /**< Task policy | 任务调度方式*/
    UINTPTR         auwArgs[4];    /**< Task parameters, of which the maximum number is four | 入口函数的参数,最多四个*/
    UINT32          uwStackSize;   /**< Task stack size | 栈大小*/
    CHAR            *pcName;       /**< Task name | 任务名称*/
#ifdef LOSCFG_KERNEL_SMP
    UINT16          usCpuAffiMask; /**< Task cpu affinity mask | 任务cpu亲和力掩码        */
#endif
    UINT32          uwResved;      /**< It is automatically deleted if set to LOS_TASK_STATUS_DETACHED.
                                        It is unable to be deleted if set to 0. | 如果设置为LOS_TASK_STATUS_DETACHED，则自动删除。如果设置为0，则无法删除*/
    UINT16          consoleID;     /**< The console id of task belongs  | 任务的控制台id所属*/
    UINTPTR          processID;	///< 进程ID
    UserTaskParam   userParam;	///< 任务用户态运行时任何参数
    /* edf parameters */
    UINT32          runTimeUs;
    UINT64          deadlineUs;
    UINT64          periodUs;
} TSK_INIT_PARAM_S;

typedef struct {
    union {
        INT32 priority;
        INT32 runTimeUs;
    };
    INT32 deadlineUs;
    INT32 periodUs;
} LosSchedParam;

/**
 * @ingroup los_task
 * Task name length
 *
 */
#define LOS_TASK_NAMELEN                        32

/**
 * @ingroup los_task
 * Task information structure.
 *
 */
typedef struct tagTskInfo { //主要用于 shell task -a 使用 
    CHAR                acName[LOS_TASK_NAMELEN];   /**< Task entrance function                                     */
    UINT32              uwTaskID;                   /**< Task ID                                                    */
    UINT16              usTaskStatus;               /**< Task status                                                */
    UINT16              usTaskPrio;                 /**< Task priority                                              */
    VOID                *pTaskSem;                  /**< Semaphore pointer                                          */
    VOID                *pTaskMux;                  /**< Mutex pointer                                              */
    VOID                *taskEvent;                 /**< Event                                                      */
    UINT32              uwEventMask;                /**< Event mask                                                 */
    UINT32              uwStackSize;                /**< Task stack size                                            */
    UINTPTR             uwTopOfStack;               /**< Task stack top                                             */
    UINTPTR             uwBottomOfStack;            /**< Task stack bottom                                          */
    UINTPTR             uwSP;                       /**< Task SP pointer                                            */
    UINT32              uwCurrUsed;                 /**< Current task stack usage                                   */
    UINT32              uwPeakUsed;                 /**< Task stack usage peak                                      */
    BOOL                bOvf;                       /**< Flag that indicates whether a task stack overflow occurs   */
} TSK_INFO_S;

/**
 * @ingroup  los_task
 * @brief Create a task and suspend.
 *
 * @par Description:
 * This API is used to create a task and suspend it. This task will not be added to the queue of ready tasks
 * before resume it.
 *
 * @attention
 * <ul>
 * <li>During task creation, the task control block and task stack of the task that is previously automatically deleted
 * are deallocated.</li>
 * <li>The task name is a pointer and is not allocated memory.</li>
 * <li>If the size of the task stack of the task to be created is 0, configure #LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE
 * to specify the default task stack size. The stack size should be a reasonable value, if the size is too large, may
 * cause memory exhaustion.</li>
 * <li>The task stack size must be aligned on the boundary of 8 bytes. The size is determined by whether it is big
 * enough to avoid task stack overflow.</li>
 * <li>Less parameter value indicates higher task priority.</li>
 * <li>The task name cannot be null.</li>
 * <li>The pointer to the task executing function cannot be null.</li>
 * <li>The two parameters of this interface is pointer, it should be a correct value, otherwise, the system may be
 * abnormal.</li>
 * </ul>
 *
 * @param  taskID    [OUT] Type  #UINT32 * Task ID.
 * @param  initParam [IN]  Type  #TSK_INIT_PARAM_S * Parameter for task creation.
 *
 * @retval #LOS_ERRNO_TSK_ID_INVALID        Invalid Task ID, param taskID is NULL.
 * @retval #LOS_ERRNO_TSK_PTR_NULL          Param initParam is NULL.
 * @retval #LOS_ERRNO_TSK_NAME_EMPTY        The task name is NULL.
 * @retval #LOS_ERRNO_TSK_ENTRY_NULL        The task entrance is NULL.
 * @retval #LOS_ERRNO_TSK_PRIOR_ERROR       Incorrect task priority.
 * @retval #LOS_ERRNO_TSK_STKSZ_TOO_LARGE   The task stack size is too large.
 * @retval #LOS_ERRNO_TSK_STKSZ_TOO_SMALL   The task stack size is too small.
 * @retval #LOS_ERRNO_TSK_TCB_UNAVAILABLE   No free task control block is available.
 * @retval #LOS_ERRNO_TSK_NO_MEMORY         Insufficient memory for task creation.
 * @retval #LOS_OK                          The task is successfully created.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * <ul><li>los_config.h: the header file that contains system configuration items.</li></ul>
 * @see LOS_TaskDelete
 */
extern UINT32 LOS_TaskCreateOnly(UINT32 *taskID, TSK_INIT_PARAM_S *initParam);

/**
 * @ingroup  los_task
 * @brief Create a task.
 *
 * @par Description:
 * This API is used to create a task. If the priority of the task created after system initialized is higher than
 * the current task and task scheduling is not locked, it is scheduled for running.
 * If not, the created task is added to the queue of ready tasks.
 *
 * @attention
 * <ul>
 * <li>During task creation, the task control block and task stack of the task that is previously automatically
 * deleted are deallocated.</li>
 * <li>The task name is a pointer and is not allocated memory.</li>
 * <li>If the size of the task stack of the task to be created is 0, configure #LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE
 * to specify the default task stack size.</li>
 * <li>The task stack size must be aligned on the boundary of 8 bytes. The size is determined by whether it is big
 * enough to avoid task stack overflow.</li>
 * <li>Less parameter value indicates higher task priority.</li>
 * <li>The task name cannot be null.</li>
 * <li>The pointer to the task executing function cannot be null.</li>
 * <li>The two parameters of this interface is pointer, it should be a correct value, otherwise, the system may be
 * abnormal.</li>
 * </ul>
 *
 * @param  taskID    [OUT] Type  #UINT32 * Task ID.
 * @param  initParam [IN]  Type  #TSK_INIT_PARAM_S * Parameter for task creation.
 *
 * @retval #LOS_ERRNO_TSK_ID_INVALID        Invalid Task ID, param taskID is NULL.
 * @retval #LOS_ERRNO_TSK_PTR_NULL          Param initParam is NULL.
 * @retval #LOS_ERRNO_TSK_NAME_EMPTY        The task name is NULL.
 * @retval #LOS_ERRNO_TSK_ENTRY_NULL        The task entrance is NULL.
 * @retval #LOS_ERRNO_TSK_PRIOR_ERROR       Incorrect task priority.
 * @retval #LOS_ERRNO_TSK_STKSZ_TOO_LARGE   The task stack size is too large.
 * @retval #LOS_ERRNO_TSK_STKSZ_TOO_SMALL   The task stack size is too small.
 * @retval #LOS_ERRNO_TSK_TCB_UNAVAILABLE   No free task control block is available.
 * @retval #LOS_ERRNO_TSK_NO_MEMORY         Insufficient memory for task creation.
 * @retval #LOS_OK                          The task is successfully created.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * <ul><li>los_config.h: the header file that contains system configuration items.</li></ul>
 * @see LOS_TaskDelete
 */
extern UINT32 LOS_TaskCreate(UINT32 *taskID, TSK_INIT_PARAM_S *initParam);

/**
 * @ingroup  los_task
 * @brief Resume a task.
 *
 * @par Description:
 * This API is used to resume a suspended task.
 *
 * @attention
 * <ul>
 * <li>If the task is delayed or blocked, resume the task without adding it to the queue of ready tasks.</li>
 * <li>If the priority of the task resumed after system initialized is higher than the current task and task scheduling
 * is not locked, it is scheduled for running.</li>
 * </ul>
 *
 * @param  taskID [IN] Type #UINT32 Task ID. The task id value is obtained from task creation.
 *
 * @retval #LOS_ERRNO_TSK_ID_INVALID        Invalid Task ID
 * @retval #LOS_ERRNO_TSK_NOT_CREATED       The task is not created.
 * @retval #LOS_ERRNO_TSK_NOT_SUSPENDED     The task is not suspended.
 * @retval #LOS_OK                          The task is successfully resumed.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskSuspend
 */
extern UINT32 LOS_TaskResume(UINT32 taskID);

/**
 * @ingroup  los_task
 * @brief Suspend a task.
 *
 * @par Description:
 * This API is used to suspend a specified task, and the task will be removed from the queue of ready tasks.
 *
 * @attention
 * <ul>
 * <li>The task that is running and locked cannot be suspended.</li>
 * <li>The idle task and swtmr task cannot be suspended.</li>
 * </ul>
 *
 * @param  taskID [IN] Type #UINT32 Task ID. The task id value is obtained from task creation.
 *
 * @retval #LOS_ERRNO_TSK_OPERATE_IDLE                  Check the task ID and do not operate on the idle task.
 * @retval #LOS_ERRNO_TSK_SUSPEND_SWTMR_NOT_ALLOWED     Check the task ID and do not operate on the swtmr task.
 * @retval #LOS_ERRNO_TSK_ID_INVALID                    Invalid Task ID
 * @retval #LOS_ERRNO_TSK_NOT_CREATED                   The task is not created.
 * @retval #LOS_ERRNO_TSK_ALREADY_SUSPENDED             The task is already suspended.
 * @retval #LOS_ERRNO_TSK_SUSPEND_LOCKED                The task being suspended is current task and task scheduling
 *                                                      is locked.
 * @retval #LOS_OK                                      The task is successfully suspended.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskResume
 */
extern UINT32 LOS_TaskSuspend(UINT32 taskID);

/**
 * @ingroup  los_task
 * @brief Delete a task.
 *
 * @par Description:
 * This API is used to delete a specified task and release the resources for its task stack and task control block.
 *
 * @attention
 * <ul>
 * <li>The idle task and swtmr task cannot be deleted.</li>
 * <li>If delete current task maybe cause unexpected error.</li>
 * <li>If a task get a mutex is deleted or automatically deleted before release this mutex, other tasks pended
 * this mutex maybe never be shchduled.</li>
 * </ul>
 *
 * @param  taskID [IN] Type #UINT32 Task ID. The task id value is obtained from task creation.
 *
 * @retval #LOS_ERRNO_TSK_OPERATE_IDLE                  Check the task ID and do not operate on the idle task.
 * @retval #LOS_ERRNO_TSK_SUSPEND_SWTMR_NOT_ALLOWED     Check the task ID and do not operate on the swtmr task.
 * @retval #LOS_ERRNO_TSK_ID_INVALID                    Invalid Task ID
 * @retval #LOS_ERRNO_TSK_NOT_CREATED                   The task is not created.
 * @retval #LOS_ERRNO_TSK_DELETE_LOCKED                 The task being deleted is current task and task scheduling
 *                                                      is locked.
 * @retval #LOS_OK                                      The task is successfully deleted.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskCreate | LOS_TaskCreateOnly
 */
extern UINT32 LOS_TaskDelete(UINT32 taskID);

/**
 * @ingroup  los_task
 * @brief Delay a task.
 *
 * @par Description:
 * This API is used to delay the execution of the current task. The task is able to be scheduled after it is delayed
 * for a specified number of Ticks.
 *
 * @attention
 * <ul>
 * <li>The task fails to be delayed if it is being delayed during interrupt processing or it is locked.</li>
 * <li>If 0 is passed in and the task scheduling is not locked, execute the next task in the queue of tasks with
 * the same priority of the current task.
 * If no ready task with the priority of the current task is available, the task scheduling will not occur, and the
 * current task continues to be executed.</li>
 * <li>Using the interface before system initialized is not allowed.</li>
 * <li>DO NOT call this API in software timer callback. </li>
 * </ul>
 *
 * @param  tick [IN] Type #UINT32 Number of Ticks for which the task is delayed.
 *
 * @retval #LOS_ERRNO_TSK_DELAY_IN_INT              The task delay occurs during an interrupt.
 * @retval #LOS_ERRNO_TSK_DELAY_IN_LOCK             The task delay occurs when the task scheduling is locked.
 * @retval #LOS_ERRNO_TSK_ID_INVALID                Invalid Task ID
 * @retval #LOS_ERRNO_TSK_YIELD_NOT_ENOUGH_TASK     No tasks with the same priority is available for scheduling.
 * @retval #LOS_OK                                  The task is successfully delayed.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_TaskDelay(UINT32 tick);

/**
 * @ingroup  los_task
 * @brief Lock the task scheduling.
 *
 * @par Description:
 * This API is used to lock the task scheduling. Task switching will not occur if the task scheduling is locked.
 *
 * @attention
 * <ul>
 * <li>If the task scheduling is locked, but interrupts are not disabled, tasks are still able to be interrupted.</li>
 * <li>One is added to the number of task scheduling locks if this API is called. The number of locks is decreased by
 * one if the task scheduling is unlocked. Therefore, this API should be used together with LOS_TaskUnlock.</li>
 * </ul>
 *
 * @param  None.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskUnlock
 */
extern VOID LOS_TaskLock(VOID);

/**
 * @ingroup  los_task
 * @brief Unlock the task scheduling.
 *
 * @par Description:
 * This API is used to unlock the task scheduling. Calling this API will decrease the number of task locks by one.
 * If a task is locked more than once, the task scheduling will be unlocked only when the number of locks becomes zero.
 *
 * @attention
 * <ul>
 * <li>The number of locks is decreased by one if this API is called. One is added to the number of task scheduling
 * locks if the task scheduling is locked. Therefore, this API should be used together with LOS_TaskLock.</li>
 * </ul>
 *
 * @param  None.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskLock
 */
extern VOID LOS_TaskUnlock(VOID);

/**
 * @ingroup  los_task
 * @brief Set a task priority.
 *
 * @par Description:
 * This API is used to set the priority of a specified task.
 *
 * @attention
 * <ul>
 * <li>If the set priority is higher than the priority of the current running task, task scheduling
 * probably occurs.</li>
 * <li>Changing the priority of the current running task also probably causes task scheduling.</li>
 * <li>Using the interface to change the priority of software timer task and idle task is not allowed.</li>
 * <li>Using the interface in the interrupt is not allowed.</li>
 * </ul>
 *
 * @param  taskID   [IN] Type #UINT32 Task ID. The task id value is obtained from task creation.
 * @param  taskPrio [IN] Type #UINT16 Task priority.
 *
 * @retval #LOS_ERRNO_TSK_PRIOR_ERROR    Incorrect task priority.Re-configure the task priority
 * @retval #LOS_ERRNO_TSK_OPERATE_IDLE   Check the task ID and do not operate on the idle task.
 * @retval #LOS_ERRNO_TSK_ID_INVALID     Invalid Task ID
 * @retval #LOS_ERRNO_TSK_NOT_CREATED    The task is not created.
 * @retval #LOS_OK                       The task priority is successfully set to a specified priority.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskPriSet
 */
extern UINT32 LOS_TaskPriSet(UINT32 taskID, UINT16 taskPrio);

/**
 * @ingroup  los_task
 * @brief Set the priority of the current running task to a specified priority.
 *
 * @par Description:
 * This API is used to set the priority of the current running task to a specified priority.
 *
 * @attention
 * <ul>
 * <li>Changing the priority of the current running task probably causes task scheduling.</li>
 * <li>Using the interface to change the priority of software timer task and idle task is not allowed.</li>
 * <li>Using the interface in the interrupt is not allowed.</li>
 * </ul>
 *
 * @param  taskPrio [IN] Type #UINT16 Task priority.
 *
 * @retval #LOS_ERRNO_TSK_PRIOR_ERROR     Incorrect task priority.Re-configure the task priority
 * @retval #LOS_ERRNO_TSK_OPERATE_IDLE    Check the task ID and do not operate on the idle task.
 * @retval #LOS_ERRNO_TSK_ID_INVALID      Invalid Task ID
 * @retval #LOS_ERRNO_TSK_NOT_CREATED     The task is not created.
 * @retval #LOS_OK                        The priority of the current running task is successfully set to a specified
 *                                        priority.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskPriGet
 */
extern UINT32 LOS_CurTaskPriSet(UINT16 taskPrio);

/**
 * @ingroup  los_task
 * @brief Change the scheduling sequence of tasks with the same priority.
 *
 * @par Description:
 * This API is used to move current task in a queue of tasks with the same priority to the tail of the queue of ready
 * tasks.
 *
 * @attention
 * <ul>
 * <li>At least two ready tasks need to be included in the queue of ready tasks with the same priority. If the
 * less than two ready tasks are included in the queue, an error is reported.</li>
 * </ul>
 *
 * @param  None.
 *
 * @retval #LOS_ERRNO_TSK_ID_INVALID                    Invalid Task ID
 * @retval #LOS_ERRNO_TSK_YIELD_NOT_ENOUGH_TASK         No tasks with the same priority is available for scheduling.
 * @retval #LOS_OK                                      The scheduling sequence of tasks with same priority is
 *                                                      successfully changed.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_TaskYield(VOID);

/**
 * @ingroup  los_task
 * @brief Obtain a task priority.
 *
 * @par Description:
 * This API is used to obtain the priority of a specified task.
 *
 * @attention None.
 *
 * @param  taskID [IN] Type #UINT32 Task ID. The task id value is obtained from task creation.
 *
 * @retval #OS_INVALID      The task priority fails to be obtained.
 * @retval #UINT16          The task priority.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskPriSet
 */
extern UINT16 LOS_TaskPriGet(UINT32 taskID);

/**
 * @ingroup  los_task
 * @brief Obtain current running task ID.
 *
 * @par Description:
 * This API is used to obtain the ID of current running task.
 *
 * @attention
 * <ul>
 * <li> This interface should not be called before system initialized.</li>
 * </ul>
 *
 * @retval #LOS_ERRNO_TSK_ID_INVALID    Invalid Task ID.
 * @retval #UINT32                      Task ID.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_CurTaskIDGet(VOID);

/**
 * @ingroup  los_task
 * @brief Gets the maximum number of threads supported by the system.
 *
 * @par Description:
 * This API is used to gets the maximum number of threads supported by the system.
 *
 * @attention
 * <ul>
 * <li> This interface should not be called before system initialized.</li>
 * </ul>
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_GetSystemTaskMaximum(VOID);

/**
 * @ingroup  los_task
 * @brief Obtain a task information structure.
 *
 * @par Description:
 * This API is used to obtain a task information structure.
 *
 * @attention
 * <ul>
 * <li>One parameter of this interface is a pointer, it should be a correct value, otherwise, the system may be
 * abnormal.</li>
 * </ul>
 *
 * @param  taskID    [IN]  Type  #UINT32 Task ID. The task id value is obtained from task creation.
 * @param  taskInfo [OUT] Type  #TSK_INFO_S* Pointer to the task information structure to be obtained.
 *
 * @retval #LOS_ERRNO_TSK_PTR_NULL        Null parameter.
 * @retval #LOS_ERRNO_TSK_ID_INVALID      Invalid task ID.
 * @retval #LOS_ERRNO_TSK_NOT_CREATED     The task is not created.
 * @retval #LOS_OK                        The task information structure is successfully obtained.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_TaskInfoGet(UINT32 taskID, TSK_INFO_S *taskInfo);


/**
 * @ingroup  los_task
 * @brief Set the affinity mask of the task scheduling cpu.
 *
 * @par Description:
 * This API is used to set the affinity mask of the task scheduling cpu.
 *
 * @attention
 * <ul>
 * <li>If any low LOSCFG_KERNEL_CORE_NUM bit of the mask is not setted, an error is reported.</li>
 * </ul>
 *
 * @param  uwTaskID      [IN]  Type  #UINT32 Task ID. The task id value is obtained from task creation.
 * @param  usCpuAffiMask [IN]  Type  #UINT32 The scheduling cpu mask.The low to high bit of the mask corresponds to
 *                             the cpu number, the high bit that exceeding the CPU number is ignored.
 *
 * @retval #LOS_ERRNO_TSK_ID_INVALID                Invalid task ID.
 * @retval #LOS_ERRNO_TSK_NOT_CREATED               The task is not created.
 * @retval #LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR     The task cpu affinity mask is incorrect.
 * @retval #LOS_OK                                  The task cpu affinity mask is successfully setted.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskCpuAffiGet
 */
extern UINT32 LOS_TaskCpuAffiSet(UINT32 uwTaskID, UINT16 usCpuAffiMask);

/**
 * @ingroup  los_task
 * @brief Get the affinity mask of the task scheduling cpu.
 *
 * @par Description:
 * This API is used to get the affinity mask of the task scheduling cpu.
 *
 * @attention None.
 *
 * @param  taskID       [IN]  Type  #UINT32 Task ID. The task id value is obtained from task creation.
 *
 * @retval #0           The cpu affinity mask fails to be obtained.
 * @retval #UINT16      The scheduling cpu mask. The low to high bit of the mask corresponds to the cpu number.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TaskCpuAffiSet
 */
extern UINT16 LOS_TaskCpuAffiGet(UINT32 taskID);

/**
 * @ingroup  los_task
 * @brief Get the scheduling policy for the task.
 *
 * @par Description:
 * This API is used to get the scheduling policy for the task.
 *
 * @attention None.
 *
 * @param  taskID           [IN]  Type  #UINT32 Task ID. The task id value is obtained from task creation.
 *
 * @retval #-LOS_ESRCH  Invalid task id.
 * @retval #-LOS_EPERM  The process is not currently running.
 * @retval #INT32       the scheduling policy.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SetTaskScheduler
 */
extern INT32 LOS_GetTaskScheduler(INT32 taskID);

/**
 * @ingroup  los_task
 * @brief Set the scheduling policy and priority for the task.
 *
 * @par Description:
 * This API is used to set the scheduling policy and priority for the task.
 *
 * @attention None.
 *
 * @param  taskID       [IN]  Type  #UINT32 Task ID. The task id value is obtained from task creation.
 * @param  policy       [IN]  Type  #UINT16 Task scheduling policy.
 * @param  priority     [IN]  Type  #UINT16 Task scheduling priority.
 *
 * @retval -LOS_ESRCH       Invalid task id.
 * @retval -LOS_EOPNOTSUPP  Unsupported fields.
 * @retval -LOS_EPERM       The process is not currently running.
 * @retval #0               Set up the success.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_GetTaskScheduler
 */
extern INT32 LOS_SetTaskScheduler(INT32 taskID, UINT16 policy, UINT16 priority);

/**
 * @ingroup  los_task
 * @brief Trigger active task scheduling.
 *
 * @par Description:
 * This API is used to trigger active task scheduling.
 *
 * @attention None.
 *
 * @param None
 *
 * @retval Nobe
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 */
extern VOID LOS_Schedule(VOID);

/**
 * @ingroup  los_task
 * @brief Wait for the specified task to finish and reclaim its resources.
 *
 * @par Description:
 * This API is used to wait for the specified task to finish and reclaim its resources.
 *
 * @attention None.
 *
 * @param taskID [IN]  task ID.
 * @param retval [OUT] wait for the return value of the task.
 *
 * @retval LOS_OK      successful
 * @retval LOS_EINVAL  Invalid parameter or invalid operation
 * @retval LOS_EINTR   Disallow calls in interrupt handlers
 * @retval LOS_EPERM   Waiting tasks and calling tasks do not belong to the same process
 * @retval LOS_EDEADLK The waiting task is the same as the calling task
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 */
extern UINT32 LOS_TaskJoin(UINT32 taskID, UINTPTR *retval);

/**
 * @ingroup  los_task
 * @brief Change the joinable attribute of the task to detach.
 *
 * @par Description:
 * This API is used to change the joinable attribute of the task to detach.
 *
 * @attention None.
 *
 * @param taskID [IN] task ID.
 *
 * @retval LOS_OK      successful
 * @retval LOS_EINVAL  Invalid parameter or invalid operation
 * @retval LOS_EINTR   Disallow calls in interrupt handlers
 * @retval LOS_EPERM   Waiting tasks and calling tasks do not belong to the same process
 * @retval LOS_ESRCH   Cannot modify the Joinable attribute of a task that is waiting for completion.
 * @par Dependency:
 * <ul><li>los_task.h: the header file that contains the API declaration.</li></ul>
 */
extern UINT32 LOS_TaskDetach(UINT32 taskID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TASK_H */
