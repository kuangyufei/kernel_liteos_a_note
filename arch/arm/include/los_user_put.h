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

#ifndef _LOS_USER_PUT_H
#define _LOS_USER_PUT_H

#include "los_typedef.h"
#include "arm_user_put.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_user
 * @brief 将数据从内核空间拷贝到用户空间
 *
 * @par 描述
 * 此函数在拷贝数据前会先验证用户模式是否有权限访问目标地址空间，
 * 确保内核空间数据安全地拷贝到用户空间，防止非法内存访问
 *
 * @param dst [OUT] 类型 #用户空间指针: 用户空间的目标缓冲区地址
 * @param src [IN]  类型 #内核空间指针: 内核空间的源缓冲区地址
 *
 * @note
 * <ul>
 * <li>仅支持32位平台上的简单数据类型，如char(1字节)、short(2字节)、int(4字节)、long(4字节)</li>
 * <li>复杂数据类型或结构体需使用其他内存拷贝接口</li>
 * </ul>
 *
 * @retval #-EFAULT (-14) 拷贝失败，通常是由于用户空间地址无效或不可访问
 * @retval #0 拷贝成功
 */
#define LOS_PutUser(src, dst) _arm_put_user((dst), (src), sizeof(*(dst)), sizeof(*(src)))  /* 调用ARM架构专用实现，传递目标地址、源地址及两者数据大小 */

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_USER_PUT_H */
