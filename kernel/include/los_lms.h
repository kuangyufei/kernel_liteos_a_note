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

#ifndef _LOS_LMS_H
#define _LOS_LMS_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_KERNEL_LMS  // 若启用内核内存安全检查功能
/**
 * @ingroup los_lms
 * @brief 添加内存池到LMS检查列表
 *
 * @par 描述
 * 将指定内存池添加到LMS（内存安全）模块的检查列表，用于后续内存安全验证
 *
 * @param pool  [IN] 指向内存池起始地址的指针
 * @param size  [IN] 内存池大小（单位：字节）
 *
 * @retval #LOS_OK  内存池添加成功
 * @retval #其他错误码  添加失败（具体含义参见错误码定义）
 * @par 依赖
 * <ul><li>los_lms.h: 包含API声明的头文件</li></ul>
 */
UINT32 LOS_LmsCheckPoolAdd(const VOID *pool, UINT32 size);

/**
 * @ingroup los_lms
 * @brief 从LMS检查列表中删除内存池
 *
 * @par 描述
 * 将指定内存池从LMS（内存安全）模块的检查列表中移除，停止对其进行内存安全验证
 *
 * @param pool  [IN] 指向内存池起始地址的指针（必须是已添加的内存池）
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_lms.h: 包含API声明的头文件</li></ul>
 */
VOID LOS_LmsCheckPoolDel(const VOID *pool);

/**
 * @ingroup los_lms
 * @brief 启用指定地址范围的内存保护
 *
 * @par 描述
 * 对指定起始地址和结束地址范围内的内存区域启用保护机制，防止非法访问
 *
 * @param addrStart  [IN] 内存保护区域的起始地址（必须对齐）
 * @param addrEnd    [IN] 内存保护区域的结束地址（必须对齐，不包含在保护范围内）
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_lms.h: 包含API声明的头文件</li></ul>
 */
VOID LOS_LmsAddrProtect(UINTPTR addrStart, UINTPTR addrEnd);

/**
 * @ingroup los_lms
 * @brief 禁用指定地址范围的内存保护
 *
 * @par 描述
 * 对指定起始地址和结束地址范围内的内存区域禁用保护机制，允许正常访问
 *
 * @param addrStart  [IN] 内存保护区域的起始地址（必须是已启用保护的地址）
 * @param addrEnd    [IN] 内存保护区域的结束地址（必须是已启用保护的地址）
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_lms.h: 包含API声明的头文件</li></ul>
 */
VOID LOS_LmsAddrDisableProtect(UINTPTR addrStart, UINTPTR addrEnd);

#endif /* LOSCFG_KERNEL_LMS */  // 内存安全检查功能条件编译结束

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif  /* _LOS_LMS_H */