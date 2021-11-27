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

#ifndef _LOS_INIT_H
#define _LOS_INIT_H

#include "los_init_info.h"

/**
 * Kernel Module Init Level | 内核模块初始化等级
 */
#define LOS_INIT_LEVEL_EARLIEST                     0	///< 最早期初始化, 说明：不依赖架构，单板以及后续模块会对其有依赖的纯软件模块初始化 例如：Trace模块
#define LOS_INIT_LEVEL_ARCH_EARLY                   1	///< 架构早期,说明：架构相关，后续模块会对其有依赖的模块初始化，如启动过程中非必需的功能，建议放到LOS_INIT_LEVEL_ARCH层
#define LOS_INIT_LEVEL_PLATFORM_EARLY               2	///< 平台早期,说明：单板平台、驱动相关，后续模块会对其有依赖的模块初始化，如启动过程中必需的功能，
															///< 建议放到LOS_INIT_LEVEL_PLATFORM层 例如：uart模块
#define LOS_INIT_LEVEL_KMOD_PREVM                   3	///< 内存初始化前的内核模块初始化 说明：在内存初始化之前需要使能的模块初始化
#define LOS_INIT_LEVEL_VM_COMPLETE                  4	///< 基础内存就绪后的初始化 说明：此时内存初始化完毕，需要进行使能且不依赖进程间通讯机制与系统进程的模块初始化
															///< 例如：共享内存功能
#define LOS_INIT_LEVEL_ARCH                         5	///< 架构后期初始化 说明：架构拓展功能相关，后续模块会对其有依赖的模块初始化
#define LOS_INIT_LEVEL_PLATFORM                     6	///< 平台后期初始化 说明：单板平台、驱动相关，后续模块会对其有依赖的模块初始化 例如：驱动内核抽象层初始化（mmc、mtd）
#define LOS_INIT_LEVEL_KMOD_BASIC                   7	///< 内核基础模块初始化 说明：内核可拆卸的基础模块初始化 例如：VFS初始化
#define LOS_INIT_LEVEL_KMOD_EXTENDED                8	///< 内核扩展模块初始化 ,说明：内核可拆卸的扩展模块初始化
															///< 例如：系统调用初始化、ProcFS初始化、Futex初始化、HiLog初始化、HiEvent初始化、LiteIPC初始化
#define LOS_INIT_LEVEL_KMOD_TASK                    9	///< 内核任务创建 说明：进行内核任务的创建（内核任务，软件定时器任务）
															///< 例如：资源回收系统常驻任务的创建、SystemInit任务创建、CPU占用率统计任务创建
#define LOS_INIT_LEVEL_FINISH                       10

/**
 * @ingroup  los_init
 * @brief Register a startup module to the startup process. | 将启动模块注册到启动过程,在内核启动过程中注册一个新模块作为内核功能组件的一部分。
 *
 * @par Description:
 * This API is used to register a startup module to the startup process.
 *
 * @attention
 * <ul>
 * <li>Register a new module in the boot process of the kernel as part of the kernel capability component.</li>
 * <li>In the startup framework, within the same _level, the startup sequence is sorted by
 * the registered function name </li>
 * <li>If the registration is not accompanied by the startup process after calling this interface,
 * try to add -u_hook to liteos_tables_ldflags.mk </li>
 * </ul>
 *
 * @param  _hook    [IN] Type  #UINT32 (*)(VOID)    Register function.
 * @param  _level   [IN] Type  #UINT32              Init level in the kernel.
 *
 * @retval None
 * @par Dependency:
 * <ul><li>los_init.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
#define LOS_MODULE_INIT(_hook, _level)                  OS_INIT_HOOK_REG(kernel, _hook, _level)

#endif /* _LOS_INIT_H */
