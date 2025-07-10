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
 * @brief 
 * @verbatim 
	调用API接口时可能会出现错误，此时接口会返回对应的错误码，以便快速定位错误原因。

	错误码是一个32位的无符号整型数，31~24位表示错误等级，23~16位表示错误码标志（当前该标志值为0），
	15~8位代表错误码所属模块，7~0位表示错误码序号。

	错误码中的错误等级
		错误等级		数值		含义
		NORMAL		0		提示
		WARN		1		告警
		ERR			2		严重
		FATAL		3		致命 
		
	例如
		#define LOS_ERRNO_TSK_NO_MEMORY  LOS_ERRNO_OS_FATAL(LOS_MOD_TSK, 0x00)
		#define LOS_ERRNO_OS_FATAL(MID, ERRNO)  \
			(LOS_ERRTYPE_FATAL | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))
	说明
		LOS_ERRTYPE_FATAL：错误等级为FATAL，值为0x03000000 LOS_ERRNO_OS_ID：错误码标志，
		值为0x000000 MID：所属模块，LOS_MOD_TSK的值为0x2 ERRNO：错误码序号 
		所以LOS_ERRNO_TSK_NO_MEMORY的值为0x03000200
		
	错误码接管
		有时只靠错误码不能快速准确的定位问题，为方便用户分析错误，错误处理模块支持
		注册错误处理的钩子函数，发生错误时，用户可以调用LOS_ErrHandle接口以执行错误处理函数。
 * @endverbatim
 */

/**
 * @defgroup los_errno Error code
 * @ingroup kernel
 */

#ifndef _LOS_ERRNO_H
#define _LOS_ERRNO_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */



/**
 * @ingroup los_errno
 * OS错误码标志位，用于标识错误码属于操作系统模块
 * 取值：0x00U << 16 = 0x00000000（十进制0）
 * 说明：占据32位错误码的16-23位（共8位），当前未启用子系统区分
 */
#define LOS_ERRNO_OS_ID (0x00U << 16)

/**
 * @ingroup los_errno
 * 定义信息性错误级别（最低严重程度）
 * 取值：0x00U << 24 = 0x00000000（十进制0）
 * 说明：占据32位错误码的24-31位（高8位），用于表示非错误性状态信息
 */
#define LOS_ERRTYPE_NORMAL (0x00U << 24)

/**
 * @ingroup los_errno
 * 定义警告级错误级别
 * 取值：0x01U << 24 = 0x01000000（十进制16777216）
 * 说明：表示不影响主流程但需要关注的异常情况
 */
#define LOS_ERRTYPE_WARN (0x01U << 24)

/**
 * @ingroup los_errno
 * 定义严重级错误级别
 * 取值：0x02U << 24 = 0x02000000（十进制33554432）
 * 说明：表示功能模块异常但系统仍可运行的错误状态
 */
#define LOS_ERRTYPE_ERROR (0x02U << 24)

/**
 * @ingroup los_errno
 * 定义致命级错误级别（最高严重程度）
 * 取值：0x03U << 24 = 0x03000000（十进制50331648）
 * 说明：表示可能导致系统崩溃或无法恢复的严重错误
 */
#define LOS_ERRTYPE_FATAL (0x03U << 24)

/**
 * @ingroup los_errno
 * 构造致命级OS错误码
 * @param MID 模块ID，取值范围参考<mcfile name="los_err.h" path="d:/project/kernel_liteos_a_note/kernel/include/los_err.h"></mcfile>中的LOS_MOUDLE_ID枚举
 * @param ERRNO 模块内错误编号（0-255）
 * @return 32位错误码，格式为：[错误级别(8位)][OS标志(8位)][模块ID(8位)][错误编号(8位)]
 * @example LOS_ERRNO_OS_FATAL(LOS_MOD_MEM, 0x05) = 0x03000105（十进制50331909）
 */
#define LOS_ERRNO_OS_FATAL(MID, ERRNO) \
    (LOS_ERRTYPE_FATAL | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

/**
 * @ingroup los_errno
 * 构造严重级OS错误码
 * @param MID 模块ID（参考LOS_MOUDLE_ID枚举）
 * @param ERRNO 模块内错误编号（0-255）
 * @return 32位错误码，格式同上
 * @example LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x0A) = 0x0200020A（十进制33554954）
 */
#define LOS_ERRNO_OS_ERROR(MID, ERRNO) \
    (LOS_ERRTYPE_ERROR | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

/**
 * @ingroup los_errno
 * 构造警告级OS错误码
 * @param MID 模块ID（参考LOS_MOUDLE_ID枚举）
 * @param ERRNO 模块内错误编号（0-255）
 * @return 32位错误码，格式同上
 * @example LOS_ERRNO_OS_WARN(LOS_MOD_SEM, 0x03) = 0x01000703（十进制16779011）
 */
#define LOS_ERRNO_OS_WARN(MID, ERRNO) \
    (LOS_ERRTYPE_WARN | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

/**
 * @ingroup los_errno
 * 构造信息级OS错误码
 * @param MID 模块ID（参考LOS_MOUDLE_ID枚举）
 * @param ERRNO 模块内错误编号（0-255）
 * @return 32位错误码，格式同上
 * @example LOS_ERRNO_OS_NORMAL(LOS_MOD_LOG, 0x00) = 0x00000E00（十进制3584）
 */
#define LOS_ERRNO_OS_NORMAL(MID, ERRNO) \
    (LOS_ERRTYPE_NORMAL | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_ERRNO_H */
