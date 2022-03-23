/*!
 * @file    los_vm_common.h
 * @brief
 * @link
   @verbatim
   @note_pic
   鸿蒙虚拟内存-用户空间图 从 USER_ASPACE_BASE 至 USER_ASPACE_TOP_MAX
   鸿蒙源码分析系列篇: 	https://blog.csdn.net/kuangyufei 
					   https://my.oschina.net/u/3751245
   
   |		   /\			   |
   |		   ||			   |
   |---------------------------|内核空间结束位置KERNEL_ASPACE_BASE + KERNEL_ASPACE_SIZE
   |						   |
   |	   内核空间 			   |
   |						   |
   |						   |
   |---------------------------|内核空间开始位置 KERNEL_ASPACE_BASE
   |						   |
   |	   16M 预留			   |
   |---------------------------|用户空间栈顶 USER_ASPACE_TOP_MAX = USER_ASPACE_BASE + USER_ASPACE_SIZE
   |						   |	   
   |	   stack区 自上而下	   	   |
   |						   |
   |		   ||			   |
   |		   ||			   |
   |		   ||			   |
   |		   \/			   |
   |						   |
   |---------------------------|映射区结束位置 USER_MAP_BASE + USER_MAP_SIZE
   | 映射区 (文件,匿名,I/O映射)        |
   |	      		           |
   |						   |
   |	共享库 .so				   |   
   |						   |
   |	L1/L2页表				   |   
   |---------------------------|映射区开始位置 USER_MAP_BASE = (USER_ASPACE_TOP_MAX >> 1)
   |						   |
   |						   |
   |		   /\			   |
   |		   ||			   |
   |		   ||			   |
   |		   ||			   |
   |						   |
   |	   heap区 自下而上	   	   |
   |						   |
   |---------------------------|用户空间堆区开始位置 USER_HEAP_BASE = USER_ASPACE_TOP_MAX >> 2
   |						   |
   |	   .bss 			   |
   |	   .data			   |
   |	   .text			   |
   |---------------------------|用户空间开始位置 USER_ASPACE_BASE = 0x01000000UL
   |						   |
   |	   16M预留			   |
   |---------------------------|虚拟内存开始位置 0x00000000

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

/**
 * @defgroup los_vm_map vm common definition
 * @ingroup kernel
 */

#ifndef __LOS_VM_COMMON_H__
#define __LOS_VM_COMMON_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */


/* user address space, defaults to below kernel space with a 16MB guard gap on either side */
#ifndef USER_ASPACE_BASE ///< 用户地址空间，默认为低于内核空间，两侧各有16MB的保护间隙
#define USER_ASPACE_BASE            ((vaddr_t)0x01000000UL) ///< 用户空间基地址 从16M位置开始 user_ram : ORIGIN = 0x1000000, LENGTH = 0x100000
#endif
#ifndef USER_ASPACE_SIZE
#define USER_ASPACE_SIZE            ((vaddr_t)KERNEL_ASPACE_BASE - USER_ASPACE_BASE - 0x01000000UL)///< 用户空间 < 内核空间 2个16M
#endif

#define USER_ASPACE_TOP_MAX         ((vaddr_t)(USER_ASPACE_BASE + USER_ASPACE_SIZE))///< 用户空间顶部位置
#define USER_HEAP_BASE              ((vaddr_t)(USER_ASPACE_TOP_MAX >> 2))			///< 堆的开始地址
#define USER_MAP_BASE               ((vaddr_t)(USER_ASPACE_TOP_MAX >> 1))			///< 用户映射区开始地址
#define USER_MAP_SIZE               ((vaddr_t)(USER_ASPACE_SIZE >> 3))				///< 用户空间映射大小 = 1/8 用户空间

#ifndef PAGE_SIZE
#define PAGE_SIZE                        (0x1000U) ///<  页大小4K
#endif
#define PAGE_MASK                        (~(PAGE_SIZE - 1)) ///< 页掩码,用于页内偏移地址的计算
#define PAGE_SHIFT                       (12)///<  12位 - 4K 偏移

#define KB                               (1024UL)
#define MB                               (1024UL * 1024UL)
#define GB                               (1024UL * 1024UL * 1024UL)

/**
 * @brief 
 * @verbatim
    圆整通常被理解为为满足某种要求而进行的数据修正。按照修正后的数据在数值上是否比原数据大，
    又可分为向上圆整和向下圆整。它们很像对模拟信号进行采样，对一定范围的数据向一个固定的数据靠拢。
    举例理解: 
    	ROUNDUP(7,4) = 8 		,ROUNDUP(8,4) = 8 		,ROUNDUP(9,4) = 12  
        ROUNDDOWN(7,4) = 4 	,ROUNDDOWN(8,4) = 8 	,ROUNDDOWN(9,4) = 8
        ROUNDOFFSET(7,4) = 3	,ROUNDOFFSET(8,4) = 0	,ROUNDOFFSET(9,4) = 1
    
 * @endverbatim
 */
