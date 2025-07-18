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

#ifndef _LOS_LMS_PRI_H
#define _LOS_LMS_PRI_H

#include "los_lms.h"
#include "los_typedef.h"
#include "los_list.h"
#include "securec.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define COMMON_ERRMODE  3  // 通用错误模式
#define FREE_ERRORMODE  2  // 释放错误模式
#define STORE_ERRMODE   1  // 存储错误模式
#define LOAD_ERRMODE    0  // 加载错误模式

#define SANITIZER_INTERFACE_ATTRIBUTE  //  sanitizer接口属性定义
#define ATTRIBUTE_NO_SANITIZE_ADDRESS   __attribute__((no_sanitize_address))  // 禁用地址 sanitize 检查的属性

#define LMS_SHADOW_BITS_PER_CELL       2  // 每个影子单元格的位数
#define LMS_MEM_BYTES_PER_SHADOW_CELL  4  // 每个影子单元格对应的内存字节数
#define LMS_SHADOW_U8_CELL_NUM         4  // U8类型影子单元格数量
#define LMS_SHADOW_U8_REFER_BYTES      16  // U8类型影子引用字节数

#define LMS_POOL_RESIZE(size)          ((size) / (LMS_SHADOW_U8_REFER_BYTES + 1) * LMS_SHADOW_U8_REFER_BYTES)  // 内存池大小调整宏
#define LMS_ADDR_ALIGN(p)              (((UINTPTR)(p) + sizeof(UINTPTR) - 1) & ~((UINTPTR)(sizeof(UINTPTR) - 1)))  // 地址对齐宏

#define LMS_SHADOW_ACCESSIBLE          0x00  // 影子内存可访问标记
#define LMS_SHADOW_AFTERFREE           0x03  // 释放后影子内存标记
#define LMS_SHADOW_REDZONE             0x02  // 红区影子内存标记
#define LMS_SHADOW_PAINT               0x01  // 标记影子内存标记
#define LMS_SHADOW_MASK                0x03  // 影子内存掩码

#define LMS_SHADOW_ACCESSIBLE_U8       0x00  // U8类型影子内存可访问标记
#define LMS_SHADOW_AFTERFREE_U8        0xFF  // U8类型释放后影子内存标记
#define LMS_SHADOW_REDZONE_U8          0xAA  // U8类型红区影子内存标记
#define LMS_SHADOW_MASK_U8             0xFF  // U8类型影子内存掩码
#define LMS_SHADOW_PAINT_U8            0x55  // U8类型标记影子内存标记

#define MEM_REGION_SIZE_1              1  // 内存区域大小1字节
#define MEM_REGION_SIZE_2              2  // 内存区域大小2字节
#define MEM_REGION_SIZE_4              4  // 内存区域大小4字节
#define MEM_REGION_SIZE_8              8  // 内存区域大小8字节
#define MEM_REGION_SIZE_16             16  // 内存区域大小16字节

/**
 * @brief 内存池链表节点结构体
 * @details 用于管理内存池的链表节点，记录内存池的地址、大小及影子内存信息
 */
typedef struct {
    LOS_DL_LIST node;         // 双向链表节点
    UINT32 used;              // 内存池使用标记
    UINTPTR poolAddr;         // 内存池起始地址
    UINT32 poolSize;          // 内存池大小
    UINTPTR shadowStart;      // 影子内存起始地址
    UINT32 shadowSize;        // 影子内存大小
} LmsMemListNode;

/**
 * @brief 地址信息结构体
 * @details 记录内存地址及其对应的影子内存地址、偏移和值
 */
typedef struct {
    UINTPTR memAddr;          // 内存地址
    UINTPTR shadowAddr;       // 影子内存地址
    UINT32 shadowOffset;      // 影子内存偏移
    UINT32 shadowValue;       // 影子内存值
} LmsAddrInfo;

/**
 * @brief LMS钩子函数结构体
 * @details 定义LMS模块的钩子函数集合，包括初始化、去初始化、内存标记和检查等操作
 */
typedef struct {
    UINT32 (*init)(const VOID *pool, UINT32 size);  // 初始化钩子函数
    VOID (*deInit)(const VOID *pool);               // 去初始化钩子函数
    VOID (*mallocMark)(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize);  // 内存分配标记函数
    VOID (*freeMark)(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize);    // 内存释放标记函数
    VOID (*simpleMark)(UINTPTR startAddr, UINTPTR endAddr, UINT32 value);  // 简单内存标记函数
    VOID (*check)(UINTPTR checkAddr, BOOL isFreeCheck);  // 内存检查函数
} LmsHook;

extern LmsHook* g_lms;

VOID OsLmsCheckValid(UINTPTR checkAddr, BOOL isFreeCheck);
VOID OsLmsLosMallocMark(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize);
VOID OsLmsLosFreeMark(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize);
VOID OsLmsSimpleMark(UINTPTR startAddr, UINTPTR endAddr, UINT32 value);

VOID OsLmsPrintPoolListInfo(VOID);
VOID OsLmsReportError(UINTPTR p, UINT32 size, UINT32 errMod);

VOID CheckValid(const CHAR *dest, const CHAR *src);

extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_store1_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_store4_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_load4_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_load1_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_loadN_noabort(UINTPTR p, UINT32 size);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_storeN_noabort(UINTPTR p, UINT32 size);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_store2_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_load2_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_store8_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_load8_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_load16_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_store16_noabort(UINTPTR p);
extern SANITIZER_INTERFACE_ATTRIBUTE VOID __asan_handle_no_return(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_LMS_PRI_H */