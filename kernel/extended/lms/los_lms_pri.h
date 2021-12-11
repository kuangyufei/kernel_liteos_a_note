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

#define COMMON_ERRMODE  3
#define FREE_ERRORMODE  2
#define STORE_ERRMODE   1
#define LOAD_ERRMODE    0

#define SANITIZER_INTERFACE_ATTRIBUTE
#define ATTRIBUTE_NO_SANITIZE_ADDRESS   __attribute__((no_sanitize_address))

#define LMS_SHADOW_BITS_PER_CELL       2
#define LMS_MEM_BYTES_PER_SHADOW_CELL  4
#define LMS_SHADOW_U8_CELL_NUM         4
#define LMS_SHADOW_U8_REFER_BYTES      16

#define LMS_POOL_RESIZE(size)          ((size) / (LMS_SHADOW_U8_REFER_BYTES + 1) * LMS_SHADOW_U8_REFER_BYTES)
#define LMS_ADDR_ALIGN(p)              (((UINTPTR)(p) + sizeof(UINTPTR) - 1) & ~((UINTPTR)(sizeof(UINTPTR) - 1)))

#define LMS_SHADOW_ACCESSABLE          0x00
#define LMS_SHADOW_AFTERFREE           0x03
#define LMS_SHADOW_REDZONE             0x02
#define LMS_SHADOW_PAINT               0x01
#define LMS_SHADOW_MASK                0x03

#define LMS_SHADOW_ACCESSABLE_U8       0x00
#define LMS_SHADOW_AFTERFREE_U8        0xFF
#define LMS_SHADOW_REDZONE_U8          0xAA
#define LMS_SHADOW_MASK_U8             0xFF
#define LMS_SHADOW_PAINT_U8            0x55

#define MEM_REGION_SIZE_1              1
#define MEM_REGION_SIZE_2              2
#define MEM_REGION_SIZE_4              4
#define MEM_REGION_SIZE_8              8
#define MEM_REGION_SIZE_16             16

typedef struct {
    LOS_DL_LIST node;
    UINT32 used;
    UINTPTR poolAddr;
    UINT32 poolSize;
    UINTPTR shadowStart;
    UINT32 shadowSize;
} LmsMemListNode;

typedef struct {
    UINTPTR memAddr;
    UINTPTR shadowAddr;
    UINT32 shadowOffset;
    UINT32 shadowValue;
} LmsAddrInfo;

typedef struct {
    UINT32 (*init)(const VOID *pool, UINT32 size);
    VOID (*mallocMark)(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize);
    VOID (*freeMark)(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize);
    VOID (*simpleMark)(UINTPTR startAddr, UINTPTR endAddr, UINT32 value);
    VOID (*check)(UINTPTR checkAddr, BOOL isFreeCheck);
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