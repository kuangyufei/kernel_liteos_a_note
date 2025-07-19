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

#include "los_config.h"
#include "los_sched_pri.h"

/**
 * @brief 内核主入口函数
 * @details 操作系统启动的第一个C语言函数，完成内核初始化并启动调度器
 * 内核入口函数,由汇编调用,见于reset_vector_up.S 和 reset_vector_mp.S 
 * up指单核CPU, mp指多核CPU bl        main
 * @return INT32 理论上永不返回，仅在初始化失败时返回LOS_NOK
 * @attention 函数使用LITE_OS_SEC_TEXT_INIT属性，确保代码被放置在安全可执行段
 * @ingroup kernel_bootstrap
 */
LITE_OS_SEC_TEXT_INIT INT32 main(VOID)
{
    UINT32 ret = OsMain();  // 调用内核主初始化函数
    if (ret != LOS_OK) {    // 检查初始化结果
        return (INT32)LOS_NOK;  // 初始化失败，返回错误码
    }
    // 设置CPU映射表：逻辑CPU 0映射到物理CPU ID
    CPU_MAP_SET(0, OsHwIDGet());

    OsSchedStart();  // 启动内核调度器，从此开始多任务调度

    // 调度器启动后应永不返回至此，若返回则进入低功耗等待
    while (1) {
        __asm volatile("wfi");  // 执行等待中断指令，进入低功耗状态
    }
}
