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

#ifndef _PERF_H
#define _PERF_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup perf
 * @brief 获取调用者的程序计数器(PC)和帧指针(FP)寄存器值
 *
 * @par 描述
 * 此宏用于获取当前函数调用者的PC和FP寄存器值，并存储到指定的寄存器结构体中
 * 主要用于性能分析时记录函数调用上下文信息
 *
 * @attention
 * <ul>
 * <li>依赖GCC内置函数__builtin_return_address和__builtin_frame_address实现</li>
 * <li>regs参数必须指向有效的寄存器结构体指针，该结构体需包含pc和fp成员</li>
 * </ul>
 *
 * @param regs [OUT] 类型 #寄存器结构体指针: 用于存储获取到的PC和FP寄存器值
 */
#define OsPerfArchFetchCallerRegs(regs) \
    do { \
        (regs)->pc = (UINTPTR)__builtin_return_address(0); /* 获取当前函数的返回地址(调用者的下一条指令地址)作为PC值 */ \
        (regs)->fp = (UINTPTR)__builtin_frame_address(0);  /* 获取当前栈帧地址作为FP值 */ \
    } while (0)

/**
 * @ingroup perf
 * @brief 从中断上下文中获取PC和FP寄存器值
 *
 * @par 描述
 * 此宏用于从中断处理上下文的任务控制块(TCB)中提取PC和FP寄存器值，并存储到指定的寄存器结构体中
 * 主要用于中断场景下的性能数据采集和任务上下文恢复
 *
 * @attention
 * <ul>
 * <li>tcb参数必须指向有效的任务控制块结构体，该结构体需包含pc和fp成员</li>
 * <li>regs参数必须指向有效的寄存器结构体指针，该结构体需包含pc和fp成员</li>
 * </ul>
 *
 * @param regs [OUT] 类型 #寄存器结构体指针: 用于存储获取到的PC和FP寄存器值
 * @param tcb  [IN]  类型 #TCB结构体指针: 指向包含中断上下文信息的任务控制块
 */
#define OsPerfArchFetchIrqRegs(regs, tcb) \
    do { \
        (regs)->pc = (tcb)->pc;  /* 从TCB中获取中断发生时保存的PC寄存器值 */ \
        (regs)->fp = (tcb)->fp;  /* 从TCB中获取中断发生时保存的FP寄存器值 */ \
    } while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _PERF_H */
