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

#include <sys/mman.h>
#include "los_lms_pri.h"
#include "debug.h"

#define LMS_FREE_NODE_SIZE 16

struct MmapNode {
    uintptr_t addr;
    size_t mapSize;
    struct MmapNode *next;
};

struct MmapNode g_freeNode[LMS_FREE_NODE_SIZE];

struct MmapNode *g_mmapNode = NULL;

uint32_t g_shadowStartAddr = SHADOW_BASE;

pthread_mutex_t g_lmsMutex = PTHREAD_MUTEX_INITIALIZER;

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsMem2Shadow(uintptr_t memAddr, uintptr_t *shadowAddr, uint32_t *shadowOffset)
{
    uint32_t memOffset = memAddr - USPACE_MAP_BASE;
    *shadowAddr = g_shadowStartAddr + memOffset / LMS_SHADOW_U8_REFER_BYTES;
    *shadowOffset = ((memOffset % LMS_SHADOW_U8_REFER_BYTES) / LMS_SHADOW_U8_CELL_NUM) * LMS_SHADOW_BITS_PER_CELL;
}

ATTRIBUTE_NO_SANITIZE_ADDRESS uint32_t LmsIsShadowAddrMapped(uintptr_t sdStartAddr, uintptr_t sdEndAddr)
{
    struct MmapNode *node = g_mmapNode;
    while (node != NULL) {
        if ((sdStartAddr >= node->addr) && (sdEndAddr < node->addr + node->mapSize)) {
            return LMS_OK;
        }
        node = node->next;
    }
    return LMS_NOK;
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsAddMapNode(uintptr_t sdStartAddr, uintptr_t sdEndAddr)
{
    static uint32_t freeNodeIdx = 0;
    struct MmapNode *node = g_mmapNode;
    size_t mapSize;
    uintptr_t shadowPageStartAddr = LMS_MEM_ALIGN_DOWN(sdStartAddr, 0x1000);
    uintptr_t shadowPageEndAddr = LMS_MEM_ALIGN_UP(sdEndAddr, 0x1000);

    struct MmapNode *expandNode = NULL;

    while (node != NULL) {
        if ((shadowPageStartAddr >= node->addr) && (shadowPageStartAddr <= node->addr + node->mapSize)) {
            expandNode = node;
            break;
        }
        node = node->next;
    }

    if (expandNode != NULL) {
        shadowPageStartAddr = expandNode->addr + expandNode->mapSize;
        expandNode->mapSize = shadowPageEndAddr - expandNode->addr;
    }

    mapSize = shadowPageEndAddr - shadowPageStartAddr;
    void *mapPtr =
        mmap((void *)shadowPageStartAddr, mapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapPtr == (void *)-1) {
        LMS_OUTPUT_INFO("mmap error! file:%s line:%d\n", __FILE__, __LINE__);
        return;
    }
    __real_memset(mapPtr, 0, mapSize);

    if (expandNode != NULL) {
        return;
    }

    if (freeNodeIdx >= LMS_FREE_NODE_SIZE) {
        LMS_OUTPUT_INFO("Add new mmap node error! file:%s line:%d\n", __FILE__, __LINE__);
        return;
    }

    struct MmapNode *newNode = &g_freeNode[freeNodeIdx];
    freeNodeIdx++;
    newNode->addr = shadowPageStartAddr;
    newNode->mapSize = mapSize;
    newNode->next = g_mmapNode;
    g_mmapNode = newNode;
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsSetShadowValue(uintptr_t startAddr, uintptr_t endAddr, char value)
{
    uintptr_t shadowStart;
    uintptr_t shadowEnd;
    uint32_t startOffset;
    uint32_t endOffset;

    char shadowValueMask;
    char shadowValue;

    /* endAddr - 1, then we mark [startAddr, endAddr) to value */
    LmsMem2Shadow(startAddr, &shadowStart, &startOffset);
    LmsMem2Shadow(endAddr - 1, &shadowEnd, &endOffset);

    if (shadowStart == shadowEnd) { /* in the same u8 */
        /* because endAddr - 1, the endOffset falls into the previous cell,
        so endOffset + 2 is required for calculation */
        shadowValueMask = LMS_SHADOW_MASK_U8;
        shadowValueMask =
            (shadowValueMask << startOffset) & (~(shadowValueMask << (endOffset + LMS_SHADOW_BITS_PER_CELL)));
        shadowValue = value & shadowValueMask;
        *(char *)shadowStart &= ~shadowValueMask;
        *(char *)shadowStart |= shadowValue;
    } else {
        /* Adjust startAddr to left util it reach the beginning of a u8 */
        if (startOffset > 0) {
            shadowValueMask = LMS_SHADOW_MASK_U8;
            shadowValueMask = shadowValueMask << startOffset;
            shadowValue = value & shadowValueMask;
            *(char *)shadowStart &= ~shadowValueMask;
            *(char *)shadowStart |= shadowValue;
            shadowStart += 1;
        }

        /* Adjust endAddr to right util it reach the end of a u8 */
        if (endOffset < (LMS_SHADOW_U8_CELL_NUM - 1) * LMS_SHADOW_BITS_PER_CELL) {
            shadowValueMask = LMS_SHADOW_MASK_U8;
            shadowValueMask &= ~(shadowValueMask << (endOffset + LMS_SHADOW_BITS_PER_CELL));
            shadowValue = value & shadowValueMask;
            *(char *)shadowEnd &= ~shadowValueMask;
            *(char *)shadowEnd |= shadowValue;
            shadowEnd -= 1;
        }

        if (shadowEnd + 1 > shadowStart) {
            (void)__real_memset((void *)shadowStart, value & LMS_SHADOW_MASK_U8, shadowEnd + 1 - shadowStart);
        }
    }
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsGetShadowValue(uintptr_t addr, uint32_t *shadowValue)
{
    uintptr_t shadowAddr;
    uint32_t shadowOffset;
    LmsMem2Shadow(addr, &shadowAddr, &shadowOffset);
    /* If the shadow addr is not mapped then regarded as legal access */
    if (LmsIsShadowAddrMapped(shadowAddr, shadowAddr) != LMS_OK) {
        *shadowValue = LMS_SHADOW_ACCESSABLE_U8;
        return;
    }

    *shadowValue = ((*(char *)shadowAddr) >> shadowOffset) & LMS_SHADOW_MASK;
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsMallocMark(uintptr_t preRzStart, uintptr_t accessMemStart, uintptr_t nextRzStart,
    uintptr_t RzEndAddr)
{
    LmsSetShadowValue(preRzStart, accessMemStart, LMS_SHADOW_REDZONE_U8);
    LmsSetShadowValue(accessMemStart, nextRzStart, LMS_SHADOW_ACCESSABLE_U8);
    LmsSetShadowValue(nextRzStart, RzEndAddr, LMS_SHADOW_REDZONE_U8);
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsTagMem(void *ptr, size_t origSize)
{
    g_shadowStartAddr = SHADOW_BASE;
    size_t mSize = malloc_usable_size(ptr);
    uint32_t memOffset = (uintptr_t)ptr + mSize + OVERHEAD - USPACE_MAP_BASE;
    uintptr_t shadowEndAddr = g_shadowStartAddr + memOffset / LMS_SHADOW_U8_REFER_BYTES;
    LMS_OUTPUT_INFO("SHADOW_BASE:%x g_shadowStartAddr:%x memOffset: %x\n", SHADOW_BASE, g_shadowStartAddr, memOffset);
    memOffset = (uintptr_t)ptr - OVERHEAD - USPACE_MAP_BASE;
    uintptr_t shadowStartAddr = g_shadowStartAddr + memOffset / LMS_SHADOW_U8_REFER_BYTES;

    LmsLock(&g_lmsMutex);
    if (LmsIsShadowAddrMapped(shadowStartAddr, shadowEndAddr) != LMS_OK) {
        LmsAddMapNode(shadowStartAddr, shadowEndAddr);
    }

    LmsMallocMark((uintptr_t)ptr - OVERHEAD, (uintptr_t)ptr, (uintptr_t)ptr + LMS_MEM_ALIGN_UP(origSize, LMS_RZ_SIZE),
        (uintptr_t)((uintptr_t)ptr + mSize + OVERHEAD));
    LmsUnlock(&g_lmsMutex);

    return ptr;
}

ATTRIBUTE_NO_SANITIZE_ADDRESS uint32_t LmsCheckAddr(uintptr_t addr)
{
    LmsLock(&g_lmsMutex);
    uint32_t shadowValue = -1;
    LmsGetShadowValue(addr, &shadowValue);
    LmsUnlock(&g_lmsMutex);
    return shadowValue;
}

ATTRIBUTE_NO_SANITIZE_ADDRESS uint32_t LmsCheckAddrRegion(uintptr_t addr, size_t size)
{
    if (LmsCheckAddr(addr) || LmsCheckAddr(addr + size - 1)) {
        return LMS_NOK;
    } else {
        return LMS_OK;
    }
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsPrintMemInfo(uintptr_t addr)
{
#define LMS_DUMP_OFFSET 4
#define LMS_DUMP_RANGE_DOUBLE 2

    LMS_OUTPUT_INFO("\nDump info around address [0x%8x]:\n", addr);
    uint32_t printY = LMS_DUMP_OFFSET * LMS_DUMP_RANGE_DOUBLE + 1;
    uint32_t printX = LMS_MEM_BYTES_PER_SHADOW_CELL * LMS_DUMP_RANGE_DOUBLE;
    uintptr_t dumpAddr = addr - addr % printX - LMS_DUMP_OFFSET * printX;
    uint32_t shadowValue = 0;
    uintptr_t shadowAddr = 0;
    uint32_t shadowOffset = 0;
    int isCheckAddr;

    for (int y = 0; y < printY; y++, dumpAddr += printX) {
        LmsMem2Shadow(dumpAddr, &shadowAddr, &shadowOffset);
        /* find util dumpAddr in pool region */
        if (LmsIsShadowAddrMapped(shadowAddr, shadowAddr) != LMS_OK) {
            continue;
        }
        uintptr_t maxShadowAddr;
        uint32_t maxShadowOffset;
        LmsMem2Shadow(dumpAddr + printX, &maxShadowAddr, &maxShadowOffset);
        /* finish if dumpAddr exceeds pool's upper region */
        if (LmsIsShadowAddrMapped(maxShadowAddr, maxShadowAddr) != LMS_OK) {
            goto END;
        }

        LMS_OUTPUT_INFO("\n\t[0x%x]: ", dumpAddr);
        for (int x = 0; x < printX; x++) {
            if (dumpAddr + x == addr) {
                LMS_OUTPUT_INFO("[%02x]", *(uint8_t *)(dumpAddr + x));
            } else {
                LMS_OUTPUT_INFO(" %02x ", *(uint8_t *)(dumpAddr + x));
            }
        }

        LMS_OUTPUT_INFO("|\t[0x%x | %2d]: ", shadowAddr, shadowOffset);

        for (int x = 0; x < printX; x += LMS_MEM_BYTES_PER_SHADOW_CELL) {
            LmsGetShadowValue(dumpAddr + x, &shadowValue);
            isCheckAddr = dumpAddr + x - (uintptr_t)addr + LMS_MEM_BYTES_PER_SHADOW_CELL;
            if (isCheckAddr > 0 && isCheckAddr <= LMS_MEM_BYTES_PER_SHADOW_CELL) {
                LMS_OUTPUT_INFO("[%1x]", shadowValue);
            } else {
                LMS_OUTPUT_INFO(" %1x ", shadowValue);
            }
        }
    }
END:
    LMS_OUTPUT_INFO("\n");
}

ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsGetShadowInfo(uintptr_t memAddr, LmsAddrInfo *info)
{
    uintptr_t shadowAddr;
    uint32_t shadowOffset;
    uint32_t shadowValue;

    LmsMem2Shadow(memAddr, &shadowAddr, &shadowOffset);

    shadowValue = ((*(char *)shadowAddr) >> shadowOffset) & LMS_SHADOW_MASK;
    info->memAddr = memAddr;
    info->shadowAddr = shadowAddr;
    info->shadowOffset = shadowOffset;
    info->shadowValue = shadowValue;
}

ATTRIBUTE_NO_SANITIZE_ADDRESS static void LmsGetErrorInfo(uintptr_t addr, size_t size, LmsAddrInfo *info)
{
    LmsGetShadowInfo(addr, info);
    if (info->shadowValue != LMS_SHADOW_ACCESSABLE_U8) {
        return;
    } else {
        LmsGetShadowInfo(addr + size - 1, info);
    }
}

ATTRIBUTE_NO_SANITIZE_ADDRESS static void LmsPrintErrInfo(LmsAddrInfo *info, uint32_t errMod)
{
    switch (info->shadowValue) {
        case LMS_SHADOW_AFTERFREE:
            LMS_OUTPUT_ERROR("Use after free error detected!\n");
            break;
        case LMS_SHADOW_REDZONE:
            LMS_OUTPUT_ERROR("Heap buffer overflow error detected!\n");
            break;
        case LMS_SHADOW_ACCESSABLE:
            LMS_OUTPUT_ERROR("No error!\n");
            break;
        default:
            LMS_OUTPUT_ERROR("UnKnown Error detected!\n");
            break;
    }

    switch (errMod) {
        case FREE_ERRORMODE:
            LMS_OUTPUT_ERROR("Illegal Double free address at: [0x%x]\n", info->memAddr);
            break;
        case LOAD_ERRMODE:
            LMS_OUTPUT_ERROR("Illegal READ address at: [0x%x]\n", info->memAddr);
            break;
        case STORE_ERRMODE:
            LMS_OUTPUT_ERROR("Illegal WRITE address at: [0x%x]\n", info->memAddr);
            break;
        default:
            LMS_OUTPUT_ERROR("UnKnown Error mode at: [0x%x]\n", info->memAddr);
            break;
    }

    LMS_OUTPUT_INFO("Shadow memory address: [0x%x : %d]  Shadow memory value: [%d] \n", info->shadowAddr,
        info->shadowOffset, info->shadowValue);

    LMS_OUTPUT_INFO("\n");
    LMS_OUTPUT_INFO("%-25s%d\n", "Accessable heap addr", LMS_SHADOW_ACCESSABLE);
    LMS_OUTPUT_INFO("%-25s%d\n", "Heap red zone", LMS_SHADOW_REDZONE);
    LMS_OUTPUT_INFO("%-25s%d\n", "Heap freed buffer", LMS_SHADOW_AFTERFREE);
    LMS_OUTPUT_INFO("\n");
}

ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsReportError(uintptr_t p, size_t size, uint32_t errMod)
{
    LmsAddrInfo info;
    (void)__real_memset(&info, 0, sizeof(LmsAddrInfo));

    int locked = LmsTrylock(&g_lmsMutex);
    LMS_OUTPUT_ERROR("\n*****  Lite Memory Sanitizer Error Detected  *****\n");
    LmsGetErrorInfo(p, size, &info);
    LmsPrintErrInfo(&info, errMod);
    LmsPrintMemInfo(info.memAddr);
    LMS_OUTPUT_ERROR("*****  Lite Memory Sanitizer Error Detected End *****\n");
    if (!locked) {
        LmsUnlock(&g_lmsMutex);
    }

    if (LMS_CRASH_MODE > 0) {
        LmsCrash();
    } else {
        print_trace();
    }
}

void LmsCheckValid(const char *dest, const char *src)
{
    if (LmsCheckAddr((uintptr_t)dest) != LMS_SHADOW_ACCESSABLE_U8) {
        LmsReportError((uintptr_t)dest, MEM_REGION_SIZE_1, STORE_ERRMODE);
        return;
    }

    if (LmsCheckAddr((uintptr_t)src) != LMS_SHADOW_ACCESSABLE_U8) {
        LmsReportError((uintptr_t)src, MEM_REGION_SIZE_1, LOAD_ERRMODE);
        return;
    }

    for (uint32_t i = 0; *(src + i) != '\0'; i++) {
        if (LmsCheckAddr((uintptr_t)dest + i + 1) != LMS_SHADOW_ACCESSABLE_U8) {
            LmsReportError((uintptr_t)dest + i + 1, MEM_REGION_SIZE_1, STORE_ERRMODE);
            return;
        }

        if (LmsCheckAddr((uintptr_t)src + i + 1) != LMS_SHADOW_ACCESSABLE_U8) {
            LmsReportError((uintptr_t)src + i + 1, MEM_REGION_SIZE_1, LOAD_ERRMODE);
            return;
        }
    }
}

void __asan_store1_noabort(uintptr_t p)
{
    if (LmsCheckAddr(p) != LMS_SHADOW_ACCESSABLE_U8) {
        LmsReportError(p, MEM_REGION_SIZE_1, STORE_ERRMODE);
    }
}

void __asan_store2_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_2) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_2, STORE_ERRMODE);
    }
}

void __asan_store4_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_4) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_4, STORE_ERRMODE);
    }
}

