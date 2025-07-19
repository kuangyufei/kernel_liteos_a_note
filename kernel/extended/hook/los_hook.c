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

#include "los_hook.h"
#include "los_hook_types_parse.h"

#ifdef LOSCFG_KERNEL_HOOK
#define LOS_HOOK_TYPE_DEF(type, paramList)                  \ /* 定义钩子类型，包含注册/注销/调用函数 */
    STATIC type##_FN g_fn##type;                            \ /* 静态钩子函数指针，存储注册的钩子函数 */
    UINT32 type##_RegHook(type##_FN func) {                 \ /* 注册钩子函数，返回错误码 */
        if ((func) == NULL) {                               \ /* 检查函数指针是否为空 */
            return LOS_ERRNO_HOOK_REG_INVALID;              \ /* 返回无效注册错误 */
        }                                                   \ /* 空语句，结束条件判断 */
        if (g_fn##type) {                                   \ /* 检查钩子是否已注册 */
            return LOS_ERRNO_HOOK_POOL_IS_FULL;             \ /* 返回钩子池已满错误 */
        }                                                   \ /* 空语句，结束条件判断 */
        g_fn##type = (func);                                \ /* 保存钩子函数指针 */
        return LOS_OK;                                      \ /* 返回成功 */
    }                                                       \ /* 结束注册函数定义 */
    UINT32 type##_UnRegHook(type##_FN func) {               \ /* 注销钩子函数，返回错误码 */
        if (((func) == NULL) || (g_fn##type != (func))) {   \ /* 检查函数指针有效性及匹配性 */
            return LOS_ERRNO_HOOK_UNREG_INVALID;            \ /* 返回无效注销错误 */
        }                                                   \ /* 空语句，结束条件判断 */
        g_fn##type = NULL;                                  \ /* 清除钩子函数指针 */
        return LOS_OK;                                      \ /* 返回成功 */
    }                                                       \ /* 结束注销函数定义 */
    VOID type##_CallHook paramList {                        \ /* 调用钩子函数，无返回值 */
        if (g_fn##type) {                                   \ /* 检查钩子函数是否已注册 */
            g_fn##type(PARAM_TO_ARGS paramList);            \ /* 传递参数并调用钩子函数 */
        }                                                   \ /* 空语句，结束条件判断 */
    }                                                       /* 结束调用函数定义 */

LOS_HOOK_ALL_TYPES_DEF;                                      /* 展开所有钩子类型定义 */

#undef LOS_HOOK_TYPE_DEF                                      /* 取消宏定义，避免重复展开 */


#endif /* LOSCFG_DEBUG_HOOK */

