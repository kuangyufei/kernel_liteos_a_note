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

#include "los_tick_pri.h"
#include "los_swtmr_pri.h"
#include "los_sched_pri.h"
#ifdef LOSCFG_KERNEL_VDSO
#include "los_vdso.h"
#endif


// 系统时钟频率 (Hz)，由系统初始化时配置
LITE_OS_SEC_DATA_INIT UINT32 g_sysClock;        
// 每秒的系统滴答数，即系统定时器中断频率
LITE_OS_SEC_DATA_INIT UINT32 g_tickPerSecond;    
// 周期数到纳秒的转换比例 (ns = cycle * g_cycle2NsScale)
LITE_OS_SEC_BSS DOUBLE g_cycle2NsScale;            

/* 任务模块的自旋锁，用于保护滴答相关操作的原子性 */
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_tickSpin); 


/**
 * @brief 系统滴答中断处理函数
 * @details 由定时器中断触发，负责处理滴答相关的核心逻辑，包括调试信息记录、
 *          VDSO时间更新、硬件时钟中断清除和调度器滴答处理
 * @return 无
 */
LITE_OS_SEC_TEXT VOID OsTickHandler(VOID)
{
#ifdef LOSCFG_SCHED_TICK_DEBUG 
    OsSchedDebugRecordData();  /* 记录调度调试数据，用于性能分析和问题定位 */
#endif

#ifdef LOSCFG_KERNEL_VDSO
    // 更新VDSO(虚拟动态共享对象)数据页时间
    // VDSO允许用户进程直接访问内核时间信息，无需系统调用(如:gettimeofday)
    OsVdsoTimevalUpdate();
#endif

#ifdef LOSCFG_BASE_CORE_TICK_HW_TIME
    HalClockIrqClear(); /* 清除硬件时钟中断标志，平台相关实现 */
#endif

    OsSchedTick();  /* 调用调度器滴答处理函数，进行任务调度决策 */
}