void __asan_store8_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_8) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_8, STORE_ERRMODE);
    }
}

void __asan_store16_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_16) != LMS_OK) { /* 16 byte memory for check */
        LmsReportError(p, MEM_REGION_SIZE_16, STORE_ERRMODE);
    }
}

void __asan_storeN_noabort(uintptr_t p, size_t size)
{
    if (LmsCheckAddrRegion(p, size) != LMS_OK) {
        LmsReportError(p, size, STORE_ERRMODE);
    }
}

void __asan_load1_noabort(uintptr_t p)
{
    if (LmsCheckAddr(p) != LMS_SHADOW_ACCESSABLE_U8) {
        LmsReportError(p, MEM_REGION_SIZE_1, LOAD_ERRMODE);
    }
}

void __asan_load2_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_2) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_2, LOAD_ERRMODE);
    }
}

void __asan_load4_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_4) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_4, LOAD_ERRMODE);
    }
}

void __asan_load8_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_8) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_8, LOAD_ERRMODE);
    }
}

void __asan_load16_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_16) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_16, LOAD_ERRMODE);
    }
}

void __asan_loadN_noabort(uintptr_t p, size_t size)
{
    if (LmsCheckAddrRegion(p, size) != LMS_OK) {
        LmsReportError(p, size, LOAD_ERRMODE);
    }
}

void __asan_handle_no_return(void)
{
    return;
}