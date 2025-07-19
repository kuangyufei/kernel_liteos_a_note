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

#ifndef _LOS_PM_H
#define _LOS_PM_H

#include "los_config.h"
#include "los_typedef.h"
#include "los_list.h"
#include "los_seq_buf.h"
#include "los_errno.h"

/**
 * @ingroup los_pm
 * 系统睡眠模式枚举
 * 定义系统支持的不同低功耗睡眠模式，优先级从低到高为：正常睡眠 < 轻度睡眠 < 深度睡眠 < 关机
 */
typedef enum {
    LOS_SYS_NORMAL_SLEEP = 0,  // 正常睡眠模式
    LOS_SYS_LIGHT_SLEEP,       // 轻度睡眠模式
    LOS_SYS_DEEP_SLEEP,        // 深度睡眠模式
    LOS_SYS_SHUTDOWN,          // 关机模式
} LOS_SysSleepEnum;  // 系统睡眠模式枚举类型

/**
 * @ingroup los_pm
 * 电源管理节点类型枚举
 * 定义电源管理框架支持的节点类型，用于分类管理不同硬件模块的电源策略
 */
typedef enum {
    LOS_PM_TYPE_DEVICE = 0,       // 设备类型节点
    LOS_PM_TYPE_TICK_TIMER,       // 定时器类型节点（保留）
    LOS_PM_TYPE_SYSCTRL,          // 系统控制类型节点
} LOS_PmNodeType;  // 电源管理节点类型枚举

/**
 * @ingroup los_pm
 * 设备电源管理操作结构体
 * 定义设备进入和退出低功耗模式的标准接口，由具体设备驱动实现
 */
typedef struct {
    UINT32 (*suspend)(UINT32 mode);  // 设备进入低功耗模式，解锁任务调度
    VOID   (*resume)(UINT32 mode);   // 设备退出低功耗模式，解锁任务调度
} LosPmDevice;  // 设备电源管理结构体

/**
 * @ingroup los_pm
 * 系统控制电源管理操作结构体
 * 定义系统级低功耗模式的完整控制接口，包括不同睡眠模式的进入/恢复函数
 */
typedef struct {
    /* CPU进入低功耗前的准备工作
     * 除正常模式外的所有模式都会调用
     * 解锁任务调度
     */
    UINT32 (*early)(UINT32 mode);  // 低功耗准备阶段回调
    /* 系统执行低功耗恢复工作
     * 除正常模式外的所有模式都会调用
     * 解锁任务调度
     */
    VOID (*late)(UINT32 mode);     // 低功耗恢复阶段回调
    /* 系统进入正常睡眠模式
     * 正常模式下该值不能为空
     */
    UINT32 (*normalSuspend)(VOID); // 正常睡眠进入函数
    /* 系统从正常睡眠恢复
     * 该值可以为NULL
     */
    VOID (*normalResume)(VOID);    // 正常睡眠恢复函数
    /* 系统进入轻度睡眠模式
     * 轻度睡眠模式下该值不能为空
     */
    UINT32 (*lightSuspend)(VOID);  // 轻度睡眠进入函数
    /* 系统从轻度睡眠恢复
     * 该值可以为NULL
     */
    VOID (*lightResume)(VOID);     // 轻度睡眠恢复函数
    /* 系统进入深度睡眠模式
     * 深度睡眠模式下该值不能为空
     */
    UINT32 (*deepSuspend)(VOID);   // 深度睡眠进入函数
    /* 系统从深度睡眠恢复
     * 该值可以为NULL
     */
    VOID (*deepResume)(VOID);      // 深度睡眠恢复函数
    /* 系统进入关机模式
     * 关机模式下该值不能为空
     */
    UINT32 (*shutdownSuspend)(VOID); // 关机模式进入函数
    /* 系统从关机状态恢复
     * 关机模式下该值不能为空
     */
    VOID (*shutdownResume)(VOID);  // 关机模式恢复函数
} LosPmSysctrl;  // 系统控制电源管理结构体

/**
 * @ingroup los_pm
 * @brief 注册电源管理节点
 *
 * @par 功能描述
 * 该API用于向电源管理框架注册一个电源管理节点，使节点参与系统低功耗调度
 * @attention
 * 无
 *
 * @param  type [IN] 电源管理节点类型，取值为LOS_PmNodeType枚举
 * @param  node [IN] 电源管理节点指针，指向具体节点的操作结构体
 *
 * @retval #LOS_OK 注册成功
 * @retval #其他错误码 注册失败
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 * @see LOS_PmUnregister
 */
UINT32 LOS_PmRegister(LOS_PmNodeType type, VOID *node);  // 注册电源管理节点

/**
 * @ingroup los_pm
 * @brief 注销电源管理节点
 *
 * @par 功能描述
 * 该API用于从电源管理框架注销一个已注册的电源管理节点
 * @attention
 * 无
 *
 * @param  type [IN] 电源管理节点类型，取值为LOS_PmNodeType枚举
 * @param  node [IN] 电源管理节点指针，必须与注册时的指针一致
 *
 * @retval #LOS_OK 注销成功
 * @retval #其他错误码 注销失败
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 * @see LOS_PmRegister
 */
