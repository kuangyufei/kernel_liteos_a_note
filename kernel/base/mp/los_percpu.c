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

#include "los_percpu_pri.h"
#include "los_printf.h"

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief 全局每个CPU数据结构数组
 * @details 存储系统中每个CPU核心的私有数据，数组大小由配置项 LOSCFG_KERNEL_CORE_NUM 决定
 */
Percpu g_percpu[LOSCFG_KERNEL_CORE_NUM];

/**
 * @brief 输出所有CPU核心的运行状态信息
 * @details 遍历所有CPU核心，根据其异常标志位输出对应的运行状态（运行中/已停止/异常中），最后输出当前处理异常的CPU编号
 */
VOID OsAllCpuStatusOutput(VOID)
{
    UINT32 i;  // 循环计数器，用于遍历所有CPU核心

    // 遍历系统中所有CPU核心
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        // 根据CPU的异常标志位判断并输出对应状态
        switch (g_percpu[i].excFlag) {
            case CPU_RUNNING:  // CPU处于运行状态
                PrintExcInfo("cpu%u is running.\n", i);
                break;
            case CPU_HALT:     // CPU处于停止状态
                PrintExcInfo("cpu%u is halted.\n", i);
                break;
            case CPU_EXC:      // CPU处于异常状态
                PrintExcInfo("cpu%u is in exc.\n", i);
                break;
            default:           // 未定义的状态，不输出信息
                break;
        }
    }
    // 输出当前正在处理异常的CPU编号
    PrintExcInfo("The current handling the exception is cpu%u !\n", ArchCurrCpuid());
}
#endif
