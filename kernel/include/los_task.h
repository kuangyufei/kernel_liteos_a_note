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

/**
 * @ingroup los_task
 * @brief 将CPU ID转换为亲和性掩码
 * @param cpuid CPU核心编号（从0开始）
 * @return UINT32 亲和性掩码（仅对应CPU ID的位为1，其余为0）
 * @par 计算示例：
 * CPU ID为0时返回0x00000001（二进制00000001）
 * CPU ID为3时返回0x00000008（二进制00001000）
 * @note 用于设置任务的CPU亲和性，指定任务可在哪些CPU上运行
 */
#define CPUID_TO_AFFI_MASK(cpuid)               (0x1u << (cpuid))

/**
 * @ingroup los_task
 * @brief 任务状态标志：任务自动删除
 * @note 当任务退出时，系统会自动回收其资源，无需手动调用删除接口
 */
#define LOS_TASK_STATUS_DETACHED                0x0U

/**
 * @ingroup los_task
 * @brief 任务属性标志：任务可连接
 * @note 置位此标志后，其他任务可通过LOS_TaskJoin()接口等待该任务结束并获取退出状态
 * @attention 与LOS_TASK_STATUS_DETACHED互斥，两者不能同时设置
 */
#define LOS_TASK_ATTR_JOINABLE                  0x80000000

/**
 * @ingroup los_task
 * @brief 任务错误码：创建任务时内存不足
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_FATAL (致命错误)
 * - 错误编号: 0x00
 * @par 十六进制值: 0x03000200 (十进制: 50331904)
 * @par 解决方案：
 * 增大任务创建内存分区大小，或通过LOSCFG_BASE_MEM_TSK_LIMIT调整系统任务内存池配置
 */
#define LOS_ERRNO_TSK_NO_MEMORY                 LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x00)

/**
 * @ingroup los_task
 * @brief 任务错误码：输入参数为空指针
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x01
 * @par 十六进制值: 0x02000201 (十进制: 33554945)
 * @par 解决方案：
 * 检查任务创建/控制接口的输入参数（如任务属性、入口函数等）是否为有效指针
 */
#define LOS_ERRNO_TSK_PTR_NULL                  LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x01)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务栈大小未按系统字长对齐
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x02
 * @par 十六进制值: 0x02000202 (十进制: 33554946)
 * @par 解决方案：
 * 将栈大小调整为系统字长的整数倍（32位系统对齐4字节，64位系统对齐8字节）
 * @note 可使用LOS_MEMBOX_ALIGNED()宏进行自动对齐
 */
#define LOS_ERRNO_TSK_STKSZ_NOT_ALIGN           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x02)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务优先级设置错误
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x03
 * @par 十六进制值: 0x02000203 (十进制: 33554947)
 * @par 解决方案：
 * 按系统配置的优先级范围设置（默认0-31，0为最高优先级）
 * @note 优先级范围由LOSCFG_BASE_CORE_TSK_PRIORITY_LEVEL宏定义
 */
#define LOS_ERRNO_TSK_PRIOR_ERROR               LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x03)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务入口函数为空
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x04
 * @par 十六进制值: 0x02000204 (十进制: 33554948)
 * @par 解决方案：
 * 定义有效的任务入口函数，函数原型为：VOID (*TSK_ENTRY_FUNC)(VOID *param)
 */
#define LOS_ERRNO_TSK_ENTRY_NULL                LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x04)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务名称为空
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x05
 * @par 十六进制值: 0x02000205 (十进制: 33554949)
 * @par 解决方案：
 * 通过TSK_ATTR_T结构体的pcName成员设置非空任务名称（建议不超过16字符）
 */
#define LOS_ERRNO_TSK_NAME_EMPTY                LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x05)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务栈大小过小
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x06
 * @par 十六进制值: 0x02000206 (十进制: 33554950)
 * @par 解决方案：
 * 增大栈大小参数，最小值需满足：
 * 系统最小栈大小（LOSCFG_BASE_CORE_TSK_MINSTACK_SIZE） + 任务上下文大小
 */
#define LOS_ERRNO_TSK_STKSZ_TOO_SMALL           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x06)

