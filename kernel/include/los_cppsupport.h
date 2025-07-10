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
 * @defgroup los_cppsupport c++
 * @ingroup kernel
 */

#ifndef _LOS_CPPSUPPORT_H
#define _LOS_CPPSUPPORT_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_cppsupport
 * @brief 禁用分散加载标志
 * @note 当禁用分散加载功能时，调用LOS_CppSystemInit()函数时应将此标志作为第三个参数传入
 * @value 2 (十进制)
 */
#define NO_SCATTER 2

/**
 * @ingroup los_cppsupport
 * @brief C++运行时支持系统初始化
 *
 * @par 描述
 * 该API用于初始化C++运行时环境，主要负责调用全局对象构造函数和初始化相关数据段
 * @attention
 * <ul>
 * <li>initArrayStart是.init_array节区的起始地址，initArrayEnd是.init_array节区的结束地址</li>
 * <li>initArrayStart必须小于initArrayEnd，且两者地址需满足平台对齐要求：32位平台4字节对齐，64位平台8字节对齐</li>
 * <li>flag参数必须是以下预定义值之一：BEFORE_SCATTER(分散加载前)、AFTER_SCATTER(分散加载后)或NO_SCATTER(禁用分散加载)</li>
 * </ul>
 *
 * @param[in] initArrayStart  .init_array节区的起始地址，存储C++全局构造函数指针数组
 * @param[in] initArrayEnd    .init_array节区的结束地址，用于计算构造函数数量
 * @param[in] flag            初始化时机标志，指定在分散加载过程的哪个阶段执行初始化
 *
 * @retval 0 始终返回0，表示初始化成功
 * @par 依赖
 * <ul><li>los_cppsupport.h: 包含该API声明的头文件</li></ul>
 * @see None
 */
extern INT32 LOS_CppSystemInit(UINTPTR initArrayStart, UINTPTR initArrayEnd, INT32 flag);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_CPPSUPPORT_H */
