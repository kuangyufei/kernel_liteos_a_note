/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "board.h" // 由不同的芯片平台提供,例如: hi3516dv300

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/*************************************************************************** @note_pic
*	鸿蒙虚拟内存全景图 从 0x00000000U 至 0xFFFFFFFFU 
*	鸿蒙源码分析系列篇: 			https://blog.csdn.net/kuangyufei 
*						https://my.oschina.net/u/3751245
***************************************************************************
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
|                            |
+----------------------------+ 内核动态分配开始地址 VMALLOC_START
|   DDR_MEM_SIZE             |
|   Uncached段                |
+----------------------------+ 未缓存虚拟空间基地址 UNCACHED_VMM_BASE
|   内核虚拟空间                   |
|   KERNEL_VMM_SIZE          |
|   .bss					 |
|   .rodata                  |
|   .text                    |
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

/* Physical memory address base and size * /	//物理内存地址基地址和大小
#ifdef LOSCFG_TEE_ENABLE
#define DDR_MEM_ADDR            0x81000000
#define DDR_MEM_SIZE            0x1f000000
#else
#define DDR_MEM_ADDR            0x80000000		
#define DDR_MEM_SIZE            0x20000000		//512M
#endif

/* Peripheral register address base and size * / 外围寄存器地址基和大小
#define PERIPH_PMM_BASE         0x10000000		// 256M
#define PERIPH_PMM_SIZE         0x10000000		// 256M

#ifdef LOSCFG_TEE_ENABLE
#define KERNEL_VADDR_BASE       0x41000000
#else
#define KERNEL_VADDR_BASE       0x40000000		
#endif
#define KERNEL_VADDR_SIZE       DDR_MEM_SIZE	//512M

#define SYS_MEM_BASE            DDR_MEM_ADDR	//0x80000000
#define SYS_MEM_SIZE_DEFAULT    0x07f00000		//127M
#define SYS_MEM_END             (SYS_MEM_BASE + SYS_MEM_SIZE_DEFAULT) //0x87f00000

#define EXC_INTERACT_MEM_SIZE        0x100000 //1M

*******************************************************************************************************/


#define DEFINE_(X)  X##U
#define DEFINE(X)   DEFINE_(X)

#define KERNEL_VMM_BASE         DEFINE(KERNEL_VADDR_BASE)//内核虚拟内存开始位置 //0x40000000
#define KERNEL_VMM_SIZE         DEFINE(KERNEL_VADDR_SIZE)//内核虚拟内存大小

#define KERNEL_ASPACE_BASE      KERNEL_VMM_BASE //内核空间开始地址
#define KERNEL_ASPACE_SIZE      KERNEL_VMM_SIZE //内核空间大小

/* Uncached vmm aspace */
#define UNCACHED_VMM_BASE       (KERNEL_VMM_BASE + KERNEL_VMM_SIZE)//未缓存虚拟空间基地址
#define UNCACHED_VMM_SIZE       DDR_MEM_SIZE //未缓存虚拟空间大小

#define VMALLOC_START           (UNCACHED_VMM_BASE + UNCACHED_VMM_SIZE)//动态分配基地址
#define VMALLOC_SIZE            0x08000000//128M

#define PERIPH_DEVICE_BASE      (VMALLOC_START + VMALLOC_SIZE)//外围设备基地址
#define PERIPH_DEVICE_SIZE      PERIPH_PMM_SIZE //外围设备空间大小
#define PERIPH_CACHED_BASE      (PERIPH_DEVICE_BASE + PERIPH_DEVICE_SIZE)//外围设备缓存基地址
#define PERIPH_CACHED_SIZE      PERIPH_PMM_SIZE //外围设备缓存空间大小
#define PERIPH_UNCACHED_BASE    (PERIPH_CACHED_BASE + PERIPH_CACHED_SIZE)//外围设备未缓存空间大小
#define PERIPH_UNCACHED_SIZE    PERIPH_PMM_SIZE //外围设备未缓存空间大小

#define IO_DEVICE_ADDR(paddr)        (paddr - PERIPH_PMM_BASE + PERIPH_DEVICE_BASE)//获取IO设备地址
#define IO_CACHED_ADDR(paddr)        (paddr - PERIPH_PMM_BASE + PERIPH_CACHED_BASE)//获取IO设备缓存地址
#define IO_UNCACHED_ADDR(paddr)      (paddr - PERIPH_PMM_BASE + PERIPH_UNCACHED_BASE)//获取IO设备未缓存地址
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