/**
 * @ingroup los_task
 * @brief 任务错误码：无效的任务ID
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x07
 * @par 十六进制值: 0x02000207 (十进制: 33554951)
 * @par 解决方案：
 * 检查任务ID是否在有效范围内（1 ~ LOSCFG_BASE_CORE_TSK_LIMIT - 1），0为无效ID
 */
#define LOS_ERRNO_TSK_ID_INVALID                LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x07)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务已处于挂起状态
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x08
 * @par 十六进制值: 0x02000208 (十进制: 33554952)
 * @par 解决方案：
 * 调用LOS_TaskResume()恢复任务后，再执行挂起操作
 * @note 系统维护挂起计数，多次挂起需对应多次恢复
 */
#define LOS_ERRNO_TSK_ALREADY_SUSPENDED         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x08)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务未处于挂起状态
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x09
 * @par 十六进制值: 0x02000209 (十进制: 33554953)
 * @par 解决方案：
 * 先调用LOS_TaskSuspend()挂起任务，再执行恢复操作
 */
#define LOS_ERRNO_TSK_NOT_SUSPENDED             LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x09)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务未创建
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x0a
 * @par 十六进制值: 0x0200020a (十进制: 33554954)
 * @par 解决方案：
 * 通过LOS_TaskCreate()或LOS_TaskCreateOnly()接口先创建任务
 */
#define LOS_ERRNO_TSK_NOT_CREATED               LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0a)

/**
 * @ingroup los_task
 * @brief 任务错误码：删除已加锁的任务
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_FATAL (致命错误)
 * - 错误编号: 0x0b
 * @par 十六进制值: 0x0300020b (十进制: 50331915)
 * @par 解决方案：
 * 调用LOS_TaskUnlock()释放任务锁后再执行删除操作
 * @attention 任务锁通常用于临界区保护，强制删除可能导致系统状态不一致
 */
#define LOS_ERRNO_TSK_DELETE_LOCKED             LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x0b)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务消息非空
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x0c
 * @par 十六进制值: 0x0200020c (十进制: 33554956)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务消息队列相关错误
 */
#define LOS_ERRNO_TSK_MSG_NONZERO               LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0c)

/**
 * @ingroup los_task
 * @brief 任务错误码：在中断中调用任务延迟
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_FATAL (致命错误)
 * - 错误编号: 0x0d
 * @par 十六进制值: 0x0300020d (十进制: 50331917)
 * @par 解决方案：
 * 中断上下文不允许阻塞操作，如需延迟可使用定时器或中断底半部机制
 */
#define LOS_ERRNO_TSK_DELAY_IN_INT              LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x0d)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务加锁状态下调用延迟
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x0e
 * @par 十六进制值: 0x0200020e (十进制: 33554958)
 * @par 解决方案：
 * 调用LOS_TaskUnlock()释放任务锁后再调用LOS_TaskDelay()
 */
#define LOS_ERRNO_TSK_DELAY_IN_LOCK             LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0e)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务加锁状态下调用让出CPU
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x0f
 * @par 十六进制值: 0x0200020f (十进制: 33554959)
 * @par 解决方案：
 * 调用LOS_TaskUnlock()释放任务锁后再调用LOS_TaskYield()
 */
#define LOS_ERRNO_TSK_YIELD_IN_LOCK             LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0f)

/**
 * @ingroup los_task
 * @brief 任务错误码：可调度任务数量不足
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x10
 * @par 十六进制值: 0x02000210 (十进制: 33554960)
 * @par 解决方案：
 * 创建更多就绪状态的任务，或检查其他任务是否异常占用CPU
 */
#define LOS_ERRNO_TSK_YIELD_NOT_ENOUGH_TASK     LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x10)

/**
 * @ingroup los_task
 * @brief 任务错误码：无可用任务控制块(TCB)
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x11
 * @par 十六进制值: 0x02000211 (十进制: 33554961)
 * @par 解决方案：
 * 通过LOSCFG_BASE_CORE_TSK_LIMIT宏增大系统最大任务数配置
 */
#define LOS_ERRNO_TSK_TCB_UNAVAILABLE           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x11)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务钩子函数不匹配
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x12
 * @par 十六进制值: 0x02000212 (十进制: 33554962)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务钩子注册/卸载相关错误
 */
