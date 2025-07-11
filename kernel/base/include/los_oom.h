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

#ifndef _LOS_OOM_H
#define _LOS_OOM_H
#include "los_typedef.h"

/**
 * @brief OOM（内存溢出）管理相关宏定义
 * @details 包含OOM检测任务属性、内存检查参数及阈值配置
 */
#define OOM_TASK_PRIORITY                   9               /* OOM检测任务的优先级，数值越小优先级越高 */
#define OOM_TASK_STACK_SIZE                 0x1000          /* OOM检测任务的栈大小，十六进制0x1000对应十进制4096字节 */

#define OOM_CHECK_MIN                       10              /* 最小检查间隔，单位为10ms，10表示0.1秒 */
#define OOM_DEFAULT_CHECK_INTERVAL          100             /* 默认内存检查间隔，单位为10ms，100表示1秒 */
#define OOM_CHECK_MAX                       1000            /* 最大检查间隔，单位为10ms，1000表示10秒 */

#define OOM_DEFAULT_LOW_MEM_THRESHOLD       0x80000         /* 默认低内存阈值，十六进制0x80000对应十进制524288字节（512KB） */
#define OOM_DEFAULT_LOW_MEM_THRESHOLD_MIN   0               /* 低内存阈值最小值，0表示始终触发内存不足检测 */
#define OOM_DEFAULT_LOW_MEM_THRESHOLD_MAX   0x100000        /* 低内存阈值最大值，十六进制0x100000对应十进制1048576字节（1MB） */

#define OOM_DEFAULT_RECLAIM_MEM_THRESHOLD   0x500000        /* 默认内存回收阈值，十六进制0x500000对应十进制5242880字节（5MB） */

/**
 * @brief OOM处理函数指针类型定义
 * @param param 传递给OOM处理函数的参数
 * @return UINT32 处理结果，0表示成功，非0表示失败
 */
typedef UINT32 (*OomFn)(UINTPTR param);

/**
 * @struct OomCB 
 * @brief OOM控制块结构
 * @details OOM 是Out of Memory 的缩写，指的是内存不足的情况。
 * 当系统内存不足时，OOM 机制会根据一定的策略选择一些进程进行终止，以释放内存空间。
 * 这些进程的选择策略和终止顺序是由 OOM 算法和调度器决定的。
 * OOM 算法根据进程的优先级、占用内存大小、运行时间等因素进行评分，
 * 评分高的进程优先被选择终止。
 * 调度器则根据 OOM 算法选择的进程终止顺序，依次终止进程，直到系统内存恢复正常。
 */
typedef struct {
    UINT32       lowMemThreshold;       /* 低内存阈值(字节) */
    UINT32       reclaimMemThreshold;   /* 内存回收阈值(字节) */
    UINT32       checkInterval;         /* 检查间隔(0.1秒为单位) */
    OomFn        processVictimCB;       /* 进程终止回调函数 */
    OomFn        scoreCB;               /* 进程OOM分数计算回调函数 */
    UINT16       swtmrID;               /* 定时器ID */
    BOOL         enabled;               /* OOM功能使能标志 */
} OomCB;

LITE_OS_SEC_TEXT_MINOR UINT32 OomTaskInit(VOID);
LITE_OS_SEC_TEXT_MINOR VOID OomInfodump(VOID);
LITE_OS_SEC_TEXT_MINOR VOID OomEnable(VOID);
LITE_OS_SEC_TEXT_MINOR VOID OomDisable(VOID);
LITE_OS_SEC_TEXT_MINOR VOID OomSetLowMemThreashold(UINT32 lowMemThreshold);
LITE_OS_SEC_TEXT_MINOR VOID OomSetReclaimMemThreashold(UINT32 reclaimMemThreshold);
LITE_OS_SEC_TEXT_MINOR VOID OomSetCheckInterval(UINT32 checkInterval);
LITE_OS_SEC_TEXT_MINOR BOOL OomCheckProcess(VOID);
#endif

