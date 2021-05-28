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
 * Kernel Module Init Level
 */
#define LOS_INIT_LEVEL_EARLIEST                     0
#define LOS_INIT_LEVEL_ARCH_EARLY                   1
#define LOS_INIT_LEVEL_PLATFORM_EARLY               2
#define LOS_INIT_LEVEL_KMOD_PREVM                   3
#define LOS_INIT_LEVEL_VM_COMPLETE                  4
#define LOS_INIT_LEVEL_ARCH                         5
#define LOS_INIT_LEVEL_PLATFORM                     6
#define LOS_INIT_LEVEL_KMOD_BASIC                   7
#define LOS_INIT_LEVEL_KMOD_EXTENDED                8
#define LOS_INIT_LEVEL_KMOD_TASK                    9
#define LOS_INIT_LEVEL_FINISH                       10

/**
 * @ingroup  los_init
 * @brief Register a startup module to the startup process.
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
