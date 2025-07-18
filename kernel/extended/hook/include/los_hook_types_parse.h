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
#ifndef _LOS_HOOK_TYPES_PARSE_H
#define _LOS_HOOK_TYPES_PARSE_H

#define ADDR(a) (&(a))  // 获取变量a的地址
#define ARGS(a) (a)     // 获取变量a的值
#define ADDRn(...) _CONCAT(ADDR, _NARGS(__VA_ARGS__))(__VA_ARGS__)  // 根据参数数量选择对应的ADDRn宏
#define ARGSn(...) _CONCAT(ARGS, _NARGS(__VA_ARGS__))(__VA_ARGS__)  // 根据参数数量选择对应的ARGSn宏
#define ARGS0()         // 无参数时的ARGS宏实现
#define ADDR0()         // 无参数时的ADDR宏实现
#define ARGS1(a) ARGS(a)  // 单个参数时的ARGS宏实现
#define ADDR1(a) ADDR(a)  // 单个参数时的ADDR宏实现

#define ARG_const _ARG_const(  // 处理const类型参数的宏定义
#define _ARG_const(a) ARG_CP_##a)  // 拼接参数类型与ARG_CP_前缀
#define ARG_CP_LosSemCB ADDR(  // LosSemCB类型参数使用ADDR宏处理
#define ARG_CP_LosTaskCB ADDR(  // LosTaskCB类型参数使用ADDR宏处理
#define ARG_CP_UINT32 ADDR(  // UINT32类型参数使用ADDR宏处理
#define ARG_CP_LosMux ADDR(  // LosMux类型参数使用ADDR宏处理
#define ARG_CP_LosQueueCB ADDR(  // LosQueueCB类型参数使用ADDR宏处理
#define ARG_CP_SWTMR_CTRL_S ADDR(  // SWTMR_CTRL_S类型参数使用ADDR宏处理
#define ARG_CP_IpcMsg ADDR(  // IpcMsg类型参数使用ADDR宏处理
#define ARG_UINT32 ARGS(  // UINT32类型参数使用ARGS宏处理
#define ARG_PEVENT_CB_S ARGS(  // PEVENT_CB_S类型参数使用ARGS宏处理
#define ARG_void ADDRn(  // void类型参数使用ADDRn宏处理
#define ARG(a) ARG_##a)  // 根据参数类型选择对应的ARG宏

#define PARAM_TO_ARGS1(a) ARG(a)  // 1个参数转ARGS列表
#define PARAM_TO_ARGS2(a, b) ARG(a), PARAM_TO_ARGS1(b)  // 2个参数转ARGS列表
#define PARAM_TO_ARGS3(a, b, c) ARG(a), PARAM_TO_ARGS2(b, c)  // 3个参数转ARGS列表
#define PARAM_TO_ARGS4(a, b, c, d) ARG(a), PARAM_TO_ARGS3(b, c, d)  // 4个参数转ARGS列表
#define PARAM_TO_ARGS5(a, b, c, d, e) ARG(a), PARAM_TO_ARGS4(b, c, d, e)  // 5个参数转ARGS列表
#define PARAM_TO_ARGS6(a, b, c, d, e, f) ARG(a), PARAM_TO_ARGS5(b, c, d, e, f)  // 6个参数转ARGS列表
#define PARAM_TO_ARGS7(a, b, c, d, e, f, g) ARG(a), PARAM_TO_ARGS6(b, c, d, e, f, g)  // 7个参数转ARGS列表

#define _ZERO_ARGS  7, 6, 5, 4, 3, 2, 1, 0  // 参数数量计算辅助宏
#define ___NARGS(a, b, c, d, e, f, g, h, n, ...)    n  // 获取参数数量的内部宏
#define __NARGS(...) ___NARGS(__VA_ARGS__)  // 获取参数数量的中间宏
#define _NARGS(...) __NARGS(x, __VA_ARGS__##_ZERO_ARGS, 7, 6, 5, 4, 3, 2, 1, 0)  // 获取参数数量的外部宏
#define __CONCAT(a, b) a##b  // 宏拼接内部实现
#define _CONCAT(a, b) __CONCAT(a, b)  // 宏拼接外部接口

#define PARAM_TO_ARGS(...) _CONCAT(PARAM_TO_ARGS, _NARGS(__VA_ARGS__))(__VA_ARGS__)  // 根据参数数量选择对应的PARAM_TO_ARGSn宏
#define OS_HOOK_PARAM_TO_ARGS(paramList) (PARAM_TO_ARGS paramList)  // OS钩子参数转ARGS列表的统一接口

#endif /* _LOS_HOOK_TYPES_PARSE_H */
