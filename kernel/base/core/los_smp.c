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

#include "los_smp.h"
#include "arch_config.h"
#include "los_atomic.h"
#include "los_task_pri.h"
#include "los_init_pri.h"
#include "los_process_pri.h"
#include "los_sched_pri.h"
#include "los_swtmr_pri.h"

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief SMP(对称多处理)操作接口指针
 * @details 指向平台特定的SMP操作实现，包括CPU启动、中断分发等功能
 */
STATIC struct SmpOps *g_smpOps = NULL;

/**
 * @brief  secondary CPU初始化函数
 * @param  arg - 传递给secondary CPU的参数(未使用)
 * @details 完成secondary CPU的核心初始化工作，包括任务设置、定时器初始化和调度启动
 */
STATIC VOID OsSmpSecondaryInit(VOID *arg)
{
    UNUSED(arg);  /* 未使用的参数，避免编译警告 */

    /* 设置当前任务为系统主任务 */
    OsCurrTaskSet(OsGetMainTask());

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    OsSwtmrInit();  /* 初始化软件定时器模块 */
#endif

    /* 为当前CPU创建空闲任务 */
    OsIdleTaskCreate((UINTPTR)OsGetIdleProcess());
    /* 执行LEVEL_KMOD_TASK级别的初始化回调 */
    OsInitCall(LOS_INIT_LEVEL_KMOD_TASK);

    OsSchedStart();  /* 启动CPU调度器，开始任务调度 */
}

/**
 * @brief 设置SMP操作接口
 * @param  ops - SMP操作接口实现
 * @details 注册平台特定的SMP操作函数集，必须在SMP初始化前调用
 */
VOID LOS_SmpOpsSet(struct SmpOps *ops)
{
    g_smpOps = ops;  /* 保存SMP操作接口指针 */
}

/**
 * @brief SMP系统初始化函数
 * @details 启动所有secondary CPU并完成多处理器系统的初始化
 * @note 必须先调用LOS_SmpOpsSet注册SMP操作接口
 */
VOID OsSmpInit(VOID)
{
    UINT32 cpuNum = 1;  /* 从CPU 1开始初始化secondary CPU */

    /* 检查SMP操作接口是否已注册 */
    if (g_smpOps == NULL) {
        PRINT_ERR("Must call the interface(LOS_SmpOpsSet) to register smp operations firstly!\n");
        return;
    }

    /* 遍历并启动所有secondary CPU */
    for (; cpuNum < CORE_NUM; cpuNum++) {
        /* 调用硬件接口启动指定CPU，传入初始化函数和参数 */
        HalArchCpuOn(cpuNum, OsSmpSecondaryInit, g_smpOps, 0);
    }

    return;
}
#endif