#define LOS_ERRNO_TSK_HOOK_NOT_MATCH            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x12)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务钩子函数数量超出上限
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x13
 * @par 十六进制值: 0x02000213 (十进制: 33554963)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务钩子数量限制相关错误
 */
#define LOS_ERRNO_TSK_HOOK_IS_FULL              LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x13)

/**
 * @ingroup los_task
 * @brief 任务错误码：操作系统级任务
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x14
 * @par 十六进制值: 0x02000214 (十进制: 33554964)
 * @par 历史版本：
 * 早期版本用于标识操作空闲任务错误(LOS_ERRNO_TSK_OPERATE_IDLE)
 * @par 解决方案：
 * 检查任务ID，不允许对系统关键任务（如空闲任务、定时器任务）执行删除/挂起等操作
 */
#define LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK       LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x14)

/**
 * @ingroup los_task
 * @brief 任务错误码：挂起已加锁的任务
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_FATAL (致命错误)
 * - 错误编号: 0x15
 * @par 十六进制值: 0x03000215 (十进制: 50331925)
 * @par 解决方案：
 * 调用LOS_TaskUnlock()释放任务锁后再执行挂起操作
 */
#define LOS_ERRNO_TSK_SUSPEND_LOCKED            LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x15)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务栈释放失败
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x17
 * @par 十六进制值: 0x02000217 (十进制: 33554967)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务栈内存释放相关错误
 */
#define LOS_ERRNO_TSK_FREE_STACK_FAILED         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x17)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务栈区域过小
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x18
 * @par 十六进制值: 0x02000218 (十进制: 33554968)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务栈区域检查相关错误
 */
#define LOS_ERRNO_TSK_STKAREA_TOO_SMALL         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x18)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务激活失败
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_FATAL (致命错误)
 * - 错误编号: 0x19
 * @par 十六进制值: 0x03000219 (十进制: 50331929)
 * @par 解决方案：
 * 创建空闲任务后再执行任务切换操作，确保系统至少有一个可调度任务
 */
#define LOS_ERRNO_TSK_ACTIVE_FAILED             LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x19)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务配置项过多
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x1a
 * @par 十六进制值: 0x0200021a (十进制: 33554970)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务配置项数量检查相关错误
 */
#define LOS_ERRNO_TSK_CONFIG_TOO_MANY           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1a)

/**
 * @ingroup los_task
 * @brief 任务错误码：CPU上下文保存区域未对齐
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x1b
 * @par 十六进制值: 0x0200021b (十进制: 33554971)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于CPU上下文保存区域对齐检查相关错误
 */
#define LOS_ERRNO_TSK_CP_SAVE_AREA_NOT_ALIGN    LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1b)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务消息队列数量过多
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x1d
 * @par 十六进制值: 0x0200021d (十进制: 33554973)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务消息队列数量检查相关错误
 */
#define LOS_ERRNO_TSK_MSG_Q_TOO_MANY            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1d)

/**
 * @ingroup los_task
 * @brief 任务错误码：CPU上下文保存区域为空
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x1e
 * @par 十六进制值: 0x0200021e (十进制: 33554974)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于CPU上下文保存区域有效性检查相关错误
 */
#define LOS_ERRNO_TSK_CP_SAVE_AREA_NULL         LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1e)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务自删除错误
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x1f
 * @par 十六进制值: 0x0200021f (十进制: 33554975)
 * @par 解决方案：
 * 当前版本暂未使用此错误码，预留用于任务自删除操作相关错误
 */
#define LOS_ERRNO_TSK_SELF_DELETE_ERR           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x1f)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务栈大小过大
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x20
 * @par 十六进制值: 0x02000220 (十进制: 33554976)
 * @par 解决方案：
 * 减小栈大小参数，最大值不能超过系统配置的单个任务最大栈限制
 * @note 最大栈限制由LOSCFG_BASE_CORE_TSK_MAXSTACK_SIZE宏定义
 */
#define LOS_ERRNO_TSK_STKSZ_TOO_LARGE           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x20)