#define ROUNDUP(a, b)                    (((a) + ((b) - 1)) & ~((b) - 1))	///< 向上圆整
#define ROUNDDOWN(a, b)                  ((a) & ~((b) - 1))	///< 向下圆整
#define ROUNDOFFSET(a, b)                ((a) & ((b) - 1))	///< 圆整偏移
#define MIN2(a, b)                       (((a) < (b)) ? (a) : (b))	///< 找到最小值

#define IS_ALIGNED(a, b)                 (!(((UINTPTR)(a)) & (((UINTPTR)(b)) - 1)))///< 是否按指定的参数对齐
#define IS_PAGE_ALIGNED(x)               IS_ALIGNED(x, PAGE_SIZE) ///< 是否按页大小对齐 4K
#define IS_SECTION_ALIGNED(x)            IS_ALIGNED(x, SECTION_SIZE)///< 是否按段大小对齐
//虚拟内存的异常提示
#define LOS_ERRNO_VM_NO_ERROR            (0)
#define LOS_ERRNO_VM_GENERIC             (-1)
#define LOS_ERRNO_VM_NOT_FOUND           (-2)
#define LOS_ERRNO_VM_NOT_READY           (-3)
#define LOS_ERRNO_VM_NO_MSG              (-4)
#define LOS_ERRNO_VM_NO_MEMORY           (-5)
#define LOS_ERRNO_VM_ALREADY_STARTED     (-6)
#define LOS_ERRNO_VM_NOT_VALID           (-7)
#define LOS_ERRNO_VM_INVALID_ARGS        (-8)
#define LOS_ERRNO_VM_NOT_ENOUGH_BUFFER   (-9)
#define LOS_ERRNO_VM_NOT_SUSPENDED       (-10)
#define LOS_ERRNO_VM_OBJECT_DESTROYED    (-11)
#define LOS_ERRNO_VM_NOT_BLOCKED         (-12)
#define LOS_ERRNO_VM_TIMED_OUT           (-13)
#define LOS_ERRNO_VM_ALREADY_EXISTS      (-14)
#define LOS_ERRNO_VM_CHANNEL_CLOSED      (-15)
#define LOS_ERRNO_VM_OFFLINE             (-16)
#define LOS_ERRNO_VM_NOT_ALLOWED         (-17)
#define LOS_ERRNO_VM_BAD_PATH            (-18)
#define LOS_ERRNO_VM_ALREADY_MOUNTED     (-19)
#define LOS_ERRNO_VM_IO                  (-20)
#define LOS_ERRNO_VM_NOT_DIR             (-21)
#define LOS_ERRNO_VM_NOT_FILE            (-22)
#define LOS_ERRNO_VM_RECURSE_TOO_DEEP    (-23)
#define LOS_ERRNO_VM_NOT_SUPPORTED       (-24)
#define LOS_ERRNO_VM_TOO_BIG             (-25)
#define LOS_ERRNO_VM_CANCELLED           (-26)
#define LOS_ERRNO_VM_NOT_IMPLEMENTED     (-27)
#define LOS_ERRNO_VM_CHECKSUM_FAIL       (-28)
#define LOS_ERRNO_VM_CRC_FAIL            (-29)
#define LOS_ERRNO_VM_CMD_UNKNOWN         (-30)
#define LOS_ERRNO_VM_BAD_STATE           (-31)
#define LOS_ERRNO_VM_BAD_LEN             (-32)
#define LOS_ERRNO_VM_BUSY                (-33)
#define LOS_ERRNO_VM_THREAD_DETACHED     (-34)
#define LOS_ERRNO_VM_I2C_NACK            (-35)
#define LOS_ERRNO_VM_ALREADY_EXPIRED     (-36)
#define LOS_ERRNO_VM_OUT_OF_RANGE        (-37)
#define LOS_ERRNO_VM_NOT_CONFIGURED      (-38)
#define LOS_ERRNO_VM_NOT_MOUNTED         (-39)
#define LOS_ERRNO_VM_FAULT               (-40)
#define LOS_ERRNO_VM_NO_RESOURCES        (-41)
#define LOS_ERRNO_VM_BAD_HANDLE          (-42)
#define LOS_ERRNO_VM_ACCESS_DENIED       (-43)
#define LOS_ERRNO_VM_PARTIAL_WRITE       (-44)
#define LOS_ERRNO_VM_LOCK                (-45)
#define LOS_ERRNO_VM_MAP_FAILED          (-46)

#define VM_ERR(args...) do {                     \
    PRINT_ERR("%s %d ", __FUNCTION__, __LINE__); \
    if (PRINT_LEVEL == LOS_DEBUG_LEVEL) {        \
        PRINTK(args);                            \
    }                                            \
    PRINTK("\n");                                \
} while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_COMMON_H__ */
