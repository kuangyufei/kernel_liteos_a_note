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
 * @defgroup los_event Event
 * @ingroup kernel
 */

#ifndef _LOS_EVENT_H
#define _LOS_EVENT_H

#include "los_base.h"
#include "los_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @ingroup los_event
 * 事件读取模式：任务等待所有期望事件发生。
 */
#define LOS_WAITMODE_AND                    4U  // 逻辑与模式：需所有指定事件触发

/**
 * @ingroup los_event
 * 事件读取模式：任务等待任一期望事件发生。
 */
#define LOS_WAITMODE_OR                     2U  // 逻辑或模式：任一指定事件触发即可

/**
 * @ingroup los_event
 * 事件读取模式：事件读取后立即清除事件标志。
 */
#define LOS_WAITMODE_CLR                    1U  // 清除模式：读取后清除对应事件位

/**
 * @ingroup los_event
 * 事件掩码的第25位不能设置为事件，因为该位用于错误码。
 *
 * 值：0x02001c00
 *
 * 解决方法：将事件掩码中除第25位以外的位设置为事件。
 */
#define LOS_ERRNO_EVENT_SETBIT_INVALID      LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x00)  // 事件掩码位设置无效错误码

/**
 * @ingroup los_event
 * 事件读取错误码：事件读取超时。
 *
 * 值：0x02001c01
 *
 * 解决方法：增加事件读取等待时间，或让其他任务写入事件掩码。
 */
#define LOS_ERRNO_EVENT_READ_TIMEOUT        LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x01)  // 事件读取超时错误码

/**
 * @ingroup los_event
 * 事件读取错误码：EVENTMASK输入参数值无效。输入参数值不得为0。
 *
 * 值：0x02001c02
 *
 * 解决方法：传入有效的EVENTMASK值。
 */
#define LOS_ERRNO_EVENT_EVENTMASK_INVALID   LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x02)  // 事件掩码无效错误码

/**
 * @ingroup los_event
 * 事件读取错误码：在中断中读取事件。
 *
 * 值：0x02001c03
 *
 * 解决方法：在任务中读取事件。
 */
#define LOS_ERRNO_EVENT_READ_IN_INTERRUPT   LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x03)  // 中断中读取事件错误码

/**
 * @ingroup los_event
 * 事件读取错误码：事件读取API中使用的flag输入参数值无效。
 * 该输入参数值通过对OS_EVENT_ANY或OS_EVENT_ALL的对应位与OS_EVENT_WAIT或OS_EVENT_NOWAIT的对应位执行OR运算获得。
 * 在OS_EVENT_WAIT模式下读取事件时，等待时间必须设置为非零值。
 *
 * 值：0x02001c04
 *
 * 解决方法：传入有效的flag值。
 */
#define LOS_ERRNO_EVENT_FLAGS_INVALID       LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x04)  // 标志位无效错误码

/**
 * @ingroup los_event
 * 事件读取错误码：任务已锁定，无法读取事件。
 *
 * 值：0x02001c05
 *
 * 解决方法：解锁任务后读取事件。
 */
#define LOS_ERRNO_EVENT_READ_IN_LOCK        LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x05)  // 任务锁定中读取事件错误码

/**
 * @ingroup los_event
 * 事件读取错误码：空指针。
 *
 * 值：0x02001c06
 *
 * 解决方法：检查输入参数是否为空。
 */
#define LOS_ERRNO_EVENT_PTR_NULL            LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x06)  // 空指针错误码

/**
 * @ingroup los_event
 * 事件读取错误码：在系统级任务中读取事件。
 *                旧用法：在软件定时器任务中读取事件。(LOS_ERRNO_EVENT_READ_IN_SWTMR_TSK)
 * 值：0x02001c07
 *
 * 解决方法：在有效的用户任务中读取事件。
 */
#define LOS_ERRNO_EVENT_READ_IN_SYSTEM_TASK LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x07)  // 系统任务中读取事件错误码

