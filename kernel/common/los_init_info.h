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

#ifndef _LOS_INIT_INFO_H
#define _LOS_INIT_INFO_H

#include "stdalign.h"
#include "los_toolchain.h"
#include "los_typedef.h"

/**
 * @ingroup los_init_info
 * Macro LOS_INIT_DEBUG needs to be defined here, used to debug the init framework.
 * #define LOS_INIT_DEBUG
 */

/**
 * @ingroup los_init_info
 * Macro LOS_INIT_STATISTICS needs to be defined here, used to count the kernel startup time.
 * @attention
 * <ul>
 * <li> if uses, the macro LOS_INIT_DEBUG must be undefined. </li>
 * </ul>
 * #define LOS_INIT_STATISTICS
 */

#if defined(LOS_INIT_STATISTICS) && defined(LOS_INIT_DEBUG)
#error "LOS_INIT_STATISTICS needs LOS_INIT_DEBUG to be undefined"
#endif

#define INIT_SECTION(_type, _level, _hook)          __attribute__((section(".rodata.init."#_type"."#_level"."#_hook)))
#define INIT_ALIGN                                  __attribute__((aligned(alignof(struct ModuleInitInfo))))

typedef UINT32 (*OsInitHook)(VOID);

struct ModuleInitInfo {
    OsInitHook hook;
#ifdef LOS_INIT_DEBUG
    const CHAR *name;
#endif
};

#ifdef LOS_INIT_DEBUG
#define SET_MODULE_NAME(_hook)                      .name = #_hook,
#else
#define SET_MODULE_NAME(_hook)
#endif

/**
* @ingroup  los_init_info
* @brief Add a registration module to the specified level in a startup framework.
*
* @par Description:
* This API is used to add a registration module to the specified level in a startup framework.
* @attention
 * <ul>
 * <li>It is not recommended to call directly, it is recommended that each startup framework
 * encapsulate a layer of interface in los_init.h.</li>
 * </ul>
*
* @param  _type     [IN] Type name of startup framework.
* @param  _hook     [IN] Register function.
* @param  _level    [IN] At which _level do you want to register.
*
* @retval None
* @par Dependency:
* <ul><li>los_task_info.h: the header file that contains the API declaration.</li></ul>
* @see
*/
#define OS_INIT_HOOK_REG(_type, _hook, _level)                              \
    STATIC const struct ModuleInitInfo ModuleInitInfo_##_hook               \
    USED INIT_SECTION(_type, _level, _hook) INIT_ALIGN = {                  \
        .hook = (UINT32 (*)(VOID))&_hook,                                   \
        SET_MODULE_NAME(_hook)                                              \
    };

#define EXTERN_LABEL(_type, _level)                 extern struct ModuleInitInfo __##_type##_init_level_##_level;
#define GET_LABEL(_type, _level)                    &__##_type##_init_level_##_level,

#define INIT_LABEL_REG_0(_op, _type)                \
    _op(_type, 0)
#define INIT_LABEL_REG_1(_op, _type)                \
    INIT_LABEL_REG_0(_op, _type)                    \
    _op(_type, 1)
#define INIT_LABEL_REG_2(_op, _type)                \
    INIT_LABEL_REG_1(_op, _type)                    \
    _op(_type, 2)
#define INIT_LABEL_REG_3(_op, _type)                \
    INIT_LABEL_REG_2(_op, _type)                    \
    _op(_type, 3)
#define INIT_LABEL_REG_4(_op, _type)                \
    INIT_LABEL_REG_3(_op, _type)                    \
    _op(_type, 4)
#define INIT_LABEL_REG_5(_op, _type)                \
    INIT_LABEL_REG_4(_op, _type)                    \
    _op(_type, 5)
#define INIT_LABEL_REG_6(_op, _type)                \
    INIT_LABEL_REG_5(_op, _type)                    \
    _op(_type, 6)
#define INIT_LABEL_REG_7(_op, _type)                \
    INIT_LABEL_REG_6(_op, _type)                    \
    _op(_type, 7)
#define INIT_LABEL_REG_8(_op, _type)                \
    INIT_LABEL_REG_7(_op, _type)                    \
    _op(_type, 8)
#define INIT_LABEL_REG_9(_op, _type)                \
    INIT_LABEL_REG_8(_op, _type)                    \
    _op(_type, 9)
#define INIT_LABEL_REG_10(_op, _type)               \
    INIT_LABEL_REG_9(_op, _type)                    \
    _op(_type, 10)

/**
* @ingroup  los_init_info
* @brief Define a set of levels and initialize the labels of each level.
*
* @par Description:
* This API is used to define a set of levels and initialize the labels of each level.
* @attention
 * <ul>
 * <li>This interface is used to add a new startup framework.</li>
 * <li>To use this interface, you need to add a corresponding section description in
 * the liteos.ld and liteos_llvm.ld files to match </li>
 * </ul>
*
* @param  _type [IN] Type name of startup framework.
* @param  _num  [IN] The maximum effective level of the startup framework, the level range is [0, _num].
* @param  _list [IN] Static global array, used to manage labels at all levels.
*
* @retval None
* @par Dependency:
* <ul><li>los_task_info.h: the header file that contains the API declaration.</li></ul>
* @see
*/
#define OS_INIT_LEVEL_REG(_type, _num, _list)       \
    INIT_LABEL_REG_##_num(EXTERN_LABEL, _type)      \
    STATIC struct ModuleInitInfo* _list [] = {      \
        INIT_LABEL_REG_##_num(GET_LABEL, _type)     \
    }

#endif /* _LOS_INIT_INFO_H */
