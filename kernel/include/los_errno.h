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
 * @ingroup los_errno
 * OS error code flag.
 */
#define LOS_ERRNO_OS_ID (0x00U << 16)

/**
 * @ingroup los_errno
 * Define the error level as informative.
 */
#define LOS_ERRTYPE_NORMAL (0x00U << 24)

/**
 * @ingroup los_errno
 * Define the error level as warning.
 */
#define LOS_ERRTYPE_WARN (0x01U << 24)

/**
 * @ingroup los_errno
 * Define the error level as critical.
 */
#define LOS_ERRTYPE_ERROR (0x02U << 24)

/**
 * @ingroup los_errno
 * Define the error level as fatal.
 */
#define LOS_ERRTYPE_FATAL (0x03U << 24)

/**
 * @ingroup los_errno
 * Define fatal OS errors.
 */
#define LOS_ERRNO_OS_FATAL(MID, ERRNO) \
    (LOS_ERRTYPE_FATAL | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

/**
 * @ingroup los_errno
 * Define critical OS errors.
 */
#define LOS_ERRNO_OS_ERROR(MID, ERRNO) \
    (LOS_ERRTYPE_ERROR | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

/**
 * @ingroup los_errno
 * Define warning OS errors.
 */
#define LOS_ERRNO_OS_WARN(MID, ERRNO) \
    (LOS_ERRTYPE_WARN | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

/**
 * @ingroup los_errno
 * Define informative OS errors.
 */
#define LOS_ERRNO_OS_NORMAL(MID, ERRNO) \
    (LOS_ERRTYPE_NORMAL | LOS_ERRNO_OS_ID | ((UINT32)(MID) << 8) | ((UINT32)(ERRNO)))

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_ERRNO_H */