/**
 * @ingroup los_event
 * 事件读取错误码：不应销毁事件。
 *
 * 值：0x02001c08
 *
 * 解决方法：检查事件列表是否不为空。
 */
#define LOS_ERRNO_EVENT_SHOULD_NOT_DESTROY  LOS_ERRNO_OS_ERROR(LOS_MOD_EVENT, 0x08)  // 事件不应销毁错误码

/**
 * @ingroup los_event
 * 事件控制结构体
 */
typedef struct tagEvent {
    UINT32 uwEventID;        /**< 事件控制块中的事件掩码，表示已逻辑处理的事件 */
    LOS_DL_LIST stEventList; /**< 事件控制块链表，用于管理等待事件的任务 */
} EVENT_CB_S, *PEVENT_CB_S;
/**
 * @ingroup los_event
 * @brief 初始化事件控制块
 *
 * @par 描述
 * 本API用于初始化eventCB指向的事件控制块
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param eventCB [IN/OUT] 指向待初始化的事件控制块指针
 *
 * @retval #LOS_ERRNO_EVENT_PTR_NULL 空指针错误
 * @retval #LOS_OK                    事件控制块初始化成功
 * @par 依赖
 * <ul><li>los_event.h: 包含API声明的头文件</li></ul>
 * @see LOS_EventClear
 */
extern UINT32 LOS_EventInit(PEVENT_CB_S eventCB);

/**
 * @ingroup los_event
 * @brief 获取事件ID指定的事件
 *
 * @par 描述
 * 本API用于根据事件ID、事件掩码和事件读取模式，检查用户期望的事件是否发生，并根据事件读取模式处理事件。事件ID必须指向有效内存
 * @attention
 * <ul>
 * <li>当模式为LOS_WAITMODE_CLR时，eventID为输出参数</li>
 * <li>其他模式下eventID为输入参数</li>
 * <li>错误码和事件返回值可能相同，为区分两者，事件掩码的第25位禁止使用</li>
 * </ul>
 *
 * @param eventID      [IN/OUT] 指向待检查事件ID的指针
 * @param eventMask    [IN] 用户期望发生的事件掩码，表示事件ID经逻辑处理后匹配的事件
 * @param mode         [IN] 事件读取模式，包括LOS_WAITMODE_AND、LOS_WAITMODE_OR、LOS_WAITMODE_CLR
 *
 * @retval #LOS_ERRNO_EVENT_SETBIT_INVALID     事件掩码第25位被设置（保留用于错误码）
 * @retval #LOS_ERRNO_EVENT_EVENTMASK_INVALID  传入的事件掩码不正确
 * @retval #LOS_ERRNO_EVENT_FLAGS_INVALID      传入的事件模式无效
 * @retval #LOS_ERRNO_EVENT_PTR_NULL           传入的指针为空
 * @retval 0                                   用户期望的事件未发生
 * @retval #UINT32                             用户期望的事件已发生
 * @par 依赖
 * <ul><li>los_event.h: 包含API声明的头文件</li></ul>
 * @see LOS_EventRead | LOS_EventWrite
 */
extern UINT32 LOS_EventPoll(UINT32 *eventID, UINT32 eventMask, UINT32 mode);

