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

#ifndef _LOS_LMS_PRI_H
#define _LOS_LMS_PRI_H

#include <pthread.h>
#include <malloc.h>
#include "los_lms.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define UNKNOWN_ERROR                   3
#define FREE_ERRORMODE                  2
#define STORE_ERRMODE                   1
#define LOAD_ERRMODE                    0

#define SANITIZER_INTERFACE_ATTRIBUTE
#define ATTRIBUTE_NO_SANITIZE_ADDRESS   __attribute__((no_sanitize_address))

#define LMS_SHADOW_ACCESSABLE           0x00
#define LMS_SHADOW_AFTERFREE            0x03
#define LMS_SHADOW_REDZONE              0x02
#define LMS_SHADOW_PAINT                0x01
#define LMS_SHADOW_MASK                 0x03

#define LMS_SHADOW_BITS_PER_CELL        2
#define LMS_MEM_BYTES_PER_SHADOW_CELL   4
#define LMS_SHADOW_U8_CELL_NUM          4
#define LMS_SHADOW_U8_REFER_BYTES       16

#define LMS_SHADOW_ACCESSABLE_U8        0x00
#define LMS_SHADOW_AFTERFREE_U8         0xFF
#define LMS_SHADOW_REDZONE_U8           0xAA
#define LMS_SHADOW_MASK_U8              0xFF
#define LMS_SHADOW_PAINT_U8             0x55

#define MEM_REGION_SIZE_1               1
#define MEM_REGION_SIZE_2               2
#define MEM_REGION_SIZE_4               4
#define MEM_REGION_SIZE_8               8
#define MEM_REGION_SIZE_16              16

#define LMS_RZ_SIZE                     4
#define LMS_OK                          0
#define LMS_NOK                         1

#define PAGE_ADDR_MASK                  0xFFFFE000
#define SHADOW_BASE                                                                                        \
    ((USPACE_MAP_BASE + (USPACE_MAP_SIZE / (LMS_SHADOW_U8_REFER_BYTES + 1)) * LMS_SHADOW_U8_REFER_BYTES) & \
        PAGE_ADDR_MASK)
#define OVERHEAD                        (2 * sizeof(size_t))

#define LMS_MEM_ALIGN_DOWN(value, align) (((uint32_t)(value)) & ~((uint32_t)((align) - 1)))
#define LMS_MEM_ALIGN_UP(value, align) (((uint32_t)(value) + ((align) - 1)) & ~((uint32_t)((align) - 1)))

typedef struct {
    uintptr_t memAddr;
    uintptr_t shadowAddr;
    uint32_t shadowOffset;
    uint32_t shadowValue;
} LmsAddrInfo;

extern pthread_mutex_t g_lmsMutex;

ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsLock(pthread_mutex_t *lock)
{
    (void)pthread_mutex_lock(lock);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS static inline int LmsTrylock(pthread_mutex_t *lock)
{
    return pthread_mutex_trylock(lock);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsUnlock(pthread_mutex_t *lock)
{
    (void)pthread_mutex_unlock(lock);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsCrash(void)
{
    *(volatile char *)(SHADOW_BASE - 1) = 0;
}

uint32_t LmsIsShadowAddrMapped(uintptr_t sdStartAddr, uintptr_t sdEndAddr);

void LmsSetShadowValue(uintptr_t startAddr, uintptr_t endAddr, char value);

void LmsGetShadowValue(uintptr_t addr, uint32_t *shadowValue);

void LmsReportError(uintptr_t p, size_t size, uint32_t errMod);

void LmsMem2Shadow(uintptr_t memAddr, uintptr_t *shadowAddr, uint32_t *shadowOffset);

void LmsCheckValid(const char *dest, const char *src);

void *__real_malloc(size_t);

void __real_free(void *);

void *__real_calloc(size_t, size_t);

void *__real_realloc(void *, size_t);

void *__real_valloc(size_t);

void *__real_aligned_alloc(size_t, size_t);

void *__real_memcpy(void *__restrict, const void *__restrict, size_t);

void *__real_memmove(void *, const void *, size_t);

char *__real_strcat(char *, const char *);

char *__real_strcpy(char *, const char *);

void *__real_memset(void *, int, size_t);

SANITIZER_INTERFACE_ATTRIBUTE void __asan_store1_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_store4_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_load4_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_load1_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_loadN_noabort(uintptr_t p, uint32_t size);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_storeN_noabort(uintptr_t p, uint32_t size);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_store2_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_load2_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_store8_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_load8_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_load16_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_store16_noabort(uintptr_t p);
SANITIZER_INTERFACE_ATTRIBUTE void __asan_handle_no_return(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_LMS_PRI_H */