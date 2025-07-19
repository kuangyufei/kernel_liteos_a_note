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

#ifndef _LOS_USER_INIT_H
#define _LOS_USER_INIT_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* 用户模式代码段定义 - 用于分离用户空间与内核空间代码 */
#ifndef LITE_USER_SEC_TEXT                     // 防止宏重定义的条件编译检查
#define LITE_USER_SEC_TEXT   __attribute__((section(".user.text")))  // 将函数放入用户模式代码段，由MMU权限控制访问
#endif

/* 用户模式入口段定义 - 标记用户程序入口点 */
#ifndef LITE_USER_SEC_ENTRY                    // 防止宏重定义的条件编译检查
#define LITE_USER_SEC_ENTRY   __attribute__((section(".user.entry")))  // 用户程序入口函数专用段，内核据此定位用户程序起始地址
#endif

/* 用户模式数据段定义 - 存储已初始化全局变量 */
#ifndef LITE_USER_SEC_DATA                     // 防止宏重定义的条件编译检查
#define LITE_USER_SEC_DATA   __attribute__((section(".user.data")))  // 用户模式已初始化数据段，加载时从镜像读取初始值
#endif

/* 用户模式只读数据段定义 - 存储常量数据 */
#ifndef LITE_USER_SEC_RODATA                   // 防止宏重定义的条件编译检查
#define LITE_USER_SEC_RODATA   __attribute__((section(".user.rodata")))  // 用户模式只读数据段，内容不可修改（如字符串常量）
#endif

/* 用户模式BSS段定义 - 存储未初始化全局变量 */
#ifndef LITE_USER_SEC_BSS                      // 防止宏重定义的条件编译检查
#define LITE_USER_SEC_BSS   __attribute__((section(".user.bss")))  // 用户模式未初始化数据段，加载时自动清零
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
