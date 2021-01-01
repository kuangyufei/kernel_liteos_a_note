/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "los_err.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//系统中只有一个错误处理的钩子函数。当多次注册钩子函数时，最后一次注册的钩子函数会覆盖前一次注册的函数。
LITE_OS_SEC_BSS STATIC LOS_ERRORHANDLE_FUNC g_errHandleHook = NULL;//错误接管钩子函数
/******************************************************************************
调用钩子函数，处理错误
	fileName：存放错误日志的文件名,系统内部调用时，入参为"os_unspecific_file"
	lineNo：发生错误的代码行号系统内部调用时，若值为0xa1b2c3f8，表示未传递行号
	errnoNo：错误码
	paraLen：入参para的长度系统内部调用时，入参为0
	para：错误标签系统内部调用时，入参为NULL
******************************************************************************/
LITE_OS_SEC_TEXT_INIT UINT32 LOS_ErrHandle(CHAR *fileName, UINT32 lineNo, UINT32 errorNo,
                                           UINT32 paraLen, VOID *para)
{
    if (g_errHandleHook != NULL) {
        g_errHandleHook(fileName, lineNo, errorNo, paraLen, para);
    }

    return LOS_OK;
}
//设置钩子函数，处理错误
LITE_OS_SEC_TEXT_INIT VOID LOS_SetErrHandleHook(LOS_ERRORHANDLE_FUNC fun)
{
    g_errHandleHook = fun;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
