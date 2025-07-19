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

#ifndef _LOS_VDSO_PRI_H
#define _LOS_VDSO_PRI_H

#include "los_vdso.h"
#include "los_printf.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief VDSO数据页段定义宏
 * @details 将变量或数据放入名为".data.vdso.datapage"的特殊内存段，用于VDSO（虚拟动态共享对象）机制
 * @note 该段通常映射到用户空间，供用户进程直接访问内核提供的共享数据
 */
#define LITE_VDSO_DATAPAGE __attribute__((section(".data.vdso.datapage")))  // VDSO数据页段属性定义

/**
 * @brief 获取VDSO时间数据
 * @details 从VDSO数据页中读取时间相关信息，提供用户空间快速访问系统时间的能力
 * @param[in,out] VdsoDataPage* 指向VDSO数据页结构体的指针，用于存储获取的时间数据
 * @retval 无
 * @note 此函数通常由内核实现，通过VDSO机制暴露给用户空间
 */
extern VOID OsVdsoTimeGet(VdsoDataPage *);  // 获取VDSO时间数据

/**
 * @brief VDSO数据段起始地址
 * @details 标记VDSO数据段在内存中的起始位置，用于VDSO区域的内存管理
 */
extern CHAR __vdso_data_start;  // VDSO数据段起始地址

/**
 * @brief VDSO文本段起始地址
 * @details 标记VDSO代码段在内存中的起始位置，用于动态加载和地址计算
 */
extern CHAR __vdso_text_start;  // VDSO文本段起始地址

/**
 * @brief VDSO文本段结束地址
 * @details 标记VDSO代码段在内存中的结束位置，与__vdso_text_start配合计算段大小
 */
extern CHAR __vdso_text_end;    // VDSO文本段结束地址

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* _LOS_VDSO_PRI_H */