/**
 * @ingroup los_task
 * @brief 任务错误码：不允许挂起软件定时器任务
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_ERROR (一般错误)
 * - 错误编号: 0x21
 * @par 十六进制值: 0x02000221 (十进制: 33554977)
 * @par 解决方案：
 * 检查任务ID，软件定时器任务是系统关键任务，不允许执行挂起操作
 */
#define LOS_ERRNO_TSK_SUSPEND_SWTMR_NOT_ALLOWED LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x21)

/**
 * @ingroup los_task
 * @brief 任务错误码：在软件定时器任务中调用延迟
 * @par 错误码构成：
 * - 模块ID: LOS_MOD_TSK (任务模块)
 * - 错误级别: LOS_ERRTYPE_FATAL (致命错误)
 * - 错误编号: 0x22
 * @par 十六进制值: 0x03000222 (十进制: 50331938)
 * @par 解决方案：
 * 软件定时器回调函数中不允许调用阻塞接口，可改用定时器嵌套或事件触发机制
 */
#define LOS_ERRNO_TSK_DELAY_IN_SWTMR_TSK        LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x22)


/**
 * @ingroup los_task
 * @brief 任务错误码：CPU亲和性掩码不正确
 *
 * @note 当设置的CPU亲和性掩码不符合系统要求时触发此错误
 * @par 错误值
 * 0x03000223（十进制：50332195）
 * @par 解决方案
 * 请设置正确的CPU亲和性掩码，确保与系统支持的CPU核心数量匹配
 */
#define LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR     LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x23)

/**
 * @ingroup los_task
 * @brief 任务错误码：不允许在中断中调用任务让出CPU
 *
 * @note 在中断上下文中调用LOS_TaskYield()会导致不可预期的系统行为
 * @par 错误值
 * 0x02000224（十进制：33554980）
 * @par 解决方案
 * 请勿在中断服务程序中调用LOS_TaskYield()接口
 */
#define LOS_ERRNO_TSK_YIELD_IN_INT              LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x24)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务同步资源（信号量）分配失败
 *
 * @note 多处理器环境下任务同步所需的信号量资源不足时触发
 * @par 错误值
 * 0x02000225（十进制：33554981）
 * @par 解决方案
 * 增大LOSCFG_BASE_IPC_SEM_LIMIT配置项的值以扩展信号量资源上限
 */
#define LOS_ERRNO_TSK_MP_SYNC_RESOURCE          LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x25)

/**
 * @ingroup los_task
 * @brief 任务错误码：跨核心操作运行中任务同步失败
 *
 * @note 在多处理器系统中删除或操作其他核心上的运行中任务时同步失败
 * @par 错误值
 * 0x02000226（十进制：33554982）
 * @par 解决方案
 * 检查用户场景中任务删除逻辑是否需要特殊处理跨核心同步情况
 */
#define LOS_ERRNO_TSK_MP_SYNC_FAILED            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x26)

/**
 * @ingroup los_task
 * @brief 任务错误码：任务等待超时
 *
 * @note 任务等待资源或事件时超过设定的超时时间
 * @par 错误值
 * 0x02000227（十进制：33554983）
 * @par 解决方案
 * 此为正常超时返回，应用程序需处理超时逻辑
 */
#define LOS_ERRNO_TSK_TIMEOUT                   LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x27)

/**
 * @ingroup los_task
 * @brief 任务错误码：操作非当前进程的任务
 *
 * @note 尝试操作不属于当前进程上下文的任务时触发
 * @par 错误值
 * 0x02000228（十进制：33554984）
 * @par 解决方案
 * 仅允许操作当前进程内创建的任务，检查任务ID所属进程
 */
#define LOS_ERRNO_TSK_NOT_SELF_PROCESS          LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x28)