/**
 * @ingroup los_event
 * @brief 读取事件
 *
 * @par 描述
 * 本API用于阻塞或调度读取事件的任务，需指定事件控制块、事件掩码、读取模式和超时信息
 * @attention
 * <ul>
 * <li>错误码和事件返回值可能相同，为区分两者，事件掩码的第25位禁止使用</li>
 * </ul>
 *
 * @param eventCB      [IN/OUT] 指向待检查的事件控制块指针，必须指向有效内存
 * @param eventMask    [IN] 用户期望发生的事件掩码，表示事件ID经逻辑处理后匹配的事件
 * @param mode         [IN] 事件读取模式
 * @param timeout      [IN] 事件读取超时时间间隔（单位：Tick）
 *
 * @retval #LOS_ERRNO_EVENT_SETBIT_INVALID     事件掩码第25位被设置（保留用于错误码）
 * @retval #LOS_ERRNO_EVENT_EVENTMASK_INVALID  传入的事件读取模式不正确
 * @retval #LOS_ERRNO_EVENT_READ_IN_INTERRUPT  在中断中读取事件
 * @retval #LOS_ERRNO_EVENT_FLAGS_INVALID      事件模式无效
 * @retval #LOS_ERRNO_EVENT_READ_IN_LOCK       事件读取任务已锁定
 * @retval #LOS_ERRNO_EVENT_PTR_NULL           传入的指针为空
 * @retval 0                                   用户期望的事件未发生
 * @retval #UINT32                             用户期望的事件已发生
 * @par 依赖
 * <ul><li>los_event.h: 包含API声明的头文件</li></ul>
 * @see LOS_EventPoll | LOS_EventWrite
 */
extern UINT32 LOS_EventRead(PEVENT_CB_S eventCB, UINT32 eventMask, UINT32 mode, UINT32 timeout);

/**
 * @ingroup los_event
 * @brief 写入事件
 *
 * @par 描述
 * 本API用于将传入的事件掩码指定的事件写入eventCB指向的事件控制块
 * @attention
 * <ul>
 * <li>为区分LOS_EventRead API返回的是事件还是错误码，事件掩码的第25位禁止使用</li>
 * </ul>
 *
 * @param eventCB  [IN/OUT] 指向待写入事件的事件控制块指针，必须指向有效内存
 * @param events   [IN] 待写入的事件掩码
 *
 * @retval #LOS_ERRNO_EVENT_SETBIT_INVALID  事件掩码第25位被设置为事件（保留用于错误码）
 * @retval #LOS_ERRNO_EVENT_PTR_NULL        空指针错误
 * @retval #LOS_OK                          事件写入成功
 * @par 依赖
 * <ul><li>los_event.h: 包含API声明的头文件</li></ul>
 * @see LOS_EventPoll | LOS_EventRead
 */
extern UINT32 LOS_EventWrite(PEVENT_CB_S eventCB, UINT32 events);

/**
 * @ingroup los_event
 * @brief 通过指定事件掩码清除eventCB的事件
 *
 * @par 描述
 * <ul>
 * <li>本API用于将eventCB指向的事件控制块中存储的指定掩码事件ID设置为0，eventCB必须指向有效内存</li>
 * </ul>
 * @attention
 * <ul>
 * <li>传入时events值需要取反</li>
 * </ul>
 *
 * @param eventCB     [IN/OUT] 指向待清除的事件控制块指针
 * @param eventMask   [IN] 待清除事件的掩码
 *
 * @retval #LOS_ERRNO_EVENT_PTR_NULL  空指针错误
 * @retval #LOS_OK                    事件清除成功
 * @par 依赖
 * <ul><li>los_event.h: 包含API声明的头文件</li></ul>
 * @see LOS_EventPoll | LOS_EventRead | LOS_EventWrite
 */
extern UINT32 LOS_EventClear(PEVENT_CB_S eventCB, UINT32 eventMask);

/**
 * @ingroup los_event
 * @brief 销毁事件
 *
 * @par 描述
 * <ul>
 * <li>本API用于销毁事件</li>
 * </ul>
 * @attention
 * <ul>
 * <li>指定的事件必须是有效的</li>
 * </ul>
 *
 * @param eventCB     [IN/OUT] 指向待销毁的事件控制块指针
 *
 * @retval #LOS_ERRNO_EVENT_PTR_NULL 空指针错误
 * @retval #LOS_OK                   事件销毁成功
 * @par 依赖
 * <ul><li>los_event.h: 包含API声明的头文件</li></ul>
 * @see LOS_EventPoll | LOS_EventRead | LOS_EventWrite
 */
extern UINT32 LOS_EventDestroy(PEVENT_CB_S eventCB);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_EVENT_H */
