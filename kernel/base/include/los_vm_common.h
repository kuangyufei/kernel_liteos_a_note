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
 * @date    2025-07-11
 */

#ifndef __LOS_VM_COMMON_H__
#define __LOS_VM_COMMON_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 用户地址空间配置区
 * @core 定义用户空间的基地址、大小及布局，默认位于内核空间下方，两侧各有16MB保护间隙
 * @note 保护间隙用于防止用户空间越界访问内核空间，增强系统安全性
 */
/* user address space, defaults to below kernel space with a 16MB guard gap on either side */
#ifndef USER_ASPACE_BASE
#define USER_ASPACE_BASE            ((vaddr_t)0x01000000UL)  /**< 用户地址空间基地址：物理内存起始地址 + 16MB保护间隙 */
#endif
#ifndef USER_ASPACE_SIZE
#define USER_ASPACE_SIZE            ((vaddr_t)KERNEL_ASPACE_BASE - USER_ASPACE_BASE - 0x01000000UL)  /**< 用户地址空间大小：内核空间基地址 - 用户空间基地址 - 16MB保护间隙 */
#endif

#define USER_ASPACE_TOP_MAX         ((vaddr_t)(USER_ASPACE_BASE + USER_ASPACE_SIZE))  /**< 用户地址空间最大上限地址 */
#define USER_HEAP_BASE              ((vaddr_t)(USER_ASPACE_TOP_MAX >> 2))             /**< 用户堆基地址：用户空间上限的1/4处 */
#define USER_MAP_BASE               ((vaddr_t)(USER_ASPACE_TOP_MAX >> 1))             /**< 用户映射区基地址：用户空间上限的1/2处 */
#define USER_MAP_SIZE               ((vaddr_t)(USER_ASPACE_SIZE >> 3))                /**< 用户映射区大小：用户空间总大小的1/8 */

/**
 * @brief 内存页配置及单位定义
 * @core 定义内存分页大小、掩码及常用存储单位换算
 */
#ifndef PAGE_SIZE
#define PAGE_SIZE                        (0x1000U)          /**< 内存页大小：4KB (0x1000 = 4096字节) */
#endif
#define PAGE_MASK                        (~(PAGE_SIZE - 1))  /**< 页对齐掩码：用于地址页对齐操作 (0xFFFFF000) */
#define PAGE_SHIFT                       (12)               /**< 页偏移位数：2^12 = 4096，与PAGE_SIZE对应 */

#define KB                               (1024UL)           /**< 千字节单位：1KB = 1024字节 */
#define MB                               (1024UL * 1024UL)  /**< 兆字节单位：1MB = 1024KB */
#define GB                               (1024UL * 1024UL * 1024UL)  /**< 吉字节单位：1GB = 1024MB */

/**
 * @brief 内存地址计算宏
 * @core 提供地址对齐、大小计算和边界检查的通用宏函数
 */
#define ROUNDUP(a, b)                    (((a) + ((b) - 1)) & ~((b) - 1))  /**< 向上取整到b的倍数，如ROUNDUP(5,4)=8 */
#define ROUNDDOWN(a, b)                  ((a) & ~((b) - 1))                /**< 向下取整到b的倍数，如ROUNDDOWN(5,4)=4 */
#define ROUNDOFFSET(a, b)                ((a) & ((b) - 1))                  /**< 计算地址a相对于b的偏移量，如ROUNDOFFSET(5,4)=1 */
#define MIN2(a, b)                       (((a) < (b)) ? (a) : (b))         /**< 返回两个数中的较小值 */

#define IS_ALIGNED(a, b)                 (!(((UINTPTR)(a)) & (((UINTPTR)(b)) - 1)))  /**< 检查地址a是否按b字节对齐 */
#define IS_PAGE_ALIGNED(x)               IS_ALIGNED(x, PAGE_SIZE)          /**< 检查地址x是否按页大小对齐 */
#define IS_SECTION_ALIGNED(x)            IS_ALIGNED(x, SECTION_SIZE)       /**< 检查地址x是否按段大小对齐 (需定义SECTION_SIZE) */

/**
 * @brief 虚拟内存管理错误码定义
 * @core 定义VM模块各类操作的错误返回值，便于问题定位
 * @note 错误码均为负值，0表示无错误
 */
