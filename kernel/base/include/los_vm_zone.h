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

#ifndef __VM_ZONE_H__
#define __VM_ZONE_H__

#include "target_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @file los_vm_zone.h
 * @brief 
 * @verbatim
    /@note_pic
    	鸿蒙虚拟内存全景图 从 0x00000000U 至 0xFFFFFFFFU 
    	鸿蒙源码分析系列篇: 			https://blog.csdn.net/kuangyufei 
    						https://my.oschina.net/u/3751245

    +----------------------------+ 0xFFFFFFFFU
    |  IO设备未缓存                   |
    |  PERIPH_PMM_SIZE           |
    +----------------------------+ 外围设备未缓存基地址 PERIPH_UNCACHED_BASE
    |  IO设备缓存                    |
    |  PERIPH_PMM_SIZE           |
    +----------------------------+ 外围设备缓存基地址 PERIPH_CACHED_BASE
    |  包括 IO设备                   |
    |  PERIPH_PMM_SIZE           |
    +----------------------------+ 外围设备基地址 PERIPH_DEVICE_BASE
    |  Vmalloc段                  |
    |  kernel heap               |
    |  128M                      |
    |  映射区                       |
    +----------------------------+ 内核动态分配开始地址 VMALLOC_START
    |   DDR_MEM_SIZE             |
    |   Uncached段                |
    +----------------------------+ 未缓存虚拟空间基地址 UNCACHED_VMM_BASE
    |   内核虚拟空间                   |
    |   KERNEL_VMM_SIZE          |
    |   .bss					 |
    |   .rodata                  |
    |   .text                    |
    |  映射区                       |
    +----------------------------+ 内核空间开始地址 KERNEL_ASPACE_BASE = KERNEL_VMM_BASE
    |    16M预留区                  |
    +----------------------------+ 用户空间栈顶 USER_ASPACE_TOP_MAX = USER_ASPACE_BASE + USER_ASPACE_SIZE
    |                            |
    |    用户空间                    |
    |    USER_ASPACE_SIZE        |
    |    用户栈区(stack)             |
    |    映射区(map)                |
    |    堆区	(heap)               |
    |    .bss                    |
    |    .data .text             |
    +----------------------------+ 用户空间开始地址 USER_ASPACE_BASE
    |    16M预留区	                 |
    +----------------------------+ 0x00000000U

    以下定义 可见于 ..\vendor\hi3516dv300\config\board\include\board.h
    #ifdef LOSCFG_KERNEL_MMU
    #ifdef LOSCFG_TEE_ENABLE
    #define KERNEL_VADDR_BASE       0x41000000
    #else
    #define KERNEL_VADDR_BASE       0x40000000
    #endif
    #else
    #define KERNEL_VADDR_BASE       DDR_MEM_ADDR
    #endif
    #define KERNEL_VADDR_SIZE       DDR_MEM_SIZE

    #define SYS_MEM_BASE            DDR_MEM_ADDR
    #define SYS_MEM_END             (SYS_MEM_BASE + SYS_MEM_SIZE_DEFAULT)

    #define EXC_INTERACT_MEM_SIZE   0x100000

    内核空间范围: 0x40000000 ~ 0xFFFFFFFF
    用户空间氛围: 0x00000000 ~ 0x3FFFFFFF

    cached地址和uncached地址的区别是
    对cached地址的访问是委托给CPU进行的，也就是说你的操作到底是提交给真正的外设或内存，还是转到CPU缓存，
    是由CPU决定的。CPU有一套缓存策略来决定什么时候从缓存中读取数据，什么时候同步缓存。
    对unchached地址的访问是告诉CPU忽略缓存，访问操作直接反映到外设或内存上。 
    对于IO设备一定要用uncached地址访问，是因为你的IO输出操作肯定是希望立即反映到IO设备上，不希望让CPU缓存你的操作；
    另一方面，IO设备的状态是独立于CPU的，也就是说IO口状态的改变CPU是不知道，这样就导致缓存和外设的内容不一致，
    你从IO设备读取数据时，肯定是希望直接读取IO设备的当前状态，而不是CPU缓存的过期值。
 * @endverbatim
 */

#ifdef LOSCFG_KERNEL_MMU
#ifdef LOSCFG_TEE_ENABLE
#define KERNEL_VADDR_BASE       0x41000000
#else
#define KERNEL_VADDR_BASE       0x40000000
#endif
#else
#define KERNEL_VADDR_BASE       DDR_MEM_ADDR
#endif
#define KERNEL_VADDR_SIZE       DDR_MEM_SIZE

