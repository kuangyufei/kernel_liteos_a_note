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

#ifndef _LOS_TRACE_H
#define _LOS_TRACE_H

#include "los_trace_frame.h"
#include "los_base.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* Provide a mechanism for kernel tracking. Record the data to global buffer,
 * and you can get the data from /proc/ktrace.
 */

/**
 * @ingroup los_trace
 * Task error code: User type is invalid when register new trace.
 *
 * Value: 0x02001400
 *
 * Solution: Use valid type to regeister the new trace.
 */
#define LOS_ERRNO_TRACE_TYPE_INVALID    LOS_ERRNO_OS_ERROR(LOS_MOD_TRACE, 0x00)

/**
 * @ingroup los_trace
 * Task error code: The callback function is null when register new trace.
 *
 * Value: 0x02001401
 *
 * Solution: Use valid callback function to regeister the new trace.
 */
#define LOS_ERRNO_TRACE_FUNCTION_NULL   LOS_ERRNO_OS_ERROR(LOS_MOD_TRACE, 0x01)

/**
 * @ingroup los_trace
 * Task error code: The type is already registered.
 *
 * Value: 0x02001402
 *
 * Solution: Use valid type to regeister the new trace.
 */
#define LOS_ERRNO_TRACE_TYPE_EXISTED    LOS_ERRNO_OS_ERROR(LOS_MOD_TRACE, 0x02)

/**
 * @ingroup los_trace
 * Task error code: The type is not registered.
 *
 * Value: 0x02001403
 *
 * Solution: Use valid type to regeister the new trace.
 */
#define LOS_ERRNO_TRACE_TYPE_NOT_EXISTED    LOS_ERRNO_OS_ERROR(LOS_MOD_TRACE, 0x03)

/**
* @ingroup  los_trace
* @brief Define the type of a function used to record trace.
*
* @par Description:
* This API is used to define the type of a recording trace function and call it after task or interrupt switch.
* @attention None.
*
* @param  inBuf     [IN]  Type #UINT8 * The buffer saved trace information.
* @param  bufLen    [IN]  Type #UINT32  The buffer len.
* @param  ap        [IN]  Type #va_list The trace data list.
*
* @retval trace frame length.
* @par Dependency:
* <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
* @see
*/
typedef INT32 (*WriteHook)(UINT8 *inBuf, UINT32 bufLen, va_list ap);

/**
 * @ingroup los_trace
 * Stands for the trace type can be registered.
 */
typedef enum {
    LOS_TRACE_TYPE_MIN  = 0,
    LOS_TRACE_TYPE_MAX  = 15,
} TraceType;

typedef enum {
    LOS_TRACE_DISABLE = 0,
    LOS_TRACE_ENABLE = 1,
} TraceSwitch;

/**
 * @ingroup los_trace
 * @brief main trace function is called by user to logger the information.
 *
 * @par Description:
 * This API is used to trace the infomation.
 * @attention
 * <ul>
 * <li>This API can be called only after trace type is intialized. Otherwise, the trace will be failed.</li>
 * </ul>
 *
 * @param  traceType    [IN] TraceType Type of trace information.
 * @param  ...          [IN] The trace data.
 *
 * @retval None.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_Trace
 */
VOID LOS_Trace(TraceType traceType, ...);

/**
 * @ingroup los_trace
 * @brief intialize the trace when the system startup.
 *
 * @par Description:
 * This API is used to intilize the trace for system level.
 * @attention
 * <ul>
 * <li>This API can be called only after the memory is initialized. Otherwise, the CPU usage fails to be obtained.</li>
 * </ul>
 *
 * @param None.
 *
 * @retval #LOS_OK      : The trace function is initialized successfully.
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see OsTraceInit
 */
UINT32 OsTraceInit(VOID);

/**
 * @ingroup los_trace
 * @brief register the hook for specific trace type.
 *
 * @par Description:
 * This API is used to register the hook for specific trace type.
 * @attention
 * <ul>
 * <li>This API can be called only after that trace type, input hookfnc and trace's data struct are established.</li>
 * Otherwise, the trace will be failed.</li>
 * </ul>
 *
 * @param  traceType    [IN] TraceType. Type of trace information.
 * @param  inhook       [IN] WriteHook. It's a callback function to store the useful trace information.
 * @param  typeStr      [IN] const CHAR *. The trace name of trace type.
 * @param  onOff        [IN] TraceSwitch. It only can be LOS_TRACE_DISABLE or LOS_TRACE_ENABLE.
 *
 * @retval #LOS_ERRNO_TRACE_NO_MEMORY          0x02001400: The memory is not enough for initilize.
 * @retval #LOS_ERRNO_TRACE_TYPE_INVALID       0x02001401: The trace type is invalid. Valid type is from
 *                                                         LOS_TRACE_TYPE_MAX
 * @retval #LOS_ERRNO_TRACE_FUNCTION_NULL      0x02001402: The input callback function is NULL
 * @retval #LOS_ERRNO_TRACE_MAX_SIZE_INVALID   0x02001403: The information maxmum size is 0 to store.
 * @retval #LOS_OK                             0x00000000: The registeration is successful.
 *
 * @par Dependency:
 * <ul><li>los_trace.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_TraceUserReg
 */
UINT32 LOS_TraceReg(TraceType traceType, WriteHook inHook, const CHAR *typeStr, TraceSwitch onOff);

UINT32 LOS_TraceUnreg(TraceType traceType);

UINT8 *LOS_TraceBufDataGet(UINT32 *desLen, UINT32 *relLen);

VOID LOS_TraceSwitch(TraceSwitch onOff);

UINT32 LOS_TraceTypeSwitch(TraceType traceType, TraceSwitch onOff);

#ifdef LOSCFG_FS_VFS
INT32 LOS_Trace2File(const CHAR *filename);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
