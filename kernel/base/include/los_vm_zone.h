/*!
 * @file    los_vm_zone.h
 * @brief
 * @link
   @verbatim
   虚拟地址空间全景图 从 0x00000000U 至 0xFFFFFFFFU ,外设和主存采用统一编址方式 @note_pic
   鸿蒙源码分析系列篇: http://weharmonyos.com | https://my.oschina.net/weharmony 		   
   
   +----------------------------+ 0xFFFFFFFFU
   |  IO设备未缓存 				  	|
   |  PERIPH_PMM_SIZE			|
   +----------------------------+ 外围设备未缓存基地址 PERIPH_UNCACHED_BASE
   |  IO设备缓存					|
   |  PERIPH_PMM_SIZE			|
   +----------------------------+ 外围设备缓存基地址 PERIPH_CACHED_BASE
   |  包括 IO设备					|
   |  PERIPH_PMM_SIZE			|
   +----------------------------+ 外围设备基地址 PERIPH_DEVICE_BASE
   |  Vmalloc 空间				|
   |  内核栈 内核堆   				|内核动态分配空间
   |  128M						|
   |  映射区 					  	|
   +----------------------------+ 内核动态分配开始地址 VMALLOC_START
   |   DDR_MEM_SIZE 			|
   |   Uncached段				|
   +----------------------------+ 未缓存虚拟空间基地址 UNCACHED_VMM_BASE = KERNEL_VMM_BASE + KERNEL_VMM_SIZE
   |   内核虚拟空间					|
   |   mmu table 临时页表			|临时页表的作用详见开机阶段汇编代码注释
   |   .bss					  	|Block Started by Symbol : 未初始化的全局变量,内核映射页表所在区 g_firstPageTable,这个表在内核启动后更新 
   |   .data 可读写数据区 			| 
   |   .rodata 只读数据区			|
   |   .text 代码区				|
   |  vectors 中断向量表 			|
   +----------------------------+ 内核空间开始地址 KERNEL_ASPACE_BASE = KERNEL_VMM_BASE
   |	16M预留区				  	|
   +----------------------------+ 用户空间栈顶 USER_ASPACE_TOP_MAX = USER_ASPACE_BASE + USER_ASPACE_SIZE
   |							|
   |	用户空间					|
   |	USER_ASPACE_SIZE		|
   |	用户栈区(stack) 			|
   |	映射区(map)				|
   |	堆区 (heap)				|
   |	.bss					|
   |	.data .text 			|
   +----------------------------+ 用户空间开始地址 USER_ASPACE_BASE
   |	16M预留区					|
   +----------------------------+ 0x00000000U
   
   以下定义 可见于 ..\vendor\hi3516dv300\config\board\include\board.h

   在liteos_a中, KERNEL_VADDR_BASE 是一个很常用的地址, 可以叫内核的运行起始地址
   内核的运行地址就是内核设计者希望内核运行时在内存中的位置，这个地址在内核源码中有地方可以配置，
   并且链接脚本里也会用到这个地址，编译代码时所用到的跟地址相关的值都是以内核运行基址为基础进行计算的。
   在liteos_a中，内核运行基址是在各个板子的board.h中配置的

   
	#ifdef LOSCFG_KERNEL_MMU
	#ifdef LOSCFG_TEE_ENABLE
	#define KERNEL_VADDR_BASE		0x41000000
	#else
	#define KERNEL_VADDR_BASE		0x40000000
	#endif
	#else
	#define KERNEL_VADDR_BASE		DDR_MEM_ADDR
	#endif
	#define KERNEL_VADDR_SIZE		DDR_MEM_SIZE

	#define SYS_MEM_BASE			DDR_MEM_ADDR
	#define SYS_MEM_END 			(SYS_MEM_BASE + SYS_MEM_SIZE_DEFAULT)

	#define EXC_INTERACT_MEM_SIZE	0x100000
   
   内核空间范围: 0x40000000 ~ 0xFFFFFFFF
   用户空间氛围: 0x00000000 ~ 0x3FFFFFFF
   
   cached地址和uncached地址的区别是
   对cached地址的访问是委托给CPU进行的，也就是说你的操作到底是提交给真正的外设或内存，还是转到CPU缓存，
   是由CPU决定的。CPU有一套缓存策略来决定什么时候从缓存中读取数据，什么时候同步缓存。
   对unchached地址的访问是告诉CPU忽略缓存，访问操作直接反映到外设或内存上。 
   对于IO设备一定要用uncached地址访问，是因为你的IO输出操作肯定是希望立即反映到IO设备上，不希望让CPU缓存你的操作；
   另一方面，IO设备的状态是独立于CPU的，也就是说IO口状态的改变CPU是不知道，这样就导致缓存和外设的内容不一致，
   你从IO设备读取数据时，肯定是希望直接读取IO设备的当前状态，而不是CPU缓存的过期值。

   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-30
 */
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
 * @defgroup kernel_vm_addr 内核虚拟地址空间配置
 * @brief 定义内核虚拟地址空间的基础参数，依赖MMU和TEE使能状态
 * @{ 
 */
#ifdef LOSCFG_KERNEL_MMU          /* 若使能内存管理单元(MMU) */
#ifdef LOSCFG_TEE_ENABLE          /* 若使能可信执行环境(TEE) */
#define KERNEL_VADDR_BASE       0x41000000  /*!< 内核虚拟地址基址：65MB (0x41000000 = 65 * 1024 * 1024) */
#else
#define KERNEL_VADDR_BASE       0x40000000  /*!< 内核虚拟地址基址：64MB (0x40000000 = 64 * 1024 * 1024) */
#endif
#else
#define KERNEL_VADDR_BASE       DDR_MEM_ADDR /*!< 无MMU时，内核虚拟地址直接映射物理DDR地址 */
#endif
#define KERNEL_VADDR_SIZE       DDR_MEM_SIZE /*!< 内核虚拟地址空间大小：等于DDR物理内存大小 */
/** @} */

/**
 * @defgroup system_mem 系统内存范围定义
 * @brief 定义物理内存的基础地址和范围
 * @{ 
 */
#define SYS_MEM_BASE            DDR_MEM_ADDR        /*!< 系统物理内存基址：等于DDR物理起始地址 */
#define SYS_MEM_END             (SYS_MEM_BASE + SYS_MEM_SIZE_DEFAULT) /*!< 系统物理内存结束地址 */
/** @} */

/**
 * @defgroup type_utils 类型转换工具宏
 * @brief 提供无符号32位类型转换功能
 * @{ 
 */
#define _U32_C(X)  X##U         /*!< 内部宏：为数值添加无符号后缀U */
#define U32_C(X)   _U32_C(X)    /*!< 无符号32位类型转换宏：将X转换为uint32_t类型 */
/** @} */

/**
 * @defgroup kernel_vmm 内核虚拟内存管理(VMM)配置
 * @brief 定义内核虚拟内存管理器的地址空间参数
 * @{ 
 */
#define KERNEL_VMM_BASE         U32_C(KERNEL_VADDR_BASE) /*!< 内核VMM基址：带类型转换的内核虚拟地址基址 */
#define KERNEL_VMM_SIZE         U32_C(KERNEL_VADDR_SIZE) /*!< 内核VMM大小：带类型转换的内核虚拟地址空间大小 */

#define KERNEL_ASPACE_BASE      KERNEL_VMM_BASE         /*!< 内核地址空间基址：与VMM基址相同 */
#define KERNEL_ASPACE_SIZE      KERNEL_VMM_SIZE         /*!< 内核地址空间大小：与VMM大小相同 */
/** @} */

/**
 * @defgroup uncached_vmm 非缓存虚拟内存配置
 * @brief 定义非缓存区域的虚拟地址空间参数
 * @{ 
 */
#define UNCACHED_VMM_BASE       (KERNEL_VMM_BASE + KERNEL_VMM_SIZE) /*!< 非缓存VMM基址：紧跟内核VMM之后 */
#define UNCACHED_VMM_SIZE       DDR_MEM_SIZE                        /*!< 非缓存VMM大小：等于DDR物理内存大小 */
/** @} */

/**
 * @defgroup vmalloc_region 动态虚拟内存区域
 * @brief 定义内核动态分配虚拟内存的地址范围
 * @{ 
 */
#define VMALLOC_START           (UNCACHED_VMM_BASE + UNCACHED_VMM_SIZE) /*!< VMALLOC起始地址：紧跟非缓存VMM之后 */
#define VMALLOC_SIZE            0x08000000                              /*!< VMALLOC区域大小：128MB (0x08000000 = 128 * 1024 * 1024) */
/** @} */

/**
 * @defgroup peripheral_addr 外设地址空间配置
 * @brief 定义外设寄存器的虚拟地址映射参数，区分MMU使能状态
 * @{ 
 */
#ifdef LOSCFG_KERNEL_MMU          /* 若使能MMU */
#define PERIPH_DEVICE_BASE      (VMALLOC_START + VMALLOC_SIZE) /*!< 外设设备基址：紧跟VMALLOC区域之后 */
#define PERIPH_DEVICE_SIZE      U32_C(PERIPH_PMM_SIZE)         /*!< 外设设备空间大小：带类型转换的外设物理内存大小 */
#define PERIPH_CACHED_BASE      (PERIPH_DEVICE_BASE + PERIPH_DEVICE_SIZE) /*!< 外设缓存基址：紧跟设备区域之后 */
#define PERIPH_CACHED_SIZE      U32_C(PERIPH_PMM_SIZE)         /*!< 外设缓存空间大小：等于外设物理内存大小 */
#define PERIPH_UNCACHED_BASE    (PERIPH_CACHED_BASE + PERIPH_CACHED_SIZE) /*!< 外设非缓存基址：紧跟缓存区域之后 */
#define PERIPH_UNCACHED_SIZE    U32_C(PERIPH_PMM_SIZE)         /*!< 外设非缓存空间大小：等于外设物理内存大小 */
#else
/* 无MMU时，外设虚拟地址直接映射物理地址 */
#define PERIPH_DEVICE_BASE      PERIPH_PMM_BASE                /*!< 外设设备基址：等于外设物理基址 */
#define PERIPH_DEVICE_SIZE      U32_C(PERIPH_PMM_SIZE)         /*!< 外设设备空间大小：带类型转换的外设物理内存大小 */
#define PERIPH_CACHED_BASE      PERIPH_PMM_BASE                /*!< 外设缓存基址：等于外设物理基址 */
#define PERIPH_CACHED_SIZE      U32_C(PERIPH_PMM_SIZE)         /*!< 外设缓存空间大小：等于外设物理内存大小 */
#define PERIPH_UNCACHED_BASE    PERIPH_PMM_BASE                /*!< 外设非缓存基址：等于外设物理基址 */
#define PERIPH_UNCACHED_SIZE    U32_C(PERIPH_PMM_SIZE)         /*!< 外设非缓存空间大小：等于外设物理内存大小 */
#endif
/** @} */

/**
 * @defgroup io_addr_convert I/O地址转换宏
 * @brief 提供外设物理地址到各类虚拟地址的转换
 * @{ 
 */
#define IO_DEVICE_ADDR(paddr)        ((paddr) - PERIPH_PMM_BASE + PERIPH_DEVICE_BASE)  /*!< 外设物理地址转设备虚拟地址 */
#define IO_CACHED_ADDR(paddr)        ((paddr) - PERIPH_PMM_BASE + PERIPH_CACHED_BASE)  /*!< 外设物理地址转缓存虚拟地址 */
#define IO_UNCACHED_ADDR(paddr)      ((paddr) - PERIPH_PMM_BASE + PERIPH_UNCACHED_BASE)/*!< 外设物理地址转非缓存虚拟地址 */
/** @} */

/**
 * @defgroup mem_addr_convert 内存地址转换宏
 * @brief 提供DDR物理地址到虚拟地址的转换
 * @{ 
 */
#define MEM_CACHED_ADDR(paddr)       ((paddr) - DDR_MEM_ADDR + KERNEL_VMM_BASE)  /*!< DDR物理地址转缓存虚拟地址 */
#define MEM_UNCACHED_ADDR(paddr)     ((paddr) - DDR_MEM_ADDR + UNCACHED_VMM_BASE)/*!< DDR物理地址转非缓存虚拟地址 */
/** @} */

/**
 * @defgroup vmm_addr_convert VMM地址转换宏
 * @brief 提供不同缓存属性的虚拟地址之间的转换
 * @{ 
 */
#define VMM_TO_UNCACHED_ADDR(vaddr)  ((vaddr) - KERNEL_VMM_BASE + UNCACHED_VMM_BASE) /*!< 缓存虚拟地址转非缓存虚拟地址 */
#define UNCACHED_TO_VMM_ADDR(vaddr)  ((vaddr) - UNCACHED_VMM_BASE + KERNEL_VMM_BASE) /*!< 非缓存虚拟地址转缓存虚拟地址 */
/** @} */

/**
 * @defgroup dma_addr_convert DMA地址转换宏
 * @brief 提供虚拟地址与DMA物理地址之间的转换
 * @{ 
 */
#define VMM_TO_DMA_ADDR(vaddr)  ((vaddr) - KERNEL_VMM_BASE + SYS_MEM_BASE) /*!< 虚拟地址转DMA物理地址 */
#define DMA_TO_VMM_ADDR(vaddr)  ((vaddr) - SYS_MEM_BASE + KERNEL_VMM_BASE) /*!< DMA物理地址转虚拟地址 */
/** @} */

#if (PERIPH_UNCACHED_BASE >= (0xFFFFFFFFU - PERIPH_UNCACHED_SIZE))
#error "Kernel virtual memory space has overflowed!"
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