#define SYS_MEM_BASE            DDR_MEM_ADDR
#define SYS_MEM_END             (SYS_MEM_BASE + SYS_MEM_SIZE_DEFAULT)


#define _U32_C(X)  X##U
#define U32_C(X)   _U32_C(X)

#define KERNEL_VMM_BASE         U32_C(KERNEL_VADDR_BASE) ///< 速度快,使用cache
#define KERNEL_VMM_SIZE         U32_C(KERNEL_VADDR_SIZE)

#define KERNEL_ASPACE_BASE      KERNEL_VMM_BASE ///< 内核空间开始地址
#define KERNEL_ASPACE_SIZE      KERNEL_VMM_SIZE ///< 内核空间大小

/* Uncached vmm aspace */
#define UNCACHED_VMM_BASE       (KERNEL_VMM_BASE + KERNEL_VMM_SIZE) ///< 未缓存虚拟空间基地址,适用于DMA,LCD framebuf,
#define UNCACHED_VMM_SIZE       DDR_MEM_SIZE ///<未缓存虚拟空间大小

#define VMALLOC_START           (UNCACHED_VMM_BASE + UNCACHED_VMM_SIZE) ///< 动态分配基地址
#define VMALLOC_SIZE            0x08000000 ///< 128M
//UART,LCD,摄像头,I2C,中断控制器统称为外部设备
#ifdef LOSCFG_KERNEL_MMU	//使用MMU时,只是虚拟地址不一样,但映射的物理设备空间一致.
#define PERIPH_DEVICE_BASE      (VMALLOC_START + VMALLOC_SIZE)	///< 不使用buffer,cache
#define PERIPH_DEVICE_SIZE      U32_C(PERIPH_PMM_SIZE)
#define PERIPH_CACHED_BASE      (PERIPH_DEVICE_BASE + PERIPH_DEVICE_SIZE) ///< 使用cache但不用buffer
#define PERIPH_CACHED_SIZE      U32_C(PERIPH_PMM_SIZE)
#define PERIPH_UNCACHED_BASE    (PERIPH_CACHED_BASE + PERIPH_CACHED_SIZE) ///< 不使用cache但使用buffer
#define PERIPH_UNCACHED_SIZE    U32_C(PERIPH_PMM_SIZE)
#else	//不使用MMU时,外部设备空间地址一致.
#define PERIPH_DEVICE_BASE      PERIPH_PMM_BASE
#define PERIPH_DEVICE_SIZE      U32_C(PERIPH_PMM_SIZE)
#define PERIPH_CACHED_BASE      PERIPH_PMM_BASE
#define PERIPH_CACHED_SIZE      U32_C(PERIPH_PMM_SIZE)
#define PERIPH_UNCACHED_BASE    PERIPH_PMM_BASE
#define PERIPH_UNCACHED_SIZE    U32_C(PERIPH_PMM_SIZE)
#endif

#define IO_DEVICE_ADDR(paddr)        (paddr - PERIPH_PMM_BASE + PERIPH_DEVICE_BASE)		///< 通过物理地址获取IO设备虚拟地址
#define IO_CACHED_ADDR(paddr)        (paddr - PERIPH_PMM_BASE + PERIPH_CACHED_BASE)		///< 通过物理地址获取IO设备虚拟缓存地址
#define IO_UNCACHED_ADDR(paddr)      (paddr - PERIPH_PMM_BASE + PERIPH_UNCACHED_BASE)	///< 通过物理地址获取IO设备虚拟未缓存地址
//DDR_MEM_ADDRDDR内存全称是DDR SDRAM(Double Data Rate SDRAM，双倍速率SDRAM)

#define MEM_CACHED_ADDR(paddr)       (paddr - DDR_MEM_ADDR + KERNEL_VMM_BASE)
#define MEM_UNCACHED_ADDR(paddr)     (paddr - DDR_MEM_ADDR + UNCACHED_VMM_BASE)

#define VMM_TO_UNCACHED_ADDR(vaddr)  (vaddr - KERNEL_VMM_BASE + UNCACHED_VMM_BASE)
#define UNCACHED_TO_VMM_ADDR(vaddr)  (vaddr - UNCACHED_VMM_BASE + KERNEL_VMM_BASE)

#define VMM_TO_DMA_ADDR(vaddr)  (vaddr - KERNEL_VMM_BASE + SYS_MEM_BASE)
#define DMA_TO_VMM_ADDR(vaddr)  (vaddr - SYS_MEM_BASE + KERNEL_VMM_BASE)

#if (PERIPH_UNCACHED_BASE >= (0xFFFFFFFFU - PERIPH_UNCACHED_SIZE))
#error "Kernel virtual memory space has overflowed!"
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
