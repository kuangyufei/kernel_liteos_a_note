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
/**
 * @brief 错误类型定义
 * @note 用于标识不同类型的内存错误
 */
#define UNKNOWN_ERROR                   3   // 未知错误
#define FREE_ERRORMODE                  2   // 释放错误模式
#define STORE_ERRMODE                   1   // 存储错误模式
#define LOAD_ERRMODE                    0   // 加载错误模式

/**
 * @brief 编译器属性定义
 * @note 用于控制编译器对内存检测代码的处理
 */
#define SANITIZER_INTERFACE_ATTRIBUTE        //  sanitizer接口属性
#define ATTRIBUTE_NO_SANITIZE_ADDRESS   __attribute__((no_sanitize_address))  // 禁用地址 sanitizer检查

/**
 * @brief 影子内存状态定义（2位表示）
 * @note 用于标识内存块的可访问性状态
 */
#define LMS_SHADOW_ACCESSIBLE           0x00 // 可访问内存
#define LMS_SHADOW_AFTERFREE            0x03 // 释放后内存
#define LMS_SHADOW_REDZONE              0x02 // 红色警戒区
#define LMS_SHADOW_PAINT                0x01 // 已标记内存
#define LMS_SHADOW_MASK                 0x03 // 影子内存掩码

/**
 * @brief 影子内存计算参数
 * @note 用于内存地址与影子内存地址的转换计算
 */
#define LMS_SHADOW_BITS_PER_CELL        2    // 每个影子内存单元的位数
#define LMS_MEM_BYTES_PER_SHADOW_CELL   4    // 每个影子单元映射的内存字节数
#define LMS_SHADOW_U8_CELL_NUM          4    // U8类型影子单元数量
#define LMS_SHADOW_U8_REFER_BYTES       16   // U8类型影子引用字节数

/**
 * @brief 影子内存状态定义（8位表示）
 * @note 扩展状态定义，用于更精细的内存状态标识
 */
#define LMS_SHADOW_ACCESSIBLE_U8        0x00 // 可访问内存(U8)
#define LMS_SHADOW_AFTERFREE_U8         0xFF // 释放后内存(U8)
#define LMS_SHADOW_REDZONE_U8           0xAA // 红色警戒区(U8)
#define LMS_SHADOW_MASK_U8              0xFF // 影子内存掩码(U8)
#define LMS_SHADOW_PAINT_U8             0x55 // 已标记内存(U8)

/**
 * @brief 内存区域大小定义
 * @note 用于指定不同宽度的内存操作
 */
#define MEM_REGION_SIZE_1               1    // 1字节内存区域
#define MEM_REGION_SIZE_2               2    // 2字节内存区域
#define MEM_REGION_SIZE_4               4    // 4字节内存区域
#define MEM_REGION_SIZE_8               8    // 8字节内存区域
#define MEM_REGION_SIZE_16              16   // 16字节内存区域

/**
 * @brief 内存检测通用常量
 * @note 内存检测模块的基本配置参数
 */
#define LMS_RZ_SIZE                     4    // 红色警戒区大小(字节)
#define LMS_OK                          0    // 操作成功
#define LMS_NOK                         1    // 操作失败

/**
 * @brief 内存地址计算宏
 * @note 用于内存地址对齐和影子内存基地址计算
 */
#define PAGE_ADDR_MASK                  0xFFFFE000                                   // 页地址掩码
#define SHADOW_BASE                                                                   \
    ((USPACE_MAP_BASE + (USPACE_MAP_SIZE / (LMS_SHADOW_U8_REFER_BYTES + 1)) * LMS_SHADOW_U8_REFER_BYTES) & \
        PAGE_ADDR_MASK)                                                              // 影子内存基地址
#define OVERHEAD                        (2 * sizeof(size_t))                         // 内存分配额外开销

/**
 * @brief 内存对齐宏
 * @note 用于将地址或大小按指定对齐值对齐
 */
#define LMS_MEM_ALIGN_DOWN(value, align) (((uint32_t)(value)) & ~((uint32_t)((align) - 1)))  // 向下对齐
#define LMS_MEM_ALIGN_UP(value, align) (((uint32_t)(value) + ((align) - 1)) & ~((uint32_t)((align) - 1)))  // 向上对齐

/**
 * @brief 内存地址信息结构体
 * @details 用于存储内存地址及其对应的影子内存信息
 */
typedef struct {
    uintptr_t memAddr;        // 原始内存地址
    uintptr_t shadowAddr;     // 影子内存地址
    uint32_t shadowOffset;    // 影子内存偏移量
    uint32_t shadowValue;     // 影子内存值
} LmsAddrInfo;

extern pthread_mutex_t g_lmsMutex;  // LMS模块全局互斥锁

/**
 * @brief 获取互斥锁
 * @param lock 互斥锁指针
 * @note 禁用地址sanitizer检查以避免死锁
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsLock(pthread_mutex_t *lock)
{
    (void)pthread_mutex_lock(lock);  // 获取互斥锁
}

/**
 * @brief 尝试获取互斥锁
 * @param lock 互斥锁指针
 * @return 成功获取返回0，否则返回非0
 * @note 非阻塞方式获取锁，禁用地址sanitizer检查
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS static inline int LmsTrylock(pthread_mutex_t *lock)
{
    return pthread_mutex_trylock(lock);  // 尝试获取互斥锁
}

/**
 * @brief 释放互斥锁
 * @param lock 互斥锁指针
 * @note 禁用地址sanitizer检查以避免死锁
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsUnlock(pthread_mutex_t *lock)
{
    (void)pthread_mutex_unlock(lock);  // 释放互斥锁
}

/**
 * @brief 触发系统崩溃
 * @note 通过访问非法地址强制系统崩溃，用于严重错误处理
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsCrash(void)
{
    *(volatile char *)(SHADOW_BASE - 1) = 0;  // 写入影子内存基地址-1触发崩溃
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