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

/**
 * @defgroup los_cpup CPU usage
 * @ingroup kernel
 */

#ifndef _LOS_CPUP_H
#define _LOS_CPUP_H

#include "los_hwi.h"
#include "los_base.h"
#include "los_sys.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @ingroup los_cpup
 * CPU使用率错误码：内存请求失败
 *
 * 值：0x02001e00 (十进制33558272)
 *
 * 解决方案：减少最大进程数量
 */
#define LOS_ERRNO_CPUP_NO_MEMORY     LOS_ERRNO_OS_ERROR(LOS_MOD_CPUP, 0x00)

/**
 * @ingroup los_cpup
 * CPU使用率错误码：输入参数指针错误
 *
 * 值：0x02001e01 (十进制33558273)
 *
 * 解决方案：检查输入参数是否有效
 */
#define LOS_ERRNO_CPUP_PTR_ERR       LOS_ERRNO_OS_ERROR(LOS_MOD_CPUP, 0x01)

/**
 * @ingroup los_cpup
 * CPU使用率错误码：CPU使用率未初始化
 *
 * 值：0x02001e02 (十进制33558274)
 *
 * 解决方案：检查CPU使用率是否已初始化
 */
#define LOS_ERRNO_CPUP_NO_INIT       LOS_ERRNO_OS_ERROR(LOS_MOD_CPUP, 0x02)

/**
 * @ingroup los_cpup
 * CPU使用率错误码：目标CPU使用率监控未创建
 *
 * 值：0x02001e03 (十进制33558275)
 *
 * 解决方案：检查目标CPU使用率监控是否已创建
 */
#define LOS_ERRNO_CPUP_NO_CREATED    LOS_ERRNO_OS_ERROR(LOS_MOD_CPUP, 0x03)

/**
 * @ingroup los_cpup
 * CPU使用率错误码：目标CPU使用率ID无效
 *
 * 值：0x02001e04 (十进制33558276)
 *
 * 解决方案：检查目标CPU使用率ID是否适用于当前操作
 */
#define LOS_ERRNO_CPUP_ID_INVALID    LOS_ERRNO_OS_ERROR(LOS_MOD_CPUP, 0x04)

/**
 * @ingroup los_cpup
 * 单核CPU所有进程和任务的使用率总和，单位为万分之一（0.01%）
 * 取值范围：[0, 10000]，对应0%到100%
 */
#define LOS_CPUP_SINGLE_CORE_PRECISION       10000

/**
 * @ingroup los_cpup
 * 当前CPU使用率精度转换为百分比的倍数
 * 计算方式：10000 / 100 = 100，用于将万分之一精度值转换为百分比（除以100）
 */
#define LOS_CPUP_PRECISION_MULT              (LOS_CPUP_SINGLE_CORE_PRECISION / 100)

/**
 * @ingroup los_cpup
 * 所有核心CPU所有进程的使用率总和
 * 计算方式：单核精度 × 核心数，单位为万分之一
 * 例如：双核系统中取值范围为[0, 20000]，对应0%到200%
 */
#define LOS_CPUP_PRECISION                   (LOS_CPUP_SINGLE_CORE_PRECISION * LOSCFG_KERNEL_CORE_NUM)

/**
 * @ingroup los_cpup
 * CPU使用率信息结构体，记录所有CPU核心的使用率数据
 */
typedef struct tagCpupInfo {
    UINT16 status; /**< 当前CPU使用率监控状态（0：未使用，1：已启用等） */
    UINT32 usage;  /**< CPU使用率值，取值范围[0, LOS_CPUP_SINGLE_CORE_PRECISION]，单位为万分之一 */
} CPUP_INFO_S;

/**
 * @ingroup los_cpup
 * 系统CPU使用率查询时间范围枚举
 */
