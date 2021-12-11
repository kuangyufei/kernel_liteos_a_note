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

#include "string.h"
#include "los_lms_pri.h"

#undef memset
void *memset(void *addr, int c, size_t len)
{
    __asan_storeN_noabort((UINTPTR)addr, len);
    return __memset(addr, c, len);
}

#undef memmove
void *memmove(void *dest, const void *src, size_t len)
{
    __asan_loadN_noabort((UINTPTR)src, len);
    __asan_storeN_noabort((UINTPTR)dest, len);
    return __memmove(dest, src, len);
}

#undef memcpy
void *memcpy(void *dest, const void *src, size_t len)
{
    __asan_loadN_noabort((UINTPTR)src, len);
    __asan_storeN_noabort((UINTPTR)dest, len);
    return __memcpy(dest, src, len);
}

#undef strcat
char *strcat (char *s, const char *append)
{
    if ((s == NULL) || (append == NULL)) {
        return NULL;
    }

    CHAR *end = s;
    size_t len = strlen(append);
    for (; *end != '\0'; ++end) {
    }

    __asan_storeN_noabort((UINTPTR)end, len + 1);
    __asan_loadN_noabort((UINTPTR)append, len + 1);

    return __strcat(s, append);
}

#undef strcpy
char *strcpy(char *dest, const char *src)
{
    if ((dest == NULL) || (src == NULL)) {
        return NULL;
    }

    size_t len = strlen(src);
    __asan_storeN_noabort((UINTPTR)dest, len + 1);
    __asan_loadN_noabort((UINTPTR)src, len + 1);

    return __strcpy(dest, src);
}

#undef strncat
char *strncat(char *dest, const char *src, size_t n)
{
    if ((dest == NULL) || (src == NULL)) {
        return NULL;
    }

    CHAR *end = dest;
    size_t len = strlen(src);
    size_t size = len > n ? n : len;
    for (; *end != '\0'; ++end) {
    }

    __asan_storeN_noabort((UINTPTR)end, size + 1);
    __asan_loadN_noabort((UINTPTR)src, size + 1);

    return __strncat(dest, src, n);
}

#undef strncpy
char *strncpy(char *dest, const char *src, size_t n)
{
    if ((dest == NULL) || (src == NULL)) {
        return NULL;
    }

    size_t len = strlen(src);
    size_t size = len > n ? n : len;

    __asan_storeN_noabort((UINTPTR)dest, n);
    __asan_loadN_noabort((UINTPTR)src, size + 1);
    return __strncpy(dest, src, n);
}