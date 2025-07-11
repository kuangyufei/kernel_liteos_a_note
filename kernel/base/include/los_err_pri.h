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

#ifndef _LOS_ERR_PRI_H
#define _LOS_ERR_PRI_H

#include "los_err.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_err
 * @brief 错误处理魔法字定义
 * @details 用于标识错误处理相关操作的特殊标记值，十六进制0xa1b2c3f8对应十进制2712847352
 */
#define OS_ERR_MAGIC_WORD 0xa1b2c3f8  ///< 错误处理魔法字，十六进制值：0xa1b2c3f8（十进制：2712847352）

/**
 * @ingroup los_err
 * @brief 能够返回错误码的错误处理宏
 *
 * @par 描述
 * 此API用于通过错误码调用错误处理函数，并返回相同的错误码
 * @attention
 * <ul>
 * <li>无特殊注意事项</li>
 * </ul>
 *
 * @param  errNo   [IN] 错误码
 *
 * @retval errNo 返回输入的错误码
 * @par 依赖
 * <ul><li>los_err_pri.h: 包含此API声明的头文件</li></ul>
 * @see None
 */
#define OS_RETURN_ERROR(errNo) do {                                               \
    (VOID)LOS_ErrHandle("os_unspecific_file", OS_ERR_MAGIC_WORD, errNo, 0, NULL); \
    return errNo;                                                                 \
} while (0)

/**
 * @ingroup los_err
 * @brief 带行号的错误处理宏
 *
 * @par 描述
 * 此API用于通过错误码和错误发生的行号调用错误处理函数，并返回相同的错误码
 * @attention
 * <ul>
 * <li>errLine参数应传入__LINE__宏以获取准确行号</li>
 * </ul>
 *
 * @param  errLine   [IN] 错误发生的行号
 * @param  errNo     [IN] 错误码
 *
 * @retval errNo 返回输入的错误码
 * @par 依赖
 * <ul><li>los_err_pri.h: 包含此API声明的头文件</li></ul>
 * @see None
 */
#define OS_RETURN_ERROR_P2(errLine, errNo) do {                          \
    (VOID)LOS_ErrHandle("os_unspecific_file", errLine, errNo, 0, NULL);  \
    return errNo;                                                        \
} while (0)

/**
 * @ingroup los_err
 * @brief 跳转到错误处理程序的宏
 *
 * @par 描述
 * 此API用于通过错误码调用错误处理函数，并跳转到ERR_HANDLER标签处
 * @attention
 * <ul>
 * <li>使用前必须定义errNo和errLine变量</li>
 * <li>必须存在ERR_HANDLER标签作为错误处理入口</li>
 * </ul>
 *
 * @param  errorNo   [IN] 错误码
 *
 * @retval None
 * @par 依赖
 * <ul><li>los_err_pri.h: 包含此API声明的头文件</li></ul>
 * @see None
 */
#define OS_GOTO_ERR_HANDLER(errorNo) do { \
    errNo = errorNo;                      \
    errLine = OS_ERR_MAGIC_WORD;          \
    goto ERR_HANDLER;                     \
} while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_ERR_PRI_H */