/**
 * @ingroup los_task
 * @brief 任务入口函数类型定义
 * @par 描述:
 * 定义任务入口函数的标准类型，任务创建并触发后将调用此函数
 * @attention 函数参数和返回值需严格遵循此定义，否则可能导致任务调度异常
 * @param param1 [IN] 类型 #UINTPTR 传递给任务处理函数的第一个参数
 * @param param2 [IN] 类型 #UINTPTR 传递给任务处理函数的第二个参数
 * @param param3 [IN] 类型 #UINTPTR 传递给任务处理函数的第三个参数
 * @param param4 [IN] 类型 #UINTPTR 传递给任务处理函数的第四个参数
 * @retval VOID* 任务退出时的返回值，若任务为分离状态(LOS_TASK_STATUS_DETACHED)，返回值将被忽略
 * @par 依赖:
 * <ul><li>los_task.h: 包含此API声明的头文件</li></ul>
 * @see TSK_INIT_PARAM_S::pfnTaskEntry
 */
typedef VOID *(*TSK_ENTRY_FUNC)(UINTPTR param1,
                                UINTPTR param2,
                                UINTPTR param3,
                                UINTPTR param4);

/**
 * @ingroup los_task
 * @brief 用户态任务参数结构体
 * @attention 禁止添加、删除或调整此结构体中的任何字段顺序，以免破坏用户态任务上下文
 */
typedef struct {
    UINTPTR         userArea;       /**< 用户态内存区域基地址 */
    UINTPTR         userSP;         /**< 用户态栈指针 */
    UINTPTR         userMapBase;    /**< 用户态内存映射基地址 */
    UINT32          userMapSize;    /**< 用户态内存映射大小(字节) */
} UserTaskParam;

/**
 * @ingroup los_task
 * @brief 任务创建参数结构体
 * @par 描述:
 * 任务创建时传入的指定参数集合，包含任务入口、优先级、栈大小等关键信息
 */
typedef struct tagTskInitParam {
    TSK_ENTRY_FUNC  pfnTaskEntry;   /**< 任务入口函数指针，指向任务执行的起始函数 */
    UINT16          usTaskPrio;     /**< 任务优先级，取值范围: 0 ~ LOSCFG_BASE_CORE_TSK_PRIORITY_LEVEL-1，0为最高优先级 */
    UINT16          policy;         /**< 任务调度策略，当前支持的策略包括:
                                         - SCHED_RR: 时间片轮转调度
                                         - SCHED_FIFO: 先来先服务调度
                                         - SCHED_EDF: 最早截止时间优先调度(实时任务) */
    UINTPTR         auwArgs[4];     /**< 任务参数数组，最多支持4个参数，通过TSK_ENTRY_FUNC传递给任务 */
    UINT32          uwStackSize;    /**< 任务栈大小(字节)，必须按系统字长对齐，最小值由LOSCFG_BASE_CORE_TSK_MINSTACK_SIZE定义 */
    CHAR            *pcName;        /**< 任务名称，建议长度不超过32字节，通过TSK_INFO_S::acName可查询 */
#ifdef LOSCFG_KERNEL_SMP
    UINT16          usCpuAffiMask;  /**< 任务CPU亲和性掩码，bit0对应CPU0，bit1对应CPU1，以此类推
                                         例如: 0x03表示任务可在CPU0和CPU1上运行 */
#endif
    UINT32          uwResved;       /**< 任务属性保留字段:
                                         - 设置为LOS_TASK_STATUS_DETACHED: 任务退出时自动删除
                                         - 设置为0: 任务不可自动删除，需调用LOS_TaskDelete()手动删除 */
    UINT16          consoleID;      /**< 任务所属控制台ID，用于多控制台系统中区分任务输出设备 */
    UINTPTR         processID;      /**< 进程ID，用于任务与进程的关联，0表示内核任务 */
    UserTaskParam   userParam;      /**< 用户态任务参数，仅用户态任务有效 */
    /* EDF调度参数，当policy为SCHED_EDF时有效 */
    UINT32          runTimeUs;      /**< 任务运行时间(微秒)，EDF调度算法使用 */
    UINT64          deadlineUs;     /**< 任务截止时间(微秒)，相对于系统启动时间的绝对时间戳 */
    UINT64          periodUs;       /**< 任务周期(微秒)，周期任务的时间间隔 */
} TSK_INIT_PARAM_S;

/**
 * @ingroup los_task
 * @brief 调度参数结构体
 * @par 描述:
 * 用于设置任务的调度相关参数，根据调度策略不同，使用不同的联合成员
 */
