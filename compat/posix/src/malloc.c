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

#include "stdlib.h"
#include "string.h"
#include "los_vm_map.h"

/*
 * Allocates the requested memory and returns a pointer to it. The requested
 * size is nitems each size bytes long (total memory requested is nitems*size).
 * The space is initialized to all zero bits.
 */
/**
 * @brief   分配指定数量和大小的内存块并初始化为零
 * @param   nitems  元素数量
 * @param   size    每个元素的大小(字节)
 * @return  成功返回指向分配内存的指针，失败返回NULL
 */
void *calloc(size_t nitems, size_t size)
{
    size_t real_size;                  // 计算实际需要分配的内存大小
    void *ptr = NULL;                  // 指向分配内存块的指针

    // 检查元素数量或大小是否为零
    if (nitems == 0 || size == 0) {
        return NULL;                   // 参数无效，返回NULL
    }

    real_size = (size_t)(nitems * size);  // 计算总内存大小
    ptr = LOS_KernelMalloc((UINT32) real_size);  // 调用内核malloc分配内存
    if (ptr != NULL) {
        (void) memset_s((void *) ptr, real_size, 0, real_size);  // 将分配的内存初始化为零
    }
    return ptr;                        // 返回分配的内存指针
}

/**
 * @brief   释放之前通过calloc、malloc或realloc分配的内存
 * @param   ptr 指向要释放内存的指针
 * @note    如果ptr为NULL或指向未分配/已释放的内存，结果未定义
 */
void free(void *ptr)
{
    if (ptr == NULL) {                 // 检查指针是否为NULL
        return;                        // 不执行任何操作
    }

    LOS_KernelFree(ptr);                // 调用内核free释放内存
}

/**
 * @brief   分配指定大小的内存块
 * @param   size 需要分配的内存大小(字节)
 * @return  成功返回指向分配内存的指针，失败返回NULL；内存内容未初始化
 */
void *malloc(size_t size)
{
    if (size == 0) {                   // 检查分配大小是否为零
        return NULL;                   // 返回NULL
    }

    return LOS_KernelMalloc((UINT32) size);  // 调用内核malloc分配内存并返回指针
}

/**
 * @brief   分配指定大小的内存块并初始化为零
 * @param   size 需要分配的内存大小(字节)
 * @return  成功返回指向分配内存的指针，失败返回NULL
 */
void *zalloc(size_t size)
{
    void *ptr = NULL;                  // 指向分配内存块的指针

    if (size == 0) {                   // 检查分配大小是否为零
        return NULL;                   // 返回NULL
    }

    ptr = LOS_KernelMalloc((UINT32) size);  // 调用内核malloc分配内存
    if (ptr != NULL) {
        (void) memset_s(ptr, size, 0, size);  // 将分配的内存初始化为零
    }
    return ptr;                        // 返回分配的内存指针
}

/**
 * @brief   分配指定大小且地址对齐的内存块
 * @param   boundary 对齐边界(必须是2的幂)
 * @param   size     需要分配的内存大小(字节)
 * @return  成功返回指向分配内存的指针，失败返回NULL
 */
void *memalign(size_t boundary, size_t size)
{
    if (size == 0) {                   // 检查分配大小是否为零
        return NULL;                   // 返回NULL
    }

    // 调用内核对齐malloc分配内存，指定大小和对齐边界
    return LOS_KernelMallocAlign((UINT32) size, (UINT32) boundary);
}

/**
 * @brief   调整之前分配的内存块大小
 * @param   ptr   指向之前分配内存的指针
 * @param   size  新的内存块大小(字节)
 * @return  成功返回指向新内存块的指针，失败返回NULL(原内存块保持不变)
 * @note    如果ptr为NULL，则等同于malloc(size)；如果size为0，则等同于free(ptr)
 */
void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {                 // 如果指针为NULL
        ptr = malloc(size);            // 直接调用malloc分配新内存
        return ptr;
    }

    if (size == 0) {                   // 如果新大小为零
        free(ptr);                     // 释放原内存块
        return NULL;
    }

    // 调用内核realloc调整内存大小
    return LOS_KernelRealloc(ptr, (UINT32) size);
}