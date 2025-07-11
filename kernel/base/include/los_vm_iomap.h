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

#ifndef __LOS_VM_IOMAP_H__
#define __LOS_VM_IOMAP_H__

#include "los_typedef.h"
#include "los_vm_zone.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 
 * @verbatim
    DMA，全称Direct Memory Access，即直接存储器访问。
    DMA传输将数据从一个地址空间复制到另一个地址空间，提供在外设和存储器之间或者存储器和存储器之间的高速数据传输。
    DMA的作用就是实现数据的直接传输，而去掉了传统数据传输需要CPU寄存器参与的环节，主要涉及四种情况的数据传输，
    但本质上是一样的，都是从内存的某一区域传输到内存的另一区域（外设的数据寄存器本质上就是内存的一个存储单元）
 * @endverbatim
 */

/**
 * @brief DMA内存类型枚举
 * @details 定义DMA传输时内存的缓存属性，用于决定数据是否经过CPU缓存
 */
enum DmaMemType { 
    DMA_CACHE,       /*!< 缓存型DMA内存：数据通过CPU缓存，适用于频繁访问场景 */
    DMA_NOCACHE      /*!< 非缓存型DMA内存：数据直接访问物理内存，适用于设备直接访问场景 */
};

/* 线程安全：本模块所有API均支持多线程并发调用 */
/**
 * @brief 分配DMA内存
 * @param[out] dmaAddr 输出参数，指向分配的DMA物理地址
 * @param[in] size 要分配的内存大小(字节)
 * @param[in] align 内存对齐要求(字节)，必须是2的幂次方
 * @param[in] type DMA内存类型，选择缓存或非缓存
 * @return VOID* 成功返回虚拟地址，失败返回NULL
 * @note 1. 分配的内存需要通过LOS_DmaMemFree释放
 * @note 2. 当type为DMA_CACHE时，需注意缓存一致性问题
 */
VOID *LOS_DmaMemAlloc(DMA_ADDR_T *dmaAddr, size_t size, size_t align, enum DmaMemType type);

/**
 * @brief 释放DMA内存
 * @param[in] vaddr 要释放的DMA内存虚拟地址(LOS_DmaMemAlloc返回值)
 * @note 1. 只能释放通过LOS_DmaMemAlloc分配的内存
 * @note 2. 释放后dmaAddr对应的物理地址将失效
 */
VOID LOS_DmaMemFree(VOID *vaddr);

/**
 * @brief 将DMA虚拟地址转换为物理地址
 * @param[in] vaddr DMA内存的虚拟地址
 * @return DMA_ADDR_T 对应的物理地址，失败返回0
 * @note 仅对通过LOS_DmaMemAlloc分配的地址有效
 */
DMA_ADDR_T LOS_DmaVaddrToPaddr(VOID *vaddr);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_IOMAP_H__ */