#define LOS_ERRNO_VM_NO_ERROR            (0)                 /**< 无错误 */
#define LOS_ERRNO_VM_GENERIC             (-1)                /**< 通用错误 */
#define LOS_ERRNO_VM_NOT_FOUND           (-2)                /**< 未找到指定资源 */
#define LOS_ERRNO_VM_NOT_READY           (-3)                /**< 资源未就绪 */
#define LOS_ERRNO_VM_NO_MSG              (-4)                /**< 无消息可用 */
#define LOS_ERRNO_VM_NO_MEMORY           (-5)                /**< 内存不足 */
#define LOS_ERRNO_VM_ALREADY_STARTED     (-6)                /**< 已启动 */
#define LOS_ERRNO_VM_NOT_VALID           (-7)                /**< 无效状态 */
#define LOS_ERRNO_VM_INVALID_ARGS        (-8)                /**< 参数无效 */
#define LOS_ERRNO_VM_NOT_ENOUGH_BUFFER   (-9)                /**< 缓冲区不足 */
#define LOS_ERRNO_VM_NOT_SUSPENDED       (-10)               /**< 未处于挂起状态 */
#define LOS_ERRNO_VM_OBJECT_DESTROYED    (-11)               /**< 对象已销毁 */
#define LOS_ERRNO_VM_NOT_BLOCKED         (-12)               /**< 未阻塞 */
#define LOS_ERRNO_VM_TIMED_OUT           (-13)               /**< 超时 */
#define LOS_ERRNO_VM_ALREADY_EXISTS      (-14)               /**< 已存在 */
#define LOS_ERRNO_VM_CHANNEL_CLOSED      (-15)               /**< 通道已关闭 */
#define LOS_ERRNO_VM_OFFLINE             (-16)               /**< 离线状态 */
#define LOS_ERRNO_VM_NOT_ALLOWED         (-17)               /**< 操作不允许 */
#define LOS_ERRNO_VM_BAD_PATH            (-18)               /**< 路径无效 */
#define LOS_ERRNO_VM_ALREADY_MOUNTED     (-19)               /**< 已挂载 */
#define LOS_ERRNO_VM_IO                  (-20)               /**< I/O错误 */
#define LOS_ERRNO_VM_NOT_DIR             (-21)               /**< 非目录 */
#define LOS_ERRNO_VM_NOT_FILE            (-22)               /**< 非文件 */
#define LOS_ERRNO_VM_RECURSE_TOO_DEEP    (-23)               /**< 递归过深 */
#define LOS_ERRNO_VM_NOT_SUPPORTED       (-24)               /**< 不支持的操作 */
#define LOS_ERRNO_VM_TOO_BIG             (-25)               /**< 尺寸过大 */
#define LOS_ERRNO_VM_CANCELLED           (-26)               /**< 已取消 */
#define LOS_ERRNO_VM_NOT_IMPLEMENTED     (-27)               /**< 未实现 */
#define LOS_ERRNO_VM_CHECKSUM_FAIL       (-28)               /**< 校验和失败 */
#define LOS_ERRNO_VM_CRC_FAIL            (-29)               /**< CRC校验失败 */
#define LOS_ERRNO_VM_CMD_UNKNOWN         (-30)               /**< 未知命令 */
#define LOS_ERRNO_VM_BAD_STATE           (-31)               /**< 状态错误 */
#define LOS_ERRNO_VM_BAD_LEN             (-32)               /**< 长度错误 */
#define LOS_ERRNO_VM_BUSY                (-33)               /**< 资源忙 */
#define LOS_ERRNO_VM_THREAD_DETACHED     (-34)               /**< 线程已分离 */
#define LOS_ERRNO_VM_I2C_NACK            (-35)               /**< I2C无应答 */
#define LOS_ERRNO_VM_ALREADY_EXPIRED     (-36)               /**< 已过期 */
#define LOS_ERRNO_VM_OUT_OF_RANGE        (-37)               /**< 超出范围 */
#define LOS_ERRNO_VM_NOT_CONFIGURED      (-38)               /**< 未配置 */
#define LOS_ERRNO_VM_NOT_MOUNTED         (-39)               /**< 未挂载 */
#define LOS_ERRNO_VM_FAULT               (-40)               /**< 内存访问错误 */
#define LOS_ERRNO_VM_NO_RESOURCES        (-41)               /**< 无可用资源 */
#define LOS_ERRNO_VM_BAD_HANDLE          (-42)               /**< 句柄无效 */
#define LOS_ERRNO_VM_ACCESS_DENIED       (-43)               /**< 访问被拒绝 */
#define LOS_ERRNO_VM_PARTIAL_WRITE       (-44)               /**< 部分写入 */
#define LOS_ERRNO_VM_LOCK                (-45)               /**< 锁定失败 */
#define LOS_ERRNO_VM_MAP_FAILED          (-46)               /**< 映射失败 */

/**
 * @brief VM模块错误打印宏
 * @core 输出VM模块错误信息，包含函数名、行号和调试信息
 * @param[in] args 可变参数，调试信息格式化字符串
 * @note 仅当PRINT_LEVEL为LOS_DEBUG_LEVEL时输出详细调试信息
 */
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
