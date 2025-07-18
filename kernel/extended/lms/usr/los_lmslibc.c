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

/**
 * @brief 带内存检查的释放函数
 * @param[in] ptr 待释放内存指针
 * @note 释放前验证内存合法性，释放后标记影子内存为已释放状态
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsFree(void *ptr)
{
    if (ptr == NULL) {  // 空指针检查，直接返回
        return;
    }

    size_t allocSize = malloc_usable_size(ptr);  // 获取实际分配的内存大小
    uintptr_t shadowAddr, offset;  // 影子内存地址和偏移量变量
    LmsMem2Shadow((uintptr_t)ptr, &shadowAddr, &offset);  // 计算内存对应的影子地址

    LmsLock(&g_lmsMutex);  // 获取互斥锁，保护临界区
    if (LmsIsShadowAddrMapped(shadowAddr, shadowAddr) == LMS_OK) {  // 检查影子地址是否已映射
        uint32_t acShadowValue;  // 影子内存值变量
        LmsGetShadowValue((uintptr_t)ptr, &acShadowValue);  // 获取当前影子内存值
        if (acShadowValue != LMS_SHADOW_ACCESSIBLE) {  // 判断内存是否处于可访问状态
            char erroMode = (acShadowValue == LMS_SHADOW_AFTERFREE ? FREE_ERRORMODE : UNKNOWN_ERROR);  // 确定错误模式
            LmsReportError((uintptr_t)ptr, MEM_REGION_SIZE_1, erroMode);  // 报告内存错误
            goto UNLOCK_OUT;  // 跳转到解锁处
        }
    } else {  // 影子地址未映射，释放了未分配的内存
        LMS_OUTPUT_ERROR("Error! Free an unallocated memory:%p!\n", ptr);  // 输出错误信息
        goto UNLOCK_OUT;  // 跳转到解锁处
    }

    __real_free(ptr);  // 调用实际的free函数释放内存
    LmsSetShadowValue((uintptr_t)ptr, (uintptr_t)ptr + allocSize, LMS_SHADOW_AFTERFREE_U8);  // 将影子内存标记为已释放

UNLOCK_OUT:
    LmsUnlock(&g_lmsMutex);  // 释放互斥锁
}

/**
 * @brief 带内存标记的分配函数
 * @param[in] size 分配内存大小
 * @return 成功返回内存指针，失败返回NULL
 * @note 分配后对内存进行标记处理
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMalloc(size_t size)
{
    void *ptr = __real_malloc(size);  // 调用实际的malloc函数分配内存
    if (ptr == NULL) {  // 分配失败检查
        return ptr;
    }
    return LmsTagMem(ptr, size);  // 对分配的内存进行标记
}

/**
 * @brief 带内存标记的重分配函数
 * @param[in] ptr 原内存指针
 * @param[in] size 新内存大小
 * @return 成功返回新内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsRealloc(void *ptr, size_t size)
{
    void *p = __real_realloc(ptr, size);  // 调用实际的realloc函数重分配内存
    if (p == NULL) {  // 重分配失败检查
        return p;
    }
    return LmsTagMem(p, size);  // 对新分配的内存进行标记
}

/**
 * @brief 带内存标记的计数分配函数
 * @param[in] m 元素数量
 * @param[in] n 每个元素大小
 * @return 成功返回内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsCalloc(size_t m, size_t n)
{
    void *p = __real_calloc(m, n);  // 调用实际的calloc函数分配内存
    if (p == NULL) {  // 分配失败检查
        return p;
    }
    return LmsTagMem(p, n * m);  // 对分配的内存进行标记
}

/**
 * @brief 带内存标记的页面对齐分配函数
 * @param[in] size 分配内存大小
 * @return 成功返回内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsValloc(size_t size)
{
    void *p = __real_valloc(size);  // 调用实际的valloc函数分配内存
    if (p == NULL) {  // 分配失败检查
        return p;
    }
    return LmsTagMem(p, size);  // 对分配的内存进行标记
}

/**
 * @brief 带内存标记的对齐分配函数
 * @param[in] align 对齐大小
 * @param[in] len 分配内存长度
 * @return 成功返回内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsAlignedAlloc(size_t align, size_t len)
{
    void *p = __real_aligned_alloc(align, len);  // 调用实际的aligned_alloc函数分配内存
    if (p == NULL) {  // 分配失败检查
        return p;
    }
    return LmsTagMem(p, len);  // 对分配的内存进行标记
}

/**
 * @brief aligned_alloc函数包装器
 * @param[in] align 对齐大小
 * @param[in] len 分配内存长度
 * @return 成功返回内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_aligned_alloc(size_t align, size_t len)
{
    return LmsAlignedAlloc(align, len);  // 调用带标记的对齐分配函数
}

/**
 * @brief valloc函数包装器
 * @param[in] size 分配内存大小
 * @return 成功返回内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_valloc(size_t size)
{
    return LmsValloc(size);  // 调用带标记的页面对齐分配函数
}

/**
 * @brief calloc函数包装器
 * @param[in] m 元素数量
 * @param[in] n 每个元素大小
 * @return 成功返回内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_calloc(size_t m, size_t n)
{
    return LmsCalloc(m, n);  // 调用带标记的计数分配函数
}

/**
 * @brief realloc函数包装器
 * @param[in] p 原内存指针
 * @param[in] n 新内存大小
 * @return 成功返回新内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_realloc(void *p, size_t n)
{
    return LmsRealloc(p, n);  // 调用带标记的重分配函数
}

/**
 * @brief malloc函数包装器
 * @param[in] size 分配内存大小
 * @return 成功返回内存指针，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_malloc(size_t size)
{
    return LmsMalloc(size);  // 调用带标记的分配函数
}

/**
 * @brief free函数包装器
 * @param[in] ptr 待释放内存指针
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void __wrap_free(void *ptr)
{
    return LmsFree(ptr);  // 调用带检查的释放函数
}

/**
 * @brief 带地址检查的内存设置函数
 * @param[in] p 内存起始地址
 * @param[in] n 设置值
 * @param[in] size 设置字节数
 * @return 返回内存起始地址
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMemset(void *p, int n, size_t size)
{
    __asan_storeN_noabort((uintptr_t)p, size);  // 对目标内存进行存储前地址检查

    return __real_memset(p, n, size);  // 调用实际的memset函数
}

/**
 * @brief memset函数包装器
 * @param[in] p 内存起始地址
 * @param[in] n 设置值
 * @param[in] size 设置字节数
 * @return 返回内存起始地址
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_memset(void *p, int n, size_t size)
{
    return LmsMemset(p, n, size);  // 调用带地址检查的内存设置函数
}

/**
 * @brief 带地址检查的内存拷贝函数
 * @param[in] dest 目标地址
 * @param[in] src 源地址
 * @param[in] size 拷贝字节数
 * @return 返回目标地址
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMemcpy(void *__restrict dest, const void *__restrict src, size_t size)
{
    __asan_loadN_noabort((uintptr_t)src, size);  // 对源内存进行加载前地址检查
    __asan_storeN_noabort((uintptr_t)dest, size);  // 对目标内存进行存储前地址检查

    return __real_memcpy(dest, src, size);  // 调用实际的memcpy函数
}

/**
 * @brief memcpy函数包装器
 * @param[in] dest 目标地址
 * @param[in] src 源地址
 * @param[in] size 拷贝字节数
 * @return 返回目标地址
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_memcpy(void *__restrict dest, const void *__restrict src, size_t size)
{
    return LmsMemcpy(dest, src, size);  // 调用带地址检查的内存拷贝函数
}

/**
 * @brief 带地址检查的内存移动函数
 * @param[in] dest 目标地址
 * @param[in] src 源地址
 * @param[in] len 移动字节数
 * @return 返回目标地址
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsMemmove(void *dest, const void *src, size_t len)
{
    __asan_loadN_noabort((uintptr_t)src, len);  // 对源内存进行加载前地址检查
    __asan_storeN_noabort((uintptr_t)dest, len);  // 对目标内存进行存储前地址检查

    return __real_memmove(dest, src, len);  // 调用实际的memmove函数
}

/**
 * @brief memmove函数包装器
 * @param[in] dest 目标地址
 * @param[in] src 源地址
 * @param[in] len 移动字节数
 * @return 返回目标地址
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *__wrap_memmove(void *dest, const void *src, size_t len)
{
    return LmsMemmove(dest, src, len);  // 调用带地址检查的内存移动函数
}

/**
 * @brief 带有效性检查的字符串拼接函数
 * @param[in] s 目标字符串
 * @param[in] append 待拼接字符串
 * @return 成功返回拼接后字符串地址，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS char *LmsStrcat(char *s, const char *append)
{
    if ((s == NULL) || (append == NULL)) {  // 空指针检查
        return NULL;
    }

    char *save = s;  // 保存目标字符串起始地址
    for (; *s != '\0'; ++s) {  // 移动到目标字符串末尾
    }
    LmsCheckValid(s, append);  // 检查拼接区域和源字符串有效性

    return __real_strcat(save, append);  // 调用实际的strcat函数
}

/**
 * @brief strcat函数包装器
 * @param[in] s 目标字符串
 * @param[in] append 待拼接字符串
 * @return 成功返回拼接后字符串地址，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS char *__wrap_strcat(char *s, const char *append)
{
    return LmsStrcat(s, append);  // 调用带有效性检查的字符串拼接函数
}

/**
 * @brief 带有效性检查的字符串拷贝函数
 * @param[in] dest 目标字符串
 * @param[in] src 源字符串
 * @return 成功返回目标字符串地址，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS char *LmsStrcpy(char *dest, const char *src)
{
    if ((dest == NULL) || (src == NULL)) {  // 空指针检查
        return NULL;
    }

    LmsCheckValid(dest, src);  // 检查目标和源字符串有效性
    return __real_strcpy(dest, src);  // 调用实际的strcpy函数
}

/**
 * @brief strcpy函数包装器
 * @param[in] dest 目标字符串
 * @param[in] src 源字符串
 * @return 成功返回目标字符串地址，失败返回NULL
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS char *__wrap_strcpy(char *dest, const char *src)
{
    return LmsStrcpy(dest, src);  // 调用带有效性检查的字符串拷贝函数
}
