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
/**
 * @brief 带地址 sanitize 检查的内存设置函数
 * @param[in] addr   内存起始地址
 * @param[in] c      要设置的值
 * @param[in] len    要设置的字节数
 * @return 成功返回内存起始地址，失败返回NULL
 * @note 该函数会先进行地址有效性检查，再调用底层__memset实现
 */
void *memset(void *addr, int c, size_t len)
{
    __asan_storeN_noabort((UINTPTR)addr, len);  // 对目标内存区域进行存储操作前的地址有效性检查
    return __memset(addr, c, len);              // 调用底层内存设置函数实现
}

#undef memmove
/**
 * @brief 带地址 sanitize 检查的内存移动函数
 * @param[in] dest   目标内存地址
 * @param[in] src    源内存地址
 * @param[in] len    要移动的字节数
 * @return 成功返回目标内存起始地址，失败返回NULL
 * @note 该函数会先对源和目标内存区域进行地址有效性检查，再调用底层__memmove实现
 */
void *memmove(void *dest, const void *src, size_t len)
{
    __asan_loadN_noabort((UINTPTR)src, len);   // 对源内存区域进行加载操作前的地址有效性检查
    __asan_storeN_noabort((UINTPTR)dest, len); // 对目标内存区域进行存储操作前的地址有效性检查
    return __memmove(dest, src, len);          // 调用底层内存移动函数实现
}

#undef memcpy
/**
 * @brief 带地址 sanitize 检查的内存拷贝函数
 * @param[in] dest   目标内存地址
 * @param[in] src    源内存地址
 * @param[in] len    要拷贝的字节数
 * @return 成功返回目标内存起始地址，失败返回NULL
 * @note 该函数会先对源和目标内存区域进行地址有效性检查，再调用底层__memcpy实现
 */
void *memcpy(void *dest, const void *src, size_t len)
{
    __asan_loadN_noabort((UINTPTR)src, len);   // 对源内存区域进行加载操作前的地址有效性检查
    __asan_storeN_noabort((UINTPTR)dest, len); // 对目标内存区域进行存储操作前的地址有效性检查
    return __memcpy(dest, src, len);           // 调用底层内存拷贝函数实现
}

#undef strcat
/**
 * @brief 带地址 sanitize 检查的字符串拼接函数
 * @param[in] s      目标字符串
 * @param[in] append 要拼接的字符串
 * @return 成功返回拼接后的目标字符串地址，失败返回NULL
 * @note 该函数会先检查指针有效性并计算字符串长度，进行地址有效性检查后调用底层__strcat实现
 */
char *strcat(char *s, const char *append)
{
    if ((s == NULL) || (append == NULL)) {     // 检查输入字符串指针是否为空
        return NULL;                           // 指针为空则返回NULL
    }

    CHAR *end = s;                             // 定义指针指向目标字符串末尾
    size_t len = strlen(append);               // 计算要拼接的字符串长度
    for (; *end != '\0'; ++end) {              // 循环找到目标字符串的结尾
    }

    __asan_storeN_noabort((UINTPTR)end, len + 1);  // 对目标拼接区域进行存储操作前的地址有效性检查(包含终止符)
    __asan_loadN_noabort((UINTPTR)append, len + 1); // 对源字符串进行加载操作前的地址有效性检查(包含终止符)

    return __strcat(s, append);                // 调用底层字符串拼接函数实现
}

#undef strcpy
/**
 * @brief 带地址 sanitize 检查的字符串拷贝函数
 * @param[in] dest   目标字符串地址
 * @param[in] src    源字符串地址
 * @return 成功返回目标字符串地址，失败返回NULL
 * @note 该函数会先检查指针有效性并计算字符串长度，进行地址有效性检查后调用底层__strcpy实现
 */
char *strcpy(char *dest, const char *src)
{
    if ((dest == NULL) || (src == NULL)) {     // 检查输入字符串指针是否为空
        return NULL;                           // 指针为空则返回NULL
    }

    size_t len = strlen(src);                  // 计算源字符串长度
    __asan_storeN_noabort((UINTPTR)dest, len + 1);  // 对目标字符串区域进行存储操作前的地址有效性检查(包含终止符)
    __asan_loadN_noabort((UINTPTR)src, len + 1);    // 对源字符串进行加载操作前的地址有效性检查(包含终止符)

    return __strcpy(dest, src);                // 调用底层字符串拷贝函数实现
}

#undef strncat
/**
 * @brief 带地址 sanitize 检查的指定长度字符串拼接函数
 * @param[in] dest   目标字符串
 * @param[in] src    要拼接的字符串
 * @param[in] n      最大拼接长度
 * @return 成功返回拼接后的目标字符串地址，失败返回NULL
 * @note 该函数会先检查指针有效性并计算安全拼接长度，进行地址有效性检查后调用底层__strncat实现
 */
char *strncat(char *dest, const char *src, size_t n)
{
    if ((dest == NULL) || (src == NULL)) {     // 检查输入字符串指针是否为空
        return NULL;                           // 指针为空则返回NULL
    }

    CHAR *end = dest;                          // 定义指针指向目标字符串末尾
    size_t len = strlen(src);                  // 计算源字符串长度
    size_t size = len > n ? n : len;           // 确定实际要拼接的长度(不超过n)
    for (; *end != '\0'; ++end) {              // 循环找到目标字符串的结尾
    }

    __asan_storeN_noabort((UINTPTR)end, size + 1);  // 对目标拼接区域进行存储操作前的地址有效性检查(包含终止符)
    __asan_loadN_noabort((UINTPTR)src, size + 1);   // 对源字符串进行加载操作前的地址有效性检查(包含终止符)

    return __strncat(dest, src, n);            // 调用底层指定长度字符串拼接函数实现
}

#undef strncpy
/**
 * @brief 带地址 sanitize 检查的指定长度字符串拷贝函数
 * @param[in] dest   目标字符串地址
 * @param[in] src    源字符串地址
 * @param[in] n      最大拷贝长度
 * @return 成功返回目标字符串地址，失败返回NULL
 * @note 该函数会先检查指针有效性并计算安全拷贝长度，进行地址有效性检查后调用底层__strncpy实现
 */
char *strncpy(char *dest, const char *src, size_t n)
{
    if ((dest == NULL) || (src == NULL)) {     // 检查输入字符串指针是否为空
        return NULL;                           // 指针为空则返回NULL
    }

    size_t len = strlen(src);                  // 计算源字符串长度
    size_t size = len > n ? n : len;           // 确定实际要拷贝的长度(不超过n)

    __asan_storeN_noabort((UINTPTR)dest, n);   // 对目标字符串区域进行存储操作前的地址有效性检查(长度为n)
    __asan_loadN_noabort((UINTPTR)src, size + 1);   // 对源字符串进行加载操作前的地址有效性检查(包含终止符)
    return __strncpy(dest, src, n);            // 调用底层指定长度字符串拷贝函数实现
}
