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

#include "los_lms_pri.h"

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsFree(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    size_t allocSize = malloc_usable_size(ptr);
    uintptr_t shadowAddr, offset;
    LmsMem2Shadow((uintptr_t)ptr, &shadowAddr, &offset);

    LmsLock(&g_lmsMutex);
    if (LmsIsShadowAddrMapped(shadowAddr, shadowAddr) == LMS_OK) {
        uint32_t acShadowValue;
        LmsGetShadowValue((uintptr_t)ptr, &acShadowValue);
        if (acShadowValue != LMS_SHADOW_ACCESSABLE) {
            char erroMode = (acShadowValue == LMS_SHADOW_AFTERFREE ? FREE_ERRORMODE : UNKNOWN_ERROR);
            LmsReportError((uintptr_t)ptr, MEM_REGION_SIZE_1, erroMode);
            goto UNLOCK_OUT;
        }
    } else {
        LMS_OUTPUT_ERROR("Error! Free an unallocated memory:%p!\n", ptr);
        goto UNLOCK_OUT;
    }

    __real_free(ptr);
    LmsSetShadowValue((uintptr_t)ptr, (uintptr_t)ptr + allocSize, LMS_SHADOW_AFTERFREE_U8);

UNLOCK_OUT:
    LmsUnlock(&g_lmsMutex);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMalloc(size_t size)
{
    void *ptr = __real_malloc(size);
    if (ptr == NULL) {
        return ptr;
    }
    return LmsTagMem(ptr, size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsRealloc(void *ptr, size_t size)
{
    void *p = __real_realloc(ptr, size);
    if (p == NULL) {
        return p;
    }
    return LmsTagMem(p, size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsCalloc(size_t m, size_t n)
{
    void *p = __real_calloc(m, n);
    if (p == NULL) {
        return p;
    }
    return LmsTagMem(p, n * m);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsValloc(size_t size)
{
    void *p = __real_valloc(size);
    if (p == NULL) {
        return p;
    }
    return LmsTagMem(p, size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsAlignedAlloc(size_t align, size_t len)
{
    void *p = __real_aligned_alloc(align, len);
    if (p == NULL) {
        return p;
    }
    return LmsTagMem(p, len);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_aligned_alloc(size_t align, size_t len)
{
    return LmsAlignedAlloc(align, len);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_valloc(size_t size)
{
    return LmsValloc(size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_calloc(size_t m, size_t n)
{
    return LmsCalloc(m, n);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_realloc(void *p, size_t n)
{
    return LmsRealloc(p, n);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_malloc(size_t size)
{
    return LmsMalloc(size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void __wrap_free(void *ptr)
{
    return LmsFree(ptr);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMemset(void *p, int n, size_t size)
{
    __asan_storeN_noabort((uintptr_t)p, size);

    return __real_memset(p, n, size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_memset(void *p, int n, size_t size)
{
    return LmsMemset(p, n, size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMemcpy(void *__restrict dest, const void *__restrict src, size_t size)
{
    __asan_loadN_noabort((uintptr_t)src, size);
    __asan_storeN_noabort((uintptr_t)dest, size);

    return __real_memcpy(dest, src, size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_memcpy(void *__restrict dest, const void *__restrict src, size_t size)
{
    return LmsMemcpy(dest, src, size);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMemmove(void *dest, const void *src, size_t len)
{
    __asan_loadN_noabort((uintptr_t)src, len);
    __asan_storeN_noabort((uintptr_t)dest, len);

    return __real_memmove(dest, src, len);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_memmove(void *dest, const void *src, size_t len)
{
    return LmsMemmove(dest, src, len);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS char *LmsStrcat(char *s, const char *append)
{
    if ((s == NULL) || (append == NULL)) {
        return NULL;
    }

    char *save = s;
    for (; *s != '\0'; ++s) {
    }
    LmsCheckValid(s, append);

    return __real_strcat(save, append);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS char *__wrap_strcat(char *s, const char *append)
{
    return LmsStrcat(s, append);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS char *LmsStrcpy(char *dest, const char *src)
{
    if ((dest == NULL) || (src == NULL)) {
        return NULL;
    }

    LmsCheckValid(dest, src);
    return __real_strcpy(dest, src);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS char *__wrap_strcpy(char *dest, const char *src)
{
    return LmsStrcpy(dest, src);
}