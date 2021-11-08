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
 * @defgroup los_sys System time
 * @ingroup kernel
 */

#ifndef _LOS_SYS_H
#define _LOS_SYS_H

#include "los_base.h"
#include "los_hwi.h"
#include "los_hw.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @file los_sys.h
 * @brief 
@verbatim 
基本概念
	时间管理以系统时钟为基础，给应用程序提供所有和时间有关的服务。

	系统时钟是由定时器/计数器产生的输出脉冲触发中断产生的，一般定义为整数或长整数。
	输出脉冲的周期叫做一个“时钟滴答”。系统时钟也称为时标或者Tick。

	用户以秒、毫秒为单位计时，而操作系统以Tick为单位计时，当用户需要对系统进行操作时，
	例如任务挂起、延时等，此时需要时间管理模块对Tick和秒/毫秒进行转换。
	时间管理模块提供时间转换、统计、延迟功能

相关概念
	Cycle
	系统最小的计时单位。Cycle的时长由系统主时钟频率决定，系统主时钟频率就是每秒钟的Cycle数。

	Tick
	Tick是操作系统的基本时间单位，由用户配置的每秒Tick数决定。

使用场景
	用户需要了解当前系统运行的时间以及Tick与秒、毫秒之间的转换关系等。 

时间管理的典型开发流程
	根据实际需求，在板级配置适配时确认是否使能LOSCFG_BASE_CORE_TICK_HW_TIME宏选择外部定时器，
	并配置系统主时钟频率OS_SYS_CLOCK（单位Hz）。OS_SYS_CLOCK的默认值基于硬件平台配置。
	通过make menuconfig配置LOSCFG_BASE_CORE_TICK_PER_SECOND。
	
注意事项
	时间管理不是单独的功能模块，依赖于OS_SYS_CLOCK和LOSCFG_BASE_CORE_TICK_PER_SECOND两个配置选项。
	系统的Tick数在关中断的情况下不进行计数，故系统Tick数不能作为准确时间使用。

@endverbatim 
 */


/**
 * @ingroup los_sys
 * System time basic function error code: Null pointer.
 *
 * Value: 0x02000010
 *
 * Solution: Check whether the input parameter is null.
 */
#define LOS_ERRNO_SYS_PTR_NULL                 LOS_ERRNO_OS_ERROR(LOS_MOD_SYS, 0x10)

/**
 * @ingroup los_sys
 * System time basic function error code: Invalid system clock configuration.
 *
 * Value: 0x02000011
 *
 * Solution: Configure a valid system clock in los_config.h.
 */
#define LOS_ERRNO_SYS_CLOCK_INVALID            LOS_ERRNO_OS_ERROR(LOS_MOD_SYS, 0x11)

/**
 * @ingroup los_sys
 * System time basic function error code: This error code is not in use temporarily.
 *
 * Value: 0x02000012
 *
 * Solution: None.
 */
#define LOS_ERRNO_SYS_MAXNUMOFCORES_IS_INVALID LOS_ERRNO_OS_ERROR(LOS_MOD_SYS, 0x12)

/**
 * @ingroup los_sys
 * System time error code: This error code is not in use temporarily.
 *
 * Value: 0x02000013
 *
 * Solution: None.
 */
#define LOS_ERRNO_SYS_PERIERRCOREID_IS_INVALID LOS_ERRNO_OS_ERROR(LOS_MOD_SYS, 0x13)

/**
 * @ingroup los_sys
 * System time error code: This error code is not in use temporarily.
 *
 * Value: 0x02000014
 *
 * Solution: None.
 */
#define LOS_ERRNO_SYS_HOOK_IS_FULL             LOS_ERRNO_OS_ERROR(LOS_MOD_SYS, 0x14)

/**
 * @ingroup los_typedef
 * system time structure.
 */
typedef struct tagSysTime {
    UINT16 uwYear;   /**< value 1970 ~ 2038 or 1970 ~ 2100 */
    UINT8 ucMonth;   /**< value 1 - 12 */
    UINT8 ucDay;     /**< value 1 - 31 */
    UINT8 ucHour;    /**< value 0 - 23 */
    UINT8 ucMinute;  /**< value 0 - 59 */
    UINT8 ucSecond;  /**< value 0 - 59 */
    UINT8 ucWeek;    /**< value 0 - 6  */
} SYS_TIME_S;

/**
 * @ingroup los_sys
 * @brief Obtain the number of Ticks.
 *
 * @par Description:
 * This API is used to obtain the number of Ticks.
 * @attention
 * <ul>
 * <li>None</li>
 * </ul>
 *
 * @param  None
 *
 * @retval UINT64 The number of Ticks.
 * @par Dependency:
 * <ul><li>los_sys.h: the header file that contains the API declaration.</li></ul>
 * @see None
 */
extern UINT64 LOS_TickCountGet(VOID);

/**
 * @ingroup los_sys
 * @brief Obtain the number of cycles in one second.
 *
 * @par Description:
 * This API is used to obtain the number of cycles in one second.
 * @attention
 * <ul>
 * <li>None</li>
 * </ul>
 *
 * @param  None
 *
 * @retval UINT32 Number of cycles obtained in one second.
 * @par Dependency:
 * <ul><li>los_sys.h: the header file that contains the API declaration.</li></ul>
 * @see None
 */
extern UINT32 LOS_CyclePerTickGet(VOID);

/**
 * @ingroup los_sys
 * @brief Convert Ticks to milliseconds.
 *
 * @par Description:
 * This API is used to convert Ticks to milliseconds.
 * @attention
 * <ul>
 * <li>The number of milliseconds obtained through the conversion is 32-bit.</li>
 * </ul>
 *
 * @param  tick  [IN] Number of Ticks. The value range is (0,OS_SYS_CLOCK).
 *
 * @retval UINT32 Number of milliseconds obtained through the conversion. Ticks are successfully converted to
 * milliseconds.
 * @par  Dependency:
 * <ul><li>los_sys.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MS2Tick
 */
extern UINT32 LOS_Tick2MS(UINT32 tick);

/**
 * @ingroup los_sys
 * @brief Convert milliseconds to Ticks.
 *
 * @par Description:
 * This API is used to convert milliseconds to Ticks.
 * @attention
 * <ul>
 * <li>If the parameter passed in is equal to 0xFFFFFFFF, the retval is 0xFFFFFFFF. Pay attention to the value to be
 * converted because data possibly overflows.</li>
 * </ul>
 *
 * @param  millisec  [IN] Number of milliseconds.
 *
 * @retval UINT32 Number of Ticks obtained through the conversion.
 * @par Dependency:
 * <ul><li>los_sys.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_Tick2MS
 */
extern UINT32 LOS_MS2Tick(UINT32 millisec);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SYS_H */
