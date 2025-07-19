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
 * @defgroup los_sem Semaphore
 * @ingroup kernel
 */

#ifndef _LOS_SEM_H
#define _LOS_SEM_H

#include "los_base.h"
#include "los_err.h"
#include "los_list.h"
#include "los_task.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_sem
 * @brief 信号量错误码：内存不足
 *
 * @par 错误描述
 * 当系统内存资源不足，无法分配信号量控制结构所需内存时触发此错误
 *
 * @par 错误值
 * 十六进制：0x02000700，十进制：33554944
 *
 * @par 解决方案
 * 1. 检查系统内存使用情况，释放不必要的内存资源
 * 2. 调整系统内存分配策略，增加可用内存
 * 3. 减少同时创建的信号量数量
 */
#define LOS_ERRNO_SEM_NO_MEMORY  LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x00)  // 信号量模块内存不足错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：无效参数
 *
 * @par 错误描述
 * 当传入信号量API的参数值超出有效范围或不符合参数规范时触发此错误
 * 常见场景：
 * - 信号量句柄为无效值
 * - 信号量计数值超出允许范围
 * - 空指针参数
 *
 * @par 错误值
 * 十六进制：0x02000701，十进制：33554945
 *
 * @par 解决方案
 * 1. 检查API调用时传入的所有参数值
 * 2. 确保信号量句柄有效且已创建
 * 3. 验证参数范围是否符合函数要求
 */
#define LOS_ERRNO_SEM_INVALID      LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x01)  // 信号量模块无效参数错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：空指针
 *
 * @par 错误描述
 * 当传入信号量API的指针参数为NULL（空指针）时触发此错误
 * 常见于需要输出参数的函数，如LOS_SemCreate的semHandle参数
 *
 * @par 错误值
 * 十六进制：0x02000702，十进制：33554946
 *
 * @par 解决方案
 * 1. 检查所有指针参数是否正确初始化
 * 2. 确保指针指向有效的内存空间
 * 3. 避免将未初始化的指针传递给信号量API
 */
#define LOS_ERRNO_SEM_PTR_NULL     LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x02)  // 信号量模块空指针错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：无可用信号量控制结构
 *
 * @par 错误描述
 * 当系统中所有信号量控制结构均已被占用，无法创建新信号量时触发此错误
 * 受限于系统配置的最大信号量数量（LOSCFG_BASE_IPC_SEM_LIMIT）
 *
 * @par 错误值
 * 十六进制：0x02000703，十进制：33554947
 *
 * @par 解决方案
 * 1. 检查是否有未使用的信号量可删除
 * 2. 调整LOSCFG_BASE_IPC_SEM_LIMIT配置增加最大信号量数量
 * 3. 优化信号量使用策略，减少不必要的信号量创建
 */
#define LOS_ERRNO_SEM_ALL_BUSY     LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x03)  // 信号量模块控制结构耗尽错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：无效的超时参数
 *
 * @par 错误描述
 * 当传入信号量等待函数的超时参数值无效时触发此错误
 * 常见场景：
 * - 超时值为负数
 * - 超时值超出系统允许的最大范围
 * - 在不支持超时的上下文中使用超时参数
 *
 * @par 错误值
 * 十六进制：0x02000704，十进制：33554948
 *
 * @par 解决方案
 * 1. 确保超时参数取值在[0, 0xFFFFFFFF]范围内
 * 2. 特殊值0表示不等待，0xFFFFFFFF表示永久等待
 * 3. 检查API文档，确认函数是否支持超时参数
 */
#define LOS_ERRNO_SEM_UNAVAILABLE  LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x04)  // 信号量模块超时参数无效错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：中断上下文中调用
 *
 * @par 错误描述
 * 当在中断服务程序（ISR）或中断上下文中调用信号量等待类API时触发此错误
 * 受影响的API：LOS_SemPend等可能导致阻塞的函数
 *
 * @par 错误值
 * 十六进制：0x02000705，十进制：33554949
 *
 * @par 解决方案
 * 1. 避免在中断上下文中调用可能阻塞的信号量API
 * 2. 改用非阻塞方式的信号量操作（如LOS_SemTryPend）
 * 3. 将需要信号量操作的逻辑移至任务上下文执行
 */
#define LOS_ERRNO_SEM_PEND_INTERR  LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x05)  // 信号量模块中断上下文调用错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：调度锁定时调用Pend
 *
 * @par 错误描述
 * 当任务调度被锁定（如调用LOS_SchedLock）时调用信号量等待API触发此错误
 * 此时系统无法进行任务切换，等待操作会导致死锁风险
 *
 * @par 错误值
 * 十六进制：0x02000706，十进制：33554950
 *
 * @par 解决方案
 * 1. 在调用LOS_SemPend前确保已调用LOS_SchedUnlock解锁调度
 * 2. 调整代码逻辑，避免在调度锁定期间进行阻塞操作
 * 3. 改用非阻塞的信号量尝试获取方式
 */
#define LOS_ERRNO_SEM_PEND_IN_LOCK LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x06)  // 信号量模块调度锁定时调用错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：等待超时
 *
 * @par 错误描述
 * 当信号量等待操作在指定的超时时间内未能获取到信号量时触发此错误
 * 这是一种预期的正常错误，用于表示操作未在规定时间内完成
 *
 * @par 错误值
 * 十六进制：0x02000707，十进制：33554951
 *
 * @par 解决方案
 * 1. 根据业务需求决定是否重试操作
 * 2. 增加超时时间参数值
 * 3. 检查信号量释放逻辑是否正常工作
 * 4. 优化系统设计，减少信号量竞争
 */
#define LOS_ERRNO_SEM_TIMEOUT      LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x07)  // 信号量模块等待超时错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：释放溢出
 *
 * @par 错误描述
 * 当信号量释放操作次数超过获取次数，导致信号量计数值超出初始最大值时触发此错误
 * 例如：初始计数值为0的信号量被连续释放两次
 *
 * @par 错误值
 * 十六进制：0x02000708，十进制：33554952
 *
 * @par 解决方案
 * 1. 检查信号量操作逻辑，确保释放次数不超过获取次数
 * 2. 使用调试工具跟踪信号量的Pend/Post调用序列
 * 3. 考虑使用二进制信号量避免计数值溢出问题
 */
#define LOS_ERRNO_SEM_OVERFLOW     LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x08)  // 信号量模块释放溢出错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：有等待任务时删除
 *
 * @par 错误描述
 * 当信号量上仍有等待任务队列时尝试删除信号量触发此错误
 * 此时删除操作会导致等待任务无法被唤醒，造成资源泄漏
 *
 * @par 错误值
 * 十六进制：0x02000709，十进制：33554953
 *
 * @par 解决方案
 * 1. 删除信号量前确保所有等待任务已被唤醒或取消等待
 * 2. 调用LOS_SemDelete前先检查信号量状态
 * 3. 设计合理的信号量生命周期管理机制
 */
#define LOS_ERRNO_SEM_PENDED       LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x09)  // 信号量模块有等待任务错误码

/**
 * @ingroup los_sem
 * @brief 信号量错误码：系统级回调中调用
 *
 * @par 错误描述
 * 当在系统级回调（如软件定时器回调、中断底半部）中调用信号量等待API时触发此错误
 * 这类上下文中不允许阻塞操作，可能导致系统不稳定
 *
 * @par 错误值
 * 十六进制：0x0200070A，十进制：33554954
 *
 * @par 错误描述补充
 * 旧版错误名称：LOS_ERRNO_SEM_PEND_SWTERR（软件定时器回调中调用错误）
 * 新版已扩展为所有系统级回调上下文
 *
 * @par 解决方案
 * 1. 避免在系统回调中使用可能阻塞的信号量操作
 * 2. 通过任务消息队列将处理逻辑转移到任务上下文
 * 3. 使用事件标志等非阻塞同步机制替代
 */
#define LOS_ERRNO_SEM_PEND_IN_SYSTEM_TASK       LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x0A)  // 信号量模块系统回调调用错误码

/**
 * @ingroup los_sem
 * @brief 二进制信号量最大计数值
 *
 * @par 宏定义描述
 * 定义二进制信号量的最大计数值，用于限制二进制信号量的状态范围
 * 二进制信号量只能取0或1两个值，代表可用或不可用状态
 *
 * @par 取值说明
 * 固定值为1，表示二进制信号量的最大可用数量
 * 与计数信号量不同，二进制信号量不支持计数值累加
 *
 * @par 使用场景
 * 1. 在创建二进制信号量时作为计数值上限检查
 * 2. 区分二进制信号量与普通计数信号量的关键参数
 * 3. 信号量类型判断的依据之一
 */
#define OS_SEM_BINARY_COUNT_MAX                 1  // 二进制信号量最大计数值，取值为1
/**
 * @ingroup los_sem
 * @brief 创建一个计数信号量
 *
 * @par 功能描述
 * 根据指定的初始可用信号量数量count创建信号量控制结构，并返回该信号量的唯一标识符（句柄）。
 * 计数信号量支持多次获取和释放，适用于控制对有限资源的并发访问。
 *
 * @attention
 * <ul>
 * <li>信号量创建后必须调用LOS_SemDelete释放，避免资源泄漏</li>
 * <li>count参数不得超过系统配置的最大信号量计数值OS_SEM_COUNT_MAX</li>
 * <li>创建成功后semHandle将被赋值为有效的信号量句柄，失败时为INVALID_SEM_HANDLE</li>
 * </ul>
 *
 * @param count       [IN] 初始可用信号量数量，取值范围[0, OS_SEM_COUNT_MAX]
 *                         - 0：初始状态为无可用资源，任务获取时会阻塞
 *                         - n：初始状态为有n个可用资源
 * @param semHandle   [OUT] 信号量句柄指针，用于返回创建成功的信号量ID
 *
 * @retval #LOS_ERRNO_SEM_PTR_NULL  semHandle为NULL指针
 * @retval #LOS_ERRNO_SEM_OVERFLOW  count > OS_SEM_COUNT_MAX
 * @retval #LOS_ERRNO_SEM_ALL_BUSY  信号量控制结构池耗尽
 * @retval #LOS_OK                  创建成功，semHandle指向有效句柄
 *
 * @par 错误码示例
 * 若count=10且OS_SEM_COUNT_MAX=5，则返回LOS_ERRNO_SEM_OVERFLOW(0x02000701)
 *
 * @par 依赖
 * <ul><li>los_sem.h: 包含信号量API声明</li></ul>
 * @see LOS_SemDelete | LOS_SemPend | LOS_SemPost
 */
extern UINT32 LOS_SemCreate(UINT16 count, UINT32 *semHandle);  // 创建计数信号量

/**
 * @ingroup los_sem
 * @brief 创建一个二进制信号量
 *
 * @par 功能描述
 * 创建一个特殊的计数信号量，其计数值只能为0或1，适用于互斥访问或事件同步场景。
 * 二进制信号量与计数信号量的核心区别在于：
 * - 计数范围固定为[0, 1]
 * - 释放操作(Post)只能将计数值从0置为1
 * - 获取操作(Pend)只能将计数值从1置为0
 *
 * @attention
 * <ul>
 * <li>count参数只能为0或1，其他值将返回LOS_ERRNO_SEM_OVERFLOW</li>
 * <li>二进制信号量更适合实现互斥锁功能，比计数信号量更节省内存</li>
 * <li>不支持递归获取，同一任务多次Pend会导致死锁</li>
 * </ul>
 *
 * @param count        [IN] 初始可用信号量数量，仅允许取值0或1
 *                          - 0：初始为未触发状态（资源不可用）
 *                          - 1：初始为已触发状态（资源可用）
 * @param semHandle    [OUT] 信号量句柄指针，用于返回创建成功的信号量ID
 *
 * @retval #LOS_ERRNO_SEM_PTR_NULL     semHandle为NULL指针
 * @retval #LOS_ERRNO_SEM_OVERFLOW     count不在[0,1]范围内
 * @retval #LOS_ERRNO_SEM_ALL_BUSY     信号量控制结构池耗尽
 * @retval #LOS_OK                     创建成功
 *
 * @par 使用示例
 * @code
 * UINT32 semId;
 * UINT32 ret = LOS_BinarySemCreate(1, &semId); // 创建初始可用的二进制信号量
 * if (ret == LOS_OK) {
 *     // 信号量创建成功，可进行Pend/Post操作
 * }
 * @endcode
 *
 * @par 依赖
 * <ul><li>los_sem.h: 包含信号量API声明</li></ul>
 * @see LOS_SemCreate | LOS_SemDelete
 */
extern UINT32 LOS_BinarySemCreate(UINT16 count, UINT32 *semHandle);  // 创建二进制信号量

/**
 * @ingroup los_sem
 * @brief 删除一个信号量
 *
 * @par 功能描述
 * 根据信号量句柄删除指定的信号量控制结构，释放其占用的系统资源。
 * 成功删除后，该信号量句柄将失效，不能再用于Pend/Post操作。
 *
 * @attention
 * <ul>
 * <li>必须删除已创建且不再使用的信号量，否则会导致资源泄漏</li>
 * <li>若信号量上有等待任务队列（LOS_ERRNO_SEM_PENDED），必须先唤醒所有等待任务</li>
 * <li>删除操作不会释放等待该信号量的任务，需确保删除前所有任务已退出等待</li>
 * </ul>
 *
 * @param semHandle   [IN] 信号量句柄，必须是通过LOS_SemCreate或LOS_BinarySemCreate获取的有效句柄
 *
 * @retval #LOS_ERRNO_SEM_INVALID  semHandle无效（不存在或已删除）
 * @retval #LOS_ERRNO_SEM_PENDED   信号量上存在等待任务队列
 * @retval #LOS_OK                 删除成功
 *
 * @par 错误处理流程
 * 1. 若返回LOS_ERRNO_SEM_PENDED，调用LOS_SemPost释放信号量唤醒所有等待任务
 * 2. 再次调用LOS_SemDelete尝试删除
 *
 * @par 依赖
 * <ul><li>los_sem.h: 包含信号量API声明</li></ul>
 * @see LOS_SemCreate | LOS_SemPost
 */
