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

#ifndef _LOS_HOOK_H
#define _LOS_HOOK_H

#include "los_config.h"
#include "los_err.h"
#include "los_errno.h"

#ifdef LOSCFG_KERNEL_HOOK
#include "los_hook_types.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
#ifdef LOSCFG_KERNEL_HOOK
/**
 * @ingroup los_hook
 * 钩子错误码：钩子池不足。
 *
 * 值：0x02001f00
 *
 * 解决方法：注销已注册的钩子。
 */
#define LOS_ERRNO_HOOK_POOL_IS_FULL             LOS_ERRNO_OS_ERROR(LOS_MOD_HOOK, 0x00)  // 钩子池已满错误码

/**
 * @ingroup los_hook
 * 钩子错误码：无效参数。
 *
 * 值：0x02001f01
 *
 * 解决方法：检查LOS_HookReg的输入参数。
 */
#define LOS_ERRNO_HOOK_REG_INVALID              LOS_ERRNO_OS_ERROR(LOS_MOD_HOOK, 0x01)  // 钩子注册参数无效错误码

/**
 * @ingroup los_hook
 * 钩子错误码：无效参数。
 *
 * 值：0x02001f02
 *
 * 解决方法：检查LOS_HookUnReg的输入参数。
 */
#define LOS_ERRNO_HOOK_UNREG_INVALID            LOS_ERRNO_OS_ERROR(LOS_MOD_HOOK, 0x02)  // 钩子注销参数无效错误码

/**
 * @ingroup los_hook
 * @brief 钩子函数注册
 *
 * @par 描述
 * 本API用于注册钩子函数
 *
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param hookType  [IN] 要注册的钩子类型
 * @param hookFn    [IN] 要注册的钩子函数
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_hook.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_HookReg(hookType, hookFn)           hookType##_RegHook(hookFn)  // 注册钩子函数宏，通过钩子类型拼接注册函数名

/**
 * @ingroup los_hook
 * @brief 钩子函数注销
 *
 * @par 描述
 * 本API用于注销钩子函数
 *
 * @attention
 * <ul>
 * <li>无</li>
 * </ul>
 *
 * @param hookType  [IN] 要注销的钩子类型
 * @param hookFn    [IN] 要注销的钩子函数
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_hook.h: 包含API声明的头文件</li></ul>
 * @see
 */
#define LOS_HookUnReg(hookType, hookFn)         hookType##_UnRegHook(hookFn)  // 注销钩子函数宏，通过钩子类型拼接注销函数名

/**
 * 调用钩子函数
 */
#define OsHookCall(hookType, ...)               hookType##_CallHook(__VA_ARGS__)  // 调用钩子函数宏，支持可变参数

#else
#define LOS_HookReg(hookType, hookFn)           // 钩子功能未启用时的空实现
#define LOS_HookUnReg(hookType, hookFn)         // 钩子功能未启用时的空实现
#define OsHookCall(hookType, ...)               // 钩子功能未启用时的空实现
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HOOK_H */