typedef struct {
    union {
        INT32 priority;     /**< 优先级，用于SCHED_RR和SCHED_FIFO调度策略，取值范围同TSK_INIT_PARAM_S::usTaskPrio */
        INT32 runTimeUs;    /**< 运行时间(微秒)，用于SCHED_EDF调度策略，同TSK_INIT_PARAM_S::runTimeUs */
    };
    INT32 deadlineUs;       /**< 截止时间(微秒)，用于SCHED_EDF调度策略，同TSK_INIT_PARAM_S::deadlineUs */
    INT32 periodUs;         /**< 周期(微秒)，用于SCHED_EDF调度策略，同TSK_INIT_PARAM_S::periodUs */
} LosSchedParam;

/**
 * @ingroup los_task
 * @brief 任务名称长度
 * @note 包含字符串结束符'\0'，实际可显示的任务名称长度为31字节
 */
#define LOS_TASK_NAMELEN                        32

/**
 * @ingroup los_task
 * @brief 任务信息结构体
 * @par 描述:
 * 包含任务的基本信息和运行状态，通过LOS_TaskInfoGet()接口获取
 */
typedef struct tagTskInfo {
    CHAR                acName[LOS_TASK_NAMELEN];   /**< 任务名称，长度由LOS_TASK_NAMELEN定义 */
    UINT32              uwTaskID;                   /**< 任务ID，系统唯一标识，创建任务时由内核分配 */
    UINT16              usTaskStatus;               /**< 任务状态，取值范围:
                                                       - OS_TASK_STATUS_RUNNING: 运行态
                                                       - OS_TASK_STATUS_READY: 就绪态
                                                       - OS_TASK_STATUS_BLOCKED: 阻塞态
                                                       - OS_TASK_STATUS_SUSPENDED: 挂起态
                                                       - OS_TASK_STATUS_DELAY: 延时态
                                                       - OS_TASK_STATUS_PENDING: 等待态 */
    UINT16              usTaskPrio;                 /**< 任务当前优先级，动态调整后的值可能与创建时不同 */
    VOID                *pTaskSem;                  /**< 任务等待的信号量指针，为NULL表示未等待任何信号量 */
    VOID                *pTaskMux;                  /**< 任务等待的互斥锁指针，为NULL表示未等待任何互斥锁 */
    VOID                *taskEvent;                 /**< 任务等待的事件控制块指针，为NULL表示未等待任何事件 */
    UINT32              uwEventMask;                /**< 任务等待的事件掩码，与taskEvent配合使用 */
    UINT32              uwStackSize;                /**< 任务栈总大小(字节)，创建时指定的值 */
    UINTPTR             uwTopOfStack;               /**< 任务栈顶地址，高地址端 */
    UINTPTR             uwBottomOfStack;            /**< 任务栈底地址，低地址端，栈从高地址向低地址增长 */
    UINTPTR             uwSP;                       /**< 任务当前栈指针(Stack Pointer)，指向栈顶位置 */
    UINT32              uwCurrUsed;                 /**< 当前栈使用量(字节)，实时反映栈内存使用情况 */
    UINT32              uwPeakUsed;                 /**< 栈使用峰值(字节)，记录任务运行过程中的最大栈使用量 */
    BOOL                bOvf;                       /**< 栈溢出标志:
                                                       - TRUE: 发生栈溢出
                                                       - FALSE: 未发生栈溢出 */
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
 * <li>If any low LOSCFG_KERNEL_CORE_NUM bit of the mask is not set, an error is reported.</li>
 * </ul>
 *
 * @param  uwTaskID      [IN]  Type  #UINT32 Task ID. The task id value is obtained from task creation.
 * @param  usCpuAffiMask [IN]  Type  #UINT32 The scheduling cpu mask.The low to high bit of the mask corresponds to
 *                             the cpu number, the high bit that exceeding the CPU number is ignored.
 *
 * @retval #LOS_ERRNO_TSK_ID_INVALID                Invalid task ID.
 * @retval #LOS_ERRNO_TSK_NOT_CREATED               The task is not created.
 * @retval #LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR     The task cpu affinity mask is incorrect.
 * @retval #LOS_OK                                  The task cpu affinity mask is successfully set.
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