UINT32 LOS_PmUnregister(LOS_PmNodeType type, VOID *node);  // 注销电源管理节点

/**
 * @ingroup los_pm
 * @brief 设置系统唤醒标志
 *
 * @par 功能描述
 * 该API用于设置系统唤醒标志，通知电源管理框架系统需要从低功耗模式唤醒
 * @attention
 * 无
 *
 * @param 无
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 */
VOID LOS_PmWakeSet(VOID);  // 设置系统唤醒标志

/**
 * @ingroup los_pm
 * @brief 获取当前系统低功耗模式
 *
 * @par 功能描述
 * 该API用于获取系统当前设置的低功耗模式
 * @attention
 * 无
 *
 * @param 无
 *
 * @retval #LOS_SysSleepEnum 当前系统低功耗模式枚举值
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 */
LOS_SysSleepEnum LOS_PmModeGet(VOID);  // 获取当前系统低功耗模式

/**
 * @ingroup los_pm
 * @brief 设置系统低功耗模式
 *
 * @par 功能描述
 * 该API用于设置系统将要进入的低功耗模式
 * @attention
 * 无
 *
 * @param  mode [IN] 目标低功耗模式，取值为LOS_SysSleepEnum枚举
 *
 * @retval #LOS_OK 设置成功
 * @retval #其他错误码 设置失败
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 * @see LOS_PmModeGet
 */
UINT32 LOS_PmModeSet(LOS_SysSleepEnum mode);  // 设置系统低功耗模式

/**
 * @ingroup los_pm
 * @brief 获取当前PM锁持有数量
 *
 * @par 功能描述
 * 该API用于获取当前系统中PM锁的持有数量
 * @attention
 * 无
 *
 * @param 无
 *
 * @retval #UINT32 当前PM锁持有数量
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 */
UINT32 LOS_PmLockCountGet(VOID);  // 获取PM锁持有数量

/**
 * @ingroup los_pm
 * @brief 请求获取当前模式的PM锁
 *
 * @par 功能描述
 * 请求获取当前模式的PM锁，使系统下次进入空闲任务时不会进入该模式
 * @attention
 * 无
 *
 * @param  name [IN] 锁请求者名称，用于调试和跟踪
 *
 * @retval #LOS_OK 获取锁成功
 * @retval #其他错误码 获取锁失败
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 * @see LOS_PmLockRelease
 */
UINT32 LOS_PmLockRequest(const CHAR *name);  // 请求PM锁

/**
 * @ingroup los_pm
 * @brief 请求获取带超时的PM锁
 *
 * @par 功能描述
 * 请求获取当前模式的PM锁，指定时间后自动释放，防止系统进入该低功耗模式
 * @attention
 * 无
 *
 * @param  name        [IN] 锁请求者名称，用于调试和跟踪
 * @param  millisecond [IN] 自动释放锁的时间间隔（毫秒）
 *
 * @retval #LOS_OK 获取锁成功
 * @retval #其他错误码 获取锁失败
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 * @see LOS_PmLockRelease
 */
UINT32 LOS_PmTimeLockRequest(const CHAR *name, UINT64 millisecond);  // 请求超时PM锁

/**
 * @ingroup los_pm
 * @brief 释放当前模式的PM锁
 *
 * @par 功能描述
 * 释放当前模式的PM锁，使系统下次进入空闲任务时可以进入该低功耗模式
 * @attention
 * 无
 *
 * @param  name [IN] 锁释放者名称，必须与请求者名称一致
 *
 * @retval #LOS_OK 释放锁成功
 * @retval #其他错误码 释放锁失败
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 * @see LOS_PmLockRequest
 */
UINT32 LOS_PmLockRelease(const CHAR *name);  // 释放PM锁

/**
 * @ingroup los_pm
 * @brief 获取当前PM锁状态
 *
 * @par 功能描述
 * 该API用于获取当前PM锁的状态信息
 * @attention
 * 无
 *
 * @param 无
 *
 * @retval #UINT32 设备唤醒源数量
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 */
UINT32 LOS_PmReadLock(VOID);  // 读取PM锁状态

/**
 * @ingroup los_pm
 * @brief 系统进入低功耗流程
 *
 * @par 功能描述
 * 该API用于触发系统进入低功耗处理流程
 * @attention
 * 无
 *
 * @param  wakeCount [IN] 唤醒源数量
 *
 * @retval #LOS_OK 进入低功耗流程成功
 * @retval #其他错误码 进入低功耗流程失败
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 */
UINT32 LOS_PmSuspend(UINT32 wakeCount);  // 系统进入低功耗

/**
 * @ingroup los_pm
 * @brief 显示PM锁的锁定信息
 *
 * @par 功能描述
 * 该API用于输出PM锁的锁定信息，包括持有者、锁定时间等
 * @attention
 * 无
 *
 * @param m [IN] 序列缓冲区指针，用于存储输出信息
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_pm.h: 包含本接口声明的头文件</li></ul>
 * @see LOS_PmLockRequest
 */
VOID LOS_PmLockInfoShow(struct SeqBuf *m);  // 显示PM锁信息

#endif
