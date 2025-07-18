/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief LMS崩溃模式控制宏
 * @details 控制当sanitizer检测到错误时程序是否崩溃
 *          - 0: 检测到错误时不崩溃
 *          - >0: 检测到错误时崩溃
 */
#define LMS_CRASH_MODE 0  // LMS崩溃模式开关，0表示不崩溃，非0表示崩溃

/**
 * @brief 用户空间起始地址宏
 * @details 定义用户空间的起始地址
 */
#define USPACE_MAP_BASE 0x00000000  // 用户空间起始地址

/**
 * @brief 用户空间大小宏
 * @details 定义用户空间的地址大小，[USPACE_MAP_BASE, USPACE_MAP_BASE + USPACE_MAP_SIZE]必须覆盖堆区域
 */
#define USPACE_MAP_SIZE 0x3ef00000  // 用户空间大小

/**
 * @brief 错误日志输出宏
 * @details 可重定义该宏以重定向错误日志输出，默认输出红色文本
 */
#ifndef LMS_OUTPUT_ERROR
#define LMS_OUTPUT_ERROR(fmt, ...)                                             \
    do {                                                                       \
        (printf("\033[31;1m"), printf(fmt, ##__VA_ARGS__), printf("\033[0m")); \ // 设置红色文本并输出错误信息，最后重置文本颜色
    } while (0)
#endif

/**
 * @brief 信息日志输出宏
 * @details 可重定义该宏以重定向信息日志输出，默认输出黄色文本
 */
#ifndef LMS_OUTPUT_INFO
#define LMS_OUTPUT_INFO(fmt, ...)                                              \
    do {                                                                       \
        (printf("\033[33;1m"), printf(fmt, ##__VA_ARGS__), printf("\033[0m")); \ // 设置黄色文本并输出信息，最后重置文本颜色
    } while (0)
#endif



#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_LMS_H */