enum {
    CPUP_LAST_TEN_SECONDS = 0, /**< 查询最近10秒的CPU使用率 */
    CPUP_LAST_ONE_SECONDS = 1, /**< 查询最近1秒的CPU使用率 */
    CPUP_ALL_TIME = 0xffff     /**< 查询从系统启动到当前的CPU使用率（0xffff表示最大无符号16位整数） */
};

/**
 * @ingroup los_cpup
 * @brief Obtain the historical CPU usage.
 *
 * @par Description:
 * This API is used to obtain the historical CPU usage.
 * @attention
 * <ul>
 * <li>This API can be called only after the CPU usage is initialized. Otherwise, the CPU usage fails to be
 * obtained.</li>
 * </ul>
 *
 * @param  mode     [IN] UINT16. process mode. The parameter value 0 indicates that the CPU usage within 10s will be
 *                               obtained, and the parameter value 1 indicates that the CPU usage in the former 1s will
 *                               be obtained. Other values indicate that the CPU usage in all time will be obtained.
 *
 * @retval #LOS_ERRNO_CPUP_NO_INIT           The CPU usage is not initialized.
 * @retval #UINT32                           [0, LOS_CPUP_SINGLE_CORE_PRECISION], historical CPU usage,
 *                                           of which the precision is adjustable.
 * @par Dependency:
 * <ul><li>los_cpup.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_HistorySysCpuUsage(UINT16 mode);

/**
 * @ingroup los_cpup
 * @brief  Obtain the historical CPU usage of a specified process.
 *
 * @par Description:
 * This API is used to obtain the historical CPU usage of a process specified by a passed-in process ID.
 * @attention
 * <ul>
 * <li>This API can be called only after the CPU usage is initialized. Otherwise,
 * the CPU usage fails to be obtained.</li>
 * <li>The passed-in process ID must be valid and the process specified by the process ID must be created. Otherwise,
 * the CPU usage fails to be obtained.</li>
 * </ul>
 *
 * @param pid      [IN] UINT32. process ID.
 * @param mode     [IN] UINT16. cpup mode. The parameter value 0 indicates that the CPU usage within 10s will be
 *                              obtained, and the parameter value 1 indicates that the CPU usage in the former 1s will
 *                              be obtained. Other values indicate that the CPU usage in the period that is less than
 *                              1s will be obtained.
 *
 * @retval #LOS_ERRNO_CPUP_NO_INIT          The CPU usage is not initialized.
 * @retval #LOS_ERRNO_CPUP_ID_INVALID       The target process ID is invalid.
 * @retval #LOS_ERRNO_CPUP_NO_CREATED       The target process is not created.
 * @retval #UINT32                          [0, LOS_CPUP_PRECISION], CPU usage of the specified process.
 * @par Dependency:
 * <ul><li>los_cpup.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_HistoryProcessCpuUsage(UINT32 pid, UINT16 mode);

/**
 * @ingroup los_cpup
 * @brief  Obtain the historical CPU usage of a specified task.
 *
 * @par Description:
 * This API is used to obtain the historical CPU usage of a task specified by a passed-in task ID.
 * @attention
 * <ul>
 * <li>This API can be called only after the CPU usage is initialized. Otherwise,
 * the CPU usage fails to be obtained.</li>
 * <li>The passed-in task ID must be valid and the task specified by the task ID must be created. Otherwise,
 * the CPU usage fails to be obtained.</li>
 * </ul>
 *
 * @param tid      [IN] UINT32. task ID.
 * @param mode     [IN] UINT16. cpup mode. The parameter value 0 indicates that the CPU usage within 10s will be
 *                              obtained, and the parameter value 1 indicates that the CPU usage in the former 1s will
 *                              be obtained. Other values indicate that the CPU usage in the period that is less than
 *                              1s will be obtained.
 *
 * @retval #LOS_ERRNO_CPUP_NO_INIT          The CPU usage is not initialized.
 * @retval #LOS_ERRNO_CPUP_ID_INVALID       The target task ID is invalid.
 * @retval #LOS_ERRNO_CPUP_NO_CREATED       The target task is not created.
 * @retval #UINT32                          [0, LOS_CPUP_SINGLE_CORE_PRECISION], CPU usage of the specified process.
 * @par Dependency:
 * <ul><li>los_cpup.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_HistoryTaskCpuUsage(UINT32 tid, UINT16 mode);

/**
 * @ingroup los_cpup
 * @brief Obtain the CPU usage of processes.
 *
 * @par Description:
 * This API is used to obtain the CPU usage of processes.
 * @attention
 * <ul>
 * <li>This API can be called only after the CPU usage is initialized. Otherwise, the CPU usage fails to be
 * obtained.</li>
 * <li>The input parameter pointer must not be NULL, Otherwise, the CPU usage fails to be obtained.</li>
 * <li>The input parameter pointer should point to the structure array whose size be greater than
 * (LOS_GetSystemProcessMaximum() * sizeof (CPUP_INFO_S)).</li>
 * </ul>
 *
 * @param mode       [IN] UINT16. Time mode. The parameter value 0 indicates that the CPU usage within 10s will be
 *                                obtained, and the parameter value 1 indicates that the CPU usage in the former 1s
 *                                will be obtained.Other values indicate that the CPU usage in all time will be
 *                                obtained.
 * @param cpupInfo   [OUT]Type.   CPUP_INFO_S* Pointer to the CPUP information structure to be obtained.
 * @param len        [IN] UINT32. The Maximum length of processes.
 *
 * @retval #LOS_ERRNO_CPUP_NO_INIT                  The CPU usage is not initialized.
 * @retval #LOS_ERRNO_CPUP_PROCESS_PTR_ERR          The input parameter pointer is NULL or
 *                                                  len less than LOS_GetSystemProcessMaximum() * sizeof (CPUP_INFO_S).
 * @retval #LOS_OK                                  The CPU usage of all processes is successfully obtained.
 * @par Dependency:
 * <ul><li>los_cpup.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_GetAllProcessCpuUsage(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len);

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
/**
 * @ingroup los_cpup
 * @brief Obtain the CPU usage of hwi.
 *
 * @par Description:
 * This API is used to obtain the CPU usage of hwi.
 * @attention
 * <ul>
 * <li>This API can be called only after the CPU usage is initialized. Otherwise, the CPU usage fails to be
 * obtained.</li>
 * <li>The input parameter pointer must not be NULL, Otherwise, the CPU usage fails to be obtained.</li>
 * <li>The input parameter pointer should point to the structure array whose size be greater than
 * (LOS_GetSystemHwiMaximum() * sizeof (CPUP_INFO_S)).</li>
 * </ul>
 *
 * @param mode       [IN] UINT16. Time mode. The parameter value 0 indicates that the CPU usage within 10s will be
 *                                obtained, and the parameter value 1 indicates that the CPU usage in the former 1s
 *                                will be obtained.Other values indicate that the CPU usage in all time will be
 *                                obtained.
 * @param cpupInfo   [OUT]Type.   CPUP_INFO_S* Pointer to the CPUP information structure to be obtained.
 * @param len        [IN] UINT32. The Maximum length of hwis.
 *
 * @retval #LOS_ERRNO_CPUP_NO_INIT                  The CPU usage is not initialized.
 * @retval #LOS_ERRNO_CPUP_PROCESS_PTR_ERR          The input parameter pointer is NULL or
 *                                                  len less than LOS_GetSystemHwiMaximum() * sizeof (CPUP_INFO_S).
 * @retval #LOS_OK                                  The CPU usage of all hwis is successfully obtained.
 * @par Dependency:
 * <ul><li>los_cpup.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 LOS_GetAllIrqCpuUsage(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len);
#endif

/**
 * @ingroup los_cpup
 * @brief Reset the data of CPU usage.
 *
 * @par Description:
 * This API is used to reset the data of CPU usage.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param None.
 *
 * @retval #None.
 *
 * @par Dependency:
 * <ul><li>los_cpup.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern VOID LOS_CpupReset(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif
