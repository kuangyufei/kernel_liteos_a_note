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

/*
 * If LMS_CRASH_MODE == 0
 * the program would NOT be crashed once the sanitizer detected an error.
 * If LMS_CRASM_MODE > 0
 * the program would be crashed once the sanitizer detected an error.
 */
#define LMS_CRASH_MODE 0

/*
 * USPACE_MAP_BASE
 * is the start address of user space.
 */
#define USPACE_MAP_BASE 0x00000000

/*
 * USPACE_MAP_SIZE
 * is the address size of the user space, and [USPACE_MAP_BASE, USPACE_MAP_BASE + USPACE_MAP_SIZE]
 * must cover the HEAP section.
 */
#define USPACE_MAP_SIZE 0x3ef00000

/*
 * LMS_OUTPUT_ERROR can be redefined to redirect output logs.
 */
#ifndef LMS_OUTPUT_ERROR
#define LMS_OUTPUT_ERROR(fmt, ...)                                             \
    do {                                                                       \
        (printf("\033[31;1m"), printf(fmt, ##__VA_ARGS__), printf("\033[0m")); \
    } while (0)
#endif

/*
 * LMS_OUTPUT_INFO can be redefined to redirect output logs.
 */
#ifndef LMS_OUTPUT_INFO
#define LMS_OUTPUT_INFO(fmt, ...)                                              \
    do {                                                                       \
        (printf("\033[33;1m"), printf(fmt, ##__VA_ARGS__), printf("\033[0m")); \
    } while (0)
#endif


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_LMS_H */