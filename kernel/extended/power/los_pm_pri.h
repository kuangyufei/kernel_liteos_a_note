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

#ifndef _LOS_PM_PRI_H
#define _LOS_PM_PRI_H

#include "los_config.h"
#include "los_typedef.h"

/* 条件编译：当使能内核电源管理功能时(LOSCFG_KERNEL_PM为真)，声明电源模式检查函数 */
#ifdef  LOSCFG_KERNEL_PM

/**
 * @brief   检查当前系统是否处于电源管理模式
 * @return  BOOL类型：TRUE表示处于电源管理模式，FALSE表示未处于
 * @note    该函数在电源管理模块初始化后有效
 */
BOOL OsIsPmMode(VOID);

#else

/**
 * @brief   电源管理功能未使能时的默认内联实现
 * @return  固定返回FALSE，表示未处于电源管理模式
 * @note    当LOSCFG_KERNEL_PM配置关闭时使用此轻量级实现
 */
STATIC INLINE BOOL OsIsPmMode(VOID)
{
    return FALSE;  // 电源管理未使能，始终返回非电源管理模式
}
#endif

#endif
#endif