extern UINT32 LOS_SemDelete(UINT32 semHandle);  // 删除信号量

/**
 * @ingroup los_sem
 * @brief 获取（等待）一个信号量
 *
 * @par 功能描述
 * 尝试获取指定的信号量，若信号量当前可用（计数值>0），则立即获取并将计数值减1后返回；
 * 若信号量不可用（计数值=0），则当前任务将进入阻塞状态，等待超时或信号量被释放。
 *
 * @attention
 * <ul>
 * <li>绝对禁止在中断上下文中调用此函数，会返回LOS_ERRNO_SEM_PEND_INTERR</li>
 * <li>软件定时器回调、系统级回调中同样不允许调用，会返回LOS_ERRNO_SEM_PEND_IN_SYSTEM_TASK</li>
 * <li>调度锁定（LOS_SchedLock）期间调用会返回LOS_ERRNO_SEM_PEND_IN_LOCK</li>
 * <li>timeout参数为0时表示非阻塞方式获取，立即返回成功或失败</li>
 * </ul>
 *
 * @param semHandle [IN] 信号量句柄，必须是有效的信号量ID
 * @param timeout   [IN] 等待超时时间（单位：系统时钟节拍Tick）
 *                       - 0：不等待，立即返回
 *                       - 0xFFFFFFFF：永久等待，直到获取成功
 *                       - n (0 < n < 0xFFFFFFFF)：最多等待n个Tick
 *
 * @retval #LOS_ERRNO_SEM_INVALID       semHandle无效
 * @retval #LOS_ERRNO_SEM_UNAVAILABLE   非阻塞模式下信号量不可用
 * @retval #LOS_ERRNO_SEM_PEND_INTERR   中断上下文中调用
 * @retval #LOS_ERRNO_SEM_PEND_IN_LOCK  调度锁定期间调用
 * @retval #LOS_ERRNO_SEM_TIMEOUT       等待超时
 * @retval #LOS_ERRNO_SEM_PEND_IN_SYSTEM_TASK 系统回调中调用
 * @retval #LOS_OK                      获取信号量成功
 *
 * @par 阻塞行为说明
 * 任务进入阻塞状态后，将从就绪队列中移除，直到以下情况发生：
 * 1. 其他任务调用LOS_SemPost释放信号量
 * 2. 等待超时
 * 3. 信号量被删除（此时会返回错误）
 *
 * @par 依赖
 * <ul><li>los_sem.h: 包含信号量API声明</li></ul>
 * @see LOS_SemPost | LOS_SemCreate
 */
extern UINT32 LOS_SemPend(UINT32 semHandle, UINT32 timeout);  // 获取信号量

/**
 * @ingroup los_sem
 * @brief 释放（发送）一个信号量
 *
 * @par 功能描述
 * 释放指定的信号量，将其计数值加1。若有任务正在等待该信号量，则唤醒等待队列中优先级最高的任务。
 * 此函数是信号量同步机制的核心，用于通知其他等待任务资源已可用。
 *
 * @attention
 * <ul>
 * <li>只能释放已获取的信号量，过度释放会导致计数值溢出（LOS_ERRNO_SEM_OVERFLOW）</li>
 * <li>二进制信号量释放时计数值固定为1，多次释放不会累加</li>
 * <li>可在中断上下文中安全调用，不会导致阻塞</li>
 * </ul>
 *
 * @param semHandle   [IN] 信号量句柄，必须是有效的信号量ID
 *
 * @retval #LOS_ERRNO_SEM_INVALID      semHandle无效
 * @retval #LOS_ERRNO_SEM_OVERFLOW     信号量计数值溢出（超过OS_SEM_COUNT_MAX）
 * @retval #LOS_OK                     释放成功
 *
 * @par 优先级反转说明
 * 当释放信号量唤醒高优先级任务时，系统会立即进行任务调度，确保高优先级任务优先执行，
 * 这有助于避免优先级反转问题，但可能导致当前任务被抢占。
 *
 * @par 使用示例
 * @code
 * // 获取信号量
 * UINT32 ret = LOS_SemPend(semId, 0xFFFFFFFF);
 * if (ret == LOS_OK) {
 *     // 访问共享资源
 *     ...
 *     // 释放信号量
 *     LOS_SemPost(semId);
 * }
 * @endcode
 *
 * @par 依赖
 * <ul><li>los_sem.h: 包含信号量API声明</li></ul>
 * @see LOS_SemPend | LOS_SemCreate
 */
extern UINT32 LOS_SemPost(UINT32 semHandle);  // 释放信号量
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SEM_H